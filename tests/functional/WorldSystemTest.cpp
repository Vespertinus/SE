
// ---------------------------------------------------------------------------
// WorldSystemTest — Tier C: the world/sub-level lifecycle over a real
// TSceneTree + engine systems (EntityManager, StateMachineSystem, WorldSystem,
// PhysicsSystem), driven through the committed fixtures under resource/world/.
// Covers: startup policies, activation/deactivation guards, event ordering,
// deferred teardown, name-map hygiene (recursive HandleNodeUnlink regression),
// dependency pinning, spawn registries, focus registry, physics activation.
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>
#include "EngineFixture.h"
#include "PhysicsHarness.h"
#include "WorldAssets.h"

#include <world/WorldSystem.h>
#include <world/WorldAsset.h>
#include <EntityManager.h>
#include <hsm/StateMachine.h>
#include <RigidBody.h>

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace SE;

namespace {

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class WorldSystemTest : public se_test::EnginePhysTestBase {

protected:
        void SetUp() override {
                EnginePhysTestBase::SetUp();

                static bool systems_constructed = []() {
                        auto & oEngine = TEngine::Instance();
                        oEngine.Init<EntityManager>();
                        oEngine.Init<StateMachineSystem>();
                        oEngine.Init<WorldSystem>();
                        return true;
                }();
                (void)systems_constructed;

                GetSystem<EntityManager>().SetSceneTree(scene.get());
                GetSystem<WorldSystem>().SetSceneTree(scene.get());
        }

        void TearDown() override {
                auto & oWorldSystem = GetSystem<WorldSystem>();

                oWorldSystem.UnloadWorld();
                Drain();

                oWorldSystem.SetSceneTree(nullptr);
                GetSystem<EntityManager>().SetSceneTree(nullptr);

                GetSystem<TResourceManager>().Clear<StateMachineAsset>();

                EnginePhysTestBase::TearDown();
        }

        void Drain(const uint32_t n = 2) {
                for (uint32_t i = 0; i < n; ++i) { StepFrame(1.f / 60.f); }
        }

        size_t CountNodes() {
                size_t n = 0;
                scene->GetRoot()->DepthFirstWalk([&n](auto &) { ++n; return true; });
                return n;
        }

        WorldSystem & Worlds() { return GetSystem<WorldSystem>(); }
};

// ---------------------------------------------------------------------------
// Ordered capture of the four lifecycle events
// ---------------------------------------------------------------------------

// Director quiescence for lifecycle-mechanics tests: a focus far from every
// level bound means the streaming director wants nothing eligible. Without it
// the director legitimately re-requests in-range sub-levels right after a
// manual deactivation (they are wanted by initial_focus — Step 2 behavior).
WorldSystem::FocusHandle ParkFocusFarAway(TSceneTree & oScene, WorldSystem & oWorlds) {
        auto pFocus = oScene.Create("far_focus");
        pFocus->SetPos(glm::vec3(0.f, 0.f, 100000.f));
        return oWorlds.RegisterFocus(pFocus, 0.f, 0.f);
}

struct LifecycleOrderSink {

        std::vector<std::string> vOrder;

        LifecycleOrderSink() {
                auto & oEM = GetSystem<EventManager>();
                oEM.AddListener<ESubLevelActivating,   &LifecycleOrderSink::OnActivating>(this);
                oEM.AddListener<ESubLevelActivated,    &LifecycleOrderSink::OnActivated>(this);
                oEM.AddListener<ESubLevelDeactivating, &LifecycleOrderSink::OnDeactivating>(this);
                oEM.AddListener<ESubLevelDeactivated,  &LifecycleOrderSink::OnDeactivated>(this);
        }

        ~LifecycleOrderSink() {
                auto & oEM = GetSystem<EventManager>();
                oEM.RemoveListener<ESubLevelActivating,   &LifecycleOrderSink::OnActivating>(this);
                oEM.RemoveListener<ESubLevelActivated,    &LifecycleOrderSink::OnActivated>(this);
                oEM.RemoveListener<ESubLevelDeactivating, &LifecycleOrderSink::OnDeactivating>(this);
                oEM.RemoveListener<ESubLevelDeactivated,  &LifecycleOrderSink::OnDeactivated>(this);
        }

