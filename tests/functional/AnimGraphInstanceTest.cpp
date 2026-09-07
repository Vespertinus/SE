
// ---------------------------------------------------------------------------
// Tier B functional tests — AnimGraphInstance state machine.
//
// Real engine composition (Config + EventManager + FrameAllocator, real
// TResourceManager). Graphs and skeletons are stack-built in-memory resources;
// inline clip data referenced by the graph is registered into the real
// resource manager by AnimGraphInstance::Init exactly as in production.
// ---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <glm/glm.hpp>

// Compiles the engine SE_IMPL blocks (.tcc impls) into this TU — required
// before EngineFixture.h, exactly like the existing unit-test pattern.
#define SE_IMPL
#include <EngineFixture.h>
#include <FrameRunner.h>
#include <PoseAssert.h>
#include <LogCapture.h>

namespace {

using namespace se_test;

// A stack-built in-memory AnimGraph. The inline ctor re-serializes into its
// own storage, so the native graph and builder may die after construction.
struct GraphRig {
        flatbuffers::FlatBufferBuilder  oBuilder;
        std::unique_ptr<SE::AnimGraph>  pGraph;

        explicit GraphRig(const GraphPtr& pNative)
                : pGraph(std::make_unique<SE::AnimGraph>(
                                "test_graph", 0, SerializeGraph(oBuilder, *pNative))) {}
};

// Skeleton fixture (3-bone chain by default).
struct SkelRig {
        SkelPtr                        pNative;
        flatbuffers::FlatBufferBuilder oBuilder;
        const SE::FlatBuffers::Skeleton* pFb = nullptr;
        SE::Skeleton                   oSkel;

        explicit SkelRig(uint16_t n_bones = 3)
                : pNative(MakeChainSkeleton(n_bones)),
                  pFb(SerializeSkeleton(oBuilder, *pNative)),
                  oSkel("graph_test_skel", 0, pFb) {}
};

// pos-x ramp 0 -> 1 over the clip duration on the given bone.
ClipPtr MakeRampClip(float duration, bool looping, uint16_t bone = 0) {
        auto pClip = MakeClip(duration, looping);
        AddRamp(*pClip, bone, 0, duration, 0.0f, 1.0f);
        return pClip;
}

// ===========================================================================
// Init
// ===========================================================================

TEST_F(FuncTestBase, GraphInitTest_EntryStateAndDefaultsLoaded) {
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("gi_a", MakeClip(1.0f, true)))})},
                {},
                {MakeParam("speed", SE::FlatBuffers::AnimParamType::Float, 1.5f)}));

        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        EXPECT_TRUE(oInst.CurrentStateName() == SE::StrID("a"));
        EXPECT_NEAR(oInst.GetCurrentTime(), 0.0f, kTimeEps);
        EXPECT_FALSE(oInst.IsTransitioning());
        EXPECT_FLOAT_EQ(oInst.GetFloat(SE::StrID("speed")), 1.5f);
}

TEST_F(FuncTestBase, GraphInitTest_ParamsRegisteredWithDefaults) {
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("gi_a", MakeClip(1.0f, true)))})},
                {},
                {MakeParam("speed", SE::FlatBuffers::AnimParamType::Float, 0.75f),
                 MakeParam("is_grounded", SE::FlatBuffers::AnimParamType::Bool, 0.0f, true),
                 MakeParam("state", SE::FlatBuffers::AnimParamType::Int, 0.0f, false, 3),
                 MakeParam("jump", SE::FlatBuffers::AnimParamType::Trigger)}));

        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        std::vector<SE::ParamInfo> vParams;
        oInst.GetParams(vParams);
        ASSERT_EQ(vParams.size(), 4u);

        EXPECT_FLOAT_EQ(oInst.GetFloat(SE::StrID("speed")), 0.75f);
        EXPECT_EQ(oInst.GetBool(SE::StrID("is_grounded")), true);
        EXPECT_EQ(oInst.GetInt(SE::StrID("state")), 3);
}

TEST_F(FuncTestBase, GraphInitTest_TriggerParamRegisteredAsFloatSlot) {
        // CHARACTERIZATION: Init registers Trigger params as Float entries with
        // value 0 (ConsumeTrigger checks entry.triggered, not entry.type), so a
        // freshly initialized trigger reads back as type Float / inactive.
        // Changing this is fine — update the test with the fix.
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("gi_a", MakeClip(1.0f, true)))})},
                {},
                {MakeParam("jump", SE::FlatBuffers::AnimParamType::Trigger)}));

        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        std::vector<SE::ParamInfo> vParams;
        oInst.GetParams(vParams);
        ASSERT_EQ(vParams.size(), 1u);
        EXPECT_EQ(vParams[0].type, SE::AnimParamStore::Type::Float);
        EXPECT_FALSE(oInst.Params().PeekTrigger(SE::StrID("jump")));
}

TEST_F(FuncTestBase, GraphInitTest_EntryStateMissingFromGraphIsInert) {
        // Entry name that no state defines: the state machine has nothing to
        // tick, and blend evaluation leaves the bind pose untouched.
        GraphRig oGraphRig(MakeGraph("ghost",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("gi_a", MakeRampClip(1.0f, true)))})}));

        SkelRig oSkelRig;
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);
        EXPECT_TRUE(oInst.CurrentStateName() == SE::StrID("ghost"));

        oInst.Update(0.1f);
        SE::LocalPose oPose = EvalFrame(oInst, oSkelRig.oSkel, Alloc(), 0.1f);
        EXPECT_POSE_NEAR_BIND(oPose, oSkelRig.oSkel);
        Alloc().reset();
}

TEST_F(FuncTestBase, GraphInitTest_ClipLoadFailureLogsAndContinuesWithBindPose) {
        // Inline holder with a name but no clip data -> Init logs an error and
        // stores an invalid handle; evaluation then keeps the bind pose.
        auto pGraphData = std::make_unique<FbGraph>();
        pGraphData->entry_state = "a";
        SE::FlatBuffers::ClipNodeDataT oData;
        auto pHolder = std::make_unique<SE::FlatBuffers::AnimClipHolderT>();
        pHolder->name = "ghost_clip";     // no clip attached
        oData.clip = std::move(pHolder);
        auto pState = std::make_unique<SE::FlatBuffers::AnimStateT>();
        pState->id = "a";
        pState->nodes.push_back(std::make_unique<SE::FlatBuffers::BlendTreeNodeT>());
        pState->nodes[0]->data.Set(std::move(oData));
        pGraphData->states.push_back(std::move(pState));
        GraphRig oGraphRig(std::move(pGraphData));

        SkelRig oSkelRig;
        SE::AnimGraphInstance oInst;
        {
                LogCapture oCapture;
                oInst.Init(*oGraphRig.pGraph);
                oCapture.ExpectLine("failed to create AnimClip resource");
        }

        // State ticks, but sampling has no clip: bind pose out, no crash.
        oInst.Update(0.1f);
        SE::LocalPose oPose = EvalFrame(oInst, oSkelRig.oSkel, Alloc(), 0.1f);
        EXPECT_POSE_NEAR_BIND(oPose, oSkelRig.oSkel);
        Alloc().reset();
}

