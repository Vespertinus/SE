
#ifdef SE_IMPL

#include <nav/NavMeshAsset.h>

#include <DetourNavMesh.h>

#include <Logging.h>
#include <flatbuffers/flatbuffers.h>

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace SE {

// ---------------------------------------------------------------------------

NavMeshAsset::NavMeshAsset(const std::string & sName, rid_t rid)
        : ResourceHolder(rid, sName) {

        std::ifstream f(sName, std::ios::binary | std::ios::ate);
        if (!f.is_open()) {
                throw std::runtime_error(fmt::format("NavMeshAsset: failed to open '{}'", sName));
        }

        const size_t sz = static_cast<size_t>(f.tellg());
        f.seekg(0);
        vBuffer.resize(sz);
        f.read(reinterpret_cast<char *>(vBuffer.data()), static_cast<std::streamsize>(sz));

        BuildFromBytes(sName, uint32_t(sz));
}

NavMeshAsset::NavMeshAsset(const std::string & sName, rid_t rid, const void * pData, size_t sz)
        : ResourceHolder(rid, sName) {

        vBuffer.assign(static_cast<const uint8_t *>(pData),
                       static_cast<const uint8_t *>(pData) + sz);

        BuildFromBytes(fmt::format("{} (inline)", sName), uint32_t(sz));
}

void NavMeshAsset::BuildFromBytes(const std::string & sSource, const uint32_t nBytes) {

        flatbuffers::Verifier verifier(vBuffer.data(), vBuffer.size());
        if (!SE::FlatBuffers::VerifyNavMeshAssetBuffer(verifier)) {
                throw std::runtime_error(
                                fmt::format("NavMeshAsset: FlatBuffer verify failed for '{}'", sSource));
        }

        Build(SE::FlatBuffers::GetNavMeshAsset(vBuffer.data()));
        size = nBytes;

        log_d("NavMeshAsset: loaded '{}' ({} bytes, {} tiles, {} groups, {} areas, {} links)",
              sSource, nBytes, vTiles.size(), vGroups.size(), vAreas.size(), vLinks.size());
}

void NavMeshAsset::Build(const SE::FlatBuffers::NavMeshAsset * pFB) {

        if (!pFB || !pFB->agent_class() || pFB->agent_class()->size() == 0) {
                throw std::runtime_error(fmt::format("NavMeshAsset '{}': missing agent_class", sName));
        }
        if (!pFB->origin()) {
                throw std::runtime_error(fmt::format("NavMeshAsset '{}': missing origin", sName));
        }
        fTileDim = pFB->tile_dim();
        if (!(fTileDim > 0.f)) {
                throw std::runtime_error(fmt::format("NavMeshAsset '{}': tile_dim must be > 0", sName));
        }

        sAgentClass = pFB->agent_class()->str();
        vOrigin     = {pFB->origin()->x(), pFB->origin()->y(), pFB->origin()->z()};

        if (pFB->areas()) {
                vAreas.reserve(pFB->areas()->size());
                for (const auto * pArea : *pFB->areas()) {
                        vAreas.push_back(pArea ? pArea->str() : std::string());
                }
        }

        // Group ids in first-sight order (deterministic; the bake emits the same
        // order its input groups were iterated in).
        auto ResolveGroup = [this](const std::string & sGroup) -> uint32_t {
                const StrID key(sGroup);
                auto it = mGroupLookup.find(key);
                if (it != mGroupLookup.end()) { return it->second; }
                const uint32_t idx = uint32_t(vGroups.size());
                vGroups.push_back(sGroup);
                mGroupLookup.emplace(key, idx);
                return idx;
        };

        if (pFB->tiles()) {
                vTiles.reserve(pFB->tiles()->size());

                for (const auto * pTile : *pFB->tiles()) {
                        if (!pTile || !pTile->group() || !pTile->data()) {
                                throw std::runtime_error(
                                                fmt::format("NavMeshAsset '{}': tile missing group or data", sName));
                        }

                        const uint8_t * pBytes = pTile->data()->data();
                        const uint32_t  nBytes = pTile->data()->size();

                        // Tile bytes are raw dtCreateNavMeshData output: validate the
                        // embedded header and harvest the poly count for the ref-bit
                        // budget (NavigationSystem dtNavMeshParams::maxPolys floor).
                        if (nBytes < sizeof(dtMeshHeader)) {
                                throw std::runtime_error(fmt::format(
                                                "NavMeshAsset '{}': tile data truncated ({} bytes)", sName, nBytes));
                        }
                        const dtMeshHeader * pHeader = reinterpret_cast<const dtMeshHeader *>(pBytes);
                        if (pHeader->magic != DT_NAVMESH_MAGIC) {
                                throw std::runtime_error(
                                                fmt::format("NavMeshAsset '{}': tile magic mismatch", sName));
                        }
                        if (pHeader->version != DT_NAVMESH_VERSION) {
                                throw std::runtime_error(fmt::format(
                                                "NavMeshAsset '{}': tile version {} != supported {}",
                                                sName, pHeader->version, DT_NAVMESH_VERSION));
                        }

                        STile t;
                        t.x         = pTile->x();
                        t.y         = pTile->y();
                        t.layer     = pTile->layer();
                        t.group_idx = ResolveGroup(pTile->group()->str());
                        t.poly_count = uint32_t(pHeader->polyCount);
                        t.pData     = pBytes;
                        t.size      = nBytes;

                        // header/table cross-check — a corrupt or hand-edited bake
                        if (uint32_t(pHeader->x) != t.x || uint32_t(pHeader->y) != t.y
                            || uint16_t(pHeader->layer) != t.layer) {
                                throw std::runtime_error(fmt::format(
                                                "NavMeshAsset '{}': tile header ({},{},{}) disagrees with table ({},{},{})",
                                                sName, pHeader->x, pHeader->y, pHeader->layer, t.x, t.y, t.layer));
                        }

                        max_tile_polys = std::max(max_tile_polys, t.poly_count);
                        tiles_x = std::max(tiles_x, t.x + 1);
                        tiles_y = std::max(tiles_y, t.y + 1);
                        vTiles.push_back(t);
                }
        }

        if (pFB->links()) {
                for (const auto * pLink : *pFB->links()) {
                        if (!pLink || !pLink->start() || !pLink->end() || !pLink->group()) { continue; }
                        SLink l;
                        l.start = {pLink->start()->x(), pLink->start()->y(), pLink->start()->z()};
                        l.end   = {pLink->end()->x(), pLink->end()->y(), pLink->end()->z()};
                        l.radius        = pLink->radius();
                        l.area          = pLink->area();
                        l.flags         = pLink->flags();
                        l.bidirectional = pLink->bidirectional();
                        l.group_idx     = ResolveGroup(pLink->group()->str());
                        vLinks.push_back(l);
                }
        }

        // Per-group tile index lists — a subsequence of the (y, x, layer)-sorted
        // tile table, so each stays sorted (For.RectForAABB binary searches).
        vGroupTiles.resize(vGroups.size());
        for (uint32_t i = 0; i < uint32_t(vTiles.size()); ++i) {
                vGroupTiles[vTiles[i].group_idx].push_back(i);
        }
}

// ---------------------------------------------------------------------------

int NavMeshAsset::AreaIndex(const std::string & sArea) const {
        for (size_t i = 0; i < vAreas.size(); ++i) {
                if (vAreas[i] == sArea) { return int(i); }
        }
        return -1;
}

int NavMeshAsset::GroupIndex(const std::string & sGroup) const {
        const auto it = mGroupLookup.find(StrID(sGroup));
        return it != mGroupLookup.end() ? int(it->second) : -1;
}

const std::vector<uint32_t> & NavMeshAsset::GroupTiles(uint32_t group_idx) const {
        static const std::vector<uint32_t> oEmpty;
        return group_idx < vGroupTiles.size() ? vGroupTiles[group_idx] : oEmpty;
}

void NavMeshAsset::RectForAABB(const glm::vec3 & vMin, const glm::vec3 & vMax,
                               int32_t & tx0, int32_t & tx1, int32_t & ty0, int32_t & ty1) const {

        const float fInv = 1.f / fTileDim;
        tx0 = int32_t(std::floor((vMin.x - vOrigin.x) * fInv));
        tx1 = int32_t(std::ceil((vMax.x - vOrigin.x) * fInv)) - 1;
        ty0 = int32_t(std::floor((vMin.z - vOrigin.z) * fInv));
        ty1 = int32_t(std::ceil((vMax.z - vOrigin.z) * fInv)) - 1;

        // a boundary-exact degenerate extent still covers its containing tile
        if (tx1 < tx0) { tx1 = tx0; }
        if (ty1 < ty0) { ty1 = ty0; }
}

std::string NavMeshAsset::Str() const {
        return fmt::format("navmesh class '{}' tiles={} groups={} areas={}",
                           sAgentClass, vTiles.size(), vGroups.size(), vAreas.size());
}

} // namespace SE

#endif // SE_IMPL
