
#ifdef SE_IMPL

#include <terrain/TerrainBake.h>
#include <terrain/TileData.h>

#include <algorithm>
#include <cmath>
#include <limits>

#include <Logging.h>

#include <Terrain_generated.h>
#include <flatbuffers/flatbuffers.h>

namespace SE {
namespace TerrainBake {

namespace {

using FlatBuffers::TerrainLod;
using FlatBuffers::TerrainLodT;
using FlatBuffers::TerrainT;
using FlatBuffers::TerrainTileDataT;
using FlatBuffers::TerrainTileRefT;

// Global-grid view of one LOD level: stride-sampled window of the source heights.
struct SGridLod {
        uint32_t width  = 0;   // samples per row at this LOD
        uint32_t height = 0;
        uint32_t stride = 1;
        const float * pSrc = nullptr;
        uint32_t src_width = 0;

        float At(uint32_t i, uint32_t j) const {
                return pSrc[static_cast<size_t>(i) * stride * src_width + static_cast<size_t>(j) * stride];
        }
};

OctNormal16 ComputeNormal(const SGridLod & oLod, uint32_t i, uint32_t j, float step) {

        const float hL = oLod.At(i, j > 0 ? j - 1 : 0);
        const float hR = oLod.At(i, j + 1 < oLod.width ? j + 1 : oLod.width - 1);
        const float hD = oLod.At(i > 0 ? i - 1 : 0, j);
        const float hU = oLod.At(i + 1 < oLod.height ? i + 1 : oLod.height - 1, j);

        const float dhdx = (j > 0 && j + 1 < oLod.width) ? (hR - hL) / (2.0f * step) : (hR - hL) / step;
        const float dhdz = (i > 0 && i + 1 < oLod.height) ? (hU - hD) / (2.0f * step) : (hU - hD) / step;
        return OctEncodeNormal(glm::normalize(glm::vec3(-dhdx, 1.0f, -dhdz)));
}

int16_t Quantize(float h, float offset, float scale) {

        const float q = std::round((h - offset) / scale);
        return static_cast<int16_t>(glm::clamp(q, -32768.0f, 32767.0f));
}

} // namespace

bool Bake(const float * pHeights, const SConfig & oCfg, SOutput & out) {

        if (!pHeights || oCfg.source_width < 2 || oCfg.source_height < 2) {
                log_e("TerrainBake: empty or too-small source grid");
                return false;
        }
        if (oCfg.lod0_cells == 0 || (oCfg.lod0_cells & (oCfg.lod0_cells - 1)) != 0) {
                log_e("TerrainBake: lod0_cells must be a power of two (got {})", oCfg.lod0_cells);
                return false;
        }
        if (oCfg.lod_count == 0 || oCfg.lod_count > 8) {
                log_e("TerrainBake: lod_count must be in [1, 8] (got {})", oCfg.lod_count);
                return false;
        }
        if (oCfg.id.empty()) {
                log_e("TerrainBake: id must not be empty");
                return false;
        }
        const uint32_t tiles_x = (oCfg.source_width - 1) / oCfg.lod0_cells;
        const uint32_t tiles_y = (oCfg.source_height - 1) / oCfg.lod0_cells;
        if (tiles_x == 0 || tiles_y == 0
            || (oCfg.source_width - 1) % oCfg.lod0_cells != 0
            || (oCfg.source_height - 1) % oCfg.lod0_cells != 0) {
                log_e("TerrainBake: source grid {}x{} does not tile into {}-cell tiles "
                      "((width-1) and (height-1) must divide evenly)",
                      oCfg.source_width, oCfg.source_height, oCfg.lod0_cells);
                return false;
        }

        const float base_step = oCfg.tile_size / static_cast<float>(oCfg.lod0_cells);

        out.vTiles.clear();
        out.vLodResolutions.clear();
        for (uint32_t lod = 0; lod < oCfg.lod_count; ++lod) {
                out.vLodResolutions.push_back(LodResolution(oCfg.lod0_cells, lod));
        }

        // global min/max over the source grid
        const size_t total = static_cast<size_t>(oCfg.source_width) * oCfg.source_height;
        float global_min = pHeights[0];
        float global_max = pHeights[0];
        for (size_t k = 1; k < total; ++k) {
                global_min = std::min(global_min, pHeights[k]);
                global_max = std::max(global_max, pHeights[k]);
        }
        out.height_min = global_min;
        out.height_max = global_max;
        out.tiles_x = tiles_x;
        out.tiles_y = tiles_y;

        // per-LOD global grids (stride windows over the source)
        std::vector<SGridLod> oLods(oCfg.lod_count);
        for (uint32_t lod = 0; lod < oCfg.lod_count; ++lod) {
                SGridLod & g = oLods[lod];
                g.stride   = 1u << lod;
                g.src_width = oCfg.source_width;
                g.width    = (oCfg.source_width - 1) / g.stride + 1;
                g.height   = (oCfg.source_height - 1) / g.stride + 1;
                g.pSrc     = pHeights;
        }

        // row-major tile order: y outer, x inner — deterministic output
        for (uint32_t ty = 0; ty < tiles_y; ++ty) {
                for (uint32_t tx = 0; tx < tiles_x; ++tx) {

                        // tile min/max from its LOD0 samples (superset of every higher LOD)
                        float tile_min = std::numeric_limits<float>::max();
                        float tile_max = std::numeric_limits<float>::lowest();
                        for (uint32_t k = 0; k <= oCfg.lod0_cells; ++k) {
                                for (uint32_t l = 0; l <= oCfg.lod0_cells; ++l) {
                                        const size_t idx = static_cast<size_t>(ty * oCfg.lod0_cells + k) * oCfg.source_width
                                                                + tx * oCfg.lod0_cells + l;
                                        tile_min = std::min(tile_min, pHeights[idx]);
                                        tile_max = std::max(tile_max, pHeights[idx]);
                                }
                        }
                        if (tile_max <= tile_min) {
                                tile_max = tile_min + 1.0f;   // flat tile: keep scale sane
                        }
                        const float scale  = (tile_max - tile_min) / 65535.0f;
                        const float offset = tile_min + 32768.0f * scale;

                        auto tile = std::make_unique<TerrainTileDataT>();
                        tile->x = static_cast<uint16_t>(tx);
                        tile->y = static_cast<uint16_t>(ty);
                        tile->height_offset = offset;
                        tile->height_scale  = scale;

                        for (uint32_t lod = 0; lod < oCfg.lod_count; ++lod) {
                                const SGridLod & g = oLods[lod];
                                const uint16_t res = out.vLodResolutions[lod];
                                const float lod_step = base_step * static_cast<float>(g.stride);

                                auto oLodOut = std::make_unique<TerrainLodT>();
                                oLodOut->resolution = res;
                                oLodOut->heights.resize(static_cast<size_t>(res) * res);
                                oLodOut->normals.resize(static_cast<size_t>(res) * res * 2);

                                const uint32_t base_i = ty * oCfg.lod0_cells / g.stride;
                                const uint32_t base_j = tx * oCfg.lod0_cells / g.stride;

                                for (uint32_t i = 0; i < static_cast<uint32_t>(res); ++i) {
                                        for (uint32_t j = 0; j < static_cast<uint32_t>(res); ++j) {
                                                const uint32_t gi = base_i + i;
                                                const uint32_t gj = base_j + j;

                                                const size_t h_idx = static_cast<size_t>(i) * res + j;
                                                oLodOut->heights[h_idx] = Quantize(g.At(gi, gj), offset, scale);

                                                const OctNormal16 oN = ComputeNormal(g, gi, gj, lod_step);
                                                const size_t n_idx = h_idx * 2;
                                                oLodOut->normals[n_idx]     = oN.u;
                                                oLodOut->normals[n_idx + 1] = oN.v;
                                        }
                                }
                                tile->lods.push_back(std::move(oLodOut));
                        }

                        // serialize the tile
                        flatbuffers::FlatBufferBuilder fbb;
                        fbb.Finish(FlatBuffers::CreateTerrainTileData(fbb, tile.get()), FlatBuffers::TerrainIdentifier());

                        STileBytes oOutTile;
                        oOutTile.x = static_cast<uint16_t>(tx);
                        oOutTile.y = static_cast<uint16_t>(ty);
                        oOutTile.height_min = tile_min;
                        oOutTile.height_max = tile_max;
                        oOutTile.vBytes.assign(fbb.GetBufferPointer(), fbb.GetBufferPointer() + fbb.GetSize());
                        out.vTiles.push_back(std::move(oOutTile));
                }
        }

        // manifest
        auto manifest = std::make_unique<TerrainT>();
        manifest->id = oCfg.id;
        manifest->version = 1;
        manifest->tile_size = oCfg.tile_size;
        manifest->origin = std::make_unique<FlatBuffers::Vec3>(oCfg.origin.x, oCfg.origin.y, oCfg.origin.z);
        manifest->lod_resolutions = out.vLodResolutions;
        manifest->height_min = global_min;
        manifest->height_max = global_max;

        if (!oCfg.material_path.empty()) {
                auto oMat = std::make_unique<FlatBuffers::TerrainMaterialT>();
                oMat->material_path = oCfg.material_path;
                oMat->uv_scale = oCfg.uv_scale;
                manifest->material = std::move(oMat);
        }
        {
                auto oPhys = std::make_unique<FlatBuffers::TerrainPhysicsT>();
                oPhys->friction = oCfg.friction;
                oPhys->max_slope_deg = oCfg.max_slope_deg;
                manifest->physics = std::move(oPhys);
        }

        uint16_t tile_index = 0;
        for (uint32_t ty = 0; ty < tiles_y; ++ty) {
                for (uint32_t tx = 0; tx < tiles_x; ++tx) {
                        const STileBytes & oTile = out.vTiles[tile_index++];
                        auto oRef = std::make_unique<TerrainTileRefT>();
                        oRef->x = oTile.x;
                        oRef->y = oTile.y;
                        oRef->path = fmt::format("{}_tile_{}_{}.setile", oCfg.id, oTile.x, oTile.y);
                        oRef->bounds_min = std::make_unique<FlatBuffers::Vec3>(
                                oCfg.origin.x + tx * oCfg.tile_size, oTile.height_min, oCfg.origin.z + ty * oCfg.tile_size);
                        oRef->bounds_max = std::make_unique<FlatBuffers::Vec3>(
                                oCfg.origin.x + (tx + 1) * oCfg.tile_size, oTile.height_max, oCfg.origin.z + (ty + 1) * oCfg.tile_size);
                        oRef->height_min = oTile.height_min;
                        oRef->height_max = oTile.height_max;
                        manifest->tiles.push_back(std::move(oRef));
                }
        }

        flatbuffers::FlatBufferBuilder fbb;
        fbb.Finish(FlatBuffers::CreateTerrain(fbb, manifest.get()), FlatBuffers::TerrainIdentifier());
        out.vManifest.assign(fbb.GetBufferPointer(), fbb.GetBufferPointer() + fbb.GetSize());

        log_i("TerrainBake: '{}' baked ({}x{} tiles, {} LODs, heights {:.1f}..{:.1f})",
              oCfg.id, tiles_x, tiles_y, oCfg.lod_count, global_min, global_max);
        return true;
}

} // namespace TerrainBake
} // namespace SE

#endif // SE_IMPL
