// ---------------------------------------------------------------------------
// SpawnerTest — Tier C: the spawner wired through the world_app composition
// (EntityManager, StateMachineSystem, WorldSystem, SpatialIndexSystem,
// EntityTemplateSystem, SpawnerSystem). Covers arm-on-activation, spawn
// parenting/tracking, teardown semantics (sub-level vs persistent), condition
// gating, trigger waves, respawn, determinism, the shared spawn budget, despawn
// density control, world-slot claims, and the end-to-end .setm path. Fixture
// scenes/worlds are baked inline (WorldAssets.h) into resource/world/gen/.
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>
#include "EngineFixture.h"
#include "PhysicsHarness.h"
#include "WorldAssets.h"
#include "BlackboardAssets.h"   // Step 9: global-blackboard world declaration

#include <world/WorldSystem.h>
#include <world/WorldAsset.h>
#include <world/SpatialIndexSystem.h>
#include <EntityManager.h>
#include <EntityTemplateSystem.h>
#include <spawner/SpawnerSystem.h>
#include <spawner/Population.h>
#include <spawner/SpawnPoint.h>
#include <TriggerVolume.h>
#include <ai/BlackboardSystem.h>   // Step 9: WorldFlag condition source

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace SE;

namespace {

constexpr const char * sCampScenePath = "resource/world/gen/spawner_camp.sesc";
constexpr const char * sExtraScenePath = "resource/world/gen/spawner_extra.sesc";

// A camp sub-level: root carries the population; an optional spawn volume node
// hangs off it. Built per test (members/conditions differ).
std::vector<uint8_t> MakeCampScene(
                std::vector<std::unique_ptr<FlatBuffers::SpawnMemberT>> vMembers,
                const float respawn_delay_s = -1.f,
                const FlatBuffers::SpawnLifetime lifetime = FlatBuffers::SpawnLifetime::SubLevel,
                const float despawn_radius = 0.f) {

        auto oCamp = se_test::MakeNode("Camp");
        oCamp.children.push_back(std::make_unique<FlatBuffers::NodeT>(
                se_test::MakeNode("vol_a", glm::vec3(3.f, 0.f, 3.f))));
        se_test::AddSpawnVolumeComponent(*oCamp.children.back(), glm::vec3(4.f, 2.f, 4.f));

        auto pPop = std::make_unique<FlatBuffers::PopulationT>();
        pPop->id              = "wolves";
        pPop->members         = std::move(vMembers);
        pPop->tick_interval_s = 0.05f;
        pPop->respawn_delay_s = respawn_delay_s;
        pPop->lifetime        = lifetime;
        pPop->despawn_radius  = despawn_radius;
        auto pComp = std::make_unique<FlatBuffers::ComponentT>();
        pComp->component.Set(std::move(*pPop));
        oCamp.components.push_back(std::move(pComp));

        return se_test::BakeScene(std::move(oCamp));
}

// Second sub-level, no populations (teardown-does-not-touch-others cases).
std::vector<uint8_t> MakeExtraScene() {
        return se_test::BakeScene(se_test::MakeNode("Extra"));
}

} // namespace

// ---------------------------------------------------------------------------

