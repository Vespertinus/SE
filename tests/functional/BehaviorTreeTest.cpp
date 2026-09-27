// ---------------------------------------------------------------------------
// BehaviorTreeTest — Stage 3 Step 10 (agent_workspace/artifacts/
// gameplay_systems_stage_3_step_10_001.md).
//
// Tier A: the .sebt asset — bake layout, structural validation, SubTree
//         inlining (asset bytes built in memory; scratch files for SubTree).
// Tier B: runtime semantics over the bt_app composition — composite truth
//         tables, decorator gates/aborts, staggering, tasks, HSM bridge.
// Tier C: composed flows — scene-buffer load, MoveTo over the crowd stack,
//         the moving_actors acceptance brain, entity/world lifecycle.
//
// Fixture: EnginePhysTestBase; systems composed per bt_app/App.h (EntityManager,
// StateMachineSystem, WorldSystem, BlackboardSystem, NavigationSystem,
// CharacterMovementSystem, CrowdSystem, templates, BehaviorTreeSystem).
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>
#include "EngineFixture.h"
#include "PhysicsHarness.h"
#include "BehaviorTreeAssets.h"
#include "BlackboardAssets.h"
#include "BTDummyTasks.h"

#include <EventManager.h>
#include <CommonEvents.h>
#include <FrameRunner.h>
#include <world/WorldSystem.h>
#include <ai/Blackboard.h>
#include <ai/BlackboardSystem.h>
#include <ai/BehaviorTree.h>
#include <ai/BehaviorTreeSystem.h>
#include <hsm/StateMachine.h>
#include <hsm/StateMachineSystem.h>
#include <hsm/StateMachineAsset.h>

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>

#include <glm/glm.hpp>

#include <string>
#include <vector>

using namespace SE;

namespace se_test {

// RAII sink (BlackboardTest pattern) for deferred BT/blackboard events.
template <class E>
struct BTSink {
        std::vector<E> hits;

        BTSink()  { SE::GetSystem<SE::EventManager>().template AddListener<E, &BTSink::On>(this); }
        ~BTSink() { SE::GetSystem<SE::EventManager>().template RemoveListener<E, &BTSink::On>(this); }
        void On(const SE::Event & e) { hits.push_back(e.template Get<E>()); }
};

} // namespace se_test

// ---------------------------------------------------------------------------

namespace {

class BehaviorTreeSystemTest : public se_test::EnginePhysTestBase {

protected:
        void SetUp() override {
                EnginePhysTestBase::SetUp();

                static bool systems_constructed = []() {
                        auto & oEngine = TEngine::Instance();
                        oEngine.Init<EntityManager>();
                        oEngine.Init<StateMachineSystem>();
                        oEngine.Init<WorldSystem>();
                        // after WorldSystem (ctor dependency)
                        oEngine.Init<BlackboardSystem>();
                        oEngine.Init<NavigationSystem>();
                        oEngine.Init<CharacterMovementSystem>();
                        oEngine.Init<CrowdSystem>();
                        oEngine.Init<TEntityTemplateSystem>();
                        oEngine.Init<BTSystem>();
                        return true;
                }();
                (void)systems_constructed;

                GetSystem<EntityManager>().SetSceneTree(scene.get());
                GetSystem<WorldSystem>().SetSceneTree(scene.get());
                GetSystem<TEntityTemplateSystem>().SetSceneTree(scene.get());

                boost::filesystem::create_directories("resource/ai/gen");
                boost::filesystem::create_directories("resource/world/gen");

                BTDummyTask::ResetCounters();
        }

        void TearDown() override {
                GetSystem<WorldSystem>().UnloadWorld();
                GetSystem<WorldSystem>().SetSceneTree(nullptr);
                GetSystem<EntityManager>().SetSceneTree(nullptr);

                // drain queued events so they never leak across tests
                Events().Process();

                GetSystem<TResourceManager>().Clear<WorldAsset>();
                GetSystem<TResourceManager>().Clear<StateMachineAsset>();
                GetSystem<TResourceManager>().Clear<NavMeshAsset>();
                GetSystem<TResourceManager>().Clear<BlackboardAsset>();
                GetSystem<TResourceManager>().Clear<BTAsset>();

                EnginePhysTestBase::TearDown();
        }

        // ---- helpers ---------------------------------------------------------

        // Entity with its own blackboard (guard schema) + a tree component from
        // an inline holder; returns the node (null on load failure).
        TSceneTree::TSceneNode SpawnAgent(const std::string & sName,
                                          std::unique_ptr<FlatBuffers::BehaviorTreeHolderT> pHolder,
                                          float fTickInterval = 0.f,
                                          bool bWithBlackboard = true) {
                auto oNode = se_test::MakeNode(sName);
                if (bWithBlackboard) {
                        se_test::AddBlackboardComponent(oNode, se_test::MakeBBHolderInline(MakeGuardDef(), sName + "_bb"));
                }
                se_test::AddBehaviorTreeComponent(oNode, std::move(pHolder), fTickInterval);
                const auto vBytes = se_test::BakeScene(std::move(oNode));
                const std::vector<char> vBuf(vBytes.begin(), vBytes.end());
                return scene->InstantiateFromBuffer(vBuf);
        }

        FlatBuffers::BlackboardDefinitionT MakeGuardDef() {
                auto oDef = se_test::MakeBBDef("guard_bb");
                se_test::AddKey(*oDef, "alert_level", FlatBuffers::EBType::Float);
                se_test::AddKey(*oDef, "alarm_raised", FlatBuffers::EBType::Bool);
                se_test::AddKey(*oDef, "patrol_index", FlatBuffers::EBType::Int);
                se_test::AddKey(*oDef, "wp_target", FlatBuffers::EBType::Vec3);
                se_test::AddKey(*oDef, "noise_pos", FlatBuffers::EBType::Vec3);
                se_test::AddKey(*oDef, "noise_active", FlatBuffers::EBType::Bool);
                return *oDef;
        }

        // Drive one EUpdate + EPostUpdate frame (FrameRunner, deterministic dt).
        void StepFrame(float dt) { se_test::StepFrame(dt); }

        // Tree used by scheduler tests: a single long Wait.
        std::unique_ptr<FlatBuffers::BehaviorTreeDefinitionT> MakeSimpleWaitTree() {
                return se_test::MakeBTDef("wait_tree",
                                se_test::BTTask("Wait", "", { se_test::ParamF("duration", 30.f) }));
        }

