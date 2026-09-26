
#ifndef APP_NAV_NAV_BAKE_H
#define APP_NAV_NAV_BAKE_H 1

#include <cstdint>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace SE {
namespace NavBake {

// Pure tiled-navmesh bake library. Input: world-space geometry
// collected per content group. Output: finished Detour tile payloads (raw
// dtCreateNavMeshData bytes) + the round-trip link table, byte-deterministic.
//
// No file I/O happens here — .seworld/.sesc/.sems/.seterrain decoding is the
// offline tool's job, so tests feed inline fixtures directly. This mirrors the
// TerrainBake seam: the library is shared verbatim by tools/nav_bake and the
// functional tests, and byte-determinism is a tested contract (same inputs =>
// identical tile bytes).
//
// Grid conventions:
//   - Nav tiles form a world-space grid; tile (x, y) spans
//     [origin + x*tile_dim, origin + (x+1)*tile_dim) x [origin + y*tile_dim, ...)
//     in XZ. `origin` is snapped DOWN to a multiple of tile_dim from the content
//     bounds, so tile centers are stable across bakes.
//   - tile_dim must be an integer divisor of the streaming cell_size ("activate
//     sub-level => add its tiles" is a pure set operation on dtNavMesh).
//
// Tile ownership (the stage-plan §17.4 decision — bake-time resolution):
//   - A tile belongs to the one NORMAL group whose authored bounds contain the
//     tile center. Two normal groups claiming the same tile is a hard error.
//   - COEXISTING groups (Manual interiors) also claim their cells, but bake as
//     separate Detour tile LAYERS at the same (x, y): dtNavMesh keys occupancy
//     by (x, y, layer), so each coexisting group gets a distinct layer.
//     Normal groups exclude the coexisting groups' footprint volumes from their
//     own bake (auto-exclusion), and travel between spaces is via authored
//     links (the Skyrim door-cell model).
//
// Border sampling: a tile's build input is its own claim plus every same-flag
// group's geometry inside the build bbox (tile + borderSize ring). Detour links
// two grid-adjacent tiles only through portal edges on their shared boundary,
// and erosion pulls walkable space walkableRadius back from the edge of the
// build input — so a group seam links only when walkable data reaches past the
// boundary on BOTH sides. Partitioned inputs (terrain merged into disjoint
// sub-level boxes) provide that data to neither side alone; the neighbor's
// ground inside the border band does. Claims/activation stay with the owner —
// border data only makes seams link, it never moves tile ownership.
//
// Area bytes in the final tile data: 0 = null (excluded/unwalkable),
// 1 = ground (registry[0]), i + 1 = registry[i] (named areas). Exclusion
// volumes remove triangles at rasterization time; named area boxes tag polygons
// after the poly-mesh pass.

// ---------------------------------------------------------------------------

// One agent class = one .senm: Recast bakes agent dimensions into the walkable
// geometry itself, so per-class tile sets are the only sound encoding (runtime
// filters then select area costs within a class).
struct SAgentClass {

        std::string name = "default";
        float       radius        = 0.5f;   // clearance eroded around walkable space
        float       height        = 2.0f;   // walkable clearance under obstacles
        float       max_slope_deg = 50.0f;  // matches TerrainPhysics.max_slope_deg by default
        float       step_height   = 0.4f;   // max step-up between neighboring spans
};

struct SConfig {

        std::string world_id;                 // debug/logging + output naming hint
        SAgentClass agent;

        float tile_dim   = 16.0f;   // nav tile world size (XZ); must divide cell_size
        float cell_size  = 0.0f;    // streaming cell size; 0 = skip divisibility check
        float voxel_size   = 0.2f;  // rcConfig cellSize
        float voxel_height = 0.2f;  // rcConfig cellHeight

        // Recast region tuning (voxel² areas), terrain_bake-style passthrough
        float min_region_area    = 8.0f;
        float merge_region_area  = 20.0f;
        float max_contour_error  = 1.3f;
        float max_edge_len       = 12.0f;