class SpawnerTest : public se_test::EnginePhysTestBase {

protected:
        void SetUp() override {
                EnginePhysTestBase::SetUp();

                // order matters: WorldSystem after EntityManager; SpatialIndexSystem
                // and SpawnerSystem after WorldSystem (ctor dependencies)
                static bool systems_constructed = []() {
                        auto & oEngine = TEngine::Instance();
                        oEngine.Init<EntityManager>();
                        oEngine.Init<StateMachineSystem>();
                        oEngine.Init<WorldSystem>();
                        // Step 9: after WorldSystem (ctor dependency) — worlds
                        // without a declared global leave it idle
                        oEngine.Init<BlackboardSystem>();
                        oEngine.Init<SpatialIndexSystem>();
                        oEngine.Init<TEntityTemplateSystem>();
                        oEngine.Init<SpawnerSystem>();
                        return true;
                }();
                (void)systems_constructed;

                GetSystem<EntityManager>().SetSceneTree(scene.get());
                GetSystem<WorldSystem>().SetSceneTree(scene.get());
                GetSystem<TEntityTemplateSystem>().SetSceneTree(scene.get());

                // app-level wiring: the ONE instantiation path. Default delivery is
                // a plain spawn (tests below don't register templates); the .setm
                // end-to-end test re-routes to EntityTemplateSystem::Instantiate.
                auto * pEM = &GetSystem<EntityManager>();
                pEM->SetTemplateInstantiator(
                        [pEM](const SpawnIntent & oIntent) -> TSceneTree::TSceneNodeWeak {
                                SpawnRequest oReq;
                                oReq.name         = oIntent.name.empty() ? oIntent.prefab_id
                                                                         : oIntent.name;
                                oReq.pParent      = oIntent.pParent;
                                oReq.vTranslation = oIntent.vTranslation;
                                oReq.vRotation    = oIntent.vRotation;
                                oReq.vScale       = oIntent.vScale;
                                oReq.enabled      = oIntent.enabled;
                                return pEM->Spawn(oReq);
                        });

                // deterministic-ish small budget by default; tests may override
                GetSystem<SpawnerSystem>().SetConfig({ 8 });

                boost::filesystem::create_directories("resource/world/gen");
        }

        void TearDown() override {
                GetSystem<SpawnerSystem>().SetTerrainProbe({});
                GetSystem<SpawnerSystem>().SetPhysicsProbe({});
                GetSystem<EntityManager>().SetTemplateInstantiator({});
                GetSystem<TEntityTemplateSystem>().UnloadAll();

                auto & oWorldSystem = GetSystem<WorldSystem>();
                oWorldSystem.UnloadWorld();
                Drain();

                oWorldSystem.SetSceneTree(nullptr);
                GetSystem<EntityManager>().SetSceneTree(nullptr);
                GetSystem<TEntityTemplateSystem>().SetSceneTree(nullptr);

                GetSystem<TResourceManager>().Clear<WorldAsset>();
                GetSystem<TResourceManager>().Clear<StateMachineAsset>();
                GetSystem<TResourceManager>().Clear<BlackboardAsset>();

                EnginePhysTestBase::TearDown();
        }

        void Drain(const uint32_t n = 2) {
                for (uint32_t i = 0; i < n; ++i) { StepFrame(1.f / 60.f); }
        }

        // Bake + load a one-sub-level world around the given scene bytes.
        bool LoadCampWorld(const std::vector<uint8_t> & vCampBytes,
                           const std::vector<std::string> & vWorldPopulations = {}) {
                EXPECT_TRUE(se_test::WriteBytes(sCampScenePath, vCampBytes));

                auto pWorldT = se_test::MakeWorld("spawner_world_test");
                se_test::AddSubLevel(*pWorldT, sCampScenePath,
                                     FlatBuffers::Vec3(-8.f, -8.f, -8.f),
                                     FlatBuffers::Vec3(8.f, 8.f, 8.f));
                for (const auto & sPop : vWorldPopulations) {
                        auto pDef = std::make_unique<FlatBuffers::WorldSpawnDefT>();
                        pDef->population = sPop;
                        pWorldT->spawn.push_back(std::move(pDef));
                }
                pWorldT->seed = 1234;
                se_test::SetStreaming(*pWorldT, false, 40.f, 64.f);

                const auto vBytes = se_test::BakeWorld(*pWorldT);
                auto hWorld = CreateResource<WorldAsset>("spawner_world_test",
                                                         FlatBuffers::GetWorld(vBytes.data()));
                return Worlds().LoadWorld(hWorld);
        }

        WorldSystem & Worlds() { return GetSystem<WorldSystem>(); }
        SpawnerSystem & Spawner() { return GetSystem<SpawnerSystem>(); }

        std::vector<uint8_t> TwoWolves() {
                std::vector<std::unique_ptr<FlatBuffers::SpawnMemberT>> vMembers;
                vMembers.push_back(se_test::MakeSpawnMember("npc_wolf", 2));
                return MakeCampScene(std::move(vMembers));
        }
};

