
#ifndef APP_TERRAIN_QUERY_H
#define APP_TERRAIN_QUERY_H 1

#include <cmath>

#include <glm/glm.hpp>

#include <terrain/TileData.h>

namespace SE {
namespace TerrainQuery {

// Pure heightfield sampling over a decoded tile (STileRuntime). All functions take
// TILE-LOCAL coordinates: local (x, z) in [0, tile_size], where x maps to columns (j)
// and z to rows (i). The sample-grid convention — sample (i, j) at local
// (j*step, h, i*step), step = tile_size / (resolution - 1), vertices on cell corners —
// is shared with Jolt's HeightFieldShapeSettings and with the bake tool; changing it
// anywhere means changing it everywhere.

inline float SampleStep(uint16_t resolution, float tile_size) {

        return tile_size / static_cast<float>(resolution - 1);
}

// Bilinear height at tile-local (x, z) against a specific LOD. Returns false when
// the point is outside [0, tile_size]² (half-open at the max edge, matching how a
// tile grid tiles the plane) or the LOD is empty.
inline bool SampleHeight(const STileRuntime & tile, uint32_t lod, float local_x, float local_z, float & out_height) {

        if (lod >= tile.vLods.size()) return false;
        const SDecodedLod & oLod = tile.vLods[lod];
        if (oLod.resolution < 2 || oLod.vHeights.empty()) return false;

        const float step = SampleStep(oLod.resolution, tile.tile_size);
        if (!(local_x >= 0.0f && local_x < tile.tile_size && local_z >= 0.0f && local_z < tile.tile_size)) {
                return false;
        }

        const float fx = local_x / step;
        const float fz = local_z / step;
        const uint32_t j0 = std::min(static_cast<uint32_t>(fx), oLod.resolution - 2u);
        const uint32_t i0 = std::min(static_cast<uint32_t>(fz), oLod.resolution - 2u);
        const float tx = fx - static_cast<float>(j0);
        const float tz = fz - static_cast<float>(i0);

        const int16_t * pRow0 = oLod.vHeights.data() + static_cast<size_t>(i0) * oLod.resolution;
        const int16_t * pRow1 = pRow0 + oLod.resolution;

        const float h00 = tile.Dequantize(pRow0[j0]);
        const float h01 = tile.Dequantize(pRow0[j0 + 1]);
        const float h10 = tile.Dequantize(pRow1[j0]);
        const float h11 = tile.Dequantize(pRow1[j0 + 1]);

        const float h0 = h00 + (h01 - h00) * tx;
        const float h1 = h10 + (h11 - h10) * tx;
        out_height = h0 + (h1 - h0) * tz;
        return true;
}

// Convenience overload against LOD0 (the physics/query resolution).
inline bool SampleHeight(const STileRuntime & tile, float local_x, float local_z, float & out_height) {

        return SampleHeight(tile, 0, local_x, local_z, out_height);
}

// Nearest baked normal (LOD0 by default) — good enough for gameplay slope checks;
// exact at sample points, quantized in between.
inline bool SampleNormal(const STileRuntime & tile, uint32_t lod, float local_x, float local_z, glm::vec3 & out_normal) {

        if (lod >= tile.vLods.size()) return false;
        const SDecodedLod & oLod = tile.vLods[lod];
        if (oLod.resolution < 2 || oLod.vNormals.size() < oLod.resolution * oLod.resolution * 2) return false;

        const float step = SampleStep(oLod.resolution, tile.tile_size);
        if (!(local_x >= 0.0f && local_x < tile.tile_size && local_z >= 0.0f && local_z < tile.tile_size)) {
                return false;
        }

        const uint32_t j = std::min(static_cast<uint32_t>(local_x / step), oLod.resolution - 1u);
        const uint32_t i = std::min(static_cast<uint32_t>(local_z / step), oLod.resolution - 1u);
        const size_t idx = (static_cast<size_t>(i) * oLod.resolution + j) * 2;
        OctNormal16 o{oLod.vNormals[idx], oLod.vNormals[idx + 1]};
        out_normal = OctDecodeNormal(o);
        return true;
}

// Walkability check shared with the offline nav bake (heightfield mode):
// the cosine test against the max slope must agree between runtime and tool, so it
// lives here.
inline bool IsWalkable(const glm::vec3 & vNormal, float max_slope_deg) {

        const float cos_threshold = std::cos(max_slope_deg * 3.14159265358979323846 / 180.0);
        return vNormal.y >= cos_threshold;
}

} // namespace TerrainQuery
} // namespace SE

#endif
