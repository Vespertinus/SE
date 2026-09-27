
#ifndef SE_TEST_WORLD_DEMO_HARNESS_H
#define SE_TEST_WORLD_DEMO_HARNESS_H

// ---------------------------------------------------------------------------
// WorldDemoHarness — shared fixture for the world_demo integration suites
// (Stage 3 Step 11). Full Stage 3 headless composition (worlddemo_app shadow
// App.h) over the committed demo assets under resource/:
//   resource/world/world_demo.seworld (+ sub-levels, terrain, nav, AI assets)
// Covers the streaming lifecycle, deterministic spawns, nav routing, and the
// real .sebt brains end to end.
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>

#include "EngineFixture.h"
#include "PhysicsHarness.h"
#include "WorldDemoWorldBuilder.h"

#include <EntityManager.h>
#include <hsm/StateMachineSystem.h>
#include <world/WorldSystem.h>
#include <world/WorldAsset.h>
#include <terrain/TerrainSystem.h>
#include <ai/BlackboardSystem.h>
#include <world/SpatialIndexSystem.h>
#include <nav/NavigationSystem.h>
#include <nav/WorldNavBridge.h>
#include <nav/CrowdSystem.h>
#include <nav/NavAgent.h>
#include <nav/PathFollower.h>
#include <CharacterMovementSystem.h>
#include <EntityTemplateSystem.h>
#include <spawner/SpawnerSystem.h>
#include <ai/Blackboard.h>
#include <ai/BehaviorTree.h>
#include <ai/BehaviorTreeSystem.h>

#include <flatbuffers/flatbuffers.h>
#include <EntityTemplate_generated.h>

#include <gtest/gtest.h>

#include <memory>
#include <string>

using namespace SE;

namespace se_test {

class WorldDemoTestBase : public EnginePhysTestBase {

protected:
        void SetUp() override {
                EnginePhysTestBase::SetUp();

                static bool systems_constructed = []() {
                        auto & oEngine = TEngine::Instance();
                        oEngine.Init<EntityManager>();
                        oEngine.Init<StateMachineSystem>();
                        oEngine.Init<WorldSystem>();
                        oEngine.Init<TerrainSystem>();
                        oEngine.Init<BlackboardSystem>();
                        oEngine.Init<SpatialIndexSystem>();
                        oEngine.Init<NavigationSystem>();
                        oEngine.Init<CharacterMovementSystem>();
                        oEngine.Init<WorldNavBridge>();
                        oEngine.Init<CrowdSystem>();
                        oEngine.Init<TEntityTemplateSystem>();
                        oEngine.Init<SpawnerSystem>();
                        oEngine.Init<BTSystem>();
                        return true;
                }();
                (void)systems_constructed;

                scene = std::make_unique<TSceneTree>("world_demo_test_scene", 0, true);

                auto & oEM      = GetSystem<EntityManager>();
                auto & oWorlds  = GetSystem<WorldSystem>();
                auto & oETS     = GetSystem<TEntityTemplateSystem>();
                auto & oSpawner = GetSystem<SpawnerSystem>();

                oEM.SetSceneTree(scene.get());
                oWorlds.SetSceneTree(scene.get());
                oETS.SetSceneTree(scene.get());
                oSpawner.SetIndex(&GetSystem<SpatialIndexSystem>());

                // stub templates for the mirror world's populations (headless
                // stand-ins for the outfit .setm files, which need GL)
                for (const char * szId : {"villager_a", "villager_b", "farmer", "guard"}) {
                        MakeStubTemplate(szId);
                }
                UseTemplateInstantiator();
        }

        void TearDown() override {
                auto & oWorlds = GetSystem<WorldSystem>();
                oWorlds.UnloadWorld();
                Drain(2);

                GetSystem<SpawnerSystem>().SetIndex(nullptr);
                GetSystem<TEntityTemplateSystem>().SetSceneTree(nullptr);
                GetSystem<TEntityTemplateSystem>().UnloadAll();
                GetSystem<WorldSystem>().SetSceneTree(nullptr);
                GetSystem<EntityManager>().SetSceneTree(nullptr);

                GetSystem<TResourceManager>().Clear<StateMachineAsset>();
                GetSystem<TResourceManager>().Clear<BlackboardAsset>();
                GetSystem<TResourceManager>().Clear<BTAsset>();
                GetSystem<TResourceManager>().Clear<NavMeshAsset>();
                GetSystem<TResourceManager>().Clear<TerrainAsset>();
                GetSystem<TResourceManager>().Clear<WorldAsset>();

                EnginePhysTestBase::TearDown();
        }

void Drain(const uint32_t n = 2) {
                for (uint32_t i = 0; i < n; ++i) { StepFrame(1.f / 60.f); }
        }

        bool LoadDemoWorld() {
                static bool bBuilt = []() {
                        return BuildDemoMirrorWorld();
                }();
                if (!bBuilt) { return false; }
                return GetSystem<WorldSystem>().LoadWorld(
                                "resource/world/gen/worlddemo/world_demo_test.seworld");
        }

        // Register a minimal in-memory template (no visuals) so population
        // members instantiate headless — the real .setm outfits carry meshes.
        bool MakeStubTemplate(const char * szId) {
                flatbuffers::FlatBufferBuilder fbb;
                FlatBuffers::EntityTemplateT oTmpl;
                oTmpl.id   = szId;
                oTmpl.root = std::make_unique<FlatBuffers::NodeTemplateT>();
                oTmpl.root->name    = szId;
                oTmpl.root->enabled = true;
                auto offset = FlatBuffers::EntityTemplate::Pack(fbb, &oTmpl);
                FlatBuffers::FinishEntityTemplateBuffer(fbb, offset);
                auto & oETS = GetSystem<TEntityTemplateSystem>();
                return oETS.LoadFromMemory(fbb.GetBufferPointer(), fbb.GetSize()) == uSUCCESS;
        }

        // Route population spawns through the template system (the demo's
        // instantiator adds app-level components; stub templates need none).
        void UseTemplateInstantiator() {
                auto * pETS = &GetSystem<TEntityTemplateSystem>();
                GetSystem<EntityManager>().SetTemplateInstantiator(
                                [pETS](const SpawnIntent & oIntent) -> TSceneTree::TSceneNodeWeak {
                                        return pETS->Instantiate(oIntent.prefab_id, oIntent.pParent,
                                                        oIntent.vTranslation, oIntent.vRotation,
                                                        oIntent.vScale, oIntent.enabled);
                                });
        }

public:
        TSceneTree::TSceneNode FindNode(const char * szFullName) {
                return scene->FindFullName(StrID(szFullName));
        }

        TSceneTree::TSceneNode CreateNode(const char * szName) {
                return scene->Create(szName);
        }

        uint32_t ActiveLevelCount() {
                auto & oWorlds = GetSystem<WorldSystem>();
                uint32_t n = 0;
                for (uint32_t i = 0; i < oWorlds.GetSubLevelCount(); ++i) {
                        if (oWorlds.GetLevelState(i) == WorldSystem::ELevelState::Active) { ++n; }
                }
                return n;
        }
};

} // namespace se_test

using se_test::WorldDemoTestBase;
using SE::BTComponent;
using SE::BTSystem;
using SE::BTAsset;
using SE::TMyBTTasks;
using BehaviorTree = SE::BTComponent;
using BehaviorTreeSystem = SE::BTSystem;

#endif // SE_TEST_WORLD_DEMO_HARNESS_H