// ---------------------------------------------------------------------------
// 1. Population arms on activation and spawns its quota.
// ---------------------------------------------------------------------------

TEST_F(SpawnerTest, ArmOnActivationSpawnsQuota) {
        ASSERT_TRUE(LoadCampWorld(TwoWolves()));
        Drain();

        auto pCamp = scene->GetRoot()->FindChild(StrID("world"));
        ASSERT_NE(pCamp, nullptr);
        auto pCampLevel = pCamp->FindChild(StrID("spawner_camp.sesc#0"));
        ASSERT_NE(pCampLevel, nullptr);
        auto pPopNode = pCampLevel->FindChild(StrID("Camp"));
        ASSERT_NE(pPopNode, nullptr);

        // two wolves, parented under the population node, uniquified names
        EXPECT_NE(pPopNode->FindChild(StrID("npc_wolf")), nullptr);
        EXPECT_NE(pPopNode->FindChild(StrID("npc_wolf_1")), nullptr);
}

// ---------------------------------------------------------------------------
// 2. Persistent spawns outlive ANY sub-level deactivation — their own level's
//    included (round 12: pursuers must survive the camp's streaming-edge
//    teardown) — and are reaped only by the world unload's cascade.
// ---------------------------------------------------------------------------

TEST_F(SpawnerTest, PersistentSpawns_SurviveLevelDeactivations_DieWithWorld) {
        // persistent population: spawns parent under the dynamics root
        std::vector<std::unique_ptr<FlatBuffers::SpawnMemberT>> vMembers;
        vMembers.push_back(se_test::MakeSpawnMember("npc_wolf", 2));
        auto vCamp = MakeCampScene(std::move(vMembers), -1.f,
                                   FlatBuffers::SpawnLifetime::Persistent);
        ASSERT_TRUE(se_test::WriteBytes(sCampScenePath, vCamp));
        ASSERT_TRUE(se_test::WriteBytes(sExtraScenePath, MakeExtraScene()));

        auto pWorldT = se_test::MakeWorld("spawner_world_test");
        se_test::AddSubLevel(*pWorldT, sCampScenePath,
                             FlatBuffers::Vec3(-8.f, -8.f, -8.f), FlatBuffers::Vec3(8.f, 8.f, 8.f));
        se_test::AddSubLevel(*pWorldT, sExtraScenePath,
                             FlatBuffers::Vec3(32.f, -8.f, -8.f), FlatBuffers::Vec3(48.f, 8.f, 8.f));
        se_test::SetStreaming(*pWorldT, false, 40.f, 64.f);
        const auto vWorldBytes = se_test::BakeWorld(*pWorldT);
        auto hWorld = CreateResource<WorldAsset>("spawner_world_test",
                                                 FlatBuffers::GetWorld(vWorldBytes.data()));
        ASSERT_TRUE(Worlds().LoadWorld(hWorld));
        Drain();

        auto pDyn = Worlds().GetDynamicsRoot().lock();
        ASSERT_NE(pDyn, nullptr);

        // PERSISTENT population: spawns parent under the dynamics root, not the Camp
        auto CountWolves = [this]() {
                uint32_t n = 0;
                scene->GetRoot()->DepthFirstWalk([&n](auto & oNode) {
                        if (oNode.GetName() == "npc_wolf" || oNode.GetName() == "npc_wolf_1") { ++n; }
                        return true;
                });
                return n;
        };
        EXPECT_EQ(CountWolves(), 2u);
        EXPECT_NE(pDyn->FindChild(StrID("npc_wolf")), nullptr);

        // deactivate the OTHER level: nothing happens to the spawns
        ASSERT_TRUE(Worlds().DeactivateLevel(1));
        Drain();
        EXPECT_EQ(CountWolves(), 2u);

        // deactivate the population's OWN level: the population dies with it and
        // drops its records, but the entities stay alive under the dynamics root
        ASSERT_TRUE(Worlds().DeactivateLevel(0));
        Drain(4);
        EXPECT_EQ(CountWolves(), 2u);
        EXPECT_NE(pDyn->FindChild(StrID("npc_wolf")), nullptr);

        // the world unload's dynamics-root cascade is what finally reaps them
        Worlds().UnloadWorld();
        Drain(4);
        EXPECT_EQ(CountWolves(), 0u);
}