// ===========================================================================
// Parameters
// ===========================================================================

TEST_F(FuncTestBase, GraphParamTest_SettersRoundTrip) {
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("gp_a", MakeClip(1.0f, true)))})}));

        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        oInst.SetFloat(SE::StrID("speed"), 2.5f);
        oInst.SetBool(SE::StrID("grounded"), true);
        oInst.SetInt(SE::StrID("stance"), 7);
        EXPECT_FLOAT_EQ(oInst.GetFloat(SE::StrID("speed")), 2.5f);
        EXPECT_EQ(oInst.GetBool(SE::StrID("grounded")), true);
        EXPECT_EQ(oInst.GetInt(SE::StrID("stance")), 7);
}

TEST_F(FuncTestBase, GraphParamTest_MissingParamsReadAsZeroDefaults) {
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("gp_a", MakeClip(1.0f, true)))})}));

        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        EXPECT_FLOAT_EQ(oInst.GetFloat(SE::StrID("nope")), 0.0f);
        EXPECT_EQ(oInst.GetBool(SE::StrID("nope")), false);
        EXPECT_EQ(oInst.GetInt(SE::StrID("nope")), 0);
        EXPECT_FALSE(oInst.Params().PeekTrigger(SE::StrID("nope")));
}

TEST_F(FuncTestBase, GraphParamTest_ConsumeTriggerFiresOnce) {
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("gp_a", MakeClip(1.0f, true)))})}));

        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        oInst.SetTrigger(SE::StrID("jump"));
        EXPECT_TRUE(oInst.Params().PeekTrigger(SE::StrID("jump")));
        EXPECT_TRUE(oInst.Params().ConsumeTrigger(SE::StrID("jump")));
        EXPECT_FALSE(oInst.Params().ConsumeTrigger(SE::StrID("jump")));
}

TEST_F(FuncTestBase, GraphParamTest_TriggersExpireAfterOneUpdate) {
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("gp_a", MakeClip(1.0f, true)))})}));

        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        oInst.SetTrigger(SE::StrID("jump"));
        oInst.Update(0.1f);   // no matching transition -> edge event dropped
        EXPECT_FALSE(oInst.Params().PeekTrigger(SE::StrID("jump")));
}

// ===========================================================================
// State time
// ===========================================================================

TEST_F(FuncTestBase, GraphStateTest_TimeAdvancesByDt) {
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("gs_a", MakeClip(10.0f, true)))})}));

        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);
        for (int i = 0; i < 5; ++i) oInst.Update(0.05f);
        EXPECT_NEAR(oInst.GetCurrentTime(), 0.25f, kTimeEps);
}

TEST_F(FuncTestBase, GraphStateTest_LoopingStateWrapsAtRootClipDuration) {
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("gs_a", MakeClip(2.0f, true)))})}));

        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);
        for (int i = 0; i < 50; ++i) oInst.Update(0.05f);   // 2.5 s total
        EXPECT_NEAR(oInst.GetCurrentTime(), 0.5f, kTimeEps);
}

TEST_F(FuncTestBase, GraphStateTest_NonLoopingStateClampsAtDuration) {
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("gs_a", MakeClip(1.0f, false)))})}));

        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);
        for (int i = 0; i < 50; ++i) oInst.Update(0.05f);
        EXPECT_NEAR(oInst.GetCurrentTime(), 1.0f, kTimeEps);
}

TEST_F(FuncTestBase, GraphStateTest_PauseMakesUpdateNoOp) {
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("gs_a", MakeClip(10.0f, true)))})}));

        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);
        oInst.Update(0.1f);
        oInst.SetPaused(true);
        EXPECT_TRUE(oInst.IsPaused());
        oInst.Update(0.1f);
        oInst.Update(0.1f);
        EXPECT_NEAR(oInst.GetCurrentTime(), 0.1f, kTimeEps);
        oInst.SetPaused(false);
        oInst.Update(0.1f);
        EXPECT_NEAR(oInst.GetCurrentTime(), 0.2f, kTimeEps);
}

TEST_F(FuncTestBase, GraphStateTest_PlaybackRateScalesSamplingAndDuration) {
        // Clip pos-x 0->1 over 1 s on bone 0, played at rate 2:
        // state wraps at 0.5 s and the sample time runs at 2x wall time.
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("gs_a", MakeRampClip(1.0f, true), 2.0f))})}));

        SkelRig oSkelRig;
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        oInst.Update(0.25f);   // current_time 0.25, sampled clip time 0.5
        SE::LocalPose oPose = EvalFrame(oInst, oSkelRig.oSkel, Alloc(), 0.0f);
        EXPECT_NEAR(oPose.pPos[0].x, 0.5f, kPoseEps);
        Alloc().reset();

        for (int i = 0; i < 2; ++i) oInst.Update(0.25f);   // current_time 0.75 -> wraps to 0.25
        EXPECT_NEAR(oInst.GetCurrentTime(), 0.25f, kTimeEps);
}

// ===========================================================================
// Transitions
// ===========================================================================

TEST_F(FuncTestBase, GraphTransTest_UnconditionalTransitionStartsImmediately) {
        // CHARACTERIZATION: the transition is *armed* at the end of the Update
        // that matches it — progress is 0 on that frame and first advances on
        // the NEXT Update.
        GraphRig oGraphRig(MakeTwoStateGraph("a", "b",
                MakeClip(1.0f, true), MakeClip(1.0f, true), 0.2f));

        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);
        oInst.Update(0.05f);
        EXPECT_TRUE(oInst.IsTransitioning());
        EXPECT_TRUE(oInst.TransitionTargetName() == SE::StrID("b"));
        EXPECT_TRUE(oInst.CurrentStateName() == SE::StrID("a"));
        EXPECT_NEAR(oInst.TransitionProgress(), 0.0f, kTimeEps);

        oInst.Update(0.05f);
        EXPECT_NEAR(oInst.TransitionProgress(), 0.05f / 0.2f, kTimeEps);
}

