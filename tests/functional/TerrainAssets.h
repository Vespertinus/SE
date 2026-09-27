
#ifndef SE_TEST_TERRAIN_ASSETS_H
#define SE_TEST_TERRAIN_ASSETS_H

// ---------------------------------------------------------------------------
// Terrain fixtures for the Step 3 suites (AnimationAssets.h / WorldAssets.h
// convention). Bakes tiles in memory from analytic height functions; helpers
// wrap the .seterrain manifest into a TerrainAsset via the inline-FB ctor and
// decode .setile payloads into STileRuntime.
// ---------------------------------------------------------------------------

#include <flatbuffers/flatbuffers.h>
#include <Terrain_generated.h>

#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <terrain/TerrainBake.h>
#include <terrain/TerrainAsset.h>
#include <terrain/TileData.h>
#include <terrain/TerrainQuery.h>

namespace se_test {

inline SE::TerrainBake::SConfig MakeTerrainConfig(const uint32_t tiles_per_side,
                                                  const uint32_t lod_count = 3,
                                                  const uint32_t lod0_cells = 8,
                                                  const float tile_size = 8.0f) {
        SE::TerrainBake::SConfig oCfg;
        oCfg.id            = "test_terrain";
        oCfg.source_width  = tiles_per_side * lod0_cells + 1;
        oCfg.source_height = tiles_per_side * lod0_cells + 1;
        oCfg.tile_size     = tile_size;
        oCfg.lod0_cells    = lod0_cells;
        oCfg.lod_count     = lod_count;
        oCfg.material_path = "material/terrain_test.semt";
        return oCfg;
}

// height(x, z) in world units; j = columns = world X, i = rows = world Z
inline SE::TerrainBake::SOutput BakeTerrain(const SE::TerrainBake::SConfig & oCfg,
                                            const std::function<float(float, float)> & fnHeight) {
        std::vector<float> vHeights(size_t(oCfg.source_width) * oCfg.source_height);
        for (uint32_t i = 0; i < oCfg.source_height; ++i) {
                for (uint32_t j = 0; j < oCfg.source_width; ++j) {
                        vHeights[size_t(i) * oCfg.source_width + j] =
                                        fnHeight(float(j) * (oCfg.tile_size / float(oCfg.lod0_cells)),
                                                 float(i) * (oCfg.tile_size / float(oCfg.lod0_cells)));
                }
        }
        SE::TerrainBake::SOutput oOut;
        if (!SE::TerrainBake::Bake(vHeights.data(), oCfg, oOut)) {
                return oOut;
        }
        return oOut;
}

// The 2-tile analytic surface used by the query/physics tests: a plane plus a
// smooth dome in tile (0,0) — exact at sample points, cheap to evaluate.
inline float TestHeight(const float x, const float z) {
        const float dx = x - 4.0f;
        const float dz = z - 4.0f;
        return 3.0f + 0.25f * x + 0.1f * z + 2.0f * std::exp(-(dx * dx + dz * dz) / 8.0f);
}

inline std::unique_ptr<SE::TerrainAsset> MakeTerrainAsset(
                const std::string & sName, const SE::TerrainBake::SOutput & oOut) {
        flatbuffers::Verifier oVerifier(oOut.vManifest.data(), oOut.vManifest.size());
        if (!SE::FlatBuffers::VerifyTerrainBuffer(oVerifier)) { return nullptr; }
        return std::make_unique<SE::TerrainAsset>(sName, SE::rid_t(1),
                        SE::FlatBuffers::GetTerrain(oOut.vManifest.data()));
}

inline SE::STileRuntime DecodeTile(const SE::TerrainBake::STileBytes & oTile,
                                   const float tile_size,
                                   const glm::vec3 & vBoundsMin,
                                   const glm::vec3 & vBoundsMax) {
        SE::STileRuntime oRuntime;
        const auto * pFb = flatbuffers::GetRoot<SE::FlatBuffers::TerrainTileData>(oTile.vBytes.data());
        oRuntime.x = pFb->x();
        oRuntime.y = pFb->y();
        oRuntime.height_offset = pFb->height_offset();
        oRuntime.height_scale  = pFb->height_scale();
        oRuntime.tile_size     = tile_size;
        oRuntime.vBoundsMin    = vBoundsMin;
        oRuntime.vBoundsMax    = vBoundsMax;
        if (pFb->lods()) {
                for (const auto * pLod : *pFb->lods()) {
                        SE::SDecodedLod oLod;
                        oLod.resolution = pLod->resolution();
                        oLod.vHeights.assign(pLod->heights()->begin(), pLod->heights()->end());
                        oLod.vNormals.assign(pLod->normals()->begin(), pLod->normals()->end());
                        oRuntime.vLods.push_back(std::move(oLod));
                }
        }
        return oRuntime;
}

} // namespace se_test

#endif // SE_TEST_TERRAIN_ASSETS_H
