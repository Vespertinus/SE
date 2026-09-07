
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define SE_IMPL
#include <Logging.h>
#include <Global.h>
#include <ErrCode.h>
#include <StrID.h>
#include <ResourceHolder.h>
#include <ResourceHandle.h>
#include <ResourceManagerMock.h>
#include <Engine.h>
#include <DebugRendererMock.h>
#include <EventManager.h>
#include <EventManager.tcc>
#include <CommonEvents.h>

#include <loki/Singleton.h>

// ---------------------------------------------------------------------------
// Minimal engine — EventManager + StateMachineSystem; no graphics, no files.
// ---------------------------------------------------------------------------

namespace SE {

class StateMachineSystem;
template <class TSystem> TSystem & GetSystem();

using ResourceManager   = ResourceManagerMock;
using DebugRenderer     = DebugRendererMock;
using EngineBase        = Engine<ResourceManager, DebugRenderer, EventManager, StateMachineSystem>;

using TEngine = typename Loki::SingletonHolder<
        EngineBase,
        Loki::CreateUsingNew,
        Loki::DeletableSingleton>;

template <class TSystem> TSystem & GetSystem() {
        return TEngine::Instance().Get<TSystem>();
}

template <class Resource, class ... TArgs>
H<Resource> CreateResource(const std::string& name, const TArgs& ... args) {
        return GetSystem<ResourceManager>().Create<Resource>(name, args...);
}
template <class Resource>
Resource* GetResource(H<Resource> h) {
        return GetSystem<ResourceManager>().Get<Resource>(h);
}
template <class Resource>
void DestroyResource(H<Resource> h) {
        GetSystem<ResourceManager>().Destroy<Resource>(h);
}

} // namespace SE

#include <AnimClip.tcc> //FIXME remove after moving AnimClip and other resources from SceneTree scheme to AssetPack scheme
#include <SceneTree.h>

namespace SE {
class StaticModelMock;
class StateMachine;
using TSceneTree = SceneTree<StaticModelMock, StateMachine>;
} // namespace SE

#include <StaticModelMock.h>
#include <hsm/ParameterStore.h>
#include <hsm/StateCondition.h>
#include <hsm/StateMachineSystem.h>
#include <hsm/StateMachineSystem.tcc>
#include <hsm/HSMResolver.h>
#include <hsm/HSMResolver.tcc>
#include <hsm/StateMachineAsset.h>
#include <hsm/StateMachineAsset.tcc>

#include <hsm/StateMachine.h>
#include <hsm/StateMachine.tcc>

#include <StateMachine_generated.h>
#include <flatbuffers/flatbuffers.h>

// ---------------------------------------------------------------------------
// FlatBuffer builder helpers
// ---------------------------------------------------------------------------

namespace TestHelper {

using namespace SE::FlatBuffers;

// Builds a StateMachineDefinition and wraps it in a StateMachine component table
// with an inline StateMachineHolder so no file loading is needed.
struct TwoStateMachine {

        flatbuffers::FlatBufferBuilder b{2048};
        const StateMachine*            sm = nullptr;

        void Build(float exit_time = 0.f, float idle_to_walk_priority = 0.f,
                   float tick_interval = 0.f) {
                auto idle_id = b.CreateString("Idle");
                auto walk_id = b.CreateString("Walk");

                auto idle_state = CreateSMState(b, idle_id);
                auto walk_state = CreateSMState(b, walk_id);
                auto states     = b.CreateVector(std::vector{idle_state, walk_state});

                // condition: speed > 0.1
                auto param_str = b.CreateString("speed");
                auto cond      = CreateSMCondition(b, param_str, SMConditionOp::Greater, 0.1f);
                auto conds     = b.CreateVector(std::vector{cond});

                auto from  = b.CreateString("Idle");
                auto to    = b.CreateString("Walk");
                auto trans = CreateSMTransition(b, from, to,
                                idle_to_walk_priority,
                                /*duration=*/0.f,
                                exit_time,
                                /*can_interrupt=*/false,
                                conds);
                auto transitions = b.CreateVector(std::vector{trans});

                auto initial  = b.CreateString("Idle");
                auto sm_id    = b.CreateString("test_sm");
                auto def_off  = CreateStateMachineDefinition(b, sm_id, 1, initial, states, transitions);

                auto sm_name = b.CreateString("test_state_machine");
                auto holder = CreateStateMachineHolder(b, def_off, 0, sm_name);
                auto sm_off = CreateStateMachine(b, holder, tick_interval);
                b.Finish(sm_off);
                sm = flatbuffers::GetRoot<StateMachine>(b.GetBufferPointer());
        }
};

// HSM with two sub-hierarchies (no inline SM wrapper — resolver tests use StateEntry directly)
struct HsmStates {
        std::vector<SE::StateMachineAsset::StateEntry> states;