        // Area-name registry as authored: [0] must be "" (ground); named areas
        // follow. Baked into the .senm for debug mapping; poly area bytes are
        // registry index + 1.
        std::vector<std::string> vAreas;
};

// World-space triangle soup of one group (already composed through node TRS and
// the sub-level instance transform by the caller).
struct SGeomMesh {

        std::vector<float>    vVertices;   // xyz triples
        std::vector<uint32_t> vIndices;    // triangle list
};

// One terrain tile decoded to LOD0, in the Terrain.fbs corner-grid convention:
// sample (i, j) at (origin.x + j*step, h, origin.z + i*step), step =
// tile_size / (resolution - 1). Heights are dequantized world heights.
struct SGeomHeightfield {

        glm::vec3            origin;      // world min corner of this terrain tile
        float                tile_size = 0.0f;
        uint32_t             resolution = 0;
        std::vector<float>   vHeights;    // resolution * resolution, row-major (i = Z rows)
        std::vector<uint8_t> vHoleMask;   // optional; bit per cell, 1 = hole (no span)
};

// Resolved off-mesh connection (world coordinates; end_node lookups already done).
struct SLink {

        glm::vec3 start{0.0f};
        glm::vec3 end{0.0f};
        float     radius        = 0.5f;
        uint32_t  registry_index = 0;    // area; byte = index + 1
        uint16_t  flags         = 1;     // walk
        bool      bidirectional = true;
};

// NavArea marker resolved against the group's node transforms.
struct SAreaBox {

        glm::vec3 center{0.0f};         // world
        glm::vec3 half_extents{0.0f};
        bool      exclusion = false;    // true: remove geometry inside (byte 0)
        uint32_t  registry_index = 0;   // named area (byte = index + 1); valid when !exclusion
};

// One content group: a sub-level (.sesc + instance transform applied), a
// world-level terrain grid, or an authored merge of both. Bounds are the
// authored world-space bounds (SubLevelRef / manifest extent).
struct SGroup {

        std::string id;                   // content-group id (streaming key)
        bool        coexisting = false;   // Manual interior: own tile layer at shared cells

        glm::vec3 bounds_min{0.0f};
        glm::vec3 bounds_max{0.0f};

        std::vector<SGeomMesh>       vMeshes;
        std::vector<SGeomHeightfield> vHeightfields;
        std::vector<SLink>           vLinks;
        std::vector<SAreaBox>       vAreaBoxes;

        // content AABB (meshes + heightfields), computed in Bake before claims;
        // heightfield-bearing groups claim per-tile from these extents so a
        // sparse tile set (world grid minus merged sub-levels) does not claim
        // its whole bounding box
        glm::vec3 content_min{0.0f};
        glm::vec3 content_max{0.0f};
        bool      bHasContent = false;
};

struct SBakeInput {

        std::vector<SGroup> vGroups;   // input order is the determinism order
};

struct SNavTileBytes {

        uint32_t             x = 0;
        uint32_t             y = 0;
        uint16_t             layer = 0;
        std::string          group;
        std::vector<uint8_t> vBytes;   // finished dtCreateNavMeshData payload
};

struct SLinkRecord {

        glm::vec3   start{0.0f};
        glm::vec3   end{0.0f};
        float       radius        = 0.5f;
        uint32_t    registry_index = 0;
        uint16_t    flags         = 1;
        bool        bidirectional = true;
        std::string group;
};

struct SOutput {

        glm::vec3 origin{0.0f};        // snapped grid origin (echo)
        float     tile_dim = 0.0f;
        uint32_t  tiles_x = 0;
        uint32_t  tiles_y = 0;

        std::vector<SNavTileBytes> vTiles;   // sorted (y, x, layer) — deterministic
        std::vector<SLinkRecord>   vLinks;   // input order, world coordinates

        uint32_t poly_count = 0;
        uint32_t vert_count = 0;
};

/// Bakes all tiles + the link table. Returns false (and logs) on config, input,
/// or tile-claim errors; `out` is only meaningful on success.
bool Bake(const SBakeInput & oIn, const SConfig & oCfg, SOutput & out);

} // namespace NavBake
} // namespace SE

#endif