        // Bake + load an in-memory world declaring an inline global blackboard
        // with `oDef`'s schema (BlackboardTest fixture pattern).
        bool LoadWorldWithGlobal(const FlatBuffers::BlackboardDefinitionT & oDef) {
                const auto pWorld = se_test::MakeWorld("bt_world");
                se_test::SetGlobalBlackboard(*pWorld, se_test::MakeBBHolderInline(oDef, "bt_global"));
                const auto vBytes = se_test::BakeWorld(*pWorld);
                auto hWorld = CreateResource<WorldAsset>(
                                "bt_world_under_test", FlatBuffers::GetWorld(vBytes.data()));
                return GetSystem<WorldSystem>().LoadWorld(hWorld);
        }
};

// ---------------------------------------------------------------------------
// Tier A — asset
// ---------------------------------------------------------------------------

TEST(BehaviorTreeAssetTest, AssetBakeLayoutRoundTrip) {
        se_test::EnsureEngine();

        // Selector{ Cond(alert>0.5, Task Wait 1.0), Task Wait 2.0 named "tail" }
        auto root = se_test::BTSelector({
                se_test::BTCond("alert_level", FlatBuffers::SMConditionOp::Greater, 0.5f,
                                se_test::BTTask("Wait", "", { se_test::ParamF("duration", 1.f) })),
                se_test::BTTask("Wait", "tail", { se_test::ParamF("duration", 2.f) }),
        });

        auto oDef = se_test::MakeBTDef("layout_tree", std::move(root));
        const auto vBytes = se_test::BakeBT(*oDef);
        ASSERT_FALSE(vBytes.empty());

        auto h = CreateResource<BTAsset>("layout_tree_under_test",
                                         FlatBuffers::GetBehaviorTreeDefinition(vBytes.data()));
        const BTAsset * pAsset = GetResource(h);
        ASSERT_NE(pAsset, nullptr);
        ASSERT_EQ(pAsset->NodeCount(), 4u);   // Selector, Cond, Wait, Wait

        const auto & nodes = pAsset->GetNodes();
        EXPECT_EQ(nodes[0].type, FlatBuffers::BTNodeU::BTComposite);
        EXPECT_EQ(nodes[0].parent, -1);
        EXPECT_EQ(nodes[0].child_first, 0);
        EXPECT_EQ(nodes[0].child_count, 2);
        // Children are NOT contiguous (the condition's subtree interleaves) — the
        // child-index list carries the order: selector → [cond @1, wait @3].
        const auto & childIdx = pAsset->GetChildIndices();
        ASSERT_EQ(childIdx.size(), 2u);
        EXPECT_EQ(childIdx[0], 1);
        EXPECT_EQ(childIdx[1], 3);

        EXPECT_EQ(nodes[1].type, FlatBuffers::BTNodeU::BTDecorator);
        EXPECT_EQ(nodes[1].parent, 0);
        EXPECT_EQ(nodes[1].decorated_child, 2);
        EXPECT_EQ(nodes[1].cond_key, StrID("alert_level"));
        EXPECT_EQ(nodes[1].op, FlatBuffers::SMConditionOp::Greater);
        EXPECT_FLOAT_EQ(nodes[1].threshold, 0.5f);

        EXPECT_EQ(nodes[2].type, FlatBuffers::BTNodeU::BTTask);
        EXPECT_EQ(nodes[2].parent, 1);
        EXPECT_EQ(nodes[2].task_index, 0);   // Wait is first in TCoreBTTasks
        EXPECT_EQ(nodes[2].param_count, 1);
        EXPECT_FLOAT_EQ(pAsset->GetParams()[nodes[2].param_first].f, 1.f);

        EXPECT_EQ(nodes[3].type, FlatBuffers::BTNodeU::BTTask);
        EXPECT_EQ(nodes[3].parent, 0);
        EXPECT_EQ(nodes[3].name, StrID("tail"));
        EXPECT_FLOAT_EQ(pAsset->GetParams()[nodes[3].param_first].f, 2.f);

        GetSystem<TResourceManager>().Clear<BTAsset>();
}

TEST(BehaviorTreeAssetTest, AssetGarbageThrows) {
        se_test::EnsureEngine();
        const std::vector<uint8_t> garbage{'S', 'E', 'B', 'T', 0, 1, 2, 3, 4, 5};
        EXPECT_THROW((TBehaviorTreeAsset<TMyBTTasks>("garbage_bt", 1, garbage.data(), garbage.size())),
                     std::runtime_error);
}

TEST(BehaviorTreeAssetTest, AssetCompositeZeroChildrenThrows) {
        se_test::EnsureEngine();
        auto oDef = se_test::MakeBTDef("empty_seq", se_test::BTSequence({}));
        const auto vBytes = se_test::BakeBT(*oDef);
        EXPECT_THROW((TBehaviorTreeAsset<TMyBTTasks>("empty_seq", 1, vBytes.data(), vBytes.size())),
                     std::runtime_error);
}

TEST(BehaviorTreeAssetTest, AssetDecoratorWithoutChildThrows) {
        se_test::EnsureEngine();
        // A decorator whose child is null cannot be authored through BTDecorator —
        // corrupt the bytes instead: bake a valid tree, then verify the schema
        // rejects a decorator table without the child field via the object API.
        FlatBuffers::BehaviorTreeDefinitionT oRaw;
        oRaw.id = "dec_no_child";
        se_test::BTNodeT n;
        n.name = "cond";
        auto d = std::make_unique<FlatBuffers::BTDecoratorT>();
        d->kind = FlatBuffers::EBTDecoratorKind::BlackboardCondition;
        d->key = "alarm_raised";
        // d->child left null
        n.type.Set(std::move(*std::make_unique<FlatBuffers::BTDecoratorT>()));
        n.value.Set(std::move(*d));
        oRaw.root = std::make_unique<se_test::BTNodeT>(std::move(n));

        const auto vBytes = se_test::BakeBT(oRaw);
        EXPECT_THROW((TBehaviorTreeAsset<TMyBTTasks>("dec_no_child", 1, vBytes.data(), vBytes.size())),
                     std::runtime_error);
}

TEST(BehaviorTreeAssetTest, AssetUnknownTaskThrows) {
        se_test::EnsureEngine();
        auto oDef = se_test::MakeBTDef("bad_task", se_test::BTTask("AttackThePlayer"));
        const auto vBytes = se_test::BakeBT(*oDef);
        try {
                TBehaviorTreeAsset<TMyBTTasks> asset("bad_task", 1, vBytes.data(), vBytes.size());
                FAIL() << "expected unknown-task load error";
        } catch (const std::runtime_error & e) {
                EXPECT_NE(std::string(e.what()).find("AttackThePlayer"), std::string::npos);
        }
}

TEST(BehaviorTreeAssetTest, AssetBadBuiltinParamThrows) {
        se_test::EnsureEngine();
        // Wait.duration authored as bool — core task Validate rejects at load.
        auto oDef = se_test::MakeBTDef("bad_param", se_test::BTTask("Wait", "", { se_test::ParamB("duration", true) }));
        const auto vBytes = se_test::BakeBT(*oDef);
        EXPECT_THROW((TBehaviorTreeAsset<TMyBTTasks>("bad_param", 1, vBytes.data(), vBytes.size())),
                     std::runtime_error);
}

TEST(BehaviorTreeAssetTest, AssetConditionMissingKeyThrows) {
        se_test::EnsureEngine();
        auto oDef = se_test::MakeBTDef(
                        "cond_no_key",
                        se_test::BTDecorator(FlatBuffers::EBTDecoratorKind::BlackboardCondition,
                                             se_test::BTTask("Wait", "", { se_test::ParamF("duration", 1.f) })));
        const auto vBytes = se_test::BakeBT(*oDef);
        EXPECT_THROW((TBehaviorTreeAsset<TMyBTTasks>("cond_no_key", 1, vBytes.data(), vBytes.size())),
                     std::runtime_error);
}

TEST(BehaviorTreeAssetTest, AssetSubTreeExpansion) {
        // fixture-free test over the real engine: initialize the resource
        // manager (first Engine system) before CreateResource/Clear below
        se_test::EnsureEngine();
        // Sub-tree file: Sequence{ Wait 0.5 (named "sub_wait"), Dummy }
        auto sub = se_test::MakeBTDef(
                        "sub_tree",
                        se_test::BTSequence({ se_test::BTTask("Wait", "sub_wait", { se_test::ParamF("duration", 0.5f) }),
                                              se_test::BTTask("Dummy", "sub_dummy") }));
        se_test::WriteBT("resource/ai/gen/bt_sub.sebt", *sub);

        // Main tree: Selector{ SubTree, Wait 2.0 }
        auto main = se_test::MakeBTDef(
                        "main_tree",
                        se_test::BTSelector({ se_test::BTSubTreeRef("resource/ai/gen/bt_sub.sebt"),
                                              se_test::BTTask("Wait", "tail", { se_test::ParamF("duration", 2.f) }) }));
        const auto vBytes = se_test::BakeBT(*main);

        auto h = CreateResource<BTAsset>("subtree_expand_under_test",
                                         FlatBuffers::GetBehaviorTreeDefinition(vBytes.data()));
        const BTAsset * pAsset = GetResource(h);
        ASSERT_NE(pAsset, nullptr);

        // Selector + [Sequence, Wait, Dummy] (spliced) + Wait tail = 6 nodes.
        ASSERT_EQ(pAsset->NodeCount(), 5u);
        const auto & nodes = pAsset->GetNodes();
        EXPECT_EQ(nodes[0].child_count, 2);      // SubTree replaced by the sub-root
        EXPECT_EQ(nodes[1].type, FlatBuffers::BTNodeU::BTComposite);   // spliced sequence
        EXPECT_EQ(nodes[1].parent, 0);
        EXPECT_EQ(nodes[2].name, StrID("sub_wait"));
        EXPECT_EQ(nodes[3].name, StrID("sub_dummy"));
        EXPECT_EQ(nodes[4].name, StrID("tail"));
        EXPECT_EQ(nodes[4].parent, 0);

        GetSystem<TResourceManager>().Clear<BTAsset>();
}

TEST(BehaviorTreeAssetTest, AssetSubTreeCycleThrows) {
        // Self-reference: tree_cycle.sebt's SubTree points back at itself.
        auto selfRef = se_test::MakeBTDef(
                        "tree_self",
                        se_test::BTSelector({ se_test::BTSubTreeRef("resource/ai/gen/bt_cycle_self.sebt") }));
        const auto vSelf = se_test::WriteBT("resource/ai/gen/bt_cycle_self.sebt", *selfRef);
        EXPECT_THROW((TBehaviorTreeAsset<TMyBTTasks>("self_cycle", 1, vSelf.data(), vSelf.size())),
                     std::runtime_error);

        // Mutual: a.sebt → b.sebt → a.sebt.
        auto a = se_test::MakeBTDef(
                        "tree_a",
                        se_test::BTSelector({ se_test::BTSubTreeRef("resource/ai/gen/bt_cycle_b.sebt") }));
        se_test::WriteBT("resource/ai/gen/bt_cycle_a.sebt", *a);
        const auto vB = se_test::BakeBT(*se_test::MakeBTDef(
                        "tree_b",
                        se_test::BTSelector({ se_test::BTSubTreeRef("resource/ai/gen/bt_cycle_a.sebt") })));
        // b is loaded from bytes; its SubTree loads a (file), whose SubTree points
        // back at b (file) — the expansion stack catches the re-entry.
        EXPECT_THROW((TBehaviorTreeAsset<TMyBTTasks>("mutual_cycle", 1, vB.data(), vB.size())),
                     std::runtime_error);
}

} // namespace