        void Build() {
                auto mk = [](const char* id, const char* parent = nullptr) {
                        SE::StateMachineAsset::StateEntry e;
                        e.id     = SE::StrID(id);
                        e.parent = parent ? SE::StrID(parent) : SE::StrID{};
                        return e;
                };
                states = {
                        mk("Grounded"),
                        mk("Grounded.Idle",    "Grounded"),
                        mk("Grounded.Walk",    "Grounded"),
                        mk("Combat"),
                        mk("Combat.Attacking", "Combat"),
                };
        }
};

} // namespace TestHelper

// ---------------------------------------------------------------------------
// ParameterStore — standalone unit tests
// ---------------------------------------------------------------------------

TEST(ParameterStore, SetGetFloat) {
        SE::ParameterStore ps;
        ps.SetFloat(SE::StrID("speed"), 3.14f);
        EXPECT_FLOAT_EQ(ps.GetFloat(SE::StrID("speed")), 3.14f);
        EXPECT_FLOAT_EQ(ps.GetFloat(SE::StrID("missing")), 0.f);
}

TEST(ParameterStore, SetGetBool) {
        SE::ParameterStore ps;
        ps.SetBool(SE::StrID("crouching"), true);
        EXPECT_TRUE(ps.GetBool(SE::StrID("crouching")));
        EXPECT_FALSE(ps.GetBool(SE::StrID("missing")));
}

TEST(ParameterStore, SetGetInt) {
        SE::ParameterStore ps;
        ps.SetInt(SE::StrID("ammo"), 42);
        EXPECT_EQ(ps.GetInt(SE::StrID("ammo")), 42);
}

TEST(ParameterStore, GetNumericWorksForBothFloatAndInt) {
        SE::ParameterStore ps;
        ps.SetFloat(SE::StrID("f"), 1.5f);
        ps.SetInt(SE::StrID("i"), 7);
        EXPECT_FLOAT_EQ(ps.GetNumeric(SE::StrID("f")), 1.5f);
        EXPECT_FLOAT_EQ(ps.GetNumeric(SE::StrID("i")), 7.f);
}

TEST(ParameterStore, TriggerConsumedExactlyOnce) {
        SE::ParameterStore ps;
        ps.SetTrigger(SE::StrID("landed"));
        EXPECT_TRUE(ps.ConsumeTrigger(SE::StrID("landed")));
        EXPECT_FALSE(ps.ConsumeTrigger(SE::StrID("landed")));
        EXPECT_FALSE(ps.ConsumeTrigger(SE::StrID("landed")));
}

TEST(ParameterStore, Clear) {
        SE::ParameterStore ps;
        ps.SetFloat(SE::StrID("x"), 1.f);
        ps.SetTrigger(SE::StrID("t"));
        ps.Clear();
        EXPECT_FLOAT_EQ(ps.GetFloat(SE::StrID("x")), 0.f);
        EXPECT_FALSE(ps.ConsumeTrigger(SE::StrID("t")));
}

// ---------------------------------------------------------------------------
// StateCondition — standalone unit tests
// ---------------------------------------------------------------------------

TEST(StateCondition, IsTrue) {
        SE::ParameterStore ps;
        ps.SetBool(SE::StrID("flag"), true);
        SE::StateCondition c;
        c.parameter = SE::StrID("flag");
        c.op        = SE::FlatBuffers::SMConditionOp::IsTrue;
        EXPECT_TRUE(c.Evaluate(ps));
        ps.SetBool(SE::StrID("flag"), false);
        EXPECT_FALSE(c.Evaluate(ps));
}

TEST(StateCondition, IsFalse) {
        SE::ParameterStore ps;
        ps.SetBool(SE::StrID("flag"), false);
        SE::StateCondition c;
        c.parameter = SE::StrID("flag");
        c.op        = SE::FlatBuffers::SMConditionOp::IsFalse;
        EXPECT_TRUE(c.Evaluate(ps));
}

TEST(StateCondition, Triggered) {
        SE::ParameterStore ps;
        ps.SetTrigger(SE::StrID("jump"));
        SE::StateCondition c;
        c.parameter = SE::StrID("jump");
        c.op        = SE::FlatBuffers::SMConditionOp::Triggered;
        EXPECT_TRUE(c.Evaluate(ps));
        EXPECT_FALSE(c.Evaluate(ps)); // consumed
}