TEST_F(FuncTestBase, GraphTransTest_CrossfadeProgressAdvancesLinearlyThenCompletes) {
        GraphRig oGraphRig(MakeTwoStateGraph("a", "b",
                MakeClip(10.0f, true), MakeClip(1.0f, true), 0.4f));

        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);
        oInst.Update(0.04f);   // armed; progress still 0
        EXPECT_NEAR(oInst.TransitionProgress(), 0.0f, kTimeEps);
        oInst.Update(0.04f);
        EXPECT_NEAR(oInst.TransitionProgress(), 0.1f, kTimeEps);
        oInst.Update(0.04f);
        EXPECT_NEAR(oInst.TransitionProgress(), 0.2f, kTimeEps);

        for (int i = 0; i < 8; ++i) oInst.Update(0.04f);   // reaches 1.0 and completes
        EXPECT_FALSE(oInst.IsTransitioning());
        EXPECT_TRUE(oInst.CurrentStateName() == SE::StrID("b"));
}

TEST_F(FuncTestBase, GraphTransTest_OnCompletionTimeComesFromTransitionTime) {
        // CHARACTERIZATION: the destination state's clock is transition_time
        // (started at 0 when the crossfade armed), NOT the source state's time
        // — after two 1 s steps the destination is at 1.0, while a continued
        // source clock would read 2.0.
        GraphRig oGraphRig(MakeTwoStateGraph("a", "b",
                MakeClip(10.0f, true), MakeClip(10.0f, true), 0.2f));

        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);
        oInst.Update(1.0f);   // transition armed
        oInst.Update(1.0f);   // transition_time 1.0, progress 5 -> completes
        EXPECT_FALSE(oInst.IsTransitioning());
        EXPECT_NEAR(oInst.GetCurrentTime(), 1.0f, kTimeEps);
}

TEST_F(FuncTestBase, GraphTransTest_GreaterLessConditionOps) {
        // A->B requires speed > 0.5
        GraphRig oGraphRig(MakeTwoStateGraph("a", "b",
                MakeClip(1.0f, true), MakeClip(1.0f, true), 0.2f,
                {MakeCondition("speed", SE::FlatBuffers::ConditionOp::Greater, 0.5f)}));

        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        oInst.SetFloat(SE::StrID("speed"), 0.5f);
        oInst.Update(0.05f);
        EXPECT_FALSE(oInst.IsTransitioning());   // strictly greater

        oInst.SetFloat(SE::StrID("speed"), 0.6f);
        oInst.Update(0.05f);
        EXPECT_TRUE(oInst.IsTransitioning());
}

TEST_F(FuncTestBase, GraphTransTest_EqualNotEqualUseEpsilon) {
        // threshold 0.5, eps 1e-4
        GraphRig oGraphRig(MakeTwoStateGraph("a", "b",
                MakeClip(1.0f, true), MakeClip(1.0f, true), 0.2f,
                {MakeCondition("speed", SE::FlatBuffers::ConditionOp::Equal, 0.5f)}));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        oInst.SetFloat(SE::StrID("speed"), 0.50005f);   // within 1e-4
        oInst.Update(0.05f);
        EXPECT_TRUE(oInst.IsTransitioning());

        GraphRig oGraphRig2(MakeTwoStateGraph("a", "b",
                MakeClip(1.0f, true), MakeClip(1.0f, true), 0.2f,
                {MakeCondition("speed", SE::FlatBuffers::ConditionOp::NotEqual, 0.5f)}));
        SE::AnimGraphInstance oInst2;
        oInst2.Init(*oGraphRig2.pGraph);
        oInst2.SetFloat(SE::StrID("speed"), 0.5f);
        oInst2.Update(0.05f);
        EXPECT_FALSE(oInst2.IsTransitioning());
}

TEST_F(FuncTestBase, GraphTransTest_BoolConditionOpsGateBothWays) {
        GraphRig oGraphRig(MakeTwoStateGraph("a", "b",
                MakeClip(1.0f, true), MakeClip(1.0f, true), 0.2f,
                {MakeCondition("is_go", SE::FlatBuffers::ConditionOp::IsTrue)}));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        oInst.Update(0.05f);
        EXPECT_FALSE(oInst.IsTransitioning());
        oInst.SetBool(SE::StrID("is_go"), true);
        oInst.Update(0.05f);
        EXPECT_TRUE(oInst.IsTransitioning());

        GraphRig oGraphRig2(MakeTwoStateGraph("a", "b",
                MakeClip(1.0f, true), MakeClip(1.0f, true), 0.2f,
                {MakeCondition("is_stay", SE::FlatBuffers::ConditionOp::IsFalse)}));
        SE::AnimGraphInstance oInst2;
        oInst2.Init(*oGraphRig2.pGraph);
        oInst2.SetBool(SE::StrID("is_stay"), false);
        oInst2.Update(0.05f);
        EXPECT_TRUE(oInst2.IsTransitioning());   // IsFalse passes when the flag is down
}

TEST_F(FuncTestBase, GraphTransTest_AllConditionsMustPass) {
        GraphRig oGraphRig(MakeTwoStateGraph("a", "b",
                MakeClip(1.0f, true), MakeClip(1.0f, true), 0.2f,
                {MakeCondition("speed", SE::FlatBuffers::ConditionOp::Greater, 0.1f),
                 MakeCondition("is_go", SE::FlatBuffers::ConditionOp::IsTrue)}));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        oInst.SetFloat(SE::StrID("speed"), 1.0f);   // second condition fails
        oInst.Update(0.05f);
        EXPECT_FALSE(oInst.IsTransitioning());

        oInst.SetBool(SE::StrID("is_go"), true);
        oInst.Update(0.05f);
        EXPECT_TRUE(oInst.IsTransitioning());
}