        void OnActivating(const Event &)   { vOrder.push_back("activating"); }
        void OnActivated(const Event &)    { vOrder.push_back("activated"); }
        void OnDeactivating(const Event &) { vOrder.push_back("deactivating"); }
        void OnDeactivated(const Event &)  { vOrder.push_back("deactivated"); }
};

} // namespace

// ---------------------------------------------------------------------------
// Startup policies
// ---------------------------------------------------------------------------

TEST_F(WorldSystemTest, LoadWorld_StreamingEnabled_ActivatesAlwaysLoadedOnly) {

        ASSERT_TRUE(Worlds().LoadWorld("resource/world/test_world.seworld"));

        EXPECT_TRUE(Worlds().IsLevelActive(0));    // AlwaysLoaded
        EXPECT_FALSE(Worlds().IsLevelActive(1));   // default flag, waits for distance (Step 2)
        EXPECT_FALSE(Worlds().IsLevelActive(2));
        EXPECT_FALSE(Worlds().IsLevelActive(3));   // Manual never auto-activates

        // roots exist under the scene root
        EXPECT_NE(scene->FindFullName(StrID("root|world")), nullptr);
        EXPECT_NE(scene->FindFullName(StrID("root|dynamics")), nullptr);

        // the active level's wrapper is named basename#index
        EXPECT_NE(scene->FindFullName(StrID("root|world|sub_room.sesc#0")), nullptr);
        EXPECT_EQ(Worlds().GetLevelRoot(0).lock()->GetName(), "sub_room.sesc#0");
}

TEST_F(WorldSystemTest, LoadWorld_NoStreaming_ActivatesAllNonManual) {

        ASSERT_TRUE(Worlds().LoadWorld("resource/world/test_world_no_streaming.seworld"));

        EXPECT_TRUE(Worlds().IsLevelActive(0));    // non-Manual: platformer path, same code
        EXPECT_FALSE(Worlds().IsLevelActive(1));   // Manual stays unloaded
}

// ---------------------------------------------------------------------------
// Activation / deactivation lifecycle
// ---------------------------------------------------------------------------

TEST_F(WorldSystemTest, ActivateDeactivate_EventOrderAndStates) {

        ASSERT_TRUE(Worlds().LoadWorld("resource/world/test_world.seworld"));
        ParkFocusFarAway(*scene, Worlds());

        LifecycleOrderSink oSink;

        const size_t base_nodes = CountNodes();

        EXPECT_TRUE(Worlds().ActivateLevel(1));
        EXPECT_EQ(Worlds().GetLevelState(1), WorldSystem::ELevelState::Active);

        // activation is atomic to gameplay: Activated fires on a live subtree
        EXPECT_EQ(oSink.vOrder, std::vector<std::string>({ "activating", "activated" }));
        EXPECT_NE(scene->FindFullName(StrID("root|world|sub_forest.sesc#1|Forest")), nullptr);

        EXPECT_TRUE(Worlds().DeactivateLevel(1));

        // Deactivating fired while nodes are still alive
        EXPECT_EQ(oSink.vOrder, std::vector<std::string>({ "activating", "activated", "deactivating" }));
        EXPECT_EQ(Worlds().GetLevelState(1), WorldSystem::ELevelState::Unloading);

        Drain();   // EPostUpdate flush: destroys fire EEntityDestroyed -> Deactivated

        EXPECT_EQ(Worlds().GetLevelState(1), WorldSystem::ELevelState::Unloaded);
        EXPECT_EQ(oSink.vOrder, std::vector<std::string>(
                        { "activating", "activated", "deactivating", "deactivated" }));

        // wrapper is gone and node count returned to baseline (teardown completeness)
        EXPECT_TRUE(Worlds().GetLevelRoot(1).expired());
        EXPECT_EQ(CountNodes(), base_nodes);
}

TEST_F(WorldSystemTest, Guards_DoubleActivate_And_InactiveDeactivate) {

        ASSERT_TRUE(Worlds().LoadWorld("resource/world/test_world.seworld"));

        LifecycleOrderSink oSink;

        EXPECT_TRUE(Worlds().IsLevelActive(0));
        EXPECT_FALSE(Worlds().ActivateLevel(0));       // double-activate refused
        EXPECT_FALSE(Worlds().DeactivateLevel(1));     // not active
        EXPECT_FALSE(Worlds().ActivateLevel(99));      // no such slot

        // AlwaysLoaded refuses explicit deactivation during play
        EXPECT_FALSE(Worlds().DeactivateLevel(0));

        EXPECT_TRUE(oSink.vOrder.empty());
}