// ---------------------------------------------------------------------------
// 3. FocusNear condition gates spawning; registering a nearby focus opens it.
// ---------------------------------------------------------------------------

TEST_F(SpawnerTest, FocusConditionGatesSpawning) {
        // population with a FocusNear(10) arming condition
        auto oCamp = se_test::MakeNode("Camp");
        auto pPop = std::make_unique<FlatBuffers::PopulationT>();
        pPop->id = "wolves";
        pPop->tick_interval_s = 0.05f;
        pPop->members.push_back(se_test::MakeSpawnMember("npc_wolf", 1));
        se_test::SetPopulationFocusNear(*pPop, 10.f);
        auto pComp = std::make_unique<FlatBuffers::ComponentT>();
        pComp->component.Set(std::move(*pPop));
        oCamp.components.push_back(std::move(pComp));
        const auto vCamp = se_test::BakeScene(std::move(oCamp));

        ASSERT_TRUE(LoadCampWorld(vCamp));

        // no focus → gate closed
        Drain(4);
        auto pCampLevel = scene->GetRoot()->FindChild(StrID("world"))
                                     ->FindChild(StrID("spawner_camp.sesc#0"));
        ASSERT_NE(pCampLevel, nullptr);
        auto pPopNode = pCampLevel->FindChild(StrID("Camp"));
        ASSERT_NE(pPopNode, nullptr);
        EXPECT_EQ(pPopNode->FindChild(StrID("npc_wolf")), nullptr);

        // focus nearby (population sits at origin) → gate opens
        auto pFocusNode = scene->Create("player_focus");
        pFocusNode->SetPos(glm::vec3(2.f, 0.f, 2.f));
        Worlds().RegisterFocus(pFocusNode, 0.f, 10.f);
        Drain(4);
        EXPECT_NE(pPopNode->FindChild(StrID("npc_wolf")), nullptr);
}

// ---------------------------------------------------------------------------
// 3b. WorldFlag condition (Step 9): the global blackboard gates the population.
// ---------------------------------------------------------------------------

TEST_F(SpawnerTest, WorldFlagConditionGatesSpawning) {
        // population with a WorldFlag("gate_open") arming condition
        auto oCamp = se_test::MakeNode("Camp");
        auto pPop = std::make_unique<FlatBuffers::PopulationT>();
        pPop->id = "wolves";
        pPop->tick_interval_s = 0.05f;
        pPop->members.push_back(se_test::MakeSpawnMember("npc_wolf", 1));
        se_test::SetPopulationWorldFlag(*pPop, "gate_open");
        auto pComp = std::make_unique<FlatBuffers::ComponentT>();
        pComp->component.Set(std::move(*pPop));
        oCamp.components.push_back(std::move(pComp));
        const auto vCamp = se_test::BakeScene(std::move(oCamp));
        ASSERT_TRUE(se_test::WriteBytes(sCampScenePath, vCamp));

        // world with an inline-declared global blackboard
        auto pWorldT = se_test::MakeWorld("spawner_wf_world");
        se_test::AddSubLevel(*pWorldT, sCampScenePath,
                             FlatBuffers::Vec3(-8.f, -8.f, -8.f),
                             FlatBuffers::Vec3(8.f, 8.f, 8.f));
        pWorldT->seed = 1234;
        se_test::SetStreaming(*pWorldT, false, 40.f, 64.f);

        auto oDef = se_test::MakeBBDef("wf_world_bb");
        se_test::AddKey(*oDef, "gate_open", FlatBuffers::EBType::Bool);
        se_test::SetGlobalBlackboard(*pWorldT, se_test::MakeBBHolderInline(*oDef, "wf_world_global"));

        const auto vWorld = se_test::BakeWorld(*pWorldT);
        auto hWorld = CreateResource<WorldAsset>("spawner_wf_world",
                                                 FlatBuffers::GetWorld(vWorld.data()));
        ASSERT_TRUE(Worlds().LoadWorld(hWorld));
        Drain(4);

        auto pCampLevel = scene->GetRoot()->FindChild(StrID("world"))
                                     ->FindChild(StrID("spawner_camp.sesc#0"));
        ASSERT_NE(pCampLevel, nullptr);
        auto pPopNode = pCampLevel->FindChild(StrID("Camp"));
        ASSERT_NE(pPopNode, nullptr);
        EXPECT_EQ(pPopNode->FindChild(StrID("npc_wolf")), nullptr);

        // open the gate through the global blackboard
        BlackboardCore & oGlobal = GetSystem<BlackboardSystem>().GetGlobal();
        EXPECT_TRUE(oGlobal.SetBool(StrID("gate_open"), true));
        Drain(4);
        EXPECT_NE(pPopNode->FindChild(StrID("npc_wolf")), nullptr);
}