TEST_F(FuncTestBase, GraphTransTest_FailedSiblingKeepsTriggerWithinSamePass) {
        // CHARACTERIZATION: condition peeking is non-destructive *within* one
        // Update pass — a failing sibling in transition A does not consume the
        // trigger, so a later-declared transition B can still take it in the
        // same pass. Across passes the semantics are edge events: Update ends
        // with ExpireTriggers, so a trigger never survives into the next frame.
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("gt_a", MakeClip(10.0f, true)))}),
                 MakeState("b", {MakeClipNode(MakeClipLeaf("gt_b", MakeClip(10.0f, true)))}),
                 MakeState("c", {MakeClipNode(MakeClipLeaf("gt_c", MakeClip(10.0f, true)))})},
                {MakeTransition("a", "b", 0.1f,
                                {MakeCondition("jump", SE::FlatBuffers::ConditionOp::Triggered),
                                 MakeCondition("speed", SE::FlatBuffers::ConditionOp::Greater, 0.9f)}),
                 MakeTransition("a", "c", 0.1f,
                                {MakeCondition("jump", SE::FlatBuffers::ConditionOp::Triggered)})}));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        oInst.SetFloat(SE::StrID("speed"), 0.0f);   // fails A->B's sibling condition
        oInst.SetTrigger(SE::StrID("jump"));
        oInst.Update(0.05f);
        EXPECT_TRUE(oInst.TransitionTargetName() == SE::StrID("c"));   // A->C took the trigger
        EXPECT_FALSE(oInst.Params().PeekTrigger(SE::StrID("jump")));
        // (Expiry of an entirely unmatched trigger is covered in
        // GraphParamTest_TriggersExpireAfterOneUpdate.)
}

TEST_F(FuncTestBase, GraphTransTest_TriggerConsumedWhenTransitionTaken) {
        GraphRig oGraphRig(MakeTwoStateGraph("a", "b",
                MakeClip(1.0f, true), MakeClip(1.0f, true), 0.2f,
                {MakeCondition("jump", SE::FlatBuffers::ConditionOp::Triggered)}));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        oInst.SetTrigger(SE::StrID("jump"));
        oInst.Update(0.05f);
        EXPECT_TRUE(oInst.IsTransitioning());
        EXPECT_FALSE(oInst.Params().PeekTrigger(SE::StrID("jump")));
        EXPECT_FALSE(oInst.Params().ConsumeTrigger(SE::StrID("jump")));
}

TEST_F(FuncTestBase, GraphTransTest_ExitTimeBlocksUntilNormalizedTimeReached) {
        GraphRig oGraphRig(MakeTwoStateGraph("a", "b",
                MakeClip(1.0f, true), MakeClip(1.0f, true), 0.2f,
                {}, /*has_exit_time=*/true, /*exit_time=*/0.5f));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        for (int i = 0; i < 8; ++i) oInst.Update(0.05f);   // t = 0.4
        EXPECT_FALSE(oInst.IsTransitioning());
        for (int i = 0; i < 3; ++i) oInst.Update(0.05f);   // t = 0.55
        EXPECT_TRUE(oInst.IsTransitioning());
}

TEST_F(FuncTestBase, GraphTransTest_AdditiveRootExitTimeFollowsBaseDuration) {
        // An Additive root must report its BASE child's duration: the exit-time
        // gate (and clock wrap) follow the continuing motion. With duration 0
        // the gate read normalized progress 1.0 and the state exited on the
        // very next Update — the hit_reaction state could never play.
        auto pBase = MakeClip(1.0f, true);
        auto pAdd  = MakeClip(10.0f, false);
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeAdditiveNode(1, 2, 1.0f, ""),
                                 MakeClipNode(MakeClipLeaf("ar_base", std::move(pBase))),
                                 MakeClipNode(MakeClipLeaf("ar_add", std::move(pAdd)))}),
                 MakeState("b", {MakeClipNode(MakeClipLeaf("ar_b", MakeClip(10.0f, true)))})},
                {MakeTransition("a", "b", 0.2f, {},
                                /*has_exit_time=*/true, /*exit_time=*/0.7f)}));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        for (int i = 0; i < 12; ++i) oInst.Update(0.05f);   // t = 0.6 < 0.7 * base(1.0)
        EXPECT_FALSE(oInst.IsTransitioning());
        for (int i = 0; i < 3; ++i) oInst.Update(0.05f);    // t = 0.75 >= 0.7
        EXPECT_TRUE(oInst.IsTransitioning());
}

TEST_F(FuncTestBase, GraphTransTest_CanInterruptFalseIsIgnoredMidFlight) {
        // A->B (0.4s) in flight; A->C holds but cannot interrupt.
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("gt_a", MakeClip(10.0f, true)))}),
                 MakeState("b", {MakeClipNode(MakeClipLeaf("gt_b", MakeClip(10.0f, true)))}),
                 MakeState("c", {MakeClipNode(MakeClipLeaf("gt_c", MakeClip(10.0f, true)))})},
                {MakeTransition("a", "b", 0.4f),
                 MakeTransition("a", "c", 0.1f,
                                {MakeCondition("go_c", SE::FlatBuffers::ConditionOp::IsTrue)},
                                false, 1.0f, /*can_interrupt=*/false)}));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        oInst.Update(0.05f);
        ASSERT_TRUE(oInst.IsTransitioning());
        EXPECT_TRUE(oInst.TransitionTargetName() == SE::StrID("b"));

        oInst.SetBool(SE::StrID("go_c"), true);
        oInst.Update(0.05f);   // second frame of A->B: progress 0.05/0.4
        EXPECT_TRUE(oInst.TransitionTargetName() == SE::StrID("b"));
        EXPECT_NEAR(oInst.TransitionProgress(), 0.05f / 0.4f, kTimeEps);
}

TEST_F(FuncTestBase, GraphTransTest_CanInterruptTrueReplacesInFlightTransition) {
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("gt_a", MakeClip(10.0f, true)))}),
                 MakeState("b", {MakeClipNode(MakeClipLeaf("gt_b", MakeClip(10.0f, true)))}),
                 MakeState("c", {MakeClipNode(MakeClipLeaf("gt_c", MakeClip(10.0f, true)))})},
                {MakeTransition("a", "b", 0.4f),
                 MakeTransition("a", "c", 0.1f,
                                {MakeCondition("go_c", SE::FlatBuffers::ConditionOp::IsTrue)},
                                false, 1.0f, /*can_interrupt=*/true)}));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        oInst.Update(0.05f);
        ASSERT_TRUE(oInst.IsTransitioning());

        oInst.SetBool(SE::StrID("go_c"), true);
        oInst.Update(0.05f);   // A->C replaces A->B and restarts its clock
        EXPECT_TRUE(oInst.TransitionTargetName() == SE::StrID("c"));
        EXPECT_NEAR(oInst.TransitionProgress(), 0.0f, kTimeEps);   // restarted

        // While A->C's conditions keep passing, the in-flight A->C transition is
        // NOT re-taken: re-arming the identical source->destination transition
        // every frame would reset progress and the crossfade could never
        // complete (the shipped locomotion->aiming bug). The fade advances.
        oInst.Update(0.05f);
        EXPECT_NEAR(oInst.TransitionProgress(), 0.05f / 0.1f, kTimeEps);
        // ... and it completes into the destination while the flag still holds.
        oInst.Update(0.05f);
        EXPECT_TRUE(oInst.CurrentStateName() == SE::StrID("c"));
        EXPECT_FALSE(oInst.IsTransitioning());
}

