
#ifndef APP_TERRAIN_BAKE_H
#define APP_TERRAIN_BAKE_H 1

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace SE {
namespace TerrainBake {

// Pure heightfield bake library. Input: a global float height grid.
// Output: baked .setile payloads + the .seterrain manifest, byte-deterministic.
//
// No file I/O happens here — heightmap decoding (png16 / raw16 / procedural) is the
// offline tool's job, so tests can feed analytic surfaces directly. The library is
// also the seam runtime deformation will reuse to rebuild tiles.
//
// Grid conventions (must match Terrain.fbs / TerrainQuery.h):
//   - pHeights is row-major i*source_width + j; world x = columns, world z = rows.
//   - Sample (i, j) sits at world (origin.x + j*step, h, origin.z + i*step),
//     step = tile_size / lod0_cells — vertices on cell corners; adjacent tiles
//     share edge samples exactly.
//   - Per-LOD normals are computed on the GLOBAL grid at that LOD's stride (central
//     differences, one-sided at world edges) and only then sliced into tiles, so
//     normals match across tile seams at every LOD.

struct SConfig {

        std::string id = "terrain";
        uint32_t    source_width  = 0;   // global grid samples per row (X);
        uint32_t    source_height = 0;   // rows (Z)
        float       tile_size     = 512.0f;
        uint32_t    lod0_cells    = 128;  // cells per tile edge at LOD0
        uint32_t    lod_count     = 5;    // LOD i uses stride 2^i
        glm::vec3   origin{0.0f};

        // passthrough to the manifest
        std::string material_path;
        float       uv_scale     = 0.05f;
        float       friction     = 0.8f;
        float       max_slope_deg = 50.0f;
};

struct STileBytes {

        uint16_t             x = 0;
        uint16_t             y = 0;
        float                height_min = 0.0f;
        float                height_max = 0.0f;
        std::vector<uint8_t> vBytes;   // finished .setile flatbuffer
};

struct SOutput {

        std::vector<STileBytes> vTiles;   // row-major (y, x) order — deterministic
        std::vector<uint8_t>    vManifest;
        float                   height_min = 0.0f;
        float                   height_max = 0.0f;
        uint32_t                tiles_x = 0;
        uint32_t                tiles_y = 0;
        std::vector<uint16_t>   vLodResolutions;   // samples per edge per LOD
};

inline uint16_t LodResolution(uint32_t lod0_cells, uint32_t lod) {

        return static_cast<uint16_t>(lod0_cells / (1u << lod) + 1u);
}

/// Bakes all tiles + manifest. Returns false (and logs) on config or input errors.
bool Bake(const float * pHeights, const SConfig & oCfg, SOutput & out);

} // namespace TerrainBake
} // namespace SE

#endif