TEST_F(WorldSystemTest, Reactivate_RestoresNodes_AndNameMap) {

        ASSERT_TRUE(Worlds().LoadWorld("resource/world/test_world.seworld"));
        ParkFocusFarAway(*scene, Worlds());

        const size_t base_nodes = CountNodes();

        EXPECT_TRUE(Worlds().ActivateLevel(1));
        EXPECT_NE(scene->FindFullName(StrID("root|world|sub_forest.sesc#1|Forest|Camp")), nullptr);

        EXPECT_TRUE(Worlds().DeactivateLevel(1));
        Drain();

        // recursive HandleNodeUnlink regression: no stale name-map entries survive
        EXPECT_EQ(scene->FindFullName(StrID("root|world|sub_forest.sesc#1|Forest|Camp")), nullptr);
        EXPECT_EQ(CountNodes(), base_nodes);

        // re-activation recreates the same names cleanly
        EXPECT_TRUE(Worlds().ActivateLevel(1));
        EXPECT_NE(scene->FindFullName(StrID("root|world|sub_forest.sesc#1|Forest|Camp")), nullptr);
        EXPECT_EQ(CountNodes(), base_nodes + 4);   // wrapper + Forest + Camp + Patrol

        EXPECT_TRUE(Worlds().DeactivateLevel(1));
        Drain();
        EXPECT_EQ(CountNodes(), base_nodes);
}

TEST_F(WorldSystemTest, SameScene_TwoInstances_WithTransforms) {

        ASSERT_TRUE(Worlds().LoadWorld("resource/world/test_world.seworld"));

        EXPECT_TRUE(Worlds().ActivateLevel(1));
        EXPECT_TRUE(Worlds().ActivateLevel(2));   // same .sesc, different placement

        auto pWrapper1 = Worlds().GetLevelRoot(1).lock();
        auto pWrapper2 = Worlds().GetLevelRoot(2).lock();
        ASSERT_NE(pWrapper1, nullptr);
        ASSERT_NE(pWrapper2, nullptr);

        EXPECT_NE(pWrapper1, pWrapper2);
        EXPECT_EQ(pWrapper1->GetName(), "sub_forest.sesc#1");
        EXPECT_EQ(pWrapper2->GetName(), "sub_forest.sesc#2");

        // full names stay unique even though the content names collide
        EXPECT_NE(scene->FindFullName(StrID("root|world|sub_forest.sesc#1|Forest|Camp")), nullptr);
        EXPECT_NE(scene->FindFullName(StrID("root|world|sub_forest.sesc#2|Forest|Camp")), nullptr);

        // the Level-Instance transform landed on the wrapper
        EXPECT_FLOAT_EQ(pWrapper2->GetTransform().GetPos().x, 40.f);
        EXPECT_FLOAT_EQ(pWrapper2->GetTransform().GetPos().z, 0.f);

        EXPECT_TRUE(Worlds().DeactivateLevel(1));
        EXPECT_TRUE(Worlds().DeactivateLevel(2));
        Drain();
}

// ---------------------------------------------------------------------------
// Ownership: spawn registry vs dynamics root
// ---------------------------------------------------------------------------

TEST_F(WorldSystemTest, SpawnRegistry_TeardownKillsSpawns_DynamicsRootSurvives) {

        ASSERT_TRUE(Worlds().LoadWorld("resource/world/test_world.seworld"));
        EXPECT_TRUE(Worlds().ActivateLevel(1));

        SpawnRequest oLevelSpawn;
        oLevelSpawn.pParent = Worlds().GetLevelRoot(1);
        oLevelSpawn.name    = "level_npc";
        auto pLevelNpc = GetSystem<EntityManager>().Spawn(oLevelSpawn);
        Worlds().RegisterSpawn(1, pLevelNpc);

        SpawnRequest oDynSpawn;
        oDynSpawn.pParent = Worlds().GetDynamicsRoot();
        oDynSpawn.name    = "persistent";
        auto pPersistent = GetSystem<EntityManager>().Spawn(oDynSpawn);

        EXPECT_FALSE(pLevelNpc.expired());
        EXPECT_FALSE(pPersistent.expired());

        EXPECT_TRUE(Worlds().DeactivateLevel(1));
        Drain();

        EXPECT_TRUE(pLevelNpc.expired());        // registry-owned: destroyed with the level
        EXPECT_FALSE(pPersistent.expired());     // dynamics root: never owned by a level

        // the world is still fully usable afterwards
        EXPECT_TRUE(Worlds().ActivateLevel(2));
        EXPECT_TRUE(Worlds().DeactivateLevel(2));
        Drain();
}