TEST(StateCondition, NumericComparisons) {
        SE::ParameterStore ps;
        ps.SetFloat(SE::StrID("speed"), 5.f);

        auto check = [&](SE::FlatBuffers::SMConditionOp op, float threshold) {
                SE::StateCondition c;
                c.parameter = SE::StrID("speed");
                c.op        = op;
                c.threshold = threshold;
                return c.Evaluate(ps);
        };

        using Op = SE::FlatBuffers::SMConditionOp;
        EXPECT_TRUE (check(Op::Greater,      4.f));
        EXPECT_FALSE(check(Op::Greater,      5.f));
        EXPECT_TRUE (check(Op::GreaterEqual, 5.f));
        EXPECT_TRUE (check(Op::Less,         6.f));
        EXPECT_FALSE(check(Op::Less,         5.f));
        EXPECT_TRUE (check(Op::LessEqual,    5.f));
        EXPECT_TRUE (check(Op::Equal,        5.f));
        EXPECT_FALSE(check(Op::Equal,        4.f));
        EXPECT_TRUE (check(Op::NotEqual,     4.f));
}

// ---------------------------------------------------------------------------
// HSMResolver — standalone unit tests (uses StateEntry directly, no FlatBuffers)
// ---------------------------------------------------------------------------

TEST(HSMResolver, AncestorChainFlatState) {
        TestHelper::HsmStates hm;
        hm.Build();
        SE::HSMResolver resolver;
        const auto& chain = resolver.AncestorChain(hm.states, SE::StrID("Grounded"));
        ASSERT_EQ(chain.size(), 1u);
        EXPECT_EQ(chain[0], SE::StrID("Grounded"));
}

TEST(HSMResolver, AncestorChainNestedState) {
        TestHelper::HsmStates hm;
        hm.Build();
        SE::HSMResolver resolver;
        const auto& chain = resolver.AncestorChain(hm.states, SE::StrID("Combat.Attacking"));
        ASSERT_EQ(chain.size(), 2u);
        EXPECT_EQ(chain[0], SE::StrID("Combat.Attacking"));
        EXPECT_EQ(chain[1], SE::StrID("Combat"));
}

TEST(HSMResolver, ResolveAttackingToGroundedIdle) {
        TestHelper::HsmStates hm;
        hm.Build();
        SE::HSMResolver resolver;
        const auto& path = resolver.Resolve(hm.states,
                        SE::StrID("Combat.Attacking"),
                        SE::StrID("Grounded.Idle"));

        ASSERT_EQ(path.vExitStates.size(), 2u);
        EXPECT_EQ(path.vExitStates[0], SE::StrID("Combat.Attacking"));
        EXPECT_EQ(path.vExitStates[1], SE::StrID("Combat"));

        ASSERT_EQ(path.vEnterStates.size(), 2u);
        EXPECT_EQ(path.vEnterStates[0], SE::StrID("Grounded"));
        EXPECT_EQ(path.vEnterStates[1], SE::StrID("Grounded.Idle"));
}

TEST(HSMResolver, ResolveSiblingStates) {
        TestHelper::HsmStates hm;
        hm.Build();
        SE::HSMResolver resolver;
        const auto& path = resolver.Resolve(hm.states,
                        SE::StrID("Grounded.Idle"),
                        SE::StrID("Grounded.Walk"));

        ASSERT_EQ(path.vExitStates.size(), 1u);
        EXPECT_EQ(path.vExitStates[0], SE::StrID("Grounded.Idle"));
        ASSERT_EQ(path.vEnterStates.size(), 1u);
        EXPECT_EQ(path.vEnterStates[0], SE::StrID("Grounded.Walk"));
}

// ---------------------------------------------------------------------------
// StateMachineSystem — transition logic
// ---------------------------------------------------------------------------

class StateMachineSystemTest : public ::testing::Test {
protected:
        StateMachineSystemTest() {
                SE::TEngine::Instance().Init();
        }
        ~StateMachineSystemTest() noexcept {
                Loki::DeletableSingleton<SE::EngineBase>::GracefulDelete();
        }