// ---------------------------------------------------------------------------
// 4. Trigger-driven waves: ETriggerFired arms the wave; members spawn on the
//    next due tick.
// ---------------------------------------------------------------------------

TEST_F(SpawnerTest, TriggerEventArmsWave) {
        auto oCamp = se_test::MakeNode("Camp");
        auto pPop = std::make_unique<FlatBuffers::PopulationT>();
        pPop->id = "wolves";
        pPop->tick_interval_s = 0.05f;
        pPop->members.push_back(se_test::MakeSpawnMember("npc_wolf", 1));
        std::vector<std::unique_ptr<FlatBuffers::SpawnMemberT>> vWaveMembers;
        vWaveMembers.push_back(se_test::MakeSpawnMember("ambusher", 3));
        se_test::AddWaveToPopulation(*pPop, std::move(vWaveMembers), "lever_pulled");
        auto pComp = std::make_unique<FlatBuffers::ComponentT>();
        pComp->component.Set(std::move(*pPop));
        oCamp.components.push_back(std::move(pComp));

        ASSERT_TRUE(LoadCampWorld(se_test::BakeScene(std::move(oCamp))));
        Drain(4);

        auto pCampLevel = scene->GetRoot()->FindChild(StrID("world"))
                                     ->FindChild(StrID("spawner_camp.sesc#0"));
        auto pPopNode = pCampLevel->FindChild(StrID("Camp"));
        ASSERT_NE(pPopNode, nullptr);
        EXPECT_EQ(pPopNode->FindChild(StrID("ambusher")), nullptr);

        // pull the lever
        ETriggerFired ev;
        ev.event_id = StrID("lever_pulled");
        GetSystem<EventManager>().QueueEvent(ev);
        StepFrame(1.f / 60.f);      // Process() dispatches the queued event
        Drain(4);

        EXPECT_NE(pPopNode->FindChild(StrID("ambusher")), nullptr);
        EXPECT_NE(pPopNode->FindChild(StrID("ambusher_1")), nullptr);
        EXPECT_NE(pPopNode->FindChild(StrID("ambusher_2")), nullptr);
}

// ---------------------------------------------------------------------------
// 5. Respawn: destroyed spawns are re-spawned after the population's delay.
// ---------------------------------------------------------------------------

TEST_F(SpawnerTest, RespawnAfterDestroyedSpawn) {
        std::vector<std::unique_ptr<FlatBuffers::SpawnMemberT>> vMembers;
        vMembers.push_back(se_test::MakeSpawnMember("npc_wolf", 1));
        ASSERT_TRUE(LoadCampWorld(MakeCampScene(std::move(vMembers), /*respawn*/0.5f)));
        Drain(4);

        auto pCampLevel = scene->GetRoot()->FindChild(StrID("world"))
                                     ->FindChild(StrID("spawner_camp.sesc#0"));
        auto pPopNode = pCampLevel->FindChild(StrID("Camp"));
        auto pWolf = pPopNode->FindChild(StrID("npc_wolf"));
        ASSERT_NE(pWolf, nullptr);

        // kill it (deferred destroy, flushed at EPostUpdate inside StepFrame)
        GetSystem<EntityManager>().Destroy(pWolf->weak_from_this());
        StepFrame(1.f / 60.f);
        EXPECT_EQ(pPopNode->FindChild(StrID("npc_wolf")), nullptr);

        // respawn_delay = 0.5 s: back after the delay
        Drain(40);
        EXPECT_NE(pPopNode->FindChild(StrID("npc_wolf")), nullptr);
}