TEST_F(FuncTestBase, GraphTransTest_DifferentCanInterruptStillReplacesInFlight) {
        // The self-block fix must not disable interrupts: a DIFFERENT
        // can_interrupt transition whose conditions pass mid-fade still cancels
        // and replaces the in-flight transition (restarts its clock).
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("gt_a", MakeClip(10.0f, true)))}),
                 MakeState("b", {MakeClipNode(MakeClipLeaf("gt_b", MakeClip(10.0f, true)))}),
                 MakeState("c", {MakeClipNode(MakeClipLeaf("gt_c", MakeClip(10.0f, true)))}),
                 MakeState("d", {MakeClipNode(MakeClipLeaf("gt_d", MakeClip(10.0f, true)))})},
                {MakeTransition("a", "b", 0.4f),
                 MakeTransition("a", "c", 0.4f,
                                {MakeCondition("go_c", SE::FlatBuffers::ConditionOp::IsTrue)},
                                false, 1.0f, /*can_interrupt=*/true),
                 MakeTransition("a", "d", 0.1f,
                                {MakeCondition("go_d", SE::FlatBuffers::ConditionOp::IsTrue)},
                                false, 1.0f, /*can_interrupt=*/true)}));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        oInst.Update(0.05f);                       // A->B armed
        oInst.SetBool(SE::StrID("go_c"), true);
        oInst.Update(0.05f);                       // replaced by A->C
        ASSERT_TRUE(oInst.TransitionTargetName() == SE::StrID("c"));
        oInst.Update(0.05f);                       // A->C advances (self-block fix)
        ASSERT_NEAR(oInst.TransitionProgress(), 0.05f / 0.4f, kTimeEps);

        oInst.SetBool(SE::StrID("go_d"), true);
        oInst.Update(0.05f);                       // A->D replaces A->C, restarts
        EXPECT_TRUE(oInst.TransitionTargetName() == SE::StrID("d"));
        EXPECT_NEAR(oInst.TransitionProgress(), 0.0f, kTimeEps);
}

TEST_F(FuncTestBase, GraphTransTest_DeclarationOrderWinsFirstMatch) {
        // Two simultaneously-true transitions from "a": the one declared first
        // in the FB array wins (mTransitionsFrom preserves declaration order).
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("gt_a", MakeClip(10.0f, true)))}),
                 MakeState("first", {MakeClipNode(MakeClipLeaf("gt_f", MakeClip(10.0f, true)))}),
                 MakeState("second", {MakeClipNode(MakeClipLeaf("gt_s", MakeClip(10.0f, true)))})},
                {MakeTransition("a", "first", 0.1f,
                                {MakeCondition("speed", SE::FlatBuffers::ConditionOp::Greater, 0.1f)}),
                 MakeTransition("a", "second", 0.1f,
                                {MakeCondition("speed", SE::FlatBuffers::ConditionOp::Greater, 0.1f)})}));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        oInst.SetFloat(SE::StrID("speed"), 1.0f);
        oInst.Update(0.05f);
        EXPECT_TRUE(oInst.TransitionTargetName() == SE::StrID("first"));
}

TEST_F(FuncTestBase, GraphTransTest_TransitionModeFieldIsIgnored) {
        // TransitionMode: CrossFade and AdditiveBlend advance the source clock
        // during the fade; Frozen holds it (the source pose freezes at the frame
        // the transition armed). AdditiveBlend is not implemented and falls back
        // to a plain crossfade (documented limitation).
        for (const auto eMode : {SE::FlatBuffers::TransitionMode::CrossFade,
                                 SE::FlatBuffers::TransitionMode::AdditiveBlend}) {
                SCOPED_TRACE(static_cast<int>(eMode));
                GraphRig oGraphRig(MakeTwoStateGraph("a", "b",
                        MakeClip(10.0f, true), MakeClip(10.0f, true), 0.2f,
                        {}, false, 1.0f, false, eMode));
                SE::AnimGraphInstance oInst;
                oInst.Init(*oGraphRig.pGraph);
                oInst.Update(0.05f);   // armed: clock 0.05 (progress 0 that frame)
                EXPECT_TRUE(oInst.IsTransitioning());
                EXPECT_NEAR(oInst.GetCurrentTime(), 0.05f, kTimeEps);
                oInst.Update(0.05f);
                EXPECT_NEAR(oInst.TransitionProgress(), 0.05f / 0.2f, kTimeEps);
                EXPECT_NEAR(oInst.GetCurrentTime(), 0.10f, kTimeEps);   // clock keeps running
        }
}

TEST_F(FuncTestBase, GraphTransTest_FrozenModeHoldsSourceClockDuringFade) {
        GraphRig oGraphRig(MakeTwoStateGraph("a", "b",
                MakeClip(10.0f, true), MakeClip(10.0f, true), 0.2f,
                {}, false, 1.0f, false, SE::FlatBuffers::TransitionMode::Frozen));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        oInst.Update(0.05f);   // armed at clock 0.05
        ASSERT_TRUE(oInst.IsTransitioning());
        EXPECT_NEAR(oInst.GetCurrentTime(), 0.05f, kTimeEps);

        // While the fade runs the source clock holds at the arm frame...
        for (int i = 0; i < 3; ++i) {
                oInst.Update(0.05f);
                EXPECT_NEAR(oInst.GetCurrentTime(), 0.05f, kTimeEps);
        }
        EXPECT_NEAR(oInst.TransitionProgress(), 0.15f / 0.2f, kTimeEps);

        // ... and on completion the destination's clock takes over.
        oInst.Update(0.05f);
        EXPECT_TRUE(oInst.CurrentStateName() == SE::StrID("b"));
        EXPECT_FALSE(oInst.IsTransitioning());
        EXPECT_NEAR(oInst.GetCurrentTime(), 0.20f, kTimeEps);   // = transition_time
}

// ===========================================================================
// Queries
// ===========================================================================

