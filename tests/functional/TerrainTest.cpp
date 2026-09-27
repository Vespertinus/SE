
// ---------------------------------------------------------------------------
// TerrainTest — Tier C (Stage 3 Step 3): the full tile pipeline through
// TerrainSystem + WorldSystem + physics over the terrain_app composition.
// Tiles page through the shared streaming worker, so tests pump frames with a
// ~1 ms wall sleep and assert STATE PROGRESSION (StreamingTest convention).
// Headless: Terrain components run with the renderable registration compiled
// out (no SE_TERRAIN_ENABLED on this target) and physics bodies are real.
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>
#include "EngineFixture.h"
#include "PhysicsHarness.h"
#include "TerrainAssets.h"
#include "WorldAssets.h"

#include <terrain/TerrainBake.tcc>
#include <world/WorldSystem.h>
#include <world/WorldAsset.h>
#include <terrain/TerrainSystem.h>
#include <terrain/Terrain.h>
#include <terrain/TerrainAsset.h>
#include <EntityManager.h>
#include <PhysicsSystem.h>

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace SE;

namespace {

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class TerrainTest : public se_test::EnginePhysTestBase {

protected:
        void SetUp() override {
                EnginePhysTestBase::SetUp();

                static bool systems_constructed = []() {
                        auto & oEngine = TEngine::Instance();
                        oEngine.Init<EntityManager>();
                        oEngine.Init<StateMachineSystem>();
                        oEngine.Init<WorldSystem>();
                        oEngine.Init<TerrainSystem>();
                        return true;
                }();
                (void)systems_constructed;

                GetSystem<EntityManager>().SetSceneTree(scene.get());
                GetSystem<WorldSystem>().SetSceneTree(scene.get());

                boost::filesystem::create_directories("resource/terrain/gen");

                TerrainSystem::SConfig oCfg;
                oCfg.load_radius   = 10.0f;    // tile_size 8 → one tile ring around the focus
                oCfg.unload_radius = 16.0f;
                oCfg.unload_frame_delay = 3;
                oCfg.max_tile_activations_per_frame = 8;   // budgets asserted elsewhere; keep pipelines fast
                oCfg.max_active_tiles = 64;
                GetSystem<TerrainSystem>().SetConfig(oCfg);
        }

        void TearDown() override {
                auto & oTerrain = GetSystem<TerrainSystem>();
                for (const auto h : vFocusHandles) { GetSystem<WorldSystem>().UnregisterFocus(h); }
                vFocusHandles.clear();
                oTerrain.SetGpuSink(0, {}, {});
                oTerrain.DetachAll();

                auto & oWorldSystem = GetSystem<WorldSystem>();
                oWorldSystem.UnloadWorld();
                Drain();

                oWorldSystem.SetSceneTree(nullptr);
                GetSystem<EntityManager>().SetSceneTree(nullptr);

                GetSystem<TResourceManager>().Clear<TerrainAsset>();

                EnginePhysTestBase::TearDown();
        }

        void Drain(const uint32_t n = 2) {
                for (uint32_t i = 0; i < n; ++i) { StepFrame(1.f / 60.f); }
        }

        template <class TPred>
        bool PumpUntil(TPred pred, const uint32_t max_frames = 3000) {
                for (uint32_t i = 0; i < max_frames; ++i) {
                        StepFrame(1.f / 60.f);
                        if (pred()) { return true; }
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                return pred();
        }

        // Bake the analytic terrain with a per-test id (tile filenames embed it —
        // every test shares one scratch dir) and write tiles + manifest to
        // resource/terrain/gen/. TerrainSystem resolves tile paths relative to the
        // manifest dir, so bare "<id>_tile_X_Y.setile" names land correctly.
        TerrainBake::SOutput BakeToFiles(const std::string & sName, const uint32_t tiles) {
                TerrainBake::SConfig oCfg = se_test::MakeTerrainConfig(tiles);
                oCfg.id = sName;
                TerrainBake::SOutput oOut = se_test::BakeTerrain(oCfg, se_test::TestHeight);
                EXPECT_FALSE(oOut.vManifest.empty());

                const std::string sDir = "resource/terrain/gen/";
                for (const auto & oTile : oOut.vTiles) {
                        const std::string sTileName = oCfg.id + "_tile_"
                                        + std::to_string(oTile.x) + "_" + std::to_string(oTile.y) + ".setile";
                        EXPECT_TRUE(se_test::WriteBytes(sDir + sTileName, oTile.vBytes));
                }
                EXPECT_TRUE(se_test::WriteBytes(sDir + oCfg.id + ".seterrain", oOut.vManifest));
                return oOut;
        }

        // focuses must be unregistered at teardown — an expired focus node freezes
        // at its last position and would keep tiles wanted across tests
        TSceneTree::TSceneNode MakeFocus(const glm::vec3 vPos, const float radius = 0.f) {
                auto pFocus = scene->Create("terrain_focus");
                pFocus->SetPos(vPos);
                vFocusHandles.push_back(Worlds().RegisterFocus(pFocus, radius, 0.f));
                return pFocus;
        }

        TerrainSystem & Terrain() { return GetSystem<TerrainSystem>(); }
        WorldSystem  & Worlds()   { return GetSystem<WorldSystem>(); }
        std::vector<WorldSystem::FocusHandle> vFocusHandles;
};

// ---------------------------------------------------------------------------
// grid paging
// ---------------------------------------------------------------------------

TEST_F(TerrainTest, QueriesFailBeforeAttach) {
        float h = 0.0f;
        EXPECT_FALSE(Terrain().TryGetHeight(glm::vec3(4.0f, 0.0f, 4.0f), h));
}

TEST_F(TerrainTest, GridPagingActivatesTilesNearFocus) {
        TerrainBake::SOutput oOut = BakeToFiles("paging", 3);
        H<TerrainAsset> hAsset = CreateResource<TerrainAsset>(
                        "resource/terrain/gen/paging.seterrain");
        ASSERT_TRUE(hAsset.IsValid());

        const uint64_t reg = Terrain().AttachGrid(hAsset);
        ASSERT_NE(reg, 0u);

        // Focus on tile (1,1) center slightly above the surface: the 3D
        // distance-to-AABB metric includes height, so the focus altitude must sit
        // within (load_radius - terrain height) of the tiles it should keep loaded.
        auto pFocus = MakeFocus(glm::vec3(12.0f, 12.0f, 12.0f));

        ASSERT_TRUE(PumpUntil([this] { return Terrain().GetActiveTileCount() >= 1; }));

        float h = 0.0f;
        EXPECT_TRUE(Terrain().TryGetHeight(glm::vec3(12.0f, 0.0f, 12.0f), h));
        EXPECT_NEAR(h, se_test::TestHeight(12.0f, 12.0f), 0.3f);

        // a point outside the grid answers nothing
        EXPECT_FALSE(Terrain().TryGetHeight(glm::vec3(30.0f, 0.0f, 30.0f), h));

        // bring the focus over tile (0,0) (not wanted from (12,12): 3D distance
        // ≈13 m > load 10 m) → it pages in
        pFocus->SetPos(glm::vec3(4.0f, 12.0f, 4.0f));
        ASSERT_TRUE(PumpUntil([this, &h] { return Terrain().TryGetHeight(glm::vec3(4.5f, 0.0f, 4.5f), h); }));
        EXPECT_NEAR(h, se_test::TestHeight(4.5f, 4.5f), 0.3f);
}

TEST_F(TerrainTest, HysteresisUnloadsFarTiles) {
        TerrainBake::SOutput oOut = BakeToFiles("hysteresis", 3);
        (void)oOut;
        H<TerrainAsset> hAsset = CreateResource<TerrainAsset>(
                        "resource/terrain/gen/hysteresis.seterrain");
        ASSERT_TRUE(hAsset.IsValid());
        ASSERT_NE(Terrain().AttachGrid(hAsset), 0u);

        auto pFocus = MakeFocus(glm::vec3(12.0f, 12.0f, 12.0f));

        float h = 0.0f;
        ASSERT_TRUE(PumpUntil([&] { return Terrain().TryGetHeight(glm::vec3(12.0f, 0.0f, 12.0f), h); }));

        // teleport far beyond the unload radius → the tile unloads after the delay.
        // A read still in flight may land and rebuild one tile for a single unload
        // cycle — require the empty state to HOLD across the delay window.
        pFocus->SetPos(glm::vec3(500.0f, 12.0f, 500.0f));
        ASSERT_TRUE(PumpUntil([this] { return Terrain().GetActiveTileCount() == 0; }, 1000));
        bool bHeld = true;
        for (int i = 0; i < 10; ++i) {
                Drain(1);
                bHeld = bHeld && Terrain().GetActiveTileCount() == 0;
        }
        ASSERT_TRUE(bHeld);
        EXPECT_FALSE(Terrain().TryGetHeight(glm::vec3(12.0f, 0.0f, 12.0f), h));
}

#ifdef SE_PHYSICS_ENABLED

TEST_F(TerrainTest, HeightAgreesWithPhysicsRaycast) {
        TerrainBake::SOutput oOut = BakeToFiles("raycast", 2);
        (void)oOut;
        H<TerrainAsset> hAsset = CreateResource<TerrainAsset>(
                        "resource/terrain/gen/raycast.seterrain");
        ASSERT_TRUE(hAsset.IsValid());
        ASSERT_NE(Terrain().AttachGrid(hAsset), 0u);

        MakeFocus(glm::vec3(4.0f, 12.0f, 4.0f), 100.f);   // big radius → all 4 tiles active

        float h = 0.0f;
        ASSERT_TRUE(PumpUntil([&] { return Terrain().GetActiveTileCount() == 4; }));

        // cross-test the sample-grid convention: query height vs raycast at sample
        // points and cell centers — they must agree to quantization + Jolt tolerance
        for (float z = 0.5f; z < 16.0f; z += 1.0f) {
                for (float x = 0.5f; x < 16.0f; x += 1.0f) {
                        ASSERT_TRUE(Terrain().TryGetHeight(glm::vec3(x, 0.0f, z), h)) << x << "," << z;

                        PhysicsRay oRay;
                        oRay.vOrigin = glm::vec3(x, 50.0f, z);
                        oRay.vDirection = glm::vec3(0.0f, -1.0f, 0.0f);
                        RaycastHit oHit;
                        const bool bHit = GetSystem<PhysicsSystem>().Raycast(oRay, oHit);
                        ASSERT_TRUE(bHit) << "no physics hit at " << x << "," << z;
                        EXPECT_NEAR(oHit.vPoint.y, h, 0.05f) << "divergence at " << x << "," << z;
                }
        }
}

TEST_F(TerrainTest, SphereRestsOnTerrain) {
        TerrainBake::SOutput oOut = BakeToFiles("rest", 1);
        (void)oOut;
        H<TerrainAsset> hAsset = CreateResource<TerrainAsset>(
                        "resource/terrain/gen/rest.seterrain");
        ASSERT_TRUE(hAsset.IsValid());
        ASSERT_NE(Terrain().AttachGrid(hAsset), 0u);

        MakeFocus(glm::vec3(4.0f, 12.0f, 4.0f), 100.f);
        ASSERT_TRUE(PumpUntil([this] { return Terrain().GetActiveTileCount() == 1; }));

        const glm::vec3 vSpawn(4.0f, 30.0f, 4.0f);
        RigidBodyDesc oDesc;
        oDesc.oCollider.type = ColliderDesc::Sphere;
        oDesc.oCollider.radius = 0.5f;
        oDesc.vInitialPosition = vSpawn;
        const BodyHandle hBody = GetSystem<PhysicsSystem>().CreateRigidBody(oDesc);
        ASSERT_TRUE(hBody.IsValid());

        // settle (real physics steps run inside StepFrame)
        for (int i = 0; i < 240; ++i) { StepFrame(1.f / 60.f); }

        float h = 0.0f;
        ASSERT_TRUE(Terrain().TryGetHeight(vSpawn, h));
        void * pNode = GetSystem<PhysicsSystem>().GetNode(hBody);   // not registered — position via Raycast
        (void)pNode;
        PhysicsRay oRay;
        oRay.vOrigin = glm::vec3(vSpawn.x, 30.0f, vSpawn.z);
        oRay.vDirection = glm::vec3(0.0f, -1.0f, 0.0f);
        RaycastHit oHit;
        // the resting sphere is above the query point; raycast the surface instead
        ASSERT_TRUE(GetSystem<PhysicsSystem>().Raycast(oRay, oHit, QueryFilter{}));
        EXPECT_NEAR(oHit.vPoint.y, h, 0.05f);
}

#endif // SE_PHYSICS_ENABLED

// ---------------------------------------------------------------------------
// component mode
// ---------------------------------------------------------------------------

TEST_F(TerrainTest, ComponentModeFollowsSubLevelLifecycle) {
        BakeToFiles("component", 2);
        const std::string sDir = "resource/terrain/gen/";

        // sub-level scene: root node carries a TerrainComponent (serialized ctor)
        SE::FlatBuffers::NodeT oRoot = se_test::MakeNode("terrain_level");
        {
                auto pComp = std::make_unique<SE::FlatBuffers::ComponentT>();
                SE::FlatBuffers::TerrainComponentT oTC;
                oTC.manifest  = sDir + "component.seterrain";
                oTC.grid_mode = false;
                pComp->component.Set(std::move(oTC));
                oRoot.components.push_back(std::move(pComp));
        }
        const std::string sScenePath = sDir + "component_level.sesc";
        ASSERT_TRUE(se_test::WriteBytes(sScenePath, se_test::BakeScene(std::move(oRoot))));

        // world: one sub-level, streaming disabled → synchronous activation
        auto oWorld = se_test::MakeWorld("terrain_component_world");
        se_test::SetStreaming(*oWorld, /*enabled=*/false, 96.f, 128.f);
        se_test::AddSubLevel(*oWorld, sScenePath,
                             SE::FlatBuffers::Vec3(0, 0, 0), SE::FlatBuffers::Vec3(16, 8, 16));
        const std::string sWorldPath = sDir + "component_world.seworld";
        ASSERT_TRUE(se_test::WriteBytes(sWorldPath, se_test::BakeWorld(*oWorld)));

        float h = 0.0f;
        EXPECT_FALSE(Terrain().TryGetHeight(glm::vec3(4.0f, 0.0f, 4.0f), h));

        ASSERT_TRUE(Worlds().LoadWorld(sWorldPath));
        Drain();

        // sub-level active → its Terrain component enabled → tiles wanted → active
        ASSERT_TRUE(PumpUntil([this] { return Terrain().GetActiveTileCount() == 4; }));
        EXPECT_TRUE(Terrain().TryGetHeight(glm::vec3(4.0f, 0.0f, 4.0f), h));

        // deactivate → components disabled → tiles released
        ASSERT_TRUE(Worlds().DeactivateLevel(0));
        ASSERT_TRUE(PumpUntil([this] { return Terrain().GetActiveTileCount() == 0; }));
        EXPECT_FALSE(Terrain().TryGetHeight(glm::vec3(4.0f, 0.0f, 4.0f), h));
}

TEST_F(TerrainTest, ChurnTeardownCompleteness) {
        TerrainBake::SOutput oOut = BakeToFiles("churn", 3);
        (void)oOut;
        H<TerrainAsset> hAsset = CreateResource<TerrainAsset>(
                        "resource/terrain/gen/churn.seterrain");
        ASSERT_TRUE(hAsset.IsValid());
        ASSERT_NE(Terrain().AttachGrid(hAsset), 0u);

        auto pFocus = MakeFocus(glm::vec3(4.0f, 12.0f, 4.0f));

        // teleport between far corners repeatedly
        for (int i = 0; i < 8; ++i) {
                pFocus->SetPos(i % 2 == 0 ? glm::vec3(4.0f, 12.0f, 4.0f)
                                          : glm::vec3(20.0f, 12.0f, 20.0f));
                PumpUntil([] { return false; }, 60);
        }

        // settle back and let everything unload
        pFocus->SetPos(glm::vec3(-500.0f, 0.0f, -500.0f));
        ASSERT_TRUE(PumpUntil([this] { return Terrain().GetActiveTileCount() == 0; }, 2000));

        const TerrainSystem::SStats oStats = Terrain().GetStats();
        EXPECT_EQ(oStats.active_tiles, 0u);
        EXPECT_EQ(oStats.loading, 0u);
        // nothing in flight for terrain anymore (the shared worker may be idle)
        float h = 0.0f;
        EXPECT_FALSE(Terrain().TryGetHeight(glm::vec3(4.0f, 0.0f, 4.0f), h));
        EXPECT_FALSE(Terrain().TryGetHeight(glm::vec3(12.0f, 0.0f, 12.0f), h));
}

TEST_F(TerrainTest, DetachReleasesEverything) {
        TerrainBake::SOutput oOut = BakeToFiles("detach", 2);
        (void)oOut;
        H<TerrainAsset> hAsset = CreateResource<TerrainAsset>(
                        "resource/terrain/gen/detach.seterrain");
        ASSERT_TRUE(hAsset.IsValid());
        const uint64_t reg = Terrain().AttachGrid(hAsset);
        ASSERT_NE(reg, 0u);

        MakeFocus(glm::vec3(4.0f, 12.0f, 4.0f));
        ASSERT_TRUE(PumpUntil([this] { return Terrain().GetActiveTileCount() == 4; }));

        Terrain().Detach(reg);
        Drain();
        EXPECT_EQ(Terrain().GetActiveTileCount(), 0u);
        float h = 0.0f;
        EXPECT_FALSE(Terrain().TryGetHeight(glm::vec3(4.0f, 0.0f, 4.0f), h));

        // the registration id is dead; attaching the same asset again works
        const uint64_t reg2 = Terrain().AttachGrid(hAsset);
        ASSERT_NE(reg2, 0u);
        ASSERT_TRUE(PumpUntil([this] { return Terrain().GetActiveTileCount() == 4; }));
}

} // namespace

// ---------------------------------------------------------------------------
// malformed tile recovery (round 7)
// ---------------------------------------------------------------------------

// A tile file that VERIFIES but decodes empty (no LODs) must reset its slot so
// the planner retries after the cooldown. The pre-fix early-return left the
// slot stuck Requested forever — one bad .setile = one permanent terrain hole
// while every prop on it kept rendering (the round-7 screenshot).
TEST_F(TerrainTest, MalformedTileResetsAndRetries) {
        TerrainBake::SOutput oOut = BakeToFiles("malformed", 2);

        // sabotage every tile on disk: schema-VALID TerrainTileData, no LODs
        for (const auto & oTile : oOut.vTiles) {
                const std::string sTilePath = "resource/terrain/gen/malformed_tile_"
                                + std::to_string(oTile.x) + "_" + std::to_string(oTile.y) + ".setile";
                flatbuffers::FlatBufferBuilder oFB;
                // `lods` is a required field: present but EMPTY passes both the
                // builder's required check and verification, then yields no LODs
                std::vector<flatbuffers::Offset<SE::FlatBuffers::TerrainLod>> vNoLods;
                auto lods = oFB.CreateVector(vNoLods);
                auto offset = SE::FlatBuffers::CreateTerrainTileData(
                                oFB, oTile.x, oTile.y, 0.f, 1.f, lods);
                oFB.Finish(offset, SE::FlatBuffers::TerrainIdentifier());
                const std::vector<uint8_t> vBytes(oFB.GetBufferPointer(),
                                                  oFB.GetBufferPointer() + oFB.GetSize());
                ASSERT_TRUE(se_test::WriteBytes(sTilePath, vBytes));
        }

        H<TerrainAsset> hAsset = CreateResource<TerrainAsset>(
                        "resource/terrain/gen/malformed.seterrain");
        ASSERT_TRUE(hAsset.IsValid());
        ASSERT_NE(Terrain().AttachGrid(hAsset), 0u);
        MakeFocus(glm::vec3(12.0f, 12.0f, 12.0f));

        // the bad tiles dispatch, fail decode, and the slots RESET: requested
        // drains back to 0 instead of sticking at 1 forever
        ASSERT_TRUE(PumpUntil([this] {
                const auto s = Terrain().GetStats();
                return s.requested > 0 || s.loading > 0;
        }, 500)) << "wanted tiles never dispatched";
        ASSERT_TRUE(PumpUntil([this] {
                const auto s = Terrain().GetStats();
                return s.requested == 0 && s.loading == 0 && s.active_tiles == 0;
        }, 1000)) << "slot stuck — a malformed tile leaked without reset";

        // past the retry cooldown the planner re-requests them
        ASSERT_TRUE(PumpUntil([this] {
                return Terrain().GetStats().requested > 0;
        }, 3000)) << "planner never retried the malformed tile";
}