// ---------------------------------------------------------------------------
// 6. Determinism: identical worlds (same seed) produce identical populations.
// ---------------------------------------------------------------------------

TEST_F(SpawnerTest, DeterministicPopulationSameSeed) {
        auto make_members = []() {
                std::vector<std::unique_ptr<FlatBuffers::SpawnMemberT>> vMembers;
                vMembers.push_back(se_test::MakeSpawnMember("npc_wolf", 3, /*geometry*/"vol_a"));
                return vMembers;
        };

        auto capture = [&]() {
                if (!LoadCampWorld(MakeCampScene(make_members()))) {
                        ADD_FAILURE() << "LoadCampWorld failed";
                        return std::vector<glm::vec3>{};
                }
                Drain(4);
                std::vector<glm::vec3> vPositions;
                scene->GetRoot()->DepthFirstWalk([&vPositions](auto & oNode) {
                        if (oNode.GetName().rfind("npc_wolf", 0) == 0) {
                                vPositions.push_back(oNode.GetTransform().GetWorldPos());
                        }
                        return true;
                });
                return vPositions;
        };

        const auto vFirst = capture();
        ASSERT_EQ(vFirst.size(), 3u);
        Worlds().UnloadWorld();
        Drain();

        const auto vSecond = capture();
        ASSERT_EQ(vSecond.size(), vFirst.size());
        for (size_t i = 0; i < vFirst.size(); ++i) {
                EXPECT_EQ(vFirst[i], vSecond[i]) << "spawn " << i << " diverged";
        }
}

// ---------------------------------------------------------------------------
// 7. Shared per-frame budget: leftover commands wait, round-robin doesn't starve.
// ---------------------------------------------------------------------------

TEST_F(SpawnerTest, BudgetSpreadsSpawnsAcrossFrames) {
        std::vector<std::unique_ptr<FlatBuffers::SpawnMemberT>> vMembers;
        vMembers.push_back(se_test::MakeSpawnMember("npc_wolf", 4));
        ASSERT_TRUE(LoadCampWorld(MakeCampScene(std::move(vMembers))));

        auto wolves = [&]() {
                uint32_t n = 0;
                scene->GetRoot()->DepthFirstWalk([&n](auto & oNode) {
                        if (oNode.GetName().rfind("npc_wolf", 0) == 0) { ++n; }
                        return true;
                });
                return n;
        };

        Spawner().SetConfig({ 1 });          // one spawn per frame

        // ~2 ticks fire in 0.1 s (tick interval 0.05) — at most 2 executed, never 4
        RunSeconds(0.1f);
        EXPECT_LE(wolves(), 2u);
        EXPECT_GE(wolves(), 1u);

        // budget released: the remaining quota completes
        Spawner().SetConfig({ 8 });
        RunSeconds(0.3f);
        EXPECT_EQ(wolves(), 4u);
}

// ---------------------------------------------------------------------------
// 8. Despawn density control (default off; on with despawn_radius > 0).
// ---------------------------------------------------------------------------