TEST_F(FuncTestBase, GraphQueryTest_GetActiveStatesSingleAndTransitioning) {
        GraphRig oGraphRig(MakeTwoStateGraph("a", "b",
                MakeClip(10.0f, true), MakeClip(10.0f, true), 0.2f));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        std::vector<SE::ActiveStateInfo> vActive;
        oInst.GetActiveStates(vActive);
        ASSERT_EQ(vActive.size(), 1u);
        EXPECT_TRUE(vActive[0].state_name == SE::StrID("a"));
        EXPECT_NEAR(vActive[0].weight, 1.0f, kWeightEps);

        oInst.Update(0.05f);   // transition armed: progress 0, destination clock 0
        oInst.GetActiveStates(vActive);
        ASSERT_EQ(vActive.size(), 2u);
        EXPECT_TRUE(vActive[0].state_name == SE::StrID("a"));
        EXPECT_NEAR(vActive[0].weight, 1.0f, kWeightEps);
        EXPECT_TRUE(vActive[1].state_name == SE::StrID("b"));
        EXPECT_NEAR(vActive[1].weight, 0.0f, kWeightEps);
        EXPECT_NEAR(vActive[1].local_time, 0.0f, kTimeEps);

        oInst.Update(0.05f);   // progress 0.25
        oInst.GetActiveStates(vActive);
        ASSERT_EQ(vActive.size(), 2u);
        EXPECT_NEAR(vActive[0].weight, 0.75f, kWeightEps);
        EXPECT_NEAR(vActive[0].local_time, 0.1f, kTimeEps);    // source clock keeps running
        EXPECT_NEAR(vActive[1].weight, 0.25f, kWeightEps);
        EXPECT_NEAR(vActive[1].local_time, 0.05f, kTimeEps);   // destination clock
}

TEST_F(FuncTestBase, GraphQueryTest_GetStateNamesListsAllStates) {
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("gq_a", MakeClip(1.0f, true)))}),
                 MakeState("b", {MakeClipNode(MakeClipLeaf("gq_b", MakeClip(1.0f, true)))}),
                 MakeState("c", {MakeClipNode(MakeClipLeaf("gq_c", MakeClip(1.0f, true)))})}));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        std::vector<std::string> vNames;
        oInst.GetStateNames(vNames);
        ASSERT_EQ(vNames.size(), 3u);
        EXPECT_EQ(vNames[0], "a");
        EXPECT_EQ(vNames[1], "b");
        EXPECT_EQ(vNames[2], "c");
}

TEST_F(FuncTestBase, GraphQueryTest_TransitionTargetNameUsesEmptyNameNotDefaultStrID) {
        // The "no transition" marker is kEmptyName (hash of ""), NOT StrID{}
        // (the 0xDEADBEEF sentinel). Tests and tools must compare accordingly.
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("gq_a", MakeClip(1.0f, true)))})}));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        EXPECT_TRUE(oInst.TransitionTargetName() == SE::AnimGraphInstance::kEmptyName);
        EXPECT_FALSE(oInst.TransitionTargetName() == SE::StrID{});
        EXPECT_FALSE(oInst.IsTransitioning());
}

TEST_F(FuncTestBase, GraphQueryTest_ForceSetStateJumpsAndClearsTransition) {
        GraphRig oGraphRig(MakeTwoStateGraph("a", "b",
                MakeClip(10.0f, true), MakeClip(10.0f, true), 0.2f));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        oInst.Update(0.1f);   // mid-crossfade, source time at 0.1
        ASSERT_TRUE(oInst.IsTransitioning());

        oInst.ForceSetState(SE::StrID("b"));
        EXPECT_FALSE(oInst.IsTransitioning());
        EXPECT_TRUE(oInst.CurrentStateName() == SE::StrID("b"));
        EXPECT_NEAR(oInst.GetCurrentTime(), 0.0f, kTimeEps);
        EXPECT_TRUE(oInst.TransitionTargetName() == SE::AnimGraphInstance::kEmptyName);
}

TEST_F(FuncTestBase, GraphQueryTest_ForceSetStateUnknownNameIsNoOp) {
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("gq_a", MakeClip(1.0f, true)))})}));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        oInst.Update(0.1f);
        oInst.ForceSetState(SE::StrID("ghost"));
        EXPECT_TRUE(oInst.CurrentStateName() == SE::StrID("a"));
        EXPECT_NEAR(oInst.GetCurrentTime(), 0.1f, kTimeEps);
}

TEST_F(FuncTestBase, GraphQueryTest_ClipHandleExposedForLeafClipState) {
        auto pClip = MakeRampClip(2.0f, true);
        GraphRig oGraphRig(MakeSingleStateGraph("a", "gq_leaf", std::move(pClip)));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);

        std::vector<SE::ActiveStateInfo> vActive;
        oInst.GetActiveStates(vActive);
        ASSERT_EQ(vActive.size(), 1u);
        EXPECT_TRUE(vActive[0].hClip.IsValid());
        SE::AnimClip* pRes = SE::GetResource(vActive[0].hClip);
        ASSERT_NE(pRes, nullptr);
        EXPECT_NEAR(pRes->Duration(), 2.0f, kTimeEps);
}

// ===========================================================================
// Pose-level integration
// ===========================================================================

TEST_F(FuncTestBase, GraphPoseTest_SingleStatePoseMatchesManualSampleClip) {
        auto pClip = MakeRampClip(2.0f, true, /*bone=*/1);
        GraphRig oGraphRig(MakeSingleStateGraph("a", "gp_manual", std::move(pClip)));
        SkelRig oSkelRig;

        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);
        oInst.Update(1.0f);   // current_time = 1.0

        // manual reference: sample the same clip content at the same time
        auto pRefClip = MakeRampClip(2.0f, true, 1);
        flatbuffers::FlatBufferBuilder oRefBuilder;
        SE::AnimClip oRefClip("gp_manual_ref", 0, SerializeClip(oRefBuilder, *pRefClip));

        SE::LocalPose oPose = SE::AllocatePose(oSkelRig.oSkel.BoneCount(), Alloc());
        SE::InitBindPose(oPose, oSkelRig.oSkel);
        oInst.EvaluateBlendTree(1.0f, oPose, Alloc(), oSkelRig.oSkel);
        SE::RenormalizeRotations(oPose);

        SE::LocalPose oRef = SE::AllocatePose(oSkelRig.oSkel.BoneCount(), Alloc());
        SE::InitBindPose(oRef, oSkelRig.oSkel);
        SE::SampleClip(oRefClip, 1.0f, oRef);
        SE::RenormalizeRotations(oRef);

        EXPECT_POSE_NEAR(oPose, oRef);
        Alloc().reset();
}

// ===========================================================================
// AnimState.speed — Unity-style state playback speed (clock × speed)
// ===========================================================================