        // Build an inline StateMachine component, create a scene node, and attach the
        // component.  Returns the component pointer (owned by the node).
        SE::StateMachine* MakeComponent(
                        const TestHelper::TwoStateMachine& tm,
                        std::unique_ptr<SE::TSceneTree>& out_scene) {

                out_scene = std::make_unique<SE::TSceneTree>("test_scene", 0, true);
                auto pNode = out_scene->Create("entity");

                pNode->CreateComponent<SE::StateMachine>(tm.sm);

                return pNode->GetComponent<SE::StateMachine>();
        }
};

TEST_F(StateMachineSystemTest, TransitionFiresWhenConditionMet) {
        TestHelper::TwoStateMachine tm;
        tm.Build();

        std::unique_ptr<SE::TSceneTree> pScene;
        auto* comp = MakeComponent(tm, pScene);
        ASSERT_NE(comp, nullptr);

        comp->GetParams().SetFloat(SE::StrID("speed"), 0.5f);

        SE::GetSystem<SE::StateMachineSystem>().ProcessComponent(
                *comp, nullptr, 0.016f);

        EXPECT_EQ(comp->GetCurrentState(),  SE::StrID("Walk"));
        EXPECT_EQ(comp->GetPreviousState(), SE::StrID("Idle"));
        EXPECT_NEAR(comp->GetTimeInState(), 0.f, 1e-6f);
}

TEST_F(StateMachineSystemTest, TransitionDoesNotFireWhenConditionNotMet) {
        TestHelper::TwoStateMachine tm;
        tm.Build();

        std::unique_ptr<SE::TSceneTree> pScene;
        auto* comp = MakeComponent(tm, pScene);
        ASSERT_NE(comp, nullptr);

        comp->GetParams().SetFloat(SE::StrID("speed"), 0.05f); // below threshold

        SE::GetSystem<SE::StateMachineSystem>().ProcessComponent(
                *comp, nullptr, 0.016f);

        EXPECT_EQ(comp->GetCurrentState(), SE::StrID("Idle"));
}

TEST_F(StateMachineSystemTest, TimeInStateAccumulatesBeforeTransition) {
        TestHelper::TwoStateMachine tm;
        tm.Build();

        std::unique_ptr<SE::TSceneTree> pScene;
        auto* comp = MakeComponent(tm, pScene);
        ASSERT_NE(comp, nullptr);

        auto& sys = SE::GetSystem<SE::StateMachineSystem>();
        sys.ProcessComponent(*comp, nullptr, 0.1f);
        sys.ProcessComponent(*comp, nullptr, 0.1f);

        EXPECT_NEAR(comp->GetTimeInState(), 0.2f, 1e-5f);
        EXPECT_EQ(comp->GetCurrentState(), SE::StrID("Idle"));
}

TEST_F(StateMachineSystemTest, ExitTimeBlocksEarlyTransition) {
        TestHelper::TwoStateMachine tm;
        tm.Build(/*exit_time=*/0.5f);

        std::unique_ptr<SE::TSceneTree> pScene;
        auto* comp = MakeComponent(tm, pScene);
        ASSERT_NE(comp, nullptr);

        comp->GetParams().SetFloat(SE::StrID("speed"), 1.f);

        auto& sys = SE::GetSystem<SE::StateMachineSystem>();
        sys.ProcessComponent(*comp, nullptr, 0.1f);
        EXPECT_EQ(comp->GetCurrentState(), SE::StrID("Idle")); // not yet

        sys.ProcessComponent(*comp, nullptr, 0.4f); // total = 0.5 s
        EXPECT_EQ(comp->GetCurrentState(), SE::StrID("Walk")); // fires now
}

