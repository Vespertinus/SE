
#ifndef APP_NAV_NAV_MESH_ASSET_H
#define APP_NAV_NAV_MESH_ASSET_H 1

#include <algorithm>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include <ResourceHolder.h>
#include <StrID.h>
#include <NavMesh_generated.h>

namespace SE {

// ---------------------------------------------------------------------------
// NavMeshAsset — runtime resource over a baked .senm (the
// NavBake output; one per (world, agent class)).
//
// The verified FlatBuffers buffer stays resident: tile records point into it.
// Deviation from the drop-buffer house pattern, deliberate — the tile payload
// IS the bulk of the asset, and unpacked copies would double peak memory.
// NavigationSystem hands byte ranges to dtNavMesh::addTile as dtAlloc'd copies
// flagged DT_TILE_FREE_DATA, so Detour exclusively owns the tiles it holds
// while the asset outlives add/remove cycles (re-activation re-adds from the
// same bytes).
//
// Group ids are the bake's content-group ids — sub-level .sesc paths, the
// world terrain manifest path, or mode-B scene paths. They are the streaming
// keys: NavigationSystem::ActivateGroup/DeactivateGroup and the terrain paging
// (ActivateTerrainTile) resolve tiles through them.
// ---------------------------------------------------------------------------

class NavMeshAsset : public ResourceHolder {

public:
        // One baked tile; pData/size point into the resident buffer.
        struct STile {
                uint32_t        x = 0, y = 0;
                uint16_t        layer     = 0;
                uint32_t        group_idx = 0;
                uint32_t        poly_count = 0;      // dtMeshHeader::polyCount (ref-bit budget)
                const uint8_t * pData = nullptr;
                uint32_t        size  = 0;
        };

        // Authored off-mesh connection round-trip record (debug draw; the
        // traversable polys are embedded in the owning group's tile bytes).
        struct SLink {
                glm::vec3 start{};
                glm::vec3 end{};
                float     radius        = 0.5f;
                uint16_t  area          = 0;
                uint16_t  flags         = 1;
                bool      bidirectional = true;
                uint32_t  group_idx     = 0;
        };

        // Production: loads the .senm file. Throws on I/O or verification errors.
        NavMeshAsset(const std::string & sName, rid_t rid);
        // Inline (NavMeshHolder.mesh) / test: builds from in-memory .senm bytes.
        NavMeshAsset(const std::string & sName, rid_t rid, const void * pData, size_t sz);

        const std::string & AgentClass() const { return sAgentClass; }
        const glm::vec3 &   Origin() const { return vOrigin; }
        float               TileDim() const { return fTileDim; }
        uint32_t            TilesX() const { return tiles_x; }
        uint32_t            TilesY() const { return tiles_y; }
        /// Largest per-tile polygon count — the dtNavMeshParams::maxPolys floor.
        uint32_t            MaxTilePolys() const { return max_tile_polys; }

        const std::vector<STile> &       GetTiles() const { return vTiles; }
        const std::vector<SLink> &       GetLinks() const { return vLinks; }
        const std::vector<std::string> & GetAreas() const { return vAreas; }
        const std::vector<std::string> & GetGroups() const { return vGroups; }

        /// Area registry index (== dtQueryFilter cost slot); -1 when unknown.
        int AreaIndex(const std::string & sArea) const;
        /// Group index by bake group id; -1 when unknown.
        int GroupIndex(const std::string & sGroup) const;
        /// StrID fast path (distinct name — const char* would be ambiguous).
        int GroupIndexID(const StrID groupID) const {
                const auto it = mGroupLookup.find(groupID);
                return it != mGroupLookup.end() ? int(it->second) : -1;
        }
        /// Tile indices of one group in asset order (sorted (y, x, layer)) —
        /// the deterministic add order. Empty when group_idx is out of range.
        const std::vector<uint32_t> & GroupTiles(uint32_t group_idx) const;
        /// Tiles of the group whose nav-grid column (tx) / row (ty) falls in the
        /// inclusive ranges — binary search over the group's sorted list.
        /// fn(tile_idx) per hit; allocation-free.
        template <class TFn>
        void ForEachTileInRect(uint32_t group_idx, int32_t tx0, int32_t tx1,
                               int32_t ty0, int32_t ty1, TFn && fn) const {
                if (group_idx >= vGroupTiles.size()) { return; }
                if (fTileDim <= 0.f) { return; }

                tx0 = std::max(tx0, 0);
                ty0 = std::max(ty0, 0);
                tx1 = std::min(tx1, int32_t(tiles_x) - 1);
                ty1 = std::min(ty1, int32_t(tiles_y) - 1);
                if (tx0 > tx1 || ty0 > ty1) { return; }

                const std::vector<uint32_t> & vList = vGroupTiles[group_idx];

                // the group list is sorted by packed (y, x) — one binary search
                // per row, then a bounded forward walk
                for (int32_t ty = ty0; ty <= ty1; ++ty) {
                        const uint64_t key = PackCoord(tx0, ty);
                        auto it = std::lower_bound(vList.begin(), vList.end(), key,
                                                   [this](uint32_t tile_idx, uint64_t k) {
                                                           const STile & t = vTiles[tile_idx];
                                                           return PackCoord(int32_t(t.x), int32_t(t.y)) < k;
                                                   });
                        for (; it != vList.end(); ++it) {
                                const STile & t = vTiles[*it];
                                if (int32_t(t.y) != ty) { break; }
                                if (int32_t(t.x) > tx1) { break; }
                                fn(*it);
                        }
                }
        }
        /// Inclusive nav-grid column/row range covering the world XZ AABB.
        void RectForAABB(const glm::vec3 & vMin, const glm::vec3 & vMax,
                         int32_t & tx0, int32_t & tx1, int32_t & ty0, int32_t & ty1) const;

        std::string Str() const;

private:
        std::vector<uint8_t> vBuffer;   // verified .senm bytes; tiles point into it
        std::string          sAgentClass;
        glm::vec3            vOrigin{};
        float                fTileDim = 0.f;
        uint32_t             tiles_x = 0, tiles_y = 0;
        uint32_t             max_tile_polys = 0;
        std::vector<STile>   vTiles;
        std::vector<SLink>   vLinks;
        std::vector<std::string> vAreas;
        std::vector<std::string> vGroups;
        std::unordered_map<StrID, uint32_t> mGroupLookup;
        std::vector<std::vector<uint32_t>>  vGroupTiles;

        void Build(const SE::FlatBuffers::NavMeshAsset * pFB);
        void BuildFromBytes(const std::string & sSource, uint32_t nBytes);
        static uint64_t PackCoord(int32_t tx, int32_t ty) {
                return (uint64_t(uint32_t(ty)) << 32) | uint64_t(uint32_t(tx));
        }
};

} // namespace SE

#endif