// ---------------------------------------------------------------------------
// Dependency pinning
// ---------------------------------------------------------------------------

TEST_F(WorldSystemTest, Pinning_RetainLifecycle) {

        auto & oResources = GetSystem<TResourceManager>();

        ASSERT_TRUE(Worlds().LoadWorld("resource/world/test_world.seworld"));
        ParkFocusFarAway(*scene, Worlds());

        // LoadWorld already activated AlwaysLoaded sub_room (npc_ai + player_behavior);
        // sub_forest shares npc_ai with sub_room, so activating it adds exactly one
        // new StateMachineAsset (character_movement).
        const size_t base = oResources.Size<StateMachineAsset>();

        EXPECT_TRUE(Worlds().ActivateLevel(1));
        EXPECT_EQ(oResources.Size<StateMachineAsset>(), base + 1);

        // a debug sweep must not free anything an active level holds
        DestroyUnretainedResources();
        EXPECT_EQ(oResources.Size<StateMachineAsset>(), base + 1);

        EXPECT_TRUE(Worlds().DeactivateLevel(1));
        Drain();

        // pins dropped: the sweep queues them, ProcessDeferred actually frees
        DestroyUnretainedResources();
        oResources.ProcessDeferred();
        EXPECT_EQ(oResources.Size<StateMachineAsset>(), base);
}

TEST_F(WorldSystemTest, Pinning_TwoLevelsShareOneScene) {

        auto & oResources = GetSystem<TResourceManager>();

        ASSERT_TRUE(Worlds().LoadWorld("resource/world/test_world.seworld"));
        ParkFocusFarAway(*scene, Worlds());

        // slots 1 and 2 instance the same .sesc; sub_room (AlwaysLoaded, active)
        // already shares npc_ai — so exactly one new asset (character_movement)
        // is created once and pinned by both slots.
        const size_t base = oResources.Size<StateMachineAsset>();

        EXPECT_TRUE(Worlds().ActivateLevel(1));
        EXPECT_TRUE(Worlds().ActivateLevel(2));
        EXPECT_EQ(oResources.Size<StateMachineAsset>(), base + 1);

        EXPECT_TRUE(Worlds().DeactivateLevel(1));
        Drain();

        // level 2 still holds the pins: nothing freed
        DestroyUnretainedResources();
        oResources.ProcessDeferred();
        EXPECT_EQ(oResources.Size<StateMachineAsset>(), base + 1);

        EXPECT_TRUE(Worlds().DeactivateLevel(2));
        Drain();

        DestroyUnretainedResources();
        oResources.ProcessDeferred();
        EXPECT_EQ(oResources.Size<StateMachineAsset>(), base);
}

// ---------------------------------------------------------------------------
// Focus registry
// ---------------------------------------------------------------------------

TEST_F(WorldSystemTest, FocusRegistry_MovedEvent) {

        ASSERT_TRUE(Worlds().LoadWorld("resource/world/test_world.seworld"));

        se_test::EventSink<EFocusMoved> oSink;

        auto pFocusNode = scene->Create("focus_cam");
        pFocusNode->SetPos(glm::vec3(0.f, 0.f, 0.f));

        const auto hFocus = Worlds().RegisterFocus(pFocusNode, 64.f, 1.f);
        EXPECT_NE(hFocus, 0u);
        EXPECT_EQ(Worlds().GetFocuses().size(), 1u);

        StepFrame(1.f / 60.f);         // focus pass runs on EUpdate
        EXPECT_EQ(oSink.Size(), 0u);   // no move yet

        pFocusNode->SetPos(glm::vec3(10.f, 0.f, 0.f));
        StepFrame(1.f / 60.f);
        ASSERT_EQ(oSink.Size(), 1u);
        EXPECT_FLOAT_EQ(oSink.hits[0].pos.x, 10.f);

        Worlds().UnregisterFocus(hFocus);
        pFocusNode->SetPos(glm::vec3(20.f, 0.f, 0.f));
        StepFrame(1.f / 60.f);
        EXPECT_EQ(oSink.Size(), 1u);   // unregistered: no more events
        EXPECT_TRUE(Worlds().GetFocuses().empty());
}