TEST_F(FuncTestBase, GraphStateTest_SpeedScalesClipPlayback) {
        // 10 s non-looping pos-x ramp on bone 0 at speed 2: the clip advances
        // 2 s of clip-time per second of state-time.
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("sp_a",
                                MakeRampClip(10.0f, false, 0)))}, /*speed=*/2.0f)}));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);
        SkelRig oSkelRig;

        SE::LocalPose oPose = EvalFrame(oInst, oSkelRig.oSkel, Alloc(), 0.1f);
        // state-time 0.1 -> clip-time 0.2 -> ramp 0.2/10 = 0.02
        EXPECT_NEAR(oPose.pPos[0].x, 0.02f, kPoseEps);
        Alloc().reset();

        for (int i = 0; i < 4; ++i) {
                oPose = EvalFrame(oInst, oSkelRig.oSkel, Alloc(), 0.1f);
        }
        // state-time 0.5 -> clip-time 1.0 -> 0.1
        EXPECT_NEAR(oPose.pPos[0].x, 0.1f, kPoseEps);

        // Non-looping clamp: the state clock tops out at duration/speed = 5 s.
        for (int i = 0; i < 100; ++i) {
                oPose = EvalFrame(oInst, oSkelRig.oSkel, Alloc(), 0.1f);
        }
        EXPECT_NEAR(oInst.GetCurrentTime(), 5.0f, kTimeEps);
        EXPECT_NEAR(oPose.pPos[0].x, 1.0f, kPoseEps);   // clip end held
        Alloc().reset();
}

TEST_F(FuncTestBase, GraphStateTest_DefaultAndZeroSpeed) {
        // Default speed is 1.0 (ramp 0.1 s -> clip 0.1 s over a 1 s clip).
        GraphRig oGraphRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("sp_b",
                                MakeRampClip(1.0f, false, 0)))})}));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);
        SkelRig oSkelRig;
        SE::LocalPose oPose = EvalFrame(oInst, oSkelRig.oSkel, Alloc(), 0.1f);
        EXPECT_NEAR(oPose.pPos[0].x, 0.1f, kPoseEps);
        Alloc().reset();

        // A misauthored 0-speed clamps to eps — near-frozen, no NaN.
        GraphRig oRigZero(MakeGraph("z",
                {MakeState("z", {MakeClipNode(MakeClipLeaf("sp_c",
                                MakeRampClip(1.0f, false, 0)))}, /*speed=*/0.0f)}));
        SE::AnimGraphInstance oInstZero;
        oInstZero.Init(*oRigZero.pGraph);
        for (int i = 0; i < 10; ++i) {
                oPose = EvalFrame(oInstZero, oSkelRig.oSkel, Alloc(), 0.1f);
        }
        // -ffast-math: no isfinite asserts — a near-zero ramp value proves the
        // clock stayed near-frozen and produced no garbage.
        EXPECT_NEAR(oPose.pPos[0].x, 0.0f, 1e-3f);
        Alloc().reset();
}

// ===========================================================================
// Mirror (AnimState.mirror / ClipNodeData.mirror)
// ===========================================================================

namespace {
constexpr float kS2 = 0.7071067811865476f;   // sin/cos of 90°
}

struct MirrorRig {
        SkelPtr                        pNative;
        flatbuffers::FlatBufferBuilder oBuilder;
        const SE::FlatBuffers::Skeleton* pFb = nullptr;
        SE::Skeleton                   oSkel;

        MirrorRig()
                : pNative(MakeMirrorSkeleton()),
                  pFb(SerializeSkeleton(oBuilder, *pNative)),
                  oSkel("mirror_test_skel", 0, pFb) {}
};

TEST_F(FuncTestBase, GraphStateTest_MirroredStateSwapsAndReflects) {
        // Clip holds pos-x 1 on hand_l and 3 on hand_r. Mirrored: each bone
        // reads its partner and reflects X — hand_l gets -3, hand_r gets -1.
        auto pClip = MakeClip(1.0f, true);
        AddRamp(*pClip, 1, 0, 1.0f, 1.0f, 1.0f);   // hand_l x = 1
        AddRamp(*pClip, 2, 0, 1.0f, 3.0f, 3.0f);   // hand_r x = 3

        GraphRig oGraphRig(MakeGraph("m",
                {MakeState("m", {MakeClipNode(MakeClipLeaf("mir_a", std::move(pClip)))},
                           1.0f, /*mirror=*/true)}));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);
        MirrorRig oRig;

        SE::LocalPose oPose = EvalFrame(oInst, oRig.oSkel, Alloc(), 0.016f);
        EXPECT_NEAR(oPose.pPos[1].x, -3.0f, kPoseEps);   // hand_l <- reflected hand_r
        EXPECT_NEAR(oPose.pPos[2].x, -1.0f, kPoseEps);   // hand_r <- reflected hand_l
        EXPECT_NEAR(oPose.pPos[0].x,  0.0f, kPoseEps);   // spine: center stays
        Alloc().reset();
}

TEST_F(FuncTestBase, GraphStateTest_MirroredStateReflectsRotations) {
        // hand_l rotated +90° around Z (xyzw = 0,0,s2,s2). Mirrored: hand_r
        // gets the reflected rotation (w,-x,y,-z) = (s2,0,0,-s2) — 90° about -Z.
        auto pClip = MakeClip(1.0f, true);
        const std::vector<float> vTimes = {0.0f, 1.0f};
        AddChannel(*pClip, 1, 3, vTimes, {0.0f, 0.0f});           // x
        AddChannel(*pClip, 1, 4, vTimes, {0.0f, 0.0f});           // y
        AddChannel(*pClip, 1, 5, vTimes, {kS2, kS2});             // z
        AddChannel(*pClip, 1, 6, vTimes, {kS2, kS2});             // w

        GraphRig oGraphRig(MakeGraph("m",
                {MakeState("m", {MakeClipNode(MakeClipLeaf("mir_b", std::move(pClip)))},
                           1.0f, /*mirror=*/true)}));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);
        MirrorRig oRig;

        SE::LocalPose oPose = EvalFrame(oInst, oRig.oSkel, Alloc(), 0.016f);
        const glm::quat& q = oPose.pRot[2];   // hand_r
        // reflect (w,x,y,z)=(s2,0,0,s2) -> (s2,0,0,-s2): 90° about -Z
        EXPECT_NEAR(q.w,  kS2, kPoseEps);
        EXPECT_NEAR(q.x,  0.0f, kPoseEps);
        EXPECT_NEAR(q.y,  0.0f, kPoseEps);
        EXPECT_NEAR(q.z, -kS2, kPoseEps);
        EXPECT_NEAR(glm::length(q), 1.0f, 1e-4f);
        Alloc().reset();
}

