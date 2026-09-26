
#ifndef TERRAINBAKE_HEIGHTMAP_SOURCE_H
#define TERRAINBAKE_HEIGHTMAP_SOURCE_H

// Heightmap sources for terrain_bake. The exchange formats match what terrain
// authoring tools (World Machine, Gaea, Houdini, Blender) export:
//   - png16: 16-bit grayscale PNG (stb_image, stbi_load_16)
//   - raw16: headerless 16-bit little-endian RAW (the World Machine standard;
//            dimensions come from the config)
//   - procedural: deterministic FBM value noise (no binary fixtures needed for
//            tests and demos)
// Decoded output is always a float grid, row-major i*width + j,
// world x = columns, world z = rows.

#include <cstdint>
#include <string>
#include <vector>

#include <Logging.h>

#include <algorithm>
#include <cmath>
#include <fstream>

#include <stb/stb_image.h>

namespace se_terrain_bake {

struct SSourceDesc {
        std::string type     = "procedural";   // png16 | raw16 | procedural
        std::string path;
        uint32_t    width    = 0;              // raw16 only (png16 reads the file)
        uint32_t    height   = 0;

        // procedural parameters (all deterministic from seed)
        uint64_t seed        = 1;
        uint32_t size        = 513;            // square grid samples
        float    frequency   = 3.0f;           // base feature frequency (cells across the map)
        uint32_t octaves     = 5;
        float    lacunarity  = 2.0f;
        float    gain        = 0.5f;
        float    base_height = 20.0f;          // mean height in meters
        float    amplitude   = 60.0f;          // noise relief in meters

        // png16/raw16 normalization (default 1.0/0.0 = raw values are meters):
        //   meters = raw_value * height_scale + height_offset
        float    height_scale  = 1.0f;
        float    height_offset = 0.0f;
};

inline bool LoadHeightmap(const SSourceDesc & oDesc, std::vector<float> & vHeights,
                          uint32_t & width, uint32_t & height) {

        if (oDesc.type == "png16" || oDesc.type == "raw16") {

                if (oDesc.type == "png16") {
                        int w = 0, h = 0, channels = 0;
                        stbi_us * pData = stbi_load_16(oDesc.path.c_str(), &w, &h, &channels, 1);
                        if (!pData) {
                                log_e("terrain_bake: failed to load png16 '{}'", oDesc.path);
                                return false;
                        }
                        width = uint32_t(w);
                        height = uint32_t(h);
                        vHeights.resize(size_t(width) * height);
                        for (size_t k = 0; k < vHeights.size(); ++k) {
                                vHeights[k] = float(pData[k]) * oDesc.height_scale + oDesc.height_offset;
                        }
                        stbi_image_free(pData);
                        return true;
                }

                // raw16: headerless little-endian, row-major
                width = oDesc.width;
                height = oDesc.height;
                if (width < 2 || height < 2) {
                        log_e("terrain_bake: raw16 requires size in config ({}x{})", width, height);
                        return false;
                }
                std::ifstream f(oDesc.path, std::ios::binary);
                if (!f.is_open()) {
                        log_e("terrain_bake: failed to open raw16 '{}'", oDesc.path);
                        return false;
                }
                vHeights.resize(size_t(width) * height);
                f.read(reinterpret_cast<char *>(vHeights.data()),
                       std::streamsize(vHeights.size() * sizeof(uint16_t)));
                if (size_t(f.gcount()) != vHeights.size() * sizeof(uint16_t)) {
                        log_e("terrain_bake: short read on raw16 '{}'", oDesc.path);
                        return false;
                }
                for (auto & v : vHeights) {
                        v = v * oDesc.height_scale + oDesc.height_offset;
                }
                return true;
        }

        if (oDesc.type == "procedural") {
                width = oDesc.size;
                height = oDesc.size;

                // deterministic value-noise FBM (hash-based, no state)
                auto Hash2 = [](uint64_t x, uint64_t y, uint64_t seed) -> float {
                        uint64_t h = seed;
                        h ^= x + 0x9E3779B97F4A7C15ull + (h << 6) + (h >> 2);
                        h ^= y + 0xBF58476D1CE4E5B9ull + (h << 11) + (h >> 5);
                        h *= 0x94D049BB133111EBull;
                        h ^= h >> 31;
                        return float(h & 0xFFFFFFull) / float(0xFFFFFFull) * 2.0f - 1.0f;
                };
                auto Smooth = [](float t) { return t * t * (3.0f - 2.0f * t); };
                auto ValueNoise = [&](float x, float y, uint32_t octave) -> float {
                        const float freq = oDesc.frequency * std::pow(oDesc.lacunarity, float(octave));
                        const float fx = x * freq;
                        const float fy = y * freq;
                        const uint64_t x0 = uint64_t(std::floor(fx));
                        const uint64_t y0 = uint64_t(std::floor(fy));
                        const float tx = Smooth(fx - float(x0));
                        const float ty = Smooth(fy - float(y0));
                        const float n00 = Hash2(x0, y0, oDesc.seed + octave * 7919);
                        const float n10 = Hash2(x0 + 1, y0, oDesc.seed + octave * 7919);
                        const float n01 = Hash2(x0, y0 + 1, oDesc.seed + octave * 7919);
                        const float n11 = Hash2(x0 + 1, y0 + 1, oDesc.seed + octave * 7919);
                        const float a = n00 + (n10 - n00) * tx;
                        const float b = n01 + (n11 - n01) * tx;
                        return a + (b - a) * ty;
                };

                vHeights.resize(size_t(width) * height);
                const float norm = std::max(1u, width - 1) > 0 ? 1.0f / float(width - 1) : 0.0f;
                for (uint32_t i = 0; i < height; ++i) {
                        for (uint32_t j = 0; j < width; ++j) {
                                const float u = float(j) * norm;
                                const float v = float(i) * norm;
                                float noise = 0.0f;
                                float amp = 1.0f;
                                float amp_sum = 0.0f;
                                for (uint32_t o = 0; o < oDesc.octaves; ++o) {
                                        noise += ValueNoise(u, v, o) * amp;
                                        amp_sum += amp;
                                        amp *= oDesc.gain;
                                }
                                vHeights[size_t(i) * width + j] =
                                                oDesc.base_height + oDesc.amplitude * (noise / amp_sum);
                        }
                }
                return true;
        }

        log_e("terrain_bake: unknown source type '{}'", oDesc.type);
        return false;
}

} // namespace se_terrain_bake

#endif