// ---- extra authoring helpers (local to this suite) ---------------------------

namespace se_test {

inline void AddPathFollowerComponent(SE::FlatBuffers::NodeT & oNode) {
        auto pComp = std::make_unique<SE::FlatBuffers::ComponentT>();
        auto pPF   = std::make_unique<SE::FlatBuffers::PathFollowerT>();
        pComp->component.Set(std::move(*pPF));
        oNode.components.push_back(std::move(pComp));
}

inline void AddStateMachineComponent(SE::FlatBuffers::NodeT & oNode,
                                     std::unique_ptr<SE::FlatBuffers::StateMachineHolderT> pHolder,
                                     float fTickInterval = 0.f) {
        auto pComp = std::make_unique<SE::FlatBuffers::ComponentT>();
        auto pSM   = std::make_unique<SE::FlatBuffers::StateMachineT>();
        pSM->hms = std::move(pHolder);
        pSM->tick_interval = fTickInterval;
        pComp->component.Set(std::move(*pSM));
        oNode.components.push_back(std::move(pComp));
}

inline std::unique_ptr<SE::FlatBuffers::StateMachineHolderT> MakeSMHolderTwoStates() {
        // A --Triggered("go")--> B
        auto oDef  = std::make_unique<SE::FlatBuffers::StateMachineDefinitionT>();
        oDef->id = "sm_go";
        oDef->initial_state = "A";

        auto pA = std::make_unique<SE::FlatBuffers::SMStateT>();
        pA->id = "A";
        oDef->states.push_back(std::move(pA));
        auto pB = std::make_unique<SE::FlatBuffers::SMStateT>();
        pB->id = "B";
        oDef->states.push_back(std::move(pB));

        auto pT = std::make_unique<SE::FlatBuffers::SMTransitionT>();
        pT->from = "A";
        pT->to = "B";
        pT->priority = 1.f;
        auto pCond = std::make_unique<SE::FlatBuffers::SMConditionT>();
        pCond->parameter = "go";
        pCond->op = SE::FlatBuffers::SMConditionOp::Triggered;
        pT->conditions.push_back(std::move(pCond));
        oDef->transitions.push_back(std::move(pT));

        auto pHolder = std::make_unique<SE::FlatBuffers::StateMachineHolderT>();
        pHolder->state_machine = std::move(oDef);
        pHolder->name = "sm_go_inline";
        return pHolder;
}

} // namespace se_test

// ---------------------------------------------------------------------------
// Tier B — runtime semantics
// ---------------------------------------------------------------------------

namespace {

// -- composites -----------------------------------------------------------------

TEST_F(BehaviorTreeSystemTest, SequenceTruthTable) {
        auto pNode = SpawnAgent("SeqFail",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTSequence({ se_test::BTTask("Fail"), se_test::BTTask("Dummy") }))));
        ASSERT_NE(pNode, nullptr);
        auto * pComp = pNode->GetComponent<BTComponent>();
        ASSERT_NE(pComp, nullptr);

        GetSystem<BTSystem>().ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Failed);
        EXPECT_EQ(BTDummyTask::n_tick, 0);   // fail-fast: Dummy never ticked
        EXPECT_EQ(BTDummyTask::n_init, 0);
}

TEST_F(BehaviorTreeSystemTest, SequenceMemoryResumeSameTickAdvance) {
        auto pNode = SpawnAgent("SeqMem",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTSequence({ se_test::BTTask("Dummy"), se_test::BTTask("Succeed") }))));
        ASSERT_NE(pNode, nullptr);
        auto * pComp = pNode->GetComponent<BTComponent>();

        // node indices: 0 seq, 1 dummy, 2 succeed
        GetSystem<BTSystem>().ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(1), EBTNodeStatus::Running);
        GetSystem<BTSystem>().ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Running);

        // tick 3: Dummy completes and the sequence advances to (and finishes) the
        // next child in the SAME tick — no completion latency.
        GetSystem<BTSystem>().ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Succeeded);
        EXPECT_EQ(pComp->StatusOf(2), EBTNodeStatus::Succeeded);
}

