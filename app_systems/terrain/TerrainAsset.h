
#ifndef APP_TERRAIN_ASSET_H
#define APP_TERRAIN_ASSET_H 1

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

#include <ResourceHolder.h>
#include <Terrain_generated.h>
#include <world/ResourcePinCore.h>
#include <Logging.h>

namespace SE {

// Shared, immutable terrain manifest built from a .seterrain binary.
// All data is baked from FlatBuffers at construction; the raw buffer is discarded.
// Tile payloads (.setile) are NOT held here — they stream independently through
// TerrainSystem and decode into STileRuntime.
class TerrainAsset : public ResourceHolder {

public:
        struct STileRef {
                uint16_t    x = 0;
                uint16_t    y = 0;
                std::string sPath;
                glm::vec3   vBoundsMin{0.0f};
                glm::vec3   vBoundsMax{0.0f};
                float       height_min = 0.0f;
                float       height_max = 0.0f;
                uint16_t    flags = 0;
        };

        struct SMaterial {
                std::string sPath;
                float       uv_scale = 0.05f;
        };

        struct SPhysics {
                float friction      = 0.8f;
                float max_slope_deg = 50.0f;
        };

        // Production: loads the .seterrain file, builds tables, discards raw bytes.
        TerrainAsset(const std::string & sName, rid_t rid);

        // Test / inline embedding: builds tables from an in-memory manifest.
        TerrainAsset(const std::string & sName, rid_t rid, const FlatBuffers::Terrain * pFb);

        const std::string &            GetId()             const { return sId; }
        float                          GetTileSize()       const { return tile_size; }
        const glm::vec3 &              GetOrigin()         const { return vOrigin; }
        const std::vector<uint16_t> &  GetLodResolutions() const { return vLodResolutions; }
        uint32_t                       GetLodCount()       const { return vLodResolutions.size(); }
        uint16_t                       GetTilesX()         const { return tiles_x; }
        uint16_t                       GetTilesY()         const { return tiles_y; }
        const std::vector<STileRef> &  GetTiles()          const { return vTiles; }
        const SMaterial &              GetMaterial()       const { return oMaterial; }
        const SPhysics &               GetPhysics()        const { return oPhysics; }
        float                          GetHeightMin()      const { return height_min; }
        float                          GetHeightMax()      const { return height_max; }

        const STileRef * FindTile(uint16_t x, uint16_t y) const;
        /// Dense index into GetTiles(), or -1 when the grid coords are not in the manifest.
        int32_t TileIndexAt(uint16_t x, uint16_t y) const;

        static uint64_t PackCoords(uint16_t x, uint16_t y) {
                return (static_cast<uint64_t>(x) << 16) | static_cast<uint64_t>(y);
        }

        std::string Str() const;

private:
        void Build(const FlatBuffers::Terrain * pFb);

        std::string             sId;
        float                   tile_size = 0.0f;
        glm::vec3               vOrigin{0.0f};
        std::vector<uint16_t>   vLodResolutions;
        std::vector<STileRef>   vTiles;
        std::unordered_map<uint64_t, uint32_t> mTileLookup;
        SMaterial               oMaterial;
        SPhysics                oPhysics;
        float                   height_min = 0.0f;
        float                   height_max = 0.0f;
        uint16_t                tiles_x = 0;
        uint16_t                tiles_y = 0;
};

// Declared-dependency pin (lives here, next to the resource: an explicit
// specialization compiles in every TU that includes this header).
template <> struct TResourcePin<TerrainAsset> {
        using THolder    = FlatBuffers::TerrainHolder;
        using TKeyHolder = FlatBuffers::TerrainHolderT;

        static H<TerrainAsset> Create(const THolder * pH) {
                if (pH->path() && pH->path()->size() > 0) {
                        return CreateResource<TerrainAsset>(pH->path()->str());
                }
                if (pH->name() && pH->terrain()) {
                        return CreateResource<TerrainAsset>(pH->name()->str(), pH->terrain());
                }

                log_w("ResourcePin<TerrainAsset>: anonymous inline terrain cannot be pinned");
                return H<TerrainAsset>::Null();
        }
};

} // namespace SE

#endif
