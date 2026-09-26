
#ifndef APP_TERRAIN_TILE_DATA_H
#define APP_TERRAIN_TILE_DATA_H 1

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

namespace SE {

// Octahedral normal encoding — 2 bytes per world-space normal.
// Encoded octahedral coordinates are stored as two bytes in [-1, 1] mapped to [0, 255].
struct OctNormal16 {

        uint8_t u = 128;   // encoded x/y octahedral coordinate
        uint8_t v = 128;   // encoded z (wrap) octahedral coordinate
};

inline OctNormal16 OctEncodeNormal(const glm::vec3 & vNormal) {

        const float len = glm::abs(vNormal.x) + glm::abs(vNormal.y) + glm::abs(vNormal.z);
        glm::vec2 o = len > 0.0f
                ? glm::vec2(vNormal.x, vNormal.z) / len
                : glm::vec2(0.0f);
        // wrap the lower hemisphere into the square
        if (vNormal.y <= 0.0f) {
                o = glm::vec2(1.0f - glm::abs(o.y), 1.0f - glm::abs(o.x))
                    * glm::vec2(o.x >= 0.0f ? 1.0f : -1.0f, o.y >= 0.0f ? 1.0f : -1.0f);
        }
        OctNormal16 out;
        out.u = static_cast<uint8_t>(glm::clamp(o.x * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f + 0.5f);
        out.v = static_cast<uint8_t>(glm::clamp(o.y * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f + 0.5f);
        return out;
}

inline glm::vec3 OctDecodeNormal(const OctNormal16 & o) {

        glm::vec2 e(o.u / 255.0f * 2.0f - 1.0f, o.v / 255.0f * 2.0f - 1.0f);
        glm::vec3 n(e.x, 1.0f - glm::abs(e.x) - glm::abs(e.y), e.y);
        if (n.y < 0.0f) {
                const glm::vec2 w = glm::vec2(
                        n.x >= 0.0f ? 1.0f - glm::abs(n.y) : -(1.0f - glm::abs(n.y)),
                        n.z >= 0.0f ? 1.0f - glm::abs(n.y) : -(1.0f - glm::abs(n.y)));
                n = glm::vec3(w.x, n.y, w.y);
        }
        return glm::normalize(n);
}

// One LOD level of a decoded tile: quantized heights + octahedral normals,
// row-major i*resolution + j (i = world Z row, j = world X column).
struct SDecodedLod {

        uint16_t             resolution = 0;
        std::vector<int16_t> vHeights;   // resolution * resolution
        std::vector<uint8_t> vNormals;  // resolution * resolution * 2
};

// A fully decoded .setile: the runtime form shared by TerrainSystem (paging,
// physics, queries) and the Terrain component (GPU upload).
struct STileRuntime {

        uint16_t x = 0;
        uint16_t y = 0;
        float    height_offset = 0.0f;   // world_h = height_offset + height_scale * sample
        float    height_scale  = 1.0f;
        float    tile_size     = 0.0f;
        glm::vec3 vBoundsMin{0.0f};
        glm::vec3 vBoundsMax{0.0f};

        std::vector<SDecodedLod> vLods;  // index 0 == LOD0

        float Dequantize(int16_t sample) const { return height_offset + height_scale * static_cast<float>(sample); }
};

} // namespace SE

#endif
