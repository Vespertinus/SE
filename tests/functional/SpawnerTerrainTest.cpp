// ---------------------------------------------------------------------------
// SpawnerTerrainTest — Tier C (Stage 3 Step 5): spawner snap=TerrainHeight over
// the terrain_app composition (TerrainSystem pages tiles through the shared
// worker; SpawnerSystem executes populations through EntityManager). A spawned
// NPC above a heightfield tile must land on the baked surface; without a
// terrain probe the spawn falls back to the sampled position.
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>
#include "EngineFixture.h"
#include "PhysicsHarness.h"
#include "TerrainAssets.h"
#include "WorldAssets.h"

#include <terrain/TerrainBake.tcc>
#include <terrain/TerrainSystem.h>
#include <terrain/TerrainAsset.h>
#include <world/WorldSystem.h>
#include <EntityManager.h>
#include <spawner/SpawnerSystem.h>
#include <spawner/Population.h>

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>

#include <chrono>
#include <memory>
#include <thread>
#include <vector>

using namespace SE;

namespace {

class SpawnerTerrainTest : public se_test::EnginePhysTestBase {

protected:
        void SetUp() override {
                EnginePhysTestBase::SetUp();

                static bool systems_constructed = []() {
                        auto & oEngine = TEngine::Instance();
                        oEngine.Init<EntityManager>();
                        oEngine.Init<StateMachineSystem>();
                        oEngine.Init<WorldSystem>();
                        oEngine.Init<TerrainSystem>();
                        oEngine.Init<TEntityTemplateSystem>();
                        oEngine.Init<SpawnerSystem>();
                        return true;
                }();
                (void)systems_constructed;

                GetSystem<EntityManager>().SetSceneTree(scene.get());
                GetSystem<WorldSystem>().SetSceneTree(scene.get());
                GetSystem<TEntityTemplateSystem>().SetSceneTree(scene.get());

                // plain-spawn delivery (the one instantiation path)
                auto * pEM = &GetSystem<EntityManager>();
                pEM->SetTemplateInstantiator(
                        [pEM](const SpawnIntent & oIntent) -> TSceneTree::TSceneNodeWeak {
                                SpawnRequest oReq;
                                oReq.name         = oIntent.name;
                                oReq.pParent      = oIntent.pParent;
                                oReq.vTranslation = oIntent.vTranslation;
                                oReq.vRotation    = oIntent.vRotation;
                                oReq.vScale       = oIntent.vScale;
                                oReq.enabled      = oIntent.enabled;
                                return pEM->Spawn(oReq);
                        });

                // terrain probe (the app-side seam; TerrainSystem is complete here)
                auto * pTerrain = &Terrain();
                Spawner().SetTerrainProbe(
                        [pTerrain](const glm::vec3 & vPos, float & out_height) {
                                return pTerrain->TryGetHeight(vPos, out_height);
                        });

                TerrainSystem::SConfig oCfg;
                oCfg.load_radius   = 10.0f;
                oCfg.unload_radius = 16.0f;
                oCfg.unload_frame_delay = 3;
                oCfg.max_tile_activations_per_frame = 8;
                oCfg.max_active_tiles = 64;
                Terrain().SetConfig(oCfg);

                boost::filesystem::create_directories("resource/terrain/gen");
        }