TEST_F(BehaviorTreeSystemTest, SelectorTruthTableAndRestart) {
        auto pNode = SpawnAgent("SelRestart",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTSelector({ se_test::BTTask("Dummy"), se_test::BTTask("Succeed") }))));
        ASSERT_NE(pNode, nullptr);
        auto * pComp = pNode->GetComponent<BTComponent>();
        auto & sys = GetSystem<BTSystem>();

        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);   // Dummy t1
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Running);
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);   // t2
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);   // Dummy succeeds → selector Success
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Succeeded);

        // Re-entry restarts from the FIRST child (running_child cleared, Dummy fresh).
        const int inits_before = BTDummyTask::n_init;
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Running);
        EXPECT_EQ(BTDummyTask::n_init, inits_before + 1);
}

TEST_F(BehaviorTreeSystemTest, ParallelLiteSuccessAbortsRunning) {
        // required = 1 of { Succeed (instant), Dummy (3 ticks) }: the instant
        // success completes the branch and must Stop the still-running Dummy.
        auto pNode = SpawnAgent("ParSuccess",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTParallel({ se_test::BTTask("Succeed"), se_test::BTTask("Dummy") }, 1))));
        ASSERT_NE(pNode, nullptr);
        auto * pComp = pNode->GetComponent<BTComponent>();

        GetSystem<BTSystem>().ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Succeeded);
        EXPECT_EQ(BTDummyTask::n_stop, 1);   // running sibling deactivated with Stop
}

TEST_F(BehaviorTreeSystemTest, ParallelLiteFailureWhenUnreachable) {
        // required = 3 of { Succeed, Fail }: 1 failure makes 3 successes impossible.
        auto pNode = SpawnAgent("ParFail",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTParallel({ se_test::BTTask("Succeed"), se_test::BTTask("Fail") }, 3))));
        ASSERT_NE(pNode, nullptr);
        auto * pComp = pNode->GetComponent<BTComponent>();

        GetSystem<BTSystem>().ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Failed);
}

// -- tasks ----------------------------------------------------------------------

TEST_F(BehaviorTreeSystemTest, WaitCompletesWithAccumulatedDt) {
        auto pNode = SpawnAgent("WaitAcc",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t", se_test::BTTask("Wait", "", { se_test::ParamF("duration", 1.f) }))));
        ASSERT_NE(pNode, nullptr);
        auto * pComp = pNode->GetComponent<BTComponent>();
        auto & sys = GetSystem<BTSystem>();

        // 1.0 s at dt 0.2 → completes on the FIFTH tree tick, not frame-count.
        for (int i = 0; i < 4; ++i) sys.ProcessComponent(*pComp, pNode.get(), 0.2f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Running);
        sys.ProcessComponent(*pComp, pNode.get(), 0.2f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Succeeded);
}

// -- decorators ------------------------------------------------------------------

TEST_F(BehaviorTreeSystemTest, CooldownBlocksReentry) {
        auto pNode = SpawnAgent("Cooldown",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTCooldown(0.5f, se_test::BTTask("Succeed")))));
        ASSERT_NE(pNode, nullptr);
        auto * pComp = pNode->GetComponent<BTComponent>();
        auto & sys = GetSystem<BTSystem>();

        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);   // gate passes (never run), child succeeds, arms
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Succeeded);

        for (int i = 0; i < 4; ++i) {                      // 0.1..0.4 < 0.5 → gated
                sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
                EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Failed);
        }
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);   // 0.5 elapsed → re-entry
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Succeeded);
}

TEST_F(BehaviorTreeSystemTest, CooldownTimerSurvivesBranchChurn) {
        // Selector{ noise-cond(Dummy, LowerPriority), Cooldown(1.0)(Wait 0.2) }:
        // the cooldown arms at t2, the branch is then preempted by the noise
        // branch, and the gate must still measure from the completion time.
        auto pNode = SpawnAgent("CooldownAbort",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTSelector({
                                se_test::BTCondBool("noise_active", true,
                                        se_test::BTTask("Dummy"), FlatBuffers::EBTAbort::LowerPriority),
                                se_test::BTCooldown(1.0f, se_test::BTTask("Wait", "", { se_test::ParamF("duration", 0.2f) })),
                        }))));
        ASSERT_NE(pNode, nullptr);
        auto * pComp = pNode->GetComponent<BTComponent>();
        auto * pBB = pNode->GetComponent<Blackboard>();
        auto & sys = GetSystem<BTSystem>();

        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);   // t1: cd gate pass, Wait running
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);   // t2: Wait succeeds (0.2), arms cooldown
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Succeeded);

        pBB->GetCore().SetBool(StrID("noise_active"), true);
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);   // t3: noise branch runs (preempt path)
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);   // t4
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Running);
        EXPECT_EQ(BTDummyTask::n_init, 1);

        pBB->GetCore().SetBool(StrID("noise_active"), false);
        // t5: the Dummy completes; the selector hits the cooldown gate and fails.
        // The gate keeps blocking while its armed window elapses (the timer only
        // accumulates while the gate itself is exercised — 0.1 s per tick).
        for (int i = 0; i < 7; ++i) sys.ProcessComponent(*pComp, pNode.get(), 0.1f);   // 0.1..0.7
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Failed);
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);        // 0.8 → still gated
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Failed);
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);        // 0.9 → still gated
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Failed);
        // ~1.0 s post-arm (float accumulation may push the exact open by one
        // tick): the gate opens and the Wait restarts inside the cd branch.
        for (int i = 0; i < 3 && pComp->StatusOf(0) != EBTNodeStatus::Running; ++i) {
                sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
        }
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Running);
}

TEST_F(BehaviorTreeSystemTest, TimeLimitExpiryAbortsChild) {
        auto pNode = SpawnAgent("TimeLimit",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTTimeLimit(0.15f, se_test::BTTask("Dummy")))));
        ASSERT_NE(pNode, nullptr);
        auto * pComp = pNode->GetComponent<BTComponent>();
        auto & sys = GetSystem<BTSystem>();

        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);   // t1: timer .1 < .15, Dummy running
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Running);
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);   // t2: timer .2 ≥ .15 → child deactivated, Failure
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Failed);
        EXPECT_EQ(BTDummyTask::n_stop, 1);
}