TEST_F(FuncTestBase, GraphClipNodeTest_MirroredClipNodeReflectsContribution) {
        // Node-level mirror mirrors only that clip node's contribution.
        auto pClip = MakeClip(1.0f, true);
        AddRamp(*pClip, 1, 0, 1.0f, 1.0f, 1.0f);   // hand_l x = 1
        AddRamp(*pClip, 2, 0, 1.0f, 3.0f, 3.0f);   // hand_r x = 3

        GraphRig oGraphRig(MakeGraph("m",
                {MakeState("m", {MakeClipNode(
                                MakeClipLeaf("mir_c", std::move(pClip),
                                             1.0f, /*mirror=*/true))})}));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);
        MirrorRig oRig;

        SE::LocalPose oPose = EvalFrame(oInst, oRig.oSkel, Alloc(), 0.016f);
        EXPECT_NEAR(oPose.pPos[1].x, -3.0f, kPoseEps);
        EXPECT_NEAR(oPose.pPos[2].x, -1.0f, kPoseEps);
        Alloc().reset();
}

TEST_F(FuncTestBase, GraphClipNodeTest_UnmirroredClipIsUnchanged) {
        auto pClip = MakeClip(1.0f, true);
        AddRamp(*pClip, 1, 0, 1.0f, 1.0f, 1.0f);
        AddRamp(*pClip, 2, 0, 1.0f, 3.0f, 3.0f);

        GraphRig oGraphRig(MakeGraph("m",
                {MakeState("m", {MakeClipNode(MakeClipLeaf("mir_d", std::move(pClip)))})}));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);
        MirrorRig oRig;

        SE::LocalPose oPose = EvalFrame(oInst, oRig.oSkel, Alloc(), 0.016f);
        EXPECT_NEAR(oPose.pPos[1].x, 1.0f, kPoseEps);
        EXPECT_NEAR(oPose.pPos[2].x, 3.0f, kPoseEps);
        Alloc().reset();
}

// ===========================================================================
// Blend2D.algorithm — SimpleDirectional (nearest) vs FreeformCartesian (IDW)
// ===========================================================================

namespace {

ClipPtr MakeConstClip(float duration, bool looping, uint16_t bone, uint8_t target, float value) {
        auto pClip = MakeClip(duration, looping);
        AddChannel(*pClip, bone, target, {0.0f, duration}, {value, value},
                   SE::FlatBuffers::CurveFormat::LinearF32);
        return pClip;
}

// Blend2D state: node 0 is the Blend2D node, nodes 1..3 are constant clips
// (pos-x 1, 5, 9) at blend points (0,0), (2,0), (0,2).
GraphPtr MakeBlend2DGraph(SE::FlatBuffers::Blend2DAlgorithm eAlgorithm) {
        std::vector<SE::FlatBuffers::BlendTreeNodeT> vNodes;
        std::vector<SE::FlatBuffers::BlendPoint2D> vPoints = {
                SE::FlatBuffers::BlendPoint2D(0.0f, 0.0f),
                SE::FlatBuffers::BlendPoint2D(2.0f, 0.0f),
                SE::FlatBuffers::BlendPoint2D(0.0f, 2.0f)};
        vNodes.push_back(MakeBlend2DNode("px", "py", vPoints, {1, 2, 3}, eAlgorithm));
        vNodes.push_back(MakeClipNode(MakeClipLeaf("b2d_a", MakeConstClip(1.0f, true, 1, 0, 1.0f))));
        vNodes.push_back(MakeClipNode(MakeClipLeaf("b2d_b", MakeConstClip(1.0f, true, 1, 0, 5.0f))));
        vNodes.push_back(MakeClipNode(MakeClipLeaf("b2d_c", MakeConstClip(1.0f, true, 1, 0, 9.0f))));
        return MakeGraph("b2d", {MakeState("b2d", std::move(vNodes))}, {},
                {MakeParam("px", SE::FlatBuffers::AnimParamType::Float),
                 MakeParam("py", SE::FlatBuffers::AnimParamType::Float)});
}

} // namespace

TEST_F(FuncTestBase, GraphBlend2DTest_SimpleDirectionalPicksNearestChild) {
        GraphRig oGraphRig(MakeBlend2DGraph(
                        SE::FlatBuffers::Blend2DAlgorithm::SimpleDirectional));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);
        SkelRig oSkelRig;

        oInst.SetFloat(SE::StrID("px"), 0.1f);
        oInst.SetFloat(SE::StrID("py"), 1.5f);
        SE::LocalPose oPose = EvalFrame(oInst, oSkelRig.oSkel, Alloc(), 0.016f);
        EXPECT_NEAR(oPose.pPos[1].x, 9.0f, kPoseEps);   // nearest point (0,2)
        Alloc().reset();
}

TEST_F(FuncTestBase, GraphBlend2DTest_FreeformCartesianBlendsAllChildren) {
        GraphRig oGraphRig(MakeBlend2DGraph(
                        SE::FlatBuffers::Blend2DAlgorithm::FreeformCartesian));
        SE::AnimGraphInstance oInst;
        oInst.Init(*oGraphRig.pGraph);
        SkelRig oSkelRig;

        oInst.SetFloat(SE::StrID("px"), 0.1f);
        oInst.SetFloat(SE::StrID("py"), 1.5f);
        SE::LocalPose oPose = EvalFrame(oInst, oSkelRig.oSkel, Alloc(), 0.016f);
        // Inverse-distance-squared weights: d2 = 2.26, 5.86, 0.26
        // w = 0.4425, 0.1706, 3.8461 -> normalized 0.0992, 0.0383, 0.8625
        // x = 1*0.0992 + 5*0.0383 + 9*0.8625 ≈ 8.05
        EXPECT_NEAR(oPose.pPos[1].x, 8.053f, 1e-2f);
        Alloc().reset();

        // Dead centre on a child: that child dominates.
        oInst.SetFloat(SE::StrID("px"), 2.0f);
        oInst.SetFloat(SE::StrID("py"), 0.0f);
        oPose = EvalFrame(oInst, oSkelRig.oSkel, Alloc(), 0.016f);
        EXPECT_NEAR(oPose.pPos[1].x, 5.0f, 1e-2f);
        Alloc().reset();
}

} // namespace