TEST_F(StateMachineSystemTest, TriggerConsumedOnTransition) {
        // Build a machine conditioned on a trigger parameter
        flatbuffers::FlatBufferBuilder b;
        using namespace SE::FlatBuffers;

        auto idle_s    = CreateSMState(b, b.CreateString("Idle"));
        auto walk_s    = CreateSMState(b, b.CreateString("Walk"));
        auto states    = b.CreateVector(std::vector{idle_s, walk_s});

        auto param_str = b.CreateString("jump");
        auto cond      = CreateSMCondition(b, param_str, SMConditionOp::Triggered, 0.f);
        auto conds     = b.CreateVector(std::vector{cond});

        auto tr = CreateSMTransition(b, b.CreateString("Idle"), b.CreateString("Walk"),
                                     0.f, 0.f, 0.f, false, conds);
        auto tr_v  = b.CreateVector(std::vector{tr});
        auto def   = CreateStateMachineDefinition(b, b.CreateString("trigger_sm"), 1,
                                                  b.CreateString("Idle"), states, tr_v);
        auto sm_name = b.CreateString("test_state_machine");
        auto holder = CreateStateMachineHolder(b, def, 0, sm_name);
        auto sm_off = CreateStateMachine(b, holder, 0.f);
        b.Finish(sm_off);
        const auto* pFB = flatbuffers::GetRoot<StateMachine>(b.GetBufferPointer());

        auto pScene = std::make_unique<SE::TSceneTree>("trig_scene", 0, true);
        auto pNode  = pScene->Create("entity");
        pNode->CreateComponent<SE::StateMachine>(pFB);
        auto* comp = pNode->GetComponent<SE::StateMachine>();
        ASSERT_NE(comp, nullptr);

        comp->GetParams().SetTrigger(SE::StrID("jump"));

        auto& sys = SE::GetSystem<SE::StateMachineSystem>();
        sys.ProcessComponent(*comp, nullptr, 0.016f);
        EXPECT_EQ(comp->GetCurrentState(), SE::StrID("Walk")); // trigger consumed, transition fired

        // Trigger was consumed; a second tick from Walk should not re-fire (no Walk→Idle transition)
        sys.ProcessComponent(*comp, nullptr, 0.016f);
        EXPECT_EQ(comp->GetCurrentState(), SE::StrID("Walk")); // stays in Walk
}

TEST_F(StateMachineSystemTest, HigherPriorityTransitionWins) {
        flatbuffers::FlatBufferBuilder b;
        using namespace SE::FlatBuffers;

        auto states = b.CreateVector(std::vector{
                CreateSMState(b, b.CreateString("Idle")),
                CreateSMState(b, b.CreateString("Walk")),
                CreateSMState(b, b.CreateString("Run")),
        });

        auto mk_trans = [&](const char* to_str, float prio) {
                auto param = b.CreateString("speed");
                auto cond  = CreateSMCondition(b, param, SMConditionOp::Greater, 0.1f);
                auto conds = b.CreateVector(std::vector{cond});
                return CreateSMTransition(b, b.CreateString("Idle"), b.CreateString(to_str),
                                          prio, 0.f, 0.f, false, conds);
        };

        auto tr_v = b.CreateVector(std::vector{mk_trans("Walk", 0.f), mk_trans("Run", 10.f)});
        auto def  = CreateStateMachineDefinition(b, b.CreateString("priority_sm"), 1,
                                                 b.CreateString("Idle"), states, tr_v);
        auto sm_name = b.CreateString("test_state_machine");
        auto sm_off = CreateStateMachine(b, CreateStateMachineHolder(b, def, 0, sm_name), 0.f);
        b.Finish(sm_off);
        const auto* pFB = flatbuffers::GetRoot<StateMachine>(b.GetBufferPointer());

        auto pScene = std::make_unique<SE::TSceneTree>("prio_scene", 0, true);
        auto pNode  = pScene->Create("entity");
        pNode->CreateComponent<SE::StateMachine>(pFB);
        auto* comp = pNode->GetComponent<SE::StateMachine>();
        ASSERT_NE(comp, nullptr);

        comp->GetParams().SetFloat(SE::StrID("speed"), 1.f);
        SE::GetSystem<SE::StateMachineSystem>().ProcessComponent(*comp, nullptr, 0.016f);
        EXPECT_EQ(comp->GetCurrentState(), SE::StrID("Run")); // higher priority wins
}

// ---------------------------------------------------------------------------
// Parameter defaults — initialized from definition
// ---------------------------------------------------------------------------

