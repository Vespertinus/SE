
// ---------------------------------------------------------------------------
// Tier B functional tests — animation events (EAnimEvent) and root motion.
//
// EAnimEvent is fired synchronously inside AnimGraphInstance::Update via
// EventManager::TriggerEvent for the ROOT clip of the CURRENT state only.
// ---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <glm/glm.hpp>

#define SE_IMPL
#include <EngineFixture.h>
#include <FrameRunner.h>
#include <PoseAssert.h>

namespace {

using namespace se_test;

struct EventListener {
        int count = 0;
        std::vector<std::string> vNames;
        std::vector<float> vValues;
        SE::StrID last_state;

        void OnAnimEvent(const SE::Event& oEvent) {
                const SE::EAnimEvent& oEvt = oEvent.Get<SE::EAnimEvent>();
                ++count;
                vNames.emplace_back(oEvt.name);
                vValues.push_back(oEvt.value);
                last_state = oEvt.state_name;
        }
};

// RAII listener registration (the EventManager outlives individual tests).
struct ScopedListener {
        EventListener listener;

        ScopedListener() {
                SE::GetSystem<SE::EventManager>()
                        .template AddListener<SE::EAnimEvent, &EventListener::OnAnimEvent>(
                                        &listener);
        }
        ~ScopedListener() {
                SE::GetSystem<SE::EventManager>()
                        .template RemoveListener<SE::EAnimEvent, &EventListener::OnAnimEvent>(
                                        &listener);
        }
};

// Keeps the graph resource alive while an instance is in use.
struct InstanceRig {
        flatbuffers::FlatBufferBuilder oBuilder;
        std::unique_ptr<SE::AnimGraph> pGraph;
        SE::AnimGraphInstance          inst;

