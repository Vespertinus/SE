
// ---------------------------------------------------------------------------
// TerrainBakeTest (Tier B, Stage 3 Step 3) — bake library vs analytic surfaces,
// quantization bounds, seam-continuity of normals, bake determinism, manifest
// decode, pure query sampling, the streaming worker's content-kind dispatch,
// and the camera-aware AllVisible rebuild contract (fake renderable).
// Headless: no engine systems init (Loki resource singletons only).
// ---------------------------------------------------------------------------

#include <gtest/gtest.h>

#define SE_IMPL
#include <GlobalTypes.h>

#include <Logging.h>
#include <terrain/TerrainBake.tcc>

#include "TerrainAssets.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <terrain/TerrainQuadtree.h>

using namespace SE;

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <random>
#include <thread>
#include <chrono>

namespace {

using namespace SE;
namespace fb = SE::FlatBuffers;

// ~8x8 cells, 2x2 tiles, 3 LODs — small but exercises the full chain.
static TerrainBake::SOutput BakeTestTerrain(const uint32_t tiles = 2) {
        const TerrainBake::SConfig oCfg = se_test::MakeTerrainConfig(tiles);
        return se_test::BakeTerrain(oCfg, se_test::TestHeight);
}

// ---------------------------------------------------------------------------
// bake library
// ---------------------------------------------------------------------------

TEST(TerrainBake, TileSegmentationAndBounds) {
        const TerrainBake::SOutput oOut = BakeTestTerrain();
        ASSERT_FALSE(oOut.vTiles.empty());
        EXPECT_EQ(oOut.tiles_x, 2u);
        EXPECT_EQ(oOut.tiles_y, 2u);
        ASSERT_EQ(oOut.vTiles.size(), 4u);

        const float tile_size = 8.0f;
        for (const auto & oTile : oOut.vTiles) {
                (void)tile_size;
                // every baked tile's reported range must lie in the global range
                EXPECT_GE(oTile.height_min, oOut.height_min - 1e-3f);
                EXPECT_LE(oTile.height_max, oOut.height_max + 1e-3f);
                EXPECT_LE(oTile.height_min, oTile.height_max);
        }
        // row-major order: (0,0), (1,0), (0,1), (1,1)
        EXPECT_EQ(oOut.vTiles[1].x, 1);
        EXPECT_EQ(oOut.vTiles[1].y, 0);
        EXPECT_EQ(oOut.vTiles[2].x, 0);
        EXPECT_EQ(oOut.vTiles[2].y, 1);
}

TEST(TerrainBake, LodResolutionChain) {
        const TerrainBake::SOutput oOut = BakeTestTerrain();
        // lod0_cells = 8 → 9, 5, 3 samples per edge
        ASSERT_EQ(oOut.vLodResolutions.size(), 3u);
        EXPECT_EQ(oOut.vLodResolutions[0], 9);
        EXPECT_EQ(oOut.vLodResolutions[1], 5);
        EXPECT_EQ(oOut.vLodResolutions[2], 3);
}

TEST(TerrainBake, QuantizationRoundTripErrorBound) {
        const TerrainBake::SOutput oOut = BakeTestTerrain();
        const TerrainBake::SConfig oCfg = se_test::MakeTerrainConfig(2);
        const float max_allowed = (oOut.height_max - oOut.height_min) / 65535.0f / 2.0f + 1e-4f;

        for (const auto & oTile : oOut.vTiles) {
                const STileRuntime oRuntime = se_test::DecodeTile(oTile, oCfg.tile_size,
                                glm::vec3(0.0f), glm::vec3(0.0f));
                const SDecodedLod & oLod0 = oRuntime.vLods[0];
                const float step = TerrainQuery::SampleStep(oLod0.resolution, oCfg.tile_size);
                float worst = 0.0f;
                for (uint32_t i = 0; i < oLod0.resolution; ++i) {
                        for (uint32_t j = 0; j < oLod0.resolution; ++j) {
                                const size_t k = size_t(i) * oLod0.resolution + j;
                                const float h = oRuntime.Dequantize(oLod0.vHeights[k]);
                                const float ref = se_test::TestHeight(
                                                float(oTile.x) * oCfg.tile_size + j * step,
                                                float(oTile.y) * oCfg.tile_size + i * step);
                                worst = std::max(worst, std::abs(h - ref));
                        }
                }
                EXPECT_LE(worst, max_allowed) << "tile (" << oTile.x << "," << oTile.y << ")";
        }
}

TEST(TerrainBake, NormalsOnAnalyticSurface) {
        const TerrainBake::SOutput oOut = BakeTestTerrain();
        const STileRuntime oRuntime = se_test::DecodeTile(oOut.vTiles[0], 8.0f,
                        glm::vec3(0.0f), glm::vec3(8.0f, 8.0f, 8.0f));
        // far from the dome (x=0 corner) the surface ≈ plane 0.25x + 0.1z + 3
        const glm::vec3 vExpected = glm::normalize(glm::vec3(-0.25f, 1.0f, -0.1f));
        glm::vec3 vNormal;
        ASSERT_TRUE(TerrainQuery::SampleNormal(oRuntime, 0, 0.0f, 0.0f, vNormal));
        EXPECT_NEAR(vNormal.y, vExpected.y, 0.05f);
        EXPECT_NEAR(vNormal.x, vExpected.x, 0.05f);
        EXPECT_NEAR(vNormal.z, vExpected.z, 0.05f);
}

TEST(TerrainBake, NormalSeamContinuity) {
        // normals are computed on the global grid BEFORE tiling: the last column of
        // tile (0,0) and the first column of tile (1,0) must carry identical normals
        const TerrainBake::SOutput oOut = BakeTestTerrain();
        const STileRuntime oLeft  = se_test::DecodeTile(oOut.vTiles[0], 8.0f, glm::vec3(0.0f), glm::vec3(8.0f));
        const STileRuntime oRight = se_test::DecodeTile(oOut.vTiles[1], 8.0f, glm::vec3(8.0f), glm::vec3(16.0f));

        const uint32_t res = oLeft.vLods[0].resolution;
        for (uint32_t i = 0; i < res; ++i) {
                const size_t l = (size_t(i) * res + (res - 1)) * 2;
                const size_t r = (size_t(i) * res + 0) * 2;
                EXPECT_EQ(oLeft.vLods[0].vNormals[l],     oRight.vLods[0].vNormals[r]) << "row " << i;
                EXPECT_EQ(oLeft.vLods[0].vNormals[l + 1], oRight.vLods[0].vNormals[r + 1]) << "row " << i;
        }
}

TEST(TerrainBake, Determinism) {
        const TerrainBake::SOutput oA = BakeTestTerrain();
        const TerrainBake::SOutput oB = BakeTestTerrain();
        ASSERT_EQ(oA.vManifest.size(), oB.vManifest.size());
        EXPECT_EQ(0, std::memcmp(oA.vManifest.data(), oB.vManifest.data(), oA.vManifest.size()));
        for (size_t k = 0; k < oA.vTiles.size(); ++k) {
                ASSERT_EQ(oA.vTiles[k].vBytes.size(), oB.vTiles[k].vBytes.size());
                EXPECT_EQ(0, std::memcmp(oA.vTiles[k].vBytes.data(),
                                         oB.vTiles[k].vBytes.data(),
                                         oA.vTiles[k].vBytes.size()));
        }
}

TEST(TerrainBake, RejectsBadConfigs) {
        TerrainBake::SOutput oOut;
        TerrainBake::SConfig oCfg = se_test::MakeTerrainConfig(2);
        oCfg.lod0_cells = 7;   // not a power of two
        EXPECT_FALSE(TerrainBake::Bake(nullptr, oCfg, oOut));

        oCfg = se_test::MakeTerrainConfig(2);
        oCfg.source_width = 100;   // (100-1) % 8 != 0
        std::vector<float> vDummy(oCfg.source_width * oCfg.source_height, 1.0f);
        EXPECT_FALSE(TerrainBake::Bake(vDummy.data(), oCfg, oOut));
}

// ---------------------------------------------------------------------------
// manifest decode
// ---------------------------------------------------------------------------

TEST(TerrainAsset, DecodeManifest) {
        const TerrainBake::SOutput oOut = BakeTestTerrain();
        const std::unique_ptr<TerrainAsset> pAsset = se_test::MakeTerrainAsset("test.seterrain", oOut);
        ASSERT_NE(pAsset, nullptr);

        EXPECT_EQ(pAsset->GetTilesX(), 2u);
        EXPECT_EQ(pAsset->GetTilesY(), 2u);
        EXPECT_EQ(pAsset->GetLodCount(), 3u);
        EXPECT_NEAR(pAsset->GetTileSize(), 8.0f, 1e-5f);
        EXPECT_EQ(pAsset->GetTiles().size(), 4u);
        EXPECT_FALSE(pAsset->GetMaterial().sPath.empty());
        EXPECT_EQ(pAsset->GetMaterial().sPath, "material/terrain_test.semt");

        const auto * pRef = pAsset->FindTile(1, 0);
        ASSERT_NE(pRef, nullptr);
        EXPECT_EQ(pRef->sPath, "test_terrain_tile_1_0.setile");
        EXPECT_EQ(pAsset->TileIndexAt(1, 0), 1);
        EXPECT_EQ(pAsset->TileIndexAt(5, 5), -1);
}

TEST(TerrainAsset, VerifyRejectsGarbage) {
        const std::vector<uint8_t> vGarbage(256, 0xAB);
        flatbuffers::Verifier oVerifier(vGarbage.data(), vGarbage.size());
        EXPECT_FALSE(fb::VerifyTerrainBuffer(oVerifier));
}

// ---------------------------------------------------------------------------
// pure queries over a decoded tile
// ---------------------------------------------------------------------------

TEST(TerrainQuery, BilinearMatchesAnalyticSurface) {
        const TerrainBake::SOutput oOut = BakeTestTerrain();
        const STileRuntime oRuntime = se_test::DecodeTile(oOut.vTiles[0], 8.0f,
                        glm::vec3(0.0f), glm::vec3(8.0f, 8.0f, 8.0f));

        float worst = 0.0f;
        for (float z = 0.0f; z < 8.0f; z += 0.25f) {
                for (float x = 0.0f; x < 8.0f; x += 0.25f) {
                        float h = 0.0f;
                        ASSERT_TRUE(TerrainQuery::SampleHeight(oRuntime, x, z, h)) << x << "," << z;
                        worst = std::max(worst, std::abs(h - se_test::TestHeight(x, z)));
                }
        }
        // bilinear over the quantized grid vs the smooth analytic surface
        EXPECT_LE(worst, 0.25f);
}

TEST(TerrainQuery, ExactAtSamplePoints) {
        const TerrainBake::SOutput oOut = BakeTestTerrain();
        const STileRuntime oRuntime = se_test::DecodeTile(oOut.vTiles[0], 8.0f,
                        glm::vec3(0.0f), glm::vec3(8.0f, 8.0f, 8.0f));
        const uint32_t res = oRuntime.vLods[0].resolution;
        const float step = TerrainQuery::SampleStep(res, 8.0f);
        // the max edge is half-open (owned by the neighbor tile) — sample the owned range
        for (uint32_t i = 0; i + 1 < res; ++i) {
                for (uint32_t j = 0; j + 1 < res; ++j) {
                        float h = 0.0f;
                        ASSERT_TRUE(TerrainQuery::SampleHeight(oRuntime, j * step, i * step, h));
                        // exact up to int16 quantization
                        EXPECT_NEAR(h, se_test::TestHeight(j * step, i * step), 0.01f);
                }
        }
}

TEST(TerrainQuery, OutsideTileFails) {
        const TerrainBake::SOutput oOut = BakeTestTerrain();
        const STileRuntime oRuntime = se_test::DecodeTile(oOut.vTiles[0], 8.0f,
                        glm::vec3(0.0f), glm::vec3(8.0f, 8.0f, 8.0f));
        float h = 0.0f;
        EXPECT_FALSE(TerrainQuery::SampleHeight(oRuntime, -0.1f, 4.0f, h));
        EXPECT_FALSE(TerrainQuery::SampleHeight(oRuntime, 4.0f, 8.0f, h));
        EXPECT_FALSE(TerrainQuery::SampleHeight(oRuntime, 4.0f, 9.0f, h));
}

// ---------------------------------------------------------------------------
// streaming worker: content-kind dispatch (the only world-code change)
// ---------------------------------------------------------------------------

TEST(StreamingWorker, DispatchesTerrainTileKind) {
        const TerrainBake::SOutput oOut = BakeTestTerrain();

        const std::string sDir = "resource/terrain/gen/";
        std::filesystem::create_directories(sDir);
        const std::string sTilePath = sDir + "worker_test_tile.setile";
        {
                std::ofstream f(sTilePath, std::ios::binary | std::ios::trunc);
                f.write(reinterpret_cast<const char *>(oOut.vTiles[0].vBytes.data()),
                                std::streamsize(oOut.vTiles[0].vBytes.size()));
        }

        StreamingDirector oDirector;
        bool bGot = false;
        for (int attempt = 0; attempt < 100 && !bGot; ++attempt) {
                if (oDirector.GetInFlightCount() == 0) {
                        oDirector.DispatchRead(1, 1, 0, sTilePath,
                                        StreamingDirector::EContentKind::TerrainTile);
                }
                StreamingDirector::SReadResult oRes;
                while (oDirector.TryPopResult(oRes)) {
                        bGot = oRes.ok && oRes.kind == StreamingDirector::EContentKind::TerrainTile;
                        if (bGot) {
                                // the staged bytes verify as terrain tile data
                                flatbuffers::Verifier v(reinterpret_cast<const uint8_t *>(oRes.pBytes->data()),
                                                        oRes.pBytes->size());
                                EXPECT_TRUE(v.VerifyBuffer<fb::TerrainTileData>(fb::TerrainIdentifier()));
                        }
                }
                if (!bGot) { std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
        }
        EXPECT_TRUE(bGot);
        oDirector.StopWorker();
}

TEST(StreamingWorker, RejectsGarbageTerrainTile) {
        const std::string sDir = "resource/terrain/gen/";
        std::filesystem::create_directories(sDir);
        const std::string sBadPath = sDir + "worker_test_garbage.setile";
        {
                std::ofstream f(sBadPath, std::ios::binary | std::ios::trunc);
                const char bytes[64] = {};
                f.write(bytes, 64);
        }

        StreamingDirector oDirector;
        bool bGot = false;
        for (int attempt = 0; attempt < 100 && !bGot; ++attempt) {
                if (oDirector.GetInFlightCount() == 0) {
                        oDirector.DispatchRead(1, 1, 0, sBadPath,
                                        StreamingDirector::EContentKind::TerrainTile);
                }
                StreamingDirector::SReadResult oRes;
                while (oDirector.TryPopResult(oRes)) {
                        bGot = !oRes.ok;
                }
                if (!bGot) { std::this_thread::sleep_for(std::chrono::milliseconds(2)); }
        }
        EXPECT_TRUE(bGot);
        oDirector.StopWorker();
}

// ---------------------------------------------------------------------------
// camera-aware AllVisible (fake renderable — no GL)
// ---------------------------------------------------------------------------

namespace {

struct FakeRenderable {
        std::vector<RenderCommand> vOwned;
        std::vector<const RenderCommand *> vCommands;
        const std::vector<const RenderCommand *> & GetRenderCommands() { return vCommands; }
};

} // namespace

TEST(AllVisibleCamera, RebuildsOnViewChange) {
        // the manager is the first Engine system and no longer self-constructs
        TEngine::Instance().Init<TResourceManager>();
        auto & oManager = GetSystem<TResourceManager>();

        SE::GeometryEntity oGeom(1, 3, 0, 4u);   // plain ids; never drawn headless
        // headless-safe material: no shader → no GL touched by RenderCommand ctor
        H<SE::Material> hMat = oManager.Create<SE::Material>("allvisible_test_mat", H<SE::ShaderProgram>::Null());
        SE::Material * pMat = oManager.Get(hMat);
        ASSERT_NE(pMat, nullptr);

        Transform oTransform;
        FakeRenderable a, b;
        a.vOwned.emplace_back(&oGeom, pMat, oTransform);
        a.vCommands.push_back(&a.vOwned.back());
        b.vOwned.emplace_back(&oGeom, pMat, oTransform);
        b.vCommands.push_back(&b.vOwned.back());

        AllVisible<FakeRenderable> oVisible;
        oVisible.AddRenderable(&a);

        // set changed by AddRenderable
        EXPECT_TRUE(oVisible.GetVisible().changed);
        // camera invalid → first view forces one rebuild
        const glm::mat4 mVP{1.0f};
        oVisible.UpdateCameraView(mVP);
        EXPECT_TRUE(oVisible.GetVisible().changed);
        // identical view → cached
        EXPECT_FALSE(oVisible.GetVisible().changed);
        oVisible.UpdateCameraView(mVP);
        EXPECT_FALSE(oVisible.GetVisible().changed);
        // sub-epsilon jitter (0.5 mm on one element) → cached
        glm::mat4 mJitter = mVP;
        mJitter[3][0] += 5e-4f;
        oVisible.UpdateCameraView(mJitter);
        EXPECT_FALSE(oVisible.GetVisible().changed);
        // translation (camera move)
        glm::mat4 mMoved = mJitter;
        mMoved[3][0] += 1.0f;
        oVisible.UpdateCameraView(mMoved);
        EXPECT_TRUE(oVisible.GetVisible().changed);
        EXPECT_FALSE(oVisible.GetVisible().changed);
        // rotation-only (orbiting in place must rebuild — stale-frustum regression)
        glm::mat4 mRotated = mMoved;
        mRotated[0][0] = 0.0f;
        mRotated[2][0] = 1.0f;
        oVisible.UpdateCameraView(mRotated);
        EXPECT_TRUE(oVisible.GetVisible().changed);
        EXPECT_FALSE(oVisible.GetVisible().changed);

        // second renderable changes the set
        oVisible.AddRenderable(&b);
        EXPECT_TRUE(oVisible.GetVisible().changed);
        EXPECT_FALSE(oVisible.GetVisible().changed);

        // explicit dirty
        oVisible.MarkDirty();
        EXPECT_TRUE(oVisible.GetVisible().changed);

        oVisible.RemoveRenderable(&a);
        oVisible.RemoveRenderable(&b);
}

TEST(VisibilityManagerCamera, CullsAndTracksVisibleSet) {
        TEngine::Instance().Init<TResourceManager>();
        auto & oManager = GetSystem<TResourceManager>();

        SE::GeometryEntity oGeom(1, 3, 0, 4u);
        H<SE::Material> hMat = oManager.Create<SE::Material>("vismgr_test_mat", H<SE::ShaderProgram>::Null());
        SE::Material * pMat = oManager.Get(hMat);
        ASSERT_NE(pMat, nullptr);

        // sphere-culling fake
        struct VisRenderable {
                std::vector<RenderCommand> vOwned;
                std::vector<const RenderCommand *> vCommands;
                glm::vec3 vTestCenter {0.f};
                float     fTestRadius = 1.f;
                const std::vector<const RenderCommand *> & GetRenderCommands() { return vCommands; }
                bool GetCullSphere(glm::vec3 & vCenter, float & fRadius) const {
                        vCenter = vTestCenter;
                        fRadius = fTestRadius;
                        return fTestRadius > 0.f;
                }
        };

        Transform oTransform;
        VisRenderable a;
        a.vOwned.emplace_back(&oGeom, pMat, oTransform);
        a.vCommands.push_back(&a.vOwned.back());

        VisibilityManager<VisRenderable> oVisible;
        oVisible.AddRenderable(&a);

        const glm::mat4 mVP{1.0f};
        oVisible.UpdateCameraView(mVP);
        EXPECT_TRUE(oVisible.GetVisible().changed);
        // radius-1 sphere at the origin intersects the unit-VP frustum → gathered
        EXPECT_EQ(oVisible.GetVisible().opaque.size(), 1u);
        EXPECT_TRUE(oVisible.WasRenderableVisible(&a));
        // identical view → cached, list untouched
        EXPECT_FALSE(oVisible.GetVisible().changed);

        // move the sphere out of the frustum → culled on the next rebuild
        oVisible.MarkDirty();
        a.vTestCenter = glm::vec3(1000.f, 1000.f, 1000.f);
        EXPECT_EQ(oVisible.GetVisible().opaque.size(), 0u);
        EXPECT_FALSE(oVisible.WasRenderableVisible(&a));

        oVisible.RemoveRenderable(&a);
}

// ---------------------------------------------------------------------------
// quadtree culling — the candidate set must equal brute-force per-tile frustum
// tests. Regression: MakeChild wrote child links through a reference that its
// push_back could invalidate; reallocation silently dropped whole subtrees and
// tiles vanished from the candidate set (pattern varied with the allocator).
// ---------------------------------------------------------------------------

namespace {

std::vector<uint32_t> QuadtreeBruteForce(const SFrustum & oFrustum,
                                         const uint32_t tiles_x, const uint32_t tiles_y,
                                         const glm::vec3 & vMin, const glm::vec3 & vMax) {
        const float cell_x = (vMax.x - vMin.x) / float(tiles_x);
        const float cell_z = (vMax.z - vMin.z) / float(tiles_y);
        std::vector<uint32_t> vOut;
        for (uint32_t ty = 0; ty < tiles_y; ++ty) {
                for (uint32_t tx = 0; tx < tiles_x; ++tx) {
                        // the tree's leaf boxes span the full grid Y extent
                        const glm::vec3 vLo(vMin.x + tx * cell_x, vMin.y, vMin.z + ty * cell_z);
                        const glm::vec3 vHi(vMin.x + (tx + 1) * cell_x, vMax.y, vMin.z + (ty + 1) * cell_z);
                        if (oFrustum.IntersectsAABB(vLo, vHi)) { vOut.push_back(ty * tiles_x + tx); }
                }
        }
        std::sort(vOut.begin(), vOut.end());
        return vOut;
}

std::vector<uint32_t> QuadtreeSorted(SE::TerrainQuadtree & oTree, const SFrustum & oFrustum) {
        std::vector<uint32_t> vOut;
        oTree.CollectVisible(oFrustum, vOut);
        std::sort(vOut.begin(), vOut.end());
        return vOut;
}

} // namespace

TEST(TerrainQuadtree, OpenFrustumCollectsEveryTile) {
        const glm::vec3 vMin(0.0f, 0.0f, 0.0f);
        const glm::vec3 vMax(2048.0f, 300.0f, 2048.0f);
        SE::TerrainQuadtree oTree;
        oTree.Build(vMin, vMax, 4, 4);

        // straight-down camera high above the grid: everything is inside
        const glm::mat4 mV = glm::lookAt(glm::vec3(1024.0f, 4000.0f, 1024.0f),
                                         glm::vec3(1024.0f, 0.0f, 1024.0f),
                                         glm::vec3(0.0f, 0.0f, -1.0f));
        const glm::mat4 mP = glm::perspective(glm::radians(120.0f), 1.0f, 1.0f, 9000.0f);
        const SFrustum oFrustum = SFrustum::FromVP(mP * mV);

        std::vector<uint32_t> vGot;
        oTree.CollectVisible(oFrustum, vGot);
        std::sort(vGot.begin(), vGot.end());

        std::vector<uint32_t> vWant(16);
        for (uint32_t i = 0; i < 16; ++i) { vWant[i] = i; }
        EXPECT_EQ(vGot, vWant);
}

TEST(TerrainQuadtree, MatchesBruteForceAcrossRandomViews) {
        std::mt19937 oRng(0x5EEDu);
        auto Rand = [&oRng](float lo, float hi) {
                return lo + (hi - lo) * std::uniform_real_distribution<float>(0.0f, 1.0f)(oRng);
        };

        struct SGrid { uint32_t x, y; float tile, h; };
        const SGrid aGrids[] = {
                { 4, 4, 512.0f, 300.0f },
                { 5, 3, 256.0f, 120.0f },
                { 1, 2, 512.0f,  64.0f },
                { 8, 8, 128.0f, 200.0f },
        };

        for (const SGrid & g : aGrids) {
                const glm::vec3 vMin(0.0f, 0.0f, 0.0f);
                const glm::vec3 vMax(g.x * g.tile, g.h, g.y * g.tile);
                const glm::vec3 vCenter(vMax.x * 0.5f, 0.0f, vMax.z * 0.5f);

                SE::TerrainQuadtree oTree;
                // repeated builds on one instance mirror the runtime: the vector's
                // reallocation history must never change the candidate set
                for (uint32_t iter = 0; iter < 3; ++iter) {
                        oTree.Build(vMin, vMax, g.x, g.y);
                        for (uint32_t cam = 0; cam < 24; ++cam) {
                                const glm::vec3 vPos(Rand(-0.3f, 1.3f) * vMax.x, Rand(10.0f, 1500.0f),
                                                     Rand(-0.3f, 1.3f) * vMax.z);
                                const glm::vec3 vTarget = vCenter + glm::vec3(
                                                Rand(-1.0f, 1.0f) * vMax.x * 0.5f, 0.0f,
                                                Rand(-1.0f, 1.0f) * vMax.z * 0.5f);
                                const glm::mat4 mV = glm::lookAt(vPos, vTarget, glm::vec3(0.0f, 1.0f, 0.0f));
                                const glm::mat4 mP = glm::perspective(
                                                glm::radians(Rand(45.0f, 110.0f)), Rand(0.8f, 2.2f), 0.5f, 6000.0f);
                                const SFrustum oFrustum = SFrustum::FromVP(mP * mV);
                                EXPECT_EQ(QuadtreeSorted(oTree, oFrustum),
                                          QuadtreeBruteForce(oFrustum, g.x, g.y, vMin, vMax));
                        }
                }
        }
}

} // namespace