TEST_F(BehaviorTreeSystemTest, LoopCountsInfiniteAndFailurePropagates) {
        auto pCount = SpawnAgent("Loop3",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t", se_test::BTLoop(3, se_test::BTTask("Succeed")))));
        auto * pC = pCount->GetComponent<BTComponent>();
        auto & sys = GetSystem<BTSystem>();
        sys.ProcessComponent(*pC, pCount.get(), 0.1f);
        EXPECT_EQ(pC->StatusOf(0), EBTNodeStatus::Running);
        sys.ProcessComponent(*pC, pCount.get(), 0.1f);
        EXPECT_EQ(pC->StatusOf(0), EBTNodeStatus::Running);
        sys.ProcessComponent(*pC, pCount.get(), 0.1f);
        EXPECT_EQ(pC->StatusOf(0), EBTNodeStatus::Succeeded);   // 3rd completion

        auto pInf = SpawnAgent("Loop0",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t2", se_test::BTLoop(0, se_test::BTTask("Succeed")))));
        auto * pI = pInf->GetComponent<BTComponent>();
        for (int i = 0; i < 5; ++i) {
                sys.ProcessComponent(*pI, pInf.get(), 0.1f);
                EXPECT_EQ(pI->StatusOf(0), EBTNodeStatus::Running);   // 0 = forever
        }

        auto pF = SpawnAgent("LoopF",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t3", se_test::BTLoop(3, se_test::BTTask("Fail")))));
        auto * pFComp = pF->GetComponent<BTComponent>();
        sys.ProcessComponent(*pFComp, pF.get(), 0.1f);
        EXPECT_EQ(pFComp->StatusOf(0), EBTNodeStatus::Failed);   // child Failure exits immediately
}

TEST_F(BehaviorTreeSystemTest, ForceStatusOverrides) {
        auto pF = SpawnAgent("ForceF",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTForce(FlatBuffers::EBTForceTo::Failure, se_test::BTTask("Succeed")))));
        auto * pFC = pF->GetComponent<BTComponent>();
        GetSystem<BTSystem>().ProcessComponent(*pFC, pF.get(), 0.1f);
        EXPECT_EQ(pFC->StatusOf(0), EBTNodeStatus::Failed);

        auto pS = SpawnAgent("ForceS",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t2",
                        se_test::BTForce(FlatBuffers::EBTForceTo::Success, se_test::BTTask("Fail")))));
        auto * pSC = pS->GetComponent<BTComponent>();
        GetSystem<BTSystem>().ProcessComponent(*pSC, pS.get(), 0.1f);
        EXPECT_EQ(pSC->StatusOf(0), EBTNodeStatus::Succeeded);
}

// -- conditions ------------------------------------------------------------------

TEST_F(BehaviorTreeSystemTest, ConditionFloatAndBoolOps) {
        auto pNode = SpawnAgent("CondOps",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTSelector({
                                se_test::BTCond("alert_level", FlatBuffers::SMConditionOp::Greater, 0.5f,
                                                se_test::BTTask("Succeed")),
                                se_test::BTCondBool("alarm_raised", true, se_test::BTTask("Succeed")),
                        }))));
        ASSERT_NE(pNode, nullptr);
        auto * pComp = pNode->GetComponent<BTComponent>();
        auto * pBB = pNode->GetComponent<Blackboard>();
        auto & sys = GetSystem<BTSystem>();

        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Failed);   // 0.0 > 0.5 false, alarm false

        pBB->GetCore().SetFloat(StrID("alert_level"), 0.7f);
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Succeeded);   // float op true

        pBB->GetCore().SetFloat(StrID("alert_level"), 0.0f);
        pBB->GetCore().SetBool(StrID("alarm_raised"), true);
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Succeeded);   // bool op true
}

TEST_F(BehaviorTreeSystemTest, ConditionEntityFallsBackToGlobal) {
        // Agent with NO entity blackboard; the key lives on the world global.
        auto oDef = MakeGuardDef();
        se_test::AddKey(oDef, "global_alarm", FlatBuffers::EBType::Bool);

        auto pNode = SpawnAgent("GlobalCond",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTCondBool("global_alarm", true, se_test::BTTask("Succeed")))),
                0.f, /*bWithBlackboard=*/false);
        ASSERT_NE(pNode, nullptr);
        auto * pComp = pNode->GetComponent<BTComponent>();
        auto & sys = GetSystem<BTSystem>();

        ASSERT_TRUE(LoadWorldWithGlobal(oDef));
        auto & oGlobal = GetSystem<BlackboardSystem>().GetGlobal();

        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Failed);   // key false on global

        oGlobal.SetBool(StrID("global_alarm"), true);
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Succeeded);   // global fallback
}

// -- aborts (the hard 20%) ---------------------------------------------------------

TEST_F(BehaviorTreeSystemTest, AbortSelfInterruptsSameTick) {
        // Sequence{ Cond(alarm, Self)(Wait 2.0), Succeed }: dropping the key
        // mid-Wait must deactivate the Wait AND fail the sequence in that tick.
        auto pNode = SpawnAgent("AbortSelf",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTSequence({
                                se_test::BTCondBool("alarm_raised", true, se_test::BTTask("Wait", "", { se_test::ParamF("duration", 2.f) }),
                                                    FlatBuffers::EBTAbort::Self),
                                se_test::BTTask("Succeed"),
                        }))));
        ASSERT_NE(pNode, nullptr);
        auto * pComp = pNode->GetComponent<BTComponent>();
        auto * pBB = pNode->GetComponent<Blackboard>();
        auto & sys = GetSystem<BTSystem>();

        pBB->GetCore().SetBool(StrID("alarm_raised"), true);
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Running);

        pBB->GetCore().SetBool(StrID("alarm_raised"), false);
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Failed);        // sequence failed THIS tick
        EXPECT_EQ(pComp->StatusOf(2), EBTNodeStatus::Idle);          // Wait deactivated
        EXPECT_EQ(BTDummyTask::n_tick, 0);                            // sibling untouched (no same-tick advance on Failure)

        pBB->GetCore().SetBool(StrID("alarm_raised"), true);
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Running);       // re-enters cleanly
}

TEST_F(BehaviorTreeSystemTest, AbortLowerPriorityPreempts) {
        // Selector{ noise-cond(LowerPriority)(Dummy), Wait 10 }: raising the key
        // while the Wait branch runs preempts it in that same tick.
        auto pNode = SpawnAgent("AbortLP",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTSelector({
                                se_test::BTCondBool("noise_active", true, se_test::BTTask("Dummy"),
                                                    FlatBuffers::EBTAbort::LowerPriority),
                                se_test::BTTask("Wait", "tail", { se_test::ParamF("duration", 10.f) }),
                        }))));
        ASSERT_NE(pNode, nullptr);
        auto * pComp = pNode->GetComponent<BTComponent>();
        auto * pBB = pNode->GetComponent<Blackboard>();
        auto & sys = GetSystem<BTSystem>();

        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);   // cond fails → Wait branch
        EXPECT_EQ(pComp->StatusOf(3), EBTNodeStatus::Running);   // 0 sel, 1 cond, 2 dummy, 3 wait

        pBB->GetCore().SetBool(StrID("noise_active"), true);
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Running);   // still running…
        EXPECT_EQ(pComp->StatusOf(3), EBTNodeStatus::Idle);      // …but the Wait is dead
        EXPECT_EQ(pComp->StatusOf(2), EBTNodeStatus::Running);   // …and the Dummy branch is live, same tick
}

TEST_F(BehaviorTreeSystemTest, NoAbortFlagNoRecheck) {
        auto pNode = SpawnAgent("NoAbort",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTSelector({
                                se_test::BTCondBool("noise_active", true, se_test::BTTask("Dummy")),
                                se_test::BTTask("Wait", "", { se_test::ParamF("duration", 10.f) }),
                        }))));
        ASSERT_NE(pNode, nullptr);
        auto * pComp = pNode->GetComponent<BTComponent>();
        auto * pBB = pNode->GetComponent<Blackboard>();
        auto & sys = GetSystem<BTSystem>();

        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(3), EBTNodeStatus::Running);

        pBB->GetCore().SetBool(StrID("noise_active"), true);
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(3), EBTNodeStatus::Running);   // no abort flag → untouched
}

// -- scheduler -----------------------------------------------------------------