TEST_F(SpawnerTest, DespawnPolicyKillsFarSpawnsAndRecovers) {
        auto oCamp = se_test::MakeNode("Camp");
        auto pPop = std::make_unique<FlatBuffers::PopulationT>();
        pPop->id = "wolves";
        pPop->tick_interval_s = 0.05f;
        pPop->respawn_delay_s = 0.2f;
        pPop->despawn_radius  = 10.f;
        pPop->members.push_back(se_test::MakeSpawnMember("npc_wolf", 1));
        se_test::SetPopulationFocusNear(*pPop, 15.f);
        auto pComp = std::make_unique<FlatBuffers::ComponentT>();
        pComp->component.Set(std::move(*pPop));
        oCamp.components.push_back(std::move(pComp));

        ASSERT_TRUE(LoadCampWorld(se_test::BakeScene(std::move(oCamp))));

        auto pFocusNode = scene->Create("player_focus");
        pFocusNode->SetPos(glm::vec3(2.f, 0.f, 2.f));
        Worlds().RegisterFocus(pFocusNode, 0.f, 10.f);

        Drain(4);
        auto pCampLevel = scene->GetRoot()->FindChild(StrID("world"))
                                     ->FindChild(StrID("spawner_camp.sesc#0"));
        auto pPopNode = pCampLevel->FindChild(StrID("Camp"));
        ASSERT_NE(pPopNode->FindChild(StrID("npc_wolf")), nullptr);

        // focus leaves: despawn kills the wolf
        pFocusNode->SetPos(glm::vec3(500.f, 0.f, 500.f));
        Drain(8);
        EXPECT_EQ(pPopNode->FindChild(StrID("npc_wolf")), nullptr);

        // focus returns: respawn recovers the density
        pFocusNode->SetPos(glm::vec3(1.f, 0.f, 1.f));
        Drain(30);
        EXPECT_NE(pPopNode->FindChild(StrID("npc_wolf")), nullptr);
}

// ---------------------------------------------------------------------------
// 9. World spawn population slots: validated + claimed (Step 11 spawns them).
// ---------------------------------------------------------------------------

TEST_F(SpawnerTest, WorldPopulationSlotClaim) {
        std::vector<std::unique_ptr<FlatBuffers::SpawnMemberT>> vMembers;
        vMembers.push_back(se_test::MakeSpawnMember("npc_wolf", 1));
        ASSERT_TRUE(LoadCampWorld(MakeCampScene(std::move(vMembers)),
                                  /*world populations*/ { "wolves", "ghost_pack" }));
        Drain(4);

        EXPECT_EQ(Spawner().GetStats().world_slots_total, 2u);
        EXPECT_EQ(Spawner().GetStats().world_slots_resolved, 1u);   // "wolves" claimed; ghost_pack orphaned (logged)
}

// ---------------------------------------------------------------------------
// 10. End-to-end .setm: template → instantiator → population quota, with
//     name uniquification across instances (the Part-A guarantee).
// ---------------------------------------------------------------------------

TEST_F(SpawnerTest, TemplateSpawnEndToEnd) {
        // a real .setm template with a child node
        flatbuffers::FlatBufferBuilder fbb;
        FlatBuffers::EntityTemplateT oTmpl;
        oTmpl.id   = "wolf_tmpl";
        oTmpl.root = std::make_unique<FlatBuffers::NodeTemplateT>();
        oTmpl.root->name = "WolfNPC";
        oTmpl.root->enabled = true;
        auto pChild = std::make_unique<FlatBuffers::NodeTemplateT>();
        pChild->name = "Bone";
        oTmpl.root->children.push_back(std::move(pChild));
        auto offset = FlatBuffers::EntityTemplate::Pack(fbb, &oTmpl);
        FlatBuffers::FinishEntityTemplateBuffer(fbb, offset);

        auto & oETS = GetSystem<TEntityTemplateSystem>();
        ASSERT_EQ(oETS.LoadFromMemory(fbb.GetBufferPointer(), fbb.GetSize()), uSUCCESS);
        ASSERT_TRUE(oETS.HasTemplate("wolf_tmpl"));

        // re-route the instantiator: template flavor for this test
        auto * pETS = &oETS;
        GetSystem<EntityManager>().SetTemplateInstantiator(
                [pETS](const SpawnIntent & oIntent) -> TSceneTree::TSceneNodeWeak {
                        return pETS->Instantiate(oIntent.prefab_id, oIntent.pParent,
                                        oIntent.vTranslation, oIntent.vRotation,
                                        oIntent.vScale, oIntent.enabled);
                });

        std::vector<std::unique_ptr<FlatBuffers::SpawnMemberT>> vMembers;
        vMembers.push_back(se_test::MakeSpawnMember("wolf_tmpl", 2));
        ASSERT_TRUE(LoadCampWorld(MakeCampScene(std::move(vMembers))));
        Drain(4);

        auto pCampLevel = scene->GetRoot()->FindChild(StrID("world"))
                                     ->FindChild(StrID("spawner_camp.sesc#0"));
        auto pPopNode = pCampLevel->FindChild(StrID("Camp"));
        ASSERT_NE(pPopNode, nullptr);

        auto pWolf1 = pPopNode->FindChild(StrID("WolfNPC"));
        auto pWolf2 = pPopNode->FindChild(StrID("WolfNPC_1"));
        EXPECT_NE(pWolf1, nullptr);
        EXPECT_NE(pWolf2, nullptr);
        EXPECT_NE(pWolf1->FindChild(StrID("Bone")), nullptr);      // hierarchy intact
        EXPECT_NE(pWolf2->FindChild(StrID("Bone")), nullptr);
}