// ---------------------------------------------------------------------------
// World unload
// ---------------------------------------------------------------------------

TEST_F(WorldSystemTest, UnloadWorld_ResetsToBaseline) {

        const size_t empty_nodes = CountNodes();   // pristine scene: root only

        ASSERT_TRUE(Worlds().LoadWorld("resource/world/test_world.seworld"));
        EXPECT_TRUE(Worlds().ActivateLevel(1));
        EXPECT_TRUE(Worlds().ActivateLevel(2));

        se_test::EventSink<EWorldUnloaded> oSink;

        Worlds().UnloadWorld();   // tears down Active levels, AlwaysLoaded included
        Drain();

        for (uint32_t i = 0; i < Worlds().GetSubLevelCount(); ++i) {
                EXPECT_EQ(Worlds().GetLevelState(i), WorldSystem::ELevelState::Unloaded);
        }
        EXPECT_TRUE(Worlds().GetWorldRoot().expired());
        EXPECT_TRUE(Worlds().GetDynamicsRoot().expired());
        EXPECT_EQ(CountNodes(), empty_nodes);
        EXPECT_EQ(oSink.Size(), 1u);

        // the world can be loaded again after a full drain
        EXPECT_TRUE(Worlds().LoadWorld("resource/world/test_world.seworld"));
        EXPECT_TRUE(Worlds().IsLevelActive(0));
}

// ---------------------------------------------------------------------------
// Physics: activation brings Jolt bodies up, teardown retires them
// ---------------------------------------------------------------------------

TEST_F(WorldSystemTest, Physics_RigidBodyInSubLevel) {

        // in-test .sesc with a RigidBody (compose_scene cannot emit those)
        boost::filesystem::create_directories("resource/world/gen");

        auto oRoot = se_test::MakeNode("PhysLevel");
        auto oCrate = se_test::MakeNode("Crate", glm::vec3(0.f, 1.f, 0.f));
        se_test::AddBoxRigidBodyComponent(oCrate, glm::vec3(0.5f, 0.5f, 0.5f));
        oRoot.children.push_back(
                        std::make_unique<FlatBuffers::NodeT>(std::move(oCrate)));

        const std::string sScenePath = "resource/world/gen/phys_level.sesc";
        ASSERT_TRUE(se_test::WriteBytes(sScenePath, se_test::BakeScene(std::move(oRoot))));

        auto pWorldT = se_test::MakeWorld("phys_world");
        pWorldT->streaming = std::make_unique<FlatBuffers::StreamingConfigT>();
        pWorldT->streaming->enabled = false;
        se_test::AddSubLevel(*pWorldT, sScenePath,
                        FlatBuffers::Vec3(-8, 0, -8), FlatBuffers::Vec3(8, 4, 8));

        auto vBytes = se_test::BakeWorld(*pWorldT);
        auto hWorld = CreateResource<WorldAsset>(
                        "inline_phys_world", FlatBuffers::GetWorld(vBytes.data()));
        ASSERT_TRUE(Worlds().LoadWorld(hWorld));

        // streaming disabled: the level auto-activated, bodies are live
        EXPECT_TRUE(Worlds().IsLevelActive(0));

        auto pCrate = scene->FindFullName(StrID("root|world|phys_level.sesc#0|PhysLevel|Crate"));
        ASSERT_NE(pCrate, nullptr);
        EXPECT_TRUE(pCrate->HasComponent<RigidBody>());
        EXPECT_TRUE(pCrate->GetComponent<RigidBody>()->GetHandle().IsValid());

        EXPECT_TRUE(Worlds().DeactivateLevel(0));
        Drain();

        // teardown retired the whole subtree (body destroyed with the component)
        EXPECT_EQ(scene->FindFullName(StrID("root|world|phys_level.sesc#0|PhysLevel|Crate")), nullptr);
}