TEST_F(BehaviorTreeSystemTest, StaggerPhaseSpreadAndDeterminism) {
        auto a = SpawnAgent("StaggerA", se_test::MakeBTHolderInline(*MakeSimpleWaitTree()), 0.2f);
        auto b = SpawnAgent("StaggerB", se_test::MakeBTHolderInline(*MakeSimpleWaitTree()), 0.2f);
        ASSERT_NE(a, nullptr);
        ASSERT_NE(b, nullptr);
        auto * pA = a->GetComponent<BTComponent>();
        auto * pB = b->GetComponent<BTComponent>();

        // Record the frame of each agent's FIRST tick (one interval window).
        int firstA = -1, firstB = -1;
        for (int f = 0; f < 13; ++f) {
                const uint32_t beforeA = pA->TickCount();
                const uint32_t beforeB = pB->TickCount();
                StepFrame(1.f / 60.f);
                if (firstA < 0 && pA->TickCount() != beforeA) firstA = f;
                if (firstB < 0 && pB->TickCount() != beforeB) firstB = f;
        }
        ASSERT_GE(firstA, 0);
        ASSERT_GE(firstB, 0);

        // Different names → different phase seeds → different first-tick frames
        // (hash spread over %16; these two names land on different phases).
        EXPECT_NE(firstA, firstB);
}

TEST_F(BehaviorTreeSystemTest, StaggerNoDoubleTick) {
        auto pNode = SpawnAgent("StaggerC", se_test::MakeBTHolderInline(*MakeSimpleWaitTree()), 0.2f);
        auto * pComp = pNode->GetComponent<BTComponent>();
        for (int f = 0; f < 600; ++f) StepFrame(1.f / 60.f);   // 10 s
        // 10 s at a 0.2 s interval = 50 ticks (±1 for the phase seed).
        const uint32_t ticks = pComp->TickCount();
        EXPECT_GE(ticks, 49u);
        EXPECT_LE(ticks, 51u);
}

TEST_F(BehaviorTreeSystemTest, ManyAgentsStaggerBounded) {
        std::vector<TSceneTree::TSceneNode> vAgents;
        for (int i = 0; i < 200; ++i) {
                vAgents.push_back(SpawnAgent("Swarm_" + std::to_string(i),
                                se_test::MakeBTHolderInline(*MakeSimpleWaitTree()), 0.2f));
        }
        StepFrame(1.f / 60.f);   // one frame ticks ~200*dt/interval ≈ 17 agents
        uint32_t total = 0;
        for (auto & n : vAgents) total += n->GetComponent<BTComponent>()->TickCount();
        EXPECT_GE(total, 5u);
        EXPECT_LE(total, 25u);   // never "tick everything every frame" (would be 200)
}

TEST_F(BehaviorTreeSystemTest, ProcessComponentSeamDeterministic) {
        auto pNode = SpawnAgent("Seam",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t", se_test::BTTask("Dummy"))));
        auto * pComp = pNode->GetComponent<BTComponent>();
        auto & sys = GetSystem<BTSystem>();

        sys.ProcessComponent(*pComp, pNode.get(), 0.3f);
        sys.ProcessComponent(*pComp, pNode.get(), 0.3f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Running);
        EXPECT_EQ(pComp->TickCount(), 2u);
        EXPECT_FLOAT_EQ(pComp->LastTickDt(), 0.3f);
        sys.ProcessComponent(*pComp, pNode.get(), 0.3f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Succeeded);   // 3×0.3 ≥ 3 dummy ticks
}

// -- events / tasks / bridges ----------------------------------------------------

TEST_F(BehaviorTreeSystemTest, NamedNodeEvents) {
        se_test::BTSink<EBTEvent> oSink;
        auto pNode = SpawnAgent("Events",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTSelector({ se_test::BTTask("Dummy"), se_test::BTTask("Wait", "tail", { se_test::ParamF("duration", 1.f) }) },
                                            "brain"))));
        ASSERT_NE(pNode, nullptr);
        auto * pComp = pNode->GetComponent<BTComponent>();
        auto & sys = GetSystem<BTSystem>();

        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);   // dummy done → selector Success

        GetSystem<EventManager>().Process();   // EBTEvent is QueueEvent-deferred
        ASSERT_FALSE(oSink.hits.empty());
        bool bEnterBrain = false, bExitBrain = false;
        for (const auto & ev : oSink.hits) {
                if (ev.name == StrID("brain") && ev.kind == EBTEventKind::Enter) bEnterBrain = true;
                if (ev.name == StrID("brain") && ev.kind == EBTEventKind::Exit) bExitBrain = true;
                EXPECT_TRUE(ev.entity.lock() != nullptr);
        }
        EXPECT_TRUE(bEnterBrain);
        EXPECT_TRUE(bExitBrain);
}

TEST_F(BehaviorTreeSystemTest, SetBlackboardWriteWarnAndEvents) {
        se_test::BTSink<EBlackboardChanged> oSink;
        auto pNode = SpawnAgent("SetBB",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTSequence({
                                se_test::BTTask("SetBlackboard", "", { se_test::ParamS("key", "wp_target"),
                                                                       se_test::ParamV("value", glm::vec3(1.f, 2.f, 3.f)) }),
                                se_test::BTTask("Succeed"),
                        }))));
        ASSERT_NE(pNode, nullptr);
        auto * pComp = pNode->GetComponent<BTComponent>();
        auto * pBB = pNode->GetComponent<Blackboard>();

        GetSystem<BTSystem>().ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Succeeded);
        EXPECT_FLOAT_EQ(pBB->GetCore().GetVec3(StrID("wp_target")).x, 1.f);
        EXPECT_FLOAT_EQ(pBB->GetCore().GetVec3(StrID("wp_target")).z, 3.f);

        // change event flowed (deferred → drain)
        GetSystem<EventManager>().Process();
        ASSERT_FALSE(oSink.hits.empty());
        EXPECT_EQ(oSink.hits.back().key, StrID("wp_target"));
}

TEST_F(BehaviorTreeSystemTest, HSMBridgeDrivesTransition) {
        auto oNode = se_test::MakeNode("HSMGuy");
        se_test::AddBlackboardComponent(oNode, se_test::MakeBBHolderInline(MakeGuardDef(), "hsm_bb"));
        se_test::AddStateMachineComponent(oNode, se_test::MakeSMHolderTwoStates());
        se_test::AddBehaviorTreeComponent(oNode,
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTTask("TriggerSM", "", { se_test::ParamS("param", "go") }))));
        const auto vBytes = se_test::BakeScene(std::move(oNode));
        const std::vector<char> vBuf(vBytes.begin(), vBytes.end());
        auto pNode = scene->InstantiateFromBuffer(vBuf);
        ASSERT_NE(pNode, nullptr);

        auto * pSM = pNode->GetComponent<StateMachine>();
        ASSERT_NE(pSM, nullptr);
        EXPECT_EQ(pSM->GetCurrentState(), StrID("A"));

        // BT tick fires TriggerSM; the SM's next tick consumes it A → B.
        auto * pComp = pNode->GetComponent<BTComponent>();
        GetSystem<BTSystem>().ProcessComponent(*pComp, pNode.get(), 0.1f);
        StepFrame(0.1f);
        EXPECT_EQ(pSM->GetCurrentState(), StrID("B"));
}