// ---------------------------------------------------------------------------
// 11. SpawnPoint standalone component: single placement with a lifetime cap.
// ---------------------------------------------------------------------------

TEST_F(SpawnerTest, SpawnPointStandaloneSpawns) {
        auto oCamp = se_test::MakeNode("Camp");
        auto pLever = std::make_unique<FlatBuffers::NodeT>(
                se_test::MakeNode("lever", glm::vec3(1.f, 0.f, 1.f)));
        se_test::AddSpawnPointComponent(*pLever, "npc_wolf", /*delay*/0.f, /*max*/1);
        oCamp.children.push_back(std::move(pLever));

        ASSERT_TRUE(LoadCampWorld(se_test::BakeScene(std::move(oCamp))));
        Drain(4);

        auto pCampLevel = scene->GetRoot()->FindChild(StrID("world"))
                                     ->FindChild(StrID("spawner_camp.sesc#0"));
        auto pCampNode = pCampLevel->FindChild(StrID("Camp"));
        ASSERT_NE(pCampNode, nullptr);
        auto pLeverNode = pCampNode->FindChild(StrID("lever"));
        ASSERT_NE(pLeverNode, nullptr);
        EXPECT_NE(pLeverNode->FindChild(StrID("npc_wolf")), nullptr);

        // cap respected: destroying it does NOT respawn (respawn_delay < 0)
        auto pWolf = pLeverNode->FindChild(StrID("npc_wolf"));
        GetSystem<EntityManager>().Destroy(pWolf->weak_from_this());
        Drain(20);
        EXPECT_EQ(pLeverNode->FindChild(StrID("npc_wolf")), nullptr);
}

// ---------------------------------------------------------------------------
// 12. Disabled nodes never arm their populations (spawn-into-inactive guard).
// ---------------------------------------------------------------------------

TEST_F(SpawnerTest, DisabledPopulationNodeSpawnsNothing) {
        auto oCamp = se_test::MakeNode("Camp");
        auto pDormant = std::make_unique<FlatBuffers::NodeT>(
                se_test::MakeNode("DormantCamp", glm::vec3(1.f, 0.f, 1.f), {}, {1.f, 1.f, 1.f}));
        pDormant->enabled = false;
        std::vector<std::unique_ptr<FlatBuffers::SpawnMemberT>> vMembers;
        vMembers.push_back(se_test::MakeSpawnMember("npc_wolf", 2));
        auto pPop = std::make_unique<FlatBuffers::PopulationT>();
        pPop->id = "wolves";
        pPop->tick_interval_s = 0.05f;
        pPop->members = std::move(vMembers);
        auto pComp = std::make_unique<FlatBuffers::ComponentT>();
        pComp->component.Set(std::move(*pPop));
        pDormant->components.push_back(std::move(pComp));
        oCamp.children.push_back(std::move(pDormant));

        ASSERT_TRUE(LoadCampWorld(se_test::BakeScene(std::move(oCamp))));
        Drain(6);

        auto pCampLevel = scene->GetRoot()->FindChild(StrID("world"))
                                     ->FindChild(StrID("spawner_camp.sesc#0"));
        auto pCampNode = pCampLevel->FindChild(StrID("Camp"));
        ASSERT_NE(pCampNode, nullptr);
        auto pDormantNode = pCampNode->FindChild(StrID("DormantCamp"));
        ASSERT_NE(pDormantNode, nullptr);
        EXPECT_EQ(pDormantNode->FindChild(StrID("npc_wolf")), nullptr);
}