        void TearDown() override {
                for (const auto h : vFocusHandles) { Worlds().UnregisterFocus(h); }
                vFocusHandles.clear();
                Terrain().SetGpuSink(0, {}, {});
                Terrain().DetachAll();
                Spawner().SetTerrainProbe({});
                Spawner().SetPhysicsProbe({});
                GetSystem<EntityManager>().SetTemplateInstantiator({});

                Worlds().UnloadWorld();
                Drain();

                Worlds().SetSceneTree(nullptr);
                GetSystem<EntityManager>().SetSceneTree(nullptr);
                GetSystem<TEntityTemplateSystem>().SetSceneTree(nullptr);

                GetSystem<TResourceManager>().Clear<TerrainAsset>();
                GetSystem<TResourceManager>().Clear<WorldAsset>();
                GetSystem<TResourceManager>().Clear<StateMachineAsset>();

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

        // Population hovering at hover_y over the tile; snap does the rest.
        TSceneTree::TSceneNode MakeCamp(const glm::vec3 vPos, const FlatBuffers::SpawnSnap snap) {
                auto pCamp = scene->Create("Camp");
                pCamp->SetPos(vPos);

                PopulationDesc oDesc;
                oDesc.id = "wolves";
                oDesc.tick_interval_s = 0.05f;
                PopulationMemberDesc m;
                m.template_id = "npc_wolf";
                m.count       = 1;
                oDesc.members.push_back(m);
                oDesc.snap = snap;
                pCamp->CreateComponent<Population>(oDesc);
                return pCamp;
        }

        TerrainSystem & Terrain() { return GetSystem<TerrainSystem>(); }
        WorldSystem  & Worlds()   { return GetSystem<WorldSystem>(); }
        SpawnerSystem & Spawner() { return GetSystem<SpawnerSystem>(); }

        std::vector<WorldSystem::FocusHandle> vFocusHandles;
};

// ---------------------------------------------------------------------------
// snap=TerrainHeight lands the spawn on the baked surface
// ---------------------------------------------------------------------------

TEST_F(SpawnerTerrainTest, TerrainSnapPlacesSpawnOnSurface) {
        // bake + attach a 3x3 grid; focus over tile (1,1)
        TerrainBake::SConfig oCfg = se_test::MakeTerrainConfig(3);
        oCfg.id = "spawner_snap";
        TerrainBake::SOutput oOut = se_test::BakeTerrain(oCfg, se_test::TestHeight);
        ASSERT_FALSE(oOut.vManifest.empty());
        const std::string sDir = "resource/terrain/gen/";
        for (const auto & oTile : oOut.vTiles) {
                const std::string sTileName = oCfg.id + "_tile_"
                                + std::to_string(oTile.x) + "_" + std::to_string(oTile.y) + ".setile";
                ASSERT_TRUE(se_test::WriteBytes(sDir + sTileName, oTile.vBytes));
        }
        ASSERT_TRUE(se_test::WriteBytes(sDir + oCfg.id + ".seterrain", oOut.vManifest));

        H<TerrainAsset> hAsset = CreateResource<TerrainAsset>("resource/terrain/gen/spawner_snap.seterrain");
        ASSERT_TRUE(hAsset.IsValid());
        ASSERT_NE(Terrain().AttachGrid(hAsset), 0u);

        auto pFocus = scene->Create("terrain_focus");
        pFocus->SetPos(glm::vec3(12.0f, 12.0f, 12.0f));
        vFocusHandles.push_back(Worlds().RegisterFocus(pFocus, 0.f, 0.f));

        float h = 0.f;
        ASSERT_TRUE(PumpUntil([&] { return Terrain().TryGetHeight(glm::vec3(12.0f, 0.0f, 12.0f), h); }));
        EXPECT_NEAR(h, se_test::TestHeight(12.0f, 12.0f), 0.3f);

        // camp hovers 40 m above the surface; snap must place the wolf on it
        auto pCamp = MakeCamp(glm::vec3(12.0f, 50.0f, 12.0f),
                              FlatBuffers::SpawnSnap::TerrainHeight);
        ASSERT_TRUE(PumpUntil([&] { return pCamp->FindChild(StrID("npc_wolf")) != nullptr; }));

        const glm::vec3 vWolfPos = pCamp->FindChild(StrID("npc_wolf"))->GetTransform().GetWorldPos();
        EXPECT_NEAR(vWolfPos.y, se_test::TestHeight(12.0f, 12.0f), 0.3f);
        EXPECT_LT(vWolfPos.y, 49.f);   // sanity: snapped DOWN from the 50 m hover
}

// ---------------------------------------------------------------------------
// no probe wired: snap degrades to the sampled position, spawn still happens
// ---------------------------------------------------------------------------

TEST_F(SpawnerTerrainTest, SnapFallsBackGracefullyWithoutTerrain) {
        Spawner().SetTerrainProbe({});   // unwire

        auto pCamp = MakeCamp(glm::vec3(12.0f, 7.0f, 12.0f),
                              FlatBuffers::SpawnSnap::TerrainHeight);
        PumpUntil([&] { return pCamp->FindChild(StrID("npc_wolf")) != nullptr; }, 120);

        auto pWolf = pCamp->FindChild(StrID("npc_wolf"));
        ASSERT_NE(pWolf, nullptr);
        // population origin Y (7) kept — the unsnapped fallback
        EXPECT_NEAR(pWolf->GetTransform().GetWorldPos().y, 7.0f, 0.5f);
}

} // namespace