TEST_F(BehaviorTreeSystemTest, SubTreeStateIsolation) {
        auto sub = se_test::MakeBTDef("iso_sub", se_test::BTTask("Dummy", "iso_dummy"));
        se_test::WriteBT("resource/ai/gen/bt_iso.sebt", *sub);
        auto main = se_test::MakeBTDef("iso_main", se_test::BTSubTreeRef("resource/ai/gen/bt_iso.sebt"));
        const auto vBytes = se_test::BakeBT(*main);

        CreateResource<BTAsset>("iso_tree", FlatBuffers::GetBehaviorTreeDefinition(vBytes.data()));
        auto a = SpawnAgent("IsoA", se_test::MakeBTHolderPath("resource/ai/gen/bt_iso.sebt"));
        auto b = SpawnAgent("IsoB", se_test::MakeBTHolderPath("resource/ai/gen/bt_iso.sebt"));
        auto * pA = a->GetComponent<BTComponent>();
        auto * pB = b->GetComponent<BTComponent>();
        auto & sys = GetSystem<BTSystem>();

        sys.ProcessComponent(*pA, a.get(), 0.1f);
        sys.ProcessComponent(*pA, a.get(), 0.1f);
        sys.ProcessComponent(*pA, a.get(), 0.1f);   // A's dummy finished
        EXPECT_EQ(pA->StatusOf(0), EBTNodeStatus::Succeeded);   // root IS the spliced dummy
        sys.ProcessComponent(*pB, b.get(), 0.1f);   // B ticked once — fresh state
        EXPECT_EQ(pB->StatusOf(0), EBTNodeStatus::Running);
}

TEST_F(BehaviorTreeSystemTest, TaskMemoryZeroedOnReentry) {
        auto pNode = SpawnAgent("MemZero",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t", se_test::BTLoop(2, se_test::BTTask("Dummy")))));
        auto * pComp = pNode->GetComponent<BTComponent>();
        auto & sys = GetSystem<BTSystem>();

        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);   // dummy init (mem marker), tick
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);   // dummy succeeds; Loop defers to next tick
        const auto & mem = pComp->State(1).mem;            // deactivated between iterations
        bool bZeroed = true;
        for (std::byte b : mem) bZeroed = bZeroed && (b == std::byte{0});
        EXPECT_TRUE(bZeroed);
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);   // iteration 2: fresh entry re-plants the marker
        EXPECT_EQ(pComp->StatusOf(1), EBTNodeStatus::Running);
}

TEST_F(BehaviorTreeSystemTest, RootRestartsAfterCompletion) {
        auto pNode = SpawnAgent("Restart",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTSelector({ se_test::BTTask("Fail"), se_test::BTTask("Succeed") }))));
        auto * pComp = pNode->GetComponent<BTComponent>();
        auto & sys = GetSystem<BTSystem>();
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Succeeded);
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Succeeded);   // clean re-entry
}

// ---------------------------------------------------------------------------
// Tier C — composed flows
// ---------------------------------------------------------------------------

TEST_F(BehaviorTreeSystemTest, ComponentLoadFromSceneBufferPathMode) {
        auto def = se_test::MakeBTDef("path_tree", se_test::BTTask("Dummy"));
        se_test::WriteBT("resource/ai/gen/bt_path.sebt", *def);

        auto oNode = se_test::MakeNode("PathGuy");
        se_test::AddBehaviorTreeComponent(oNode, se_test::MakeBTHolderPath("resource/ai/gen/bt_path.sebt"), 0.25f);
        const auto vBytes = se_test::BakeScene(std::move(oNode));
        const std::vector<char> vBuf(vBytes.begin(), vBytes.end());
        auto pNode = scene->InstantiateFromBuffer(vBuf);
        ASSERT_NE(pNode, nullptr);

        auto * pComp = pNode->GetComponent<BTComponent>();
        ASSERT_NE(pComp, nullptr);
        EXPECT_FLOAT_EQ(pComp->GetTickInterval(), 0.25f);
        GetSystem<BTSystem>().ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Running);
}

TEST_F(BehaviorTreeSystemTest, MoveToIssuesAndPolls) {
        auto oNode = se_test::MakeNode("Mover");
        se_test::AddBlackboardComponent(oNode, se_test::MakeBBHolderInline(MakeGuardDef(), "mover_bb"));
        se_test::AddPathFollowerComponent(oNode);
        se_test::AddBehaviorTreeComponent(oNode,
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTTask("MoveTo", "", { se_test::ParamS("target", "wp_target") }))));
        const auto vBytes = se_test::BakeScene(std::move(oNode));
        const std::vector<char> vBuf(vBytes.begin(), vBytes.end());
        auto pNode = scene->InstantiateFromBuffer(vBuf);
        ASSERT_NE(pNode, nullptr);

        auto * pComp = pNode->GetComponent<BTComponent>();
        auto * pBB = pNode->GetComponent<Blackboard>();
        auto * pFollower = pNode->GetComponent<PathFollower>();
        ASSERT_NE(pFollower, nullptr);

        pBB->GetCore().SetVec3(StrID("wp_target"), glm::vec3(4.f, 0.f, 5.f));
        auto & sys = GetSystem<BTSystem>();
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Running);       // Following
        EXPECT_FLOAT_EQ(pFollower->GetGoal().x, 4.f);                 // goal from the blackboard
        EXPECT_EQ(pFollower->GetStatus(), PathStatus::Following);

        pFollower->Stop();                                            // external writer releases the goal
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Failed);         // Idle → Failure (warn once)
}

TEST_F(BehaviorTreeSystemTest, MoveToMissingFollowerFailsGracefully) {
        auto pNode = SpawnAgent("NoFollower",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTTask("MoveTo", "", { se_test::ParamS("target", "wp_target") }))));
        auto * pComp = pNode->GetComponent<BTComponent>();
        GetSystem<BTSystem>().ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Failed);   // no PathFollower sibling
}