        explicit InstanceRig(const GraphPtr& pNative) {
                pGraph = std::make_unique<SE::AnimGraph>(
                                "events_graph", 0, SerializeGraph(oBuilder, *pNative));
                inst.Init(*pGraph);
        }
};

// 1 s looping clip on bone 1 with events at [0.95 ("tail"), 0.05 ("head")].
ClipPtr MakeWrapEventClip() {
        auto pClip = MakeClip(1.0f, true);
        AddRamp(*pClip, 1, 0, 1.0f, 0.0f, 1.0f);
        AddEvent(*pClip, 0.95f, "tail", 1.0f);
        AddEvent(*pClip, 0.05f, "head", 2.0f);
        return pClip;
}

// ===========================================================================
// Event firing
// ===========================================================================

TEST_F(FuncTestBase, AnimEventTest_FiresOnceWhenTimeCrossesEvent) {
        auto pClip = MakeClip(1.0f, true);
        AddEvent(*pClip, 0.5f, "hit", 3.5f);
        InstanceRig oRig(MakeSingleStateGraph("a", "ev_a", std::move(pClip)));

        ScopedListener oScoped;
        for (int i = 0; i < 12; ++i) {   // 0 -> 0.6 s
                oRig.inst.Update(0.05f);
        }
        EXPECT_EQ(oScoped.listener.count, 1);
        EXPECT_EQ(oScoped.listener.vNames[0], "hit");
        EXPECT_FLOAT_EQ(oScoped.listener.vValues[0], 3.5f);
}

TEST_F(FuncTestBase, AnimEventTest_DoesNotFireBeforeEventTime) {
        auto pClip = MakeClip(1.0f, true);
        AddEvent(*pClip, 0.5f, "hit", 1.0f);
        InstanceRig oRig(MakeSingleStateGraph("a", "ev_a", std::move(pClip)));

        ScopedListener oScoped;
        for (int i = 0; i < 8; ++i) {   // 0 -> 0.4 s
                oRig.inst.Update(0.05f);
        }
        EXPECT_EQ(oScoped.listener.count, 0);
}

TEST_F(FuncTestBase, AnimEventTest_StateSpeedScalesEventTimes) {
        // Events live in clip-time; the state clock runs at speed 2 — the clip
        // crosses clip-time 0.5 after 0.25 s of real time (5 updates of 0.05).
        auto pClip = MakeClip(1.0f, true);
        AddEvent(*pClip, 0.5f, "hit", 1.0f);
        auto pGraph = MakeSingleStateGraph("a", "ev_sp", std::move(pClip));
        pGraph->states[0]->speed = 2.0f;
        InstanceRig oRig(pGraph);

        ScopedListener oScoped;
        for (int i = 0; i < 4; ++i) {   // 0 -> 0.2 s: clip-time 0.4 — not yet
                oRig.inst.Update(0.05f);
        }
        EXPECT_EQ(oScoped.listener.count, 0);

        oRig.inst.Update(0.05f);        // clip-time 0.4 -> 0.6: fires once
        EXPECT_EQ(oScoped.listener.count, 1);
        EXPECT_EQ(oScoped.listener.vNames[0], "hit");
        oRig.inst.Update(0.05f);
        EXPECT_EQ(oScoped.listener.count, 1);   // no refire
}

TEST_F(FuncTestBase, AnimEventTest_LoopWrapFiresTailAndHeadExactlyOnce) {
        // The practical guarantee: every crossing fires exactly once, and the
        // tail-before-head order holds across the loop boundary. 30 steps of
        // 0.05 s over the 1 s clip = 1.5 loops, so head fires on each lap.
        InstanceRig oRig(MakeSingleStateGraph("a", "ev_a", MakeWrapEventClip()));

        ScopedListener oScoped;
        for (int i = 0; i < 30; ++i) {   // 0 -> 1.5 s
                oRig.inst.Update(0.05f);
        }
        ASSERT_EQ(oScoped.listener.count, 3u);
        EXPECT_EQ(oScoped.listener.vNames[0], "head");   // lap 1
        EXPECT_EQ(oScoped.listener.vNames[1], "tail");   // boundary
        EXPECT_EQ(oScoped.listener.vNames[2], "head");   // lap 2
}

TEST_F(FuncTestBase, AnimEventTest_NonLoopingClampDoesNotRefire) {
        // After clamping at the clip end, prev/cur stop advancing and no event
        // can re-fire on later Updates.
        auto pClip = MakeClip(1.0f, false);
        AddEvent(*pClip, 0.95f, "tail", 1.0f);
        InstanceRig oRig(MakeSingleStateGraph("a", "ev_a", std::move(pClip)));

        ScopedListener oScoped;
        for (int i = 0; i < 40; ++i) {   // far past the 1 s end
                oRig.inst.Update(0.05f);
        }
        EXPECT_EQ(oScoped.listener.count, 1);
        EXPECT_EQ(oScoped.listener.vNames[0], "tail");
}

TEST_F(FuncTestBase, AnimEventTest_EventCarriesNameIdAndStateName) {
        auto pClip = MakeClip(1.0f, true);
        AddEvent(*pClip, 0.2f, "footstep", 7.0f);
        InstanceRig oRig(MakeSingleStateGraph("state_a", "ev_a", std::move(pClip)));

        ScopedListener oScoped;
        for (int i = 0; i < 5; ++i) {   // 0 -> 0.25 s: crosses the 0.2 event
                oRig.inst.Update(0.05f);
        }

        ASSERT_EQ(oScoped.listener.count, 1);
        EXPECT_EQ(oScoped.listener.vNames[0], "footstep");
        EXPECT_TRUE(oScoped.listener.last_state == SE::StrID("state_a"));
        // name_id is the pre-hashed StrID of the event name
        // (checked indirectly through count/name above; StrID hashes are not
        // stable across stdlib versions so we never assert raw values).
}

TEST_F(FuncTestBase, AnimEventTest_OnlyRootClipNodeFiresEvents) {
        // Events on a Blend1D CHILD clip never fire: Update only inspects the
        // state's root node (index 0) when it is a ClipNodeData.
        std::vector<SE::FlatBuffers::BlendTreeNodeT> vNodes;
        vNodes.push_back(MakeBlend1DNode("speed", {0.0f, 1.0f}, {}));   // placeholder root
        auto pChild = MakeClip(1.0f, true);
        AddEvent(*pChild, 0.5f, "child_event", 1.0f);
        vNodes.push_back(MakeClipNode(MakeClipLeaf("ev_child", std::move(pChild))));
        auto pChild2 = MakeClip(1.0f, true);
        AddChannel(*pChild2, 1, 0, {0.0f, 1.0f}, {1.0f, 1.0f},
                   SE::FlatBuffers::CurveFormat::LinearF32);
        vNodes.push_back(MakeClipNode(MakeClipLeaf("ev_child2", std::move(pChild2))));
        vNodes[0] = MakeBlend1DNode("speed", {0.0f, 1.0f}, {1, 2});

        InstanceRig oRig(MakeGraph("loco",
                {MakeState("loco", std::move(vNodes))}, {},
                {MakeParam("speed", SE::FlatBuffers::AnimParamType::Float)}));

        ScopedListener oScoped;
        for (int i = 0; i < 12; ++i) {
                oRig.inst.Update(0.05f);
        }
        EXPECT_EQ(oScoped.listener.count, 0);
}

TEST_F(FuncTestBase, AnimEventTest_DuringCrossfadeOnlySourceStateFires) {
        // A and B both carry an event at t=0.05 of their clips. The transition
        // A->B arms on frame 1; B is blended but only A's (source) events fire.
        auto pClipA = MakeClip(10.0f, true);
        AddEvent(*pClipA, 0.07f, "from_a", 1.0f);
        auto pClipB = MakeClip(10.0f, true);
        AddEvent(*pClipB, 0.07f, "from_b", 2.0f);

        InstanceRig oRig(MakeTwoStateGraph("a", "b",
                std::move(pClipA), std::move(pClipB), 0.2f));

        ScopedListener oScoped;
        oRig.inst.Update(0.05f);   // arm; A time 0.05
        oRig.inst.Update(0.05f);   // A time 0.1 -> crosses A's 0.07
        oRig.inst.Update(0.05f);   // still transitioning (progress 0.75)

        ASSERT_EQ(oScoped.listener.count, 1);
        EXPECT_EQ(oScoped.listener.vNames[0], "from_a");
        EXPECT_TRUE(oScoped.listener.last_state == SE::StrID("a"));
}

// ===========================================================================
// Root motion
// ===========================================================================

// Root (bone 0) pos-x ramp 0 -> 1 over 1 s.
ClipPtr MakeRootMotionClip(float duration = 1.0f) {
        auto pClip = MakeClip(duration, true);
        AddRamp(*pClip, 0, 0, duration, 0.0f, 1.0f);
        return pClip;
}

TEST_F(FuncTestBase, RootMotionTest_DeltaZeroWhenDisabled) {
        InstanceRig oRig(MakeSingleStateGraph("a", "rm_a", MakeRootMotionClip()));

        oRig.inst.Update(0.1f);
        const SE::RootMotionDelta& oDelta = oRig.inst.GetRootMotionDelta();
        EXPECT_NEAR(oDelta.translation.x, 0.0f, kPoseEps);
        EXPECT_NEAR(oDelta.translation.z, 0.0f, kPoseEps);
}

TEST_F(FuncTestBase, RootMotionTest_TranslationDeltaMatchesClipSlope) {
        InstanceRig oRig(MakeSingleStateGraph("a", "rm_a", MakeRootMotionClip()));
        oRig.inst.SetUseRootMotion(true);

        oRig.inst.Update(0.1f);
        EXPECT_NEAR(oRig.inst.GetRootMotionDelta().translation.x, 0.1f, 1e-4f);

        // CHARACTERIZATION: the default RootMotionConfig extracts XZ only —
        // translation.y stays 0 even for clips that move vertically.
        EXPECT_NEAR(oRig.inst.GetRootMotionDelta().translation.y, 0.0f, kPoseEps);
}

TEST_F(FuncTestBase, RootMotionTest_RotationDeltaNonZeroForRotatingRoot) {
        // Root rotates 0 -> 90 deg Z over 1 s (unit quaternion channels).
        auto pClip = MakeClip(1.0f, true);
        const float k = 0.70710678f;
        AddChannel(*pClip, 0, 5, {0.0f, 1.0f}, {0.0f, k});   // rot z
        AddChannel(*pClip, 0, 6, {0.0f, 1.0f}, {1.0f, k});   // rot w
        InstanceRig oRig(MakeSingleStateGraph("a", "rm_rot", std::move(pClip)));
        oRig.inst.SetUseRootMotion(true);

        oRig.inst.Update(0.5f);   // half the arc: 45 deg
        const glm::quat q = oRig.inst.GetRootMotionDelta().rotation;
        const glm::quat qZ45 = glm::angleAxis(glm::quarter_pi<float>(), glm::vec3(0, 0, 1));
        EXPECT_QUAT_NEAR(q, qZ45);
}

TEST_F(FuncTestBase, RootMotionTest_DeltaResetsEachUpdate) {
        InstanceRig oRig(MakeSingleStateGraph("a", "rm_a", MakeRootMotionClip()));
        oRig.inst.SetUseRootMotion(true);

        oRig.inst.Update(0.1f);
        EXPECT_NEAR(oRig.inst.GetRootMotionDelta().translation.x, 0.1f, 1e-4f);
        oRig.inst.Update(0.05f);
        EXPECT_NEAR(oRig.inst.GetRootMotionDelta().translation.x, 0.05f, 1e-4f);
}

TEST_F(FuncTestBase, RootMotionTest_WrapStepUsesClampedSampling) {
        // CHARACTERIZATION: the loop-wrap branch of extractRootMotionDelta only
        // triggers when curTime < prevTime, which Update's pre-wrap clock never
        // produces — a step spanning the boundary samples the clamped end
        // (1.05 -> ramp value 1.0), so the last partial step reads 0.05.
        InstanceRig oRig(MakeSingleStateGraph("a", "rm_a", MakeRootMotionClip()));
        oRig.inst.SetUseRootMotion(true);

        for (int i = 0; i < 19; ++i) oRig.inst.Update(0.05f);   // t = 0.95
        oRig.inst.Update(0.1f);                                  // spans the wrap
        EXPECT_NEAR(oRig.inst.GetRootMotionDelta().translation.x, 0.05f, 1e-3f);
}

} // namespace