TEST(ParameterDefaults, InitializedCorrectly) {
        flatbuffers::FlatBufferBuilder b;
        using namespace SE::FlatBuffers;

        auto mk_float   = [&](const char* n, float v) {
                return CreateSMParameterDefault(b, b.CreateString(n),
                                SMParamValue::SMParamFloat,
                                CreateSMParamFloat(b, v).Union()); };
        auto mk_bool    = [&](const char* n, bool v) {
                return CreateSMParameterDefault(b, b.CreateString(n),
                                SMParamValue::SMParamBool,
                                CreateSMParamBool(b, static_cast<uint8_t>(v)).Union()); };
        auto mk_int     = [&](const char* n, int32_t v) {
                return CreateSMParameterDefault(b, b.CreateString(n),
                                SMParamValue::SMParamInt,
                                CreateSMParamInt(b, v).Union()); };
        auto mk_trigger = [&](const char* n) {
                return CreateSMParameterDefault(b, b.CreateString(n),
                                SMParamValue::SMParamTrigger,
                                CreateSMParamTrigger(b).Union()); };

        auto params = b.CreateVector(std::vector{
                mk_float("speed", 3.14f), mk_bool("crouching", true),
                mk_int("ammo", 5),        mk_trigger("jump"),
        });
        auto idle_s = CreateSMState(b, b.CreateString("Idle"));
        auto states = b.CreateVector(std::vector{idle_s});
        auto tr_v   = b.CreateVector(std::vector<flatbuffers::Offset<SMTransition>>{});
        auto def    = CreateStateMachineDefinition(b, b.CreateString("param_sm"), 1,
                                                   b.CreateString("Idle"), states, tr_v, params);
        auto sm_name = b.CreateString("test_state_machine");
        auto sm_off = CreateStateMachine(b, CreateStateMachineHolder(b, def, 0, sm_name), 0.f);
        b.Finish(sm_off);

        const auto* pRootFB = flatbuffers::GetRoot<SE::FlatBuffers::StateMachine>(b.GetBufferPointer());
        SE::StateMachineAsset asset("test", 0, pRootFB->hms()->state_machine());

        SE::ParameterStore ps;
        for (const auto& d : asset.GetDefaults()) {
                if (std::holds_alternative<float>(d.value))       ps.SetFloat(d.name, std::get<float>(d.value));
                else if (std::holds_alternative<bool>(d.value))   ps.SetBool(d.name,  std::get<bool>(d.value));
                else if (std::holds_alternative<int>(d.value))    ps.SetInt(d.name,   std::get<int>(d.value));
        }

        EXPECT_FLOAT_EQ(ps.GetFloat(SE::StrID("speed")),    3.14f);
        EXPECT_TRUE    (ps.GetBool(SE::StrID("crouching")));
        EXPECT_EQ      (ps.GetInt(SE::StrID("ammo")),        5);
        EXPECT_FALSE   (ps.ConsumeTrigger(SE::StrID("jump"))); // defaults to unset
}

// ---------------------------------------------------------------------------
// Tick interval — component driven by system
// ---------------------------------------------------------------------------

TEST_F(StateMachineSystemTest, TickIntervalThrottlesUpdates) {
        TestHelper::TwoStateMachine tm;
        tm.Build(/*exit_time=*/0.f, /*priority=*/0.f, /*tick_interval=*/0.1f);

        auto pScene = std::make_unique<SE::TSceneTree>("tick_scene", 0, true);
        auto pNode  = pScene->Create("entity");
        pNode->CreateComponent<SE::StateMachine>(tm.sm);
        auto* comp = pNode->GetComponent<SE::StateMachine>();
        ASSERT_NE(comp, nullptr);

        EXPECT_FLOAT_EQ(comp->GetTickInterval(), 0.1f);

        comp->Enable(); // registers with StateMachineSystem

        // After 0.05 s — should NOT have ticked (time_since_last_tick < 0.1)
        SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EUpdate{0.05f});
        EXPECT_NEAR(comp->GetTimeSinceLastTick(), 0.05f, 1e-6f);

        // After another 0.05 s — total = 0.1, should tick and reset
        SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EUpdate{0.05f});
        EXPECT_NEAR(comp->GetTimeSinceLastTick(), 0.f, 1e-6f);

        comp->Disable();
}

// ---------------------------------------------------------------------------
// RAII — component removed from system list on destruction
// ---------------------------------------------------------------------------

TEST_F(StateMachineSystemTest, ComponentRemovesFromSystemOnDisable) {
        TestHelper::TwoStateMachine tm;
        tm.Build();

        auto pScene = std::make_unique<SE::TSceneTree>("raii_scene", 0, true);
        auto pNode  = pScene->Create("entity");
        pNode->CreateComponent<SE::StateMachine>(tm.sm);

        auto* comp = pNode->GetComponent<SE::StateMachine>();
        ASSERT_NE(comp, nullptr);

        comp->Enable();

        auto& em = SE::GetSystem<SE::EventManager>();
        EXPECT_NO_FATAL_FAILURE(em.TriggerEvent(SE::EUpdate{0.016f}));

        comp->Disable();

        // After disable, EUpdate no longer processes this component — no crash
        EXPECT_NO_FATAL_FAILURE(em.TriggerEvent(SE::EUpdate{0.016f}));

        // Destroying the node after disable is safe
        pNode->DestroyComponent<SE::StateMachine>();
        EXPECT_NO_FATAL_FAILURE(em.TriggerEvent(SE::EUpdate{0.016f}));
}