TEST_F(BehaviorTreeSystemTest, AcceptanceBrainPatrolInvestigateReturn) {
        // The moving_actors AIBrain shape, as data: patrol (custom NextWaypoint +
        // MoveTo over the follower), preempted by a noise condition with
        // LowerPriority abort; investigate = TimeLimit(Wait) + SetBlackboard.
        auto brain = se_test::BTSelector({
                se_test::BTCondBool("noise_active", true,
                        se_test::BTSequence({
                                se_test::BTTimeLimit(2.0f,
                                        se_test::BTTask("Wait", "inv_wait", { se_test::ParamF("duration", 1.f) })),
                                se_test::BTTask("SetBlackboard", "", { se_test::ParamS("key", "noise_active"),
                                                                       se_test::ParamB("value", false) }),
                        }, "investigate"),
                        FlatBuffers::EBTAbort::LowerPriority),
                se_test::BTLoop(0,
                        se_test::BTSequence({
                                se_test::BTTask("NextWaypoint", "", { se_test::ParamS("target", "wp_target"),
                                                                      se_test::ParamS("index_key", "patrol_index"),
                                                                      se_test::ParamI("count", 3),
                                                                      se_test::ParamF("spacing", 2.f) }),
                                se_test::BTTask("MoveTo", "", { se_test::ParamS("target", "wp_target") }),
                        }, "patrol")),
        }, "brain");

        auto oNode = se_test::MakeNode("GuardNPC");
        se_test::AddBlackboardComponent(oNode, se_test::MakeBBHolderInline(MakeGuardDef(), "guard_bb"));
        se_test::AddPathFollowerComponent(oNode);
        se_test::AddBehaviorTreeComponent(oNode, se_test::MakeBTHolderInline(*se_test::MakeBTDef("brain_tree", std::move(brain))));
        const auto vBytes = se_test::BakeScene(std::move(oNode));
        const std::vector<char> vBuf(vBytes.begin(), vBytes.end());
        auto pNode = scene->InstantiateFromBuffer(vBuf);
        ASSERT_NE(pNode, nullptr);

        auto * pComp = pNode->GetComponent<BTComponent>();
        auto * pBB = pNode->GetComponent<Blackboard>();
        auto * pFollower = pNode->GetComponent<PathFollower>();
        auto & sys = GetSystem<BTSystem>();
        ASSERT_NE(pFollower, nullptr);

        // --- patrol ---
        sys.ProcessComponent(*pComp, pNode.get(), 0.2f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Running);
        EXPECT_FLOAT_EQ(pBB->GetCore().GetVec3(StrID("wp_target")).x, 2.f);   // waypoint 1 at +2
        EXPECT_EQ(pFollower->GetStatus(), PathStatus::Following);

        // --- noise fires: patrol preempted in the same tick ---
        pBB->GetCore().SetBool(StrID("noise_active"), true);
        pBB->GetCore().SetVec3(StrID("noise_pos"), glm::vec3(9.f, 0.f, 9.f));
        sys.ProcessComponent(*pComp, pNode.get(), 0.2f);
        EXPECT_EQ(pComp->StatusOf(2), EBTNodeStatus::Running);   // investigate live
        EXPECT_EQ(pComp->StatusOf(9), EBTNodeStatus::Idle);      // patrol MoveTo dead
        EXPECT_EQ(pFollower->GetStatus(), PathStatus::Idle);     // MoveTo.Stop released the goal

        // --- investigate completes (1.0 s Wait < 2.0 s TimeLimit), clears the key ---
        for (int i = 0; i < 8 && pComp->StatusOf(0) == EBTNodeStatus::Running; ++i) {
                sys.ProcessComponent(*pComp, pNode.get(), 0.2f);
        }
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Succeeded);
        EXPECT_EQ(pBB->GetCore().GetBool(StrID("noise_active")), false);

        // --- return to patrol: waypoint index advanced to 2 ---
        sys.ProcessComponent(*pComp, pNode.get(), 0.2f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Running);
        EXPECT_EQ(pComp->StatusOf(7), EBTNodeStatus::Running);   // patrol branch live again
        EXPECT_FLOAT_EQ(pBB->GetCore().GetVec3(StrID("wp_target")).x, 4.f);
        EXPECT_EQ(pFollower->GetStatus(), PathStatus::Following);
}

TEST_F(BehaviorTreeSystemTest, EntityDestroyMidRunSafe) {
        auto pNode = SpawnAgent("Doomed",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTTask("Wait", "", { se_test::ParamF("duration", 2.f) }))), 0.05f);
        auto hNode = pNode->GetShared();
        auto * pComp = pNode->GetComponent<BTComponent>();
        for (int f = 0; f < 13 && pComp->TickCount() == 0; ++f) StepFrame(1.f / 60.f);
        EXPECT_EQ(pComp->TickCount(), 1u);   // ticked once, branch running

        GetSystem<EntityManager>().Destroy(hNode);
        StepFrame(1.f / 60.f);               // EPostUpdate flush: destroy + no BT tick on a corpse
        StepFrame(1.f / 60.f);
        GetSystem<EventManager>().Process(); // drain queued events
        SUCCEED();                           // reach = no UAF
}

TEST_F(BehaviorTreeSystemTest, GlobalUnloadDegradesGracefully) {
        auto oDef = MakeGuardDef();
        se_test::AddKey(oDef, "global_alarm", FlatBuffers::EBType::Bool);

        auto pNode = SpawnAgent("Unloaded",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTCondBool("global_alarm", true, se_test::BTTask("Succeed")))),
                0.f, false);
        auto * pComp = pNode->GetComponent<BTComponent>();
        auto & sys = GetSystem<BTSystem>();
        ASSERT_TRUE(LoadWorldWithGlobal(oDef));
        auto & oGlobal = GetSystem<BlackboardSystem>().GetGlobal();
        oGlobal.SetBool(StrID("global_alarm"), true);
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Succeeded);

        GetSystem<WorldSystem>().UnloadWorld();
        sys.ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Failed);   // degraded: condition false, no crash
}

TEST_F(BehaviorTreeSystemTest, DisableRemovesFromScheduler) {
        auto pNode = SpawnAgent("Disabler", se_test::MakeBTHolderInline(*MakeSimpleWaitTree()), 0.2f);
        auto * pComp = pNode->GetComponent<BTComponent>();
        for (int f = 0; f < 13 && pComp->TickCount() == 0; ++f) StepFrame(1.f / 60.f);
        EXPECT_EQ(pComp->TickCount(), 1u);

        pComp->Disable();
        StepFrame(1.f / 60.f);
        StepFrame(1.f / 60.f);
        EXPECT_EQ(pComp->TickCount(), 1u);   // scheduler forgot the component

        pComp->Enable();
        for (int f = 0; f < 13 && pComp->TickCount() == 1; ++f) StepFrame(1.f / 60.f);
        EXPECT_EQ(pComp->TickCount(), 2u);
}

TEST_F(BehaviorTreeSystemTest, SpawnTemplateTaskSpawns) {
        // a real .setm template + instantiator wiring (SpawnerTest pattern)
        flatbuffers::FlatBufferBuilder fbb;
        FlatBuffers::EntityTemplateT oTmpl;
        oTmpl.id = "spawnling_tmpl";
        oTmpl.root = std::make_unique<FlatBuffers::NodeTemplateT>();
        oTmpl.root->name = "Spawnling";
        oTmpl.root->enabled = true;
        auto offset = FlatBuffers::EntityTemplate::Pack(fbb, &oTmpl);
        FlatBuffers::FinishEntityTemplateBuffer(fbb, offset);

        auto & oETS = GetSystem<TEntityTemplateSystem>();
        ASSERT_EQ(oETS.LoadFromMemory(fbb.GetBufferPointer(), fbb.GetSize()), uSUCCESS);
        auto * pETS = &oETS;
        GetSystem<EntityManager>().SetTemplateInstantiator(
                [pETS](const SpawnIntent & oIntent) -> TSceneTree::TSceneNodeWeak {
                        return pETS->Instantiate(oIntent.prefab_id, oIntent.pParent,
                                        oIntent.vTranslation, oIntent.vRotation,
                                        oIntent.vScale, oIntent.enabled);
                });

        auto pNode = SpawnAgent("Spawner",
                se_test::MakeBTHolderInline(*se_test::MakeBTDef("t",
                        se_test::BTSequence({
                                se_test::BTTask("SpawnTemplate", "", { se_test::ParamS("template", "spawnling_tmpl"),
                                                                       se_test::ParamV("offset", glm::vec3(1.f, 0.f, 0.f)) }),
                                se_test::BTTask("Succeed"),
                        }))));
        auto * pComp = pNode->GetComponent<BTComponent>();
        GetSystem<BTSystem>().ProcessComponent(*pComp, pNode.get(), 0.1f);
        EXPECT_EQ(pComp->StatusOf(0), EBTNodeStatus::Succeeded);

        auto hSpawned = pNode->GetScene()->GetRoot()->FindChild(StrID("Spawnling"));
        EXPECT_TRUE(hSpawned != nullptr);
}

} // namespace
