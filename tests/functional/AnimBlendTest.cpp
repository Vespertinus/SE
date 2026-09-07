
// ---------------------------------------------------------------------------
// Tier B functional tests — blending: crossfade, Blend1D phase sync, Additive,
// Layer/masks. Constant/ramp channels on a 3-bone chain give analytic expected
// values. CHARACTERIZATION tests document current (sometimes surprising)
// behavior — see the quirks appendix in the design doc.
//
// Blend1D phase note: the shared node phase advances only inside blend-tree
// EVALUATION (using the dt of the last Update), so phase assertions must go
// through EvalFrame (Update + Evaluate) per step, not bare Update loops.
// ---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <glm/glm.hpp>

#define SE_IMPL
#include <EngineFixture.h>
#include <FrameRunner.h>
#include <PoseAssert.h>
#include <LogCapture.h>

namespace {

using namespace se_test;

struct SkelRig {
        static SkelPtr BuildSkel(const char* sMask) {
                auto pSkelData = MakeChainSkeleton(3);
                if (sMask) {
                        AddMask(*pSkelData, sMask, {{1, 1.0f}, {2, 0.5f}});
                }
                return pSkelData;
        }

        SkelPtr                        pNative;
        flatbuffers::FlatBufferBuilder oBuilder;
        const SE::FlatBuffers::Skeleton* pFb = nullptr;
        SE::Skeleton                   oSkel;

        explicit SkelRig(const char* sMask = nullptr)
                : pNative(BuildSkel(sMask)),
                  pFb(SerializeSkeleton(oBuilder, *pNative)),
                  oSkel("blend_test_skel", 0, pFb) {}
};

// Keeps the graph resource alive while an instance is in use (the instance's
// FlatBuffer pointer refers into the graph's own storage).
// AnimGraphInstance is non-copyable/non-movable, hence the wrapper.
struct InstanceRig {
        flatbuffers::FlatBufferBuilder oBuilder;
        std::unique_ptr<SE::AnimGraph> pGraph;
        SE::AnimGraphInstance          inst;

        explicit InstanceRig(const GraphPtr& pNative) {
                pGraph = std::make_unique<SE::AnimGraph>(
                                "blend_graph", 0, SerializeGraph(oBuilder, *pNative));
                inst.Init(*pGraph);
        }
};

// Clip with one constant channel: bone[bone_index].target = value at all times.
ClipPtr MakeConstClip(float duration, bool looping, uint16_t bone, uint8_t target, float value) {
        auto pClip = MakeClip(duration, looping);
        AddChannel(*pClip, bone, target, {0.0f, duration}, {value, value},
                   SE::FlatBuffers::CurveFormat::LinearF32);
        return pClip;
}

// Clip with pos-x ramping 0 -> 1 over its duration on the given bone.
ClipPtr MakeRampClip(float duration, bool looping, uint16_t bone = 1) {
        auto pClip = MakeClip(duration, looping);
        AddRamp(*pClip, bone, 0, duration, 0.0f, 1.0f);
        return pClip;
}

// Clip with the same constant pos-x on all three bones.
ClipPtr MakeConstAllBones(float value) {
        auto pClip = MakeClip(1.0f, true);
        for (uint16_t b = 0; b < 3; ++b) {
                AddChannel(*pClip, b, 0, {0.0f, 1.0f}, {value, value},
                           SE::FlatBuffers::CurveFormat::LinearF32);
        }
        return pClip;
}

// Variadic clip-list builder (unique_ptr is not copyable, so no init lists).
inline std::vector<ClipPtr> Clips() { return {}; }
template <class... TRest>
inline std::vector<ClipPtr> Clips(ClipPtr pFirst, TRest... oRest) {
        std::vector<ClipPtr> v = Clips(std::move(oRest)...);
        v.insert(v.begin(), std::move(pFirst));
        return v;
}

// Blend1D root (node 0) over [sParam] with the given children after it.
GraphPtr MakeBlend1DGraph(std::vector<ClipPtr> vpClips, const std::vector<float>& vThresholds,
                          const char* sParam = "speed") {
        std::vector<SE::FlatBuffers::BlendTreeNodeT> vNodes;
        vNodes.push_back(MakeBlend1DNode(sParam, vThresholds, {}));   // placeholder root
        std::vector<uint16_t> vChildren;
        for (uint32_t i = 0; i < vpClips.size(); ++i) {
                vNodes.push_back(MakeClipNode(MakeClipLeaf(
                                "b1d_child_" + std::to_string(i), std::move(vpClips[i]))));
                vChildren.push_back(static_cast<uint16_t>(vNodes.size() - 1));
        }
        vNodes[0] = MakeBlend1DNode(sParam, vThresholds, vChildren);
        std::vector<SE::FlatBuffers::AnimStateT> vStates;
        vStates.push_back(MakeState("loco", std::move(vNodes)));
        return MakeGraph("loco", std::move(vStates), {},
                         {MakeParam(sParam, SE::FlatBuffers::AnimParamType::Float, 0.0f)});
}

// ===========================================================================
// Crossfade
// ===========================================================================

TEST_F(FuncTestBase, CrossfadeTest_HalfwayPoseIsAverageOfBothStates) {
        // A: bone1 pos-x = 0; B: bone1 pos-x = 1. At progress 0.5 -> 0.5.
        InstanceRig oRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("xf_a", MakeConstClip(10.0f, true, 1, 0, 0.0f)))}),
                 MakeState("b", {MakeClipNode(MakeClipLeaf("xf_b", MakeConstClip(10.0f, true, 1, 0, 1.0f)))})},
                {MakeTransition("a", "b", 0.2f)}));
        SkelRig oSkelRig;

        oRig.inst.Update(0.05f);   // arm transition
        oRig.inst.Update(0.10f);   // progress 0.5
        ASSERT_NEAR(oRig.inst.TransitionProgress(), 0.5f, kTimeEps);

        SE::LocalPose oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 0.0f);
        EXPECT_NEAR(oPose.pPos[1].x, 0.5f, kPoseEps);
        Alloc().reset();
}

TEST_F(FuncTestBase, CrossfadeTest_BothClocksAdvance_DestinationFromZero) {
        // CHARACTERIZATION: during a crossfade the SOURCE is evaluated at
        // current_time (keeps advancing) and the DESTINATION at transition_time
        // (started from 0 when the transition armed).
        // A: ramp 0->1 over 1 s on bone1 (value == local time); B: constant 5.
        // After arm + one 0.05 step: progress 0.25, current_time 0.1,
        // transition_time 0.05 -> pose.x = mix(0.1, 5, 0.25) = 1.325.
        InstanceRig oRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("xf2_a", MakeRampClip(1.0f, true)))}),
                 MakeState("b", {MakeClipNode(MakeClipLeaf("xf2_b", MakeConstClip(10.0f, true, 1, 0, 5.0f)))})},
                {MakeTransition("a", "b", 0.2f)}));
        SkelRig oSkelRig;

        oRig.inst.Update(0.05f);   // arm
        oRig.inst.Update(0.05f);   // progress 0.25
        SE::LocalPose oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 0.0f);
        EXPECT_NEAR(oPose.pPos[1].x, 0.1f * 0.75f + 5.0f * 0.25f, 1e-4f);
        Alloc().reset();
}

TEST_F(FuncTestBase, CrossfadeTest_ClipNodeIgnoresTopLevelWeight) {
        // CHARACTERIZATION: EvaluateBlendTree's weight argument reaches Clip
        // leaves but they replace channel values unconditionally — a Clip-root
        // state evaluated at weight 0.25 equals weight 1.0.
        InstanceRig oRig(MakeSingleStateGraph("a", "xf3_a",
                MakeConstClip(10.0f, true, 1, 0, 7.0f)));
        SkelRig oSkelRig;

        SE::LocalPose oPoseWeak = SE::AllocatePose(3, Alloc());
        SE::InitBindPose(oPoseWeak, oSkelRig.oSkel);
        oRig.inst.EvaluateBlendTree(0.25f, oPoseWeak, Alloc(), oSkelRig.oSkel);
        SE::RenormalizeRotations(oPoseWeak);

        SE::LocalPose oPoseFull = SE::AllocatePose(3, Alloc());
        SE::InitBindPose(oPoseFull, oSkelRig.oSkel);
        oRig.inst.EvaluateBlendTree(1.0f, oPoseFull, Alloc(), oSkelRig.oSkel);
        SE::RenormalizeRotations(oPoseFull);

        EXPECT_POSE_NEAR(oPoseWeak, oPoseFull);
        EXPECT_NEAR(oPoseWeak.pPos[1].x, 7.0f, kPoseEps);   // fully applied, not 1.75
        Alloc().reset();
}

// ===========================================================================
// Blend1D
// ===========================================================================

TEST_F(FuncTestBase, Blend1DTest_AtThresholdReturnsChildExactly) {
        InstanceRig oRig(MakeBlend1DGraph(Clips(MakeConstClip(1.0f, true, 1, 0, 0.0f),
                 MakeConstClip(1.0f, true, 1, 0, 1.0f),
                 MakeConstClip(1.0f, true, 1, 0, 2.0f)),
                {0.0f, 1.0f, 2.0f}));
        SkelRig oSkelRig;

        for (const auto& oCase : {std::pair{0.0f, 0.0f}, {1.0f, 1.0f}, {2.0f, 2.0f}}) {
                oRig.inst.SetFloat(SE::StrID("speed"), oCase.first);
                SE::LocalPose oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 0.016f);
                EXPECT_NEAR(oPose.pPos[1].x, oCase.second, kPoseEps) << "speed " << oCase.first;
                Alloc().reset();
        }
}

TEST_F(FuncTestBase, Blend1DTest_BetweenThresholdsIsLinearMix) {
        InstanceRig oRig(MakeBlend1DGraph(Clips(MakeConstClip(1.0f, true, 1, 0, 0.0f),
                 MakeConstClip(1.0f, true, 1, 0, 1.0f),
                 MakeConstClip(1.0f, true, 1, 0, 2.0f)),
                {0.0f, 1.0f, 2.0f}));
        SkelRig oSkelRig;

        oRig.inst.SetFloat(SE::StrID("speed"), 0.5f);
        SE::LocalPose oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 0.016f);
        EXPECT_NEAR(oPose.pPos[1].x, 0.5f, kPoseEps);   // 50/50 of children 0 and 1
        Alloc().reset();
}

TEST_F(FuncTestBase, Blend1DTest_ParamClampedToRangeNotBindPose) {
        // Out-of-range params clamp to the extreme child. Bind pose (bone1.x
        // == 1) would silently fake child 1 here, so 0 / 2 prove the clamp.
        InstanceRig oRig(MakeBlend1DGraph(Clips(MakeConstClip(1.0f, true, 1, 0, 0.0f),
                 MakeConstClip(1.0f, true, 1, 0, 1.0f),
                 MakeConstClip(1.0f, true, 1, 0, 2.0f)),
                {0.0f, 1.0f, 2.0f}));
        SkelRig oSkelRig;

        oRig.inst.SetFloat(SE::StrID("speed"), -5.0f);
        SE::LocalPose oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 0.016f);
        EXPECT_NEAR(oPose.pPos[1].x, 0.0f, kPoseEps);
        Alloc().reset();

        oRig.inst.SetFloat(SE::StrID("speed"), 7.0f);
        oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 0.016f);
        EXPECT_NEAR(oPose.pPos[1].x, 2.0f, kPoseEps);
        Alloc().reset();
}

TEST_F(FuncTestBase, Blend1DTest_LessThanTwoChildrenWritesNothing) {
        // CHARACTERIZATION: a Blend1D node with fewer than 2 children returns
        // without writing the out-pose — the result silently stays whatever the
        // caller seeded (here: bind pose, bone1.x == 1, not the child's 0).
        std::vector<SE::FlatBuffers::BlendTreeNodeT> vNodes;
        vNodes.push_back(MakeBlend1DNode("speed", {0.0f}, {}));   // placeholder
        std::vector<uint16_t> vChildren;
        vNodes.push_back(MakeClipNode(MakeClipLeaf("b1d_single",
                        MakeConstClip(1.0f, true, 1, 0, 0.0f))));
        vChildren.push_back(1);
        vNodes[0] = MakeBlend1DNode("speed", {0.0f}, vChildren);

        InstanceRig oRig(MakeGraph("loco",
                {MakeState("loco", std::move(vNodes))}, {},
                {MakeParam("speed", SE::FlatBuffers::AnimParamType::Float)}));
        SkelRig oSkelRig;

        oRig.inst.SetFloat(SE::StrID("speed"), 0.0f);
        SE::LocalPose oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 0.016f);
        EXPECT_NEAR(oPose.pPos[1].x, 1.0f, kPoseEps);   // bind pose survived
        Alloc().reset();
}

TEST_F(FuncTestBase, Blend1DTest_PhaseLocksChildrenOfDifferentLengths) {
        // Children: ramp 0->1 over 1 s and ramp 0->1 over 2 s sharing one
        // normalized phase; both are sampled at phase * own duration, i.e. at
        // the same FRACTION of their clips. Both ramps map fraction -> value,
        // so a 50/50 blend (param 1) must read exactly the phase fraction.
        // Same absolute wall time on both children would give 0.1875 instead.
        InstanceRig oRig(MakeBlend1DGraph(
                Clips(MakeRampClip(1.0f, true), MakeRampClip(2.0f, true)), {0.0f, 1.0f}));
        SkelRig oSkelRig;

        oRig.inst.SetFloat(SE::StrID("speed"), 1.0f);   // t = 0.5, anchor = child B (2 s)
        SE::LocalPose oPose{};
        for (int i = 0; i < 30; ++i) {
                oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 1.0f / 60.0f);
        }
        // phase = 30 * (1/60) / 2 = 0.25; A at 0.25 s (0.25 through), B at 0.5 s
        // (0.25 through) -> blend of two equal values = 0.25.
        EXPECT_NEAR(oPose.pPos[1].x, 0.25f, 1e-3f);
        Alloc().reset();
}

TEST_F(FuncTestBase, Blend1DTest_PhaseRateUsesAnchorAndFlipsAtMidBlend) {
        // CHARACTERIZATION: the shared phase advances by dt / anchor-duration,
        // where the anchor is the dominant child (switches at t == 0.5) — the
        // phase RATE is discontinuous as the parameter crosses the midpoint.
        // Children: A = 1 s ramp (value == time), B = 2 s ramp (value == time/2).
        InstanceRig oRig(MakeBlend1DGraph(
                Clips(MakeRampClip(1.0f, true), MakeRampClip(2.0f, true)), {0.0f, 1.0f}));
        SkelRig oSkelRig;

        // t = 0 -> anchor A (1 s): two 0.1 s steps -> phase 0.2 -> value 0.2.
        oRig.inst.SetFloat(SE::StrID("speed"), 0.0f);
        SE::LocalPose oPose{};
        for (int i = 0; i < 2; ++i) {
                oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 0.1f);
        }
        EXPECT_NEAR(oPose.pPos[1].x, 0.2f, 1e-3f);
        Alloc().reset();

        // t = 1.0 (blend fully onto B) -> anchor flips to B (2 s): the next
        // 0.1 s step advances phase by 0.05, not 0.1 -> B reads 0.05 * 0.5.
        oRig.inst.SetFloat(SE::StrID("speed"), 1.0f);
        oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 0.1f);
        EXPECT_NEAR(oPose.pPos[1].x, 0.25f, 1e-3f);
        Alloc().reset();
}

TEST_F(FuncTestBase, Blend1DTest_PhaseSurvivesNaturalTransitionNotForceSetState) {
        // CHARACTERIZATION: entering a Blend1D state via a natural transition
        // keeps the node's shared phase running, while ForceSetState clears it.
        // Detected through the 1 s ramp child's sample value == phase.
        // (Blend1D needs >= 2 children to evaluate at all — see the
        // LessThanTwoChildrenWritesNothing characterization.)
        std::vector<SE::FlatBuffers::BlendTreeNodeT> vNodesB;
        vNodesB.push_back(MakeBlend1DNode("speed", {0.0f, 1.0f}, {}));   // placeholder
        vNodesB.push_back(MakeClipNode(MakeClipLeaf("ph_ramp", MakeRampClip(1.0f, true))));
        vNodesB.push_back(MakeClipNode(MakeClipLeaf("ph_hold",
                        MakeConstClip(1.0f, true, 1, 0, 9.0f))));
        vNodesB[0] = MakeBlend1DNode("speed", {0.0f, 1.0f}, {1, 2});

        InstanceRig oRig(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("ph_a", MakeClip(10.0f, true)))}),
                 MakeState("b", std::move(vNodesB))},
                {MakeTransition("a", "b", 0.1f)},
                {MakeParam("speed", SE::FlatBuffers::AnimParamType::Float)}));
        SkelRig oSkelRig;

        oRig.inst.SetFloat(SE::StrID("speed"), 0.0f);
        for (int i = 0; i < 6; ++i) {
                oRig.inst.Update(0.05f);   // crossfade 0.1 s, then settle in b
        }
        ASSERT_FALSE(oRig.inst.IsTransitioning());

        SE::LocalPose oPose{};
        for (int i = 0; i < 3; ++i) {   // phase 0.05 -> 0.10 -> 0.15
                oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 0.05f);
        }
        EXPECT_NEAR(oPose.pPos[1].x, 0.15f, 1e-3f);   // phase kept across the natural entry
        Alloc().reset();

        // ForceSetState clears mNodePhase -> the phase restarts from dt.
        oRig.inst.ForceSetState(SE::StrID("b"));
        oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 0.05f);
        EXPECT_NEAR(oPose.pPos[1].x, 0.05f, 1e-3f);
        Alloc().reset();
}

// ===========================================================================
// Additive
// ===========================================================================

// Additive root: base clip (node 1) + additive clip (node 2).
GraphPtr MakeAdditiveGraph(ClipPtr pBase, ClipPtr pAdditive,
                           float weight, const char* sWeightParam = nullptr) {
        std::vector<SE::FlatBuffers::BlendTreeNodeT> vNodes;
        vNodes.push_back(MakeAdditiveNode(1, 2, weight, sWeightParam ? sWeightParam : ""));
        vNodes.push_back(MakeClipNode(MakeClipLeaf("add_base", std::move(pBase))));
        vNodes.push_back(MakeClipNode(MakeClipLeaf("add_layer", std::move(pAdditive))));
        std::vector<SE::FlatBuffers::AnimStateT> vStates;
        vStates.push_back(MakeState("pose", std::move(vNodes)));
        return MakeGraph("pose", std::move(vStates));
}

// Clip with bone[bone_index] pos-x ramping 3 -> 7 over [0,1] (non-looping).
// Its frame 0 (= the additive reference pose) differs from the chain-skeleton
// bind (pos-x = bone_index), which is what the old delta-from-bind behavior
// got wrong.
ClipPtr MakeStepClip(uint16_t bone) {
        auto pClip = MakeClip(1.0f, false);
        AddChannel(*pClip, bone, 0, {0.0f, 1.0f}, {3.0f, 7.0f},
                   SE::FlatBuffers::CurveFormat::LinearF32);
        return pClip;
}

// Long non-looping constant base on all three bones: the composite-root state
// clock follows this clip (duration + looping), so it must not wrap inside
// the test horizon.
ClipPtr MakeConstBaseAllBones(float value) {
        auto pClip = MakeClip(10.0f, false);
        for (uint16_t b = 0; b < 3; ++b) {
                AddChannel(*pClip, b, 0, {0.0f, 10.0f}, {value, value},
                           SE::FlatBuffers::CurveFormat::LinearF32);
        }
        return pClip;
}

TEST_F(FuncTestBase, AdditiveTest_DeltaAtReferenceFrameIsZero) {
        // Additive deltas are relative to the additive clip's own frame-0 pose,
        // NOT the skeleton bind: at the reference frame the contribution is
        // exactly zero and the output equals the base clip. (The old
        // delta-from-bind behavior applied the constant bind->frame-0 offset —
        // the hit_reaction arm-twist bug.)
        auto pBase = MakeConstClip(1.0f, true, 1, 0, 1.0f);
        auto pAdd  = MakeStepClip(1);           // frame 0 pos-x = 3, bind = 1

        InstanceRig oRig(MakeAdditiveGraph(std::move(pBase), std::move(pAdd), 1.0f));
        SkelRig oSkelRig;

        SE::LocalPose oPose = SE::AllocatePose(oSkelRig.oSkel.BoneCount(), Alloc());
        SE::InitBindPose(oPose, oSkelRig.oSkel);
        oRig.inst.EvaluateBlendTree(1.0f, oPose, Alloc(), oSkelRig.oSkel);   // time == 0
        SE::RenormalizeRotations(oPose);
        EXPECT_NEAR(oPose.pPos[1].x, 1.0f, kPoseEps);
        Alloc().reset();
}

TEST_F(FuncTestBase, AdditiveTest_DeltaRelativeToClipReference) {
        // base: bone1 pos-x = 1; additive: pos-x 3->7 and rot identity->qZ90
        // over [0,1]. At t = 1 the output must be base + (add(1) - add(0)):
        // pos = 1 + (7 - 3) = 5, rot = identity * (inv(identity) * qZ90) = qZ90.
        auto pBase = MakeConstClip(10.0f, false, 1, 0, 1.0f);
        auto pAdd  = MakeStepClip(1);
        AddChannel(*pAdd, 1, 5, {0.0f, 1.0f}, {0.0f, 0.70710678f});   // rot z
        AddChannel(*pAdd, 1, 6, {0.0f, 1.0f}, {1.0f, 0.70710678f});   // rot w

        InstanceRig oRig(MakeAdditiveGraph(std::move(pBase), std::move(pAdd), 1.0f));
        SkelRig oSkelRig;

        SE::LocalPose oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 1.0f);
        EXPECT_NEAR(oPose.pPos[1].x, 5.0f, kPoseEps);
        const glm::quat qZ90 = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0, 0, 1));
        EXPECT_QUAT_NEAR(oPose.pRot[1], qZ90);
        Alloc().reset();
}

TEST_F(FuncTestBase, AdditiveTest_WeightHalfScalesReferenceRelativeDelta) {
        InstanceRig oRig(MakeAdditiveGraph(
                MakeConstClip(10.0f, false, 1, 0, 1.0f),
                MakeStepClip(1), 0.5f));
        SkelRig oSkelRig;

        SE::LocalPose oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 1.0f);
        EXPECT_NEAR(oPose.pPos[1].x, 3.0f, kPoseEps);   // 1 + (7-3)*0.5
        Alloc().reset();
}

TEST_F(FuncTestBase, AdditiveTest_WeightParamOverridesStaticWeight) {
        InstanceRig oRig(MakeAdditiveGraph(
                MakeConstClip(10.0f, false, 1, 0, 1.0f),
                MakeStepClip(1),
                /*weight=*/1.0f, /*weight_param=*/"add_w"));
        SkelRig oSkelRig;

        oRig.inst.SetFloat(SE::StrID("add_w"), 0.25f);
        SE::LocalPose oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 1.0f);
        EXPECT_NEAR(oPose.pPos[1].x, 2.0f, kPoseEps);   // 1 + 4*0.25
        Alloc().reset();
}

TEST_F(FuncTestBase, AdditiveTest_UnanimatedBonesContributeZeroDelta) {
        // Bones the additive subtree does not animate are seeded to bind in both
        // the pose and the reference, so their delta is exactly zero and the
        // base clip's value passes through untouched.
        InstanceRig oRig(MakeAdditiveGraph(MakeConstBaseAllBones(5.0f), MakeStepClip(1), 1.0f));
        SkelRig oSkelRig;

        SE::LocalPose oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 1.0f);
        EXPECT_NEAR(oPose.pPos[1].x, 9.0f, kPoseEps);   // 5 + (7 - 3)
        EXPECT_NEAR(oPose.pPos[0].x, 5.0f, kPoseEps);
        EXPECT_NEAR(oPose.pPos[2].x, 5.0f, kPoseEps);
        Alloc().reset();
}

TEST_F(FuncTestBase, AdditiveTest_DoesNotTouchScale) {
        // CHARACTERIZATION: the additive combine only writes pos and rot —
        // scale channels in the additive clip are ignored (base scale survives).
        auto pBase = MakeConstClip(1.0f, true, 1, 0, 1.0f);
        auto pAdd  = MakeConstClip(1.0f, true, 1, 0, 3.0f);
        AddChannel(*pAdd, 1, 7, {0.0f, 1.0f}, {5.0f, 5.0f});   // scale x = 5

        InstanceRig oRig(MakeAdditiveGraph(std::move(pBase), std::move(pAdd), 1.0f));
        SkelRig oSkelRig;

        SE::LocalPose oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 0.016f);
        EXPECT_NEAR(oPose.pScl[1].x, 1.0f, kPoseEps);
        Alloc().reset();
}

// ===========================================================================
// Layer / masks
// ===========================================================================

// Layer root: base clip (node 1) + layer clip (node 2).
GraphPtr MakeLayerGraph(ClipPtr pBase, ClipPtr pLayer, const char* sMaskName,
                        float weight, const char* sWeightParam = nullptr,
                        SE::FlatBuffers::LayerBlendMode eMode =
                                SE::FlatBuffers::LayerBlendMode::Override) {
        std::vector<SE::FlatBuffers::BlendTreeNodeT> vNodes;
        vNodes.push_back(MakeLayerNode(1, 2, sMaskName ? sMaskName : "",
                                       weight, sWeightParam ? sWeightParam : "", eMode));
        vNodes.push_back(MakeClipNode(MakeClipLeaf("layer_base", std::move(pBase))));
        vNodes.push_back(MakeClipNode(MakeClipLeaf("layer_top", std::move(pLayer))));
        std::vector<SE::FlatBuffers::AnimStateT> vStates;
        vStates.push_back(MakeState("pose", std::move(vNodes)));
        return MakeGraph("pose", std::move(vStates));
}

TEST_F(FuncTestBase, LayerTest_EmptyMaskNameAppliesWeightToAllBones) {
        // CHARACTERIZATION: an empty mask_name means no mask lookup, and no
        // mask means the weight applies to EVERY bone — the whole-body override
        // that also hits the shipped character.seag aiming layer (no mask
        // ships in any .sesk).
        InstanceRig oRig(MakeLayerGraph(
                MakeConstAllBones(1.0f), MakeConstAllBones(5.0f), "", 1.0f));
        SkelRig oSkelRig;

        SE::LocalPose oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 0.016f);
        for (uint32_t i = 0; i < 3; ++i) {
                EXPECT_NEAR(oPose.pPos[i].x, 5.0f, kPoseEps) << "bone " << i;
        }
        Alloc().reset();
}

TEST_F(FuncTestBase, LayerTest_MissingMaskNameFallsBackToAllBones) {
        // A non-empty mask_name whose mask is absent from the skeleton still
        // applies the layer to all bones (documented degradation), but now it
        // WARNS — silent all-bones fallback used to hide rig/asset bugs.
        InstanceRig oRig(MakeLayerGraph(
                MakeConstAllBones(1.0f), MakeConstAllBones(5.0f), "ghost_mask", 1.0f));
        SkelRig oSkelRig;

        LogCapture oCapture;
        SE::LocalPose oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 0.016f);
        oCapture.ExpectNoErrors();
        oCapture.ExpectLine("ghost_mask");
        for (uint32_t i = 0; i < 3; ++i) {
                EXPECT_NEAR(oPose.pPos[i].x, 5.0f, kPoseEps);
        }
        Alloc().reset();
}

TEST_F(FuncTestBase, LayerTest_MaskWeightsRestrictOverridePerBone) {
        // Skeleton mask "upper_body": bone0 = 0 (default), bone1 = 1, bone2 = 0.5.
        // w = 1: bone0 keeps base, bone1 gets layer, bone2 gets mix(1,5,0.5)=3.
        InstanceRig oRig(MakeLayerGraph(
                MakeConstAllBones(1.0f), MakeConstAllBones(5.0f), "upper_body", 1.0f));
        SkelRig oSkelRig("upper_body");

        SE::LocalPose oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 0.016f);
        EXPECT_NEAR(oPose.pPos[0].x, 1.0f, kPoseEps);
        EXPECT_NEAR(oPose.pPos[1].x, 5.0f, kPoseEps);
        EXPECT_NEAR(oPose.pPos[2].x, 3.0f, kPoseEps);
        Alloc().reset();
}

TEST_F(FuncTestBase, LayerTest_PartialWeightBlendsAllBones) {
        InstanceRig oRig(MakeLayerGraph(
                MakeConstAllBones(1.0f), MakeConstAllBones(5.0f), "", 0.5f));
        SkelRig oSkelRig;

        SE::LocalPose oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 0.016f);
        for (uint32_t i = 0; i < 3; ++i) {
                EXPECT_NEAR(oPose.pPos[i].x, 3.0f, kPoseEps) << "bone " << i;
        }
        Alloc().reset();
}

TEST_F(FuncTestBase, LayerTest_WeightParamOverridesStaticWeight) {
        InstanceRig oRig(MakeLayerGraph(
                MakeConstAllBones(1.0f), MakeConstAllBones(5.0f), "",
                /*weight=*/1.0f, /*weight_param=*/"aim_w"));
        SkelRig oSkelRig;

        oRig.inst.SetFloat(SE::StrID("aim_w"), 0.0f);
        SE::LocalPose oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 0.016f);
        EXPECT_NEAR(oPose.pPos[1].x, 1.0f, kPoseEps);   // layer fully faded out
        Alloc().reset();
}

TEST_F(FuncTestBase, LayerTest_AdditiveLayerUsesLayerReference) {
        // AdditiveLayer mode adds the layer's delta from the layer clip's own
        // frame-0 pose (same convention as AdditiveNodeData), scaled by the
        // per-bone mask weight. Layer ramps pos-x 3 -> 7 over [0,1], base = 1:
        // at t=1 bone1 (mask 1): 1 + (7-3) = 5; bone2 (mask 0.5): 1 + 4*0.5 = 3.
        auto pLayer = MakeClip(1.0f, false);
        for (uint16_t b = 0; b < 3; ++b) {
                AddRamp(*pLayer, b, 0, 1.0f, 3.0f, 7.0f);
        }
        InstanceRig oRig(MakeLayerGraph(
                MakeConstBaseAllBones(1.0f), std::move(pLayer), "upper_body", 1.0f,
                nullptr, SE::FlatBuffers::LayerBlendMode::AdditiveLayer));
        SkelRig oSkelRig("upper_body");

        SE::LocalPose oPose = EvalFrame(oRig.inst, oSkelRig.oSkel, Alloc(), 1.0f);
        EXPECT_NEAR(oPose.pPos[0].x, 1.0f, kPoseEps);   // mask 0 -> untouched
        EXPECT_NEAR(oPose.pPos[1].x, 5.0f, kPoseEps);
        EXPECT_NEAR(oPose.pPos[2].x, 3.0f, kPoseEps);
        Alloc().reset();
}

TEST_F(FuncTestBase, LayerTest_AdditiveLayerDeltaAtReferenceFrameIsZero) {
        // Same contract at the layer clip's reference frame: the contribution
        // is exactly zero (the old absolute-pose behavior added the full
        // frame-0 pose here).
        auto pLayer = MakeClip(1.0f, false);
        for (uint16_t b = 0; b < 3; ++b) {
                AddRamp(*pLayer, b, 0, 1.0f, 3.0f, 7.0f);
        }
        InstanceRig oRig(MakeLayerGraph(
                MakeConstBaseAllBones(1.0f), std::move(pLayer), "upper_body", 1.0f,
                nullptr, SE::FlatBuffers::LayerBlendMode::AdditiveLayer));
        SkelRig oSkelRig("upper_body");

        SE::LocalPose oPose = SE::AllocatePose(oSkelRig.oSkel.BoneCount(), Alloc());
        SE::InitBindPose(oPose, oSkelRig.oSkel);
        oRig.inst.EvaluateBlendTree(1.0f, oPose, Alloc(), oSkelRig.oSkel);   // time == 0
        SE::RenormalizeRotations(oPose);
        EXPECT_NEAR(oPose.pPos[1].x, 1.0f, kPoseEps);
        Alloc().reset();
}

TEST_F(FuncTestBase, LayerTest_MaskEntriesBeyondBoneCountAreDropped) {
        // CHARACTERIZATION: mask weights are padded to bone count at load and
        // entries for out-of-range bones are dropped — a mask naming only a
        // nonexistent bone loads as all-zeros, making the layer invisible.
        SkelPtr pSkelData = MakeChainSkeleton(3);
        AddMask(*pSkelData, "tail_only", {{50, 1.0f}});
        flatbuffers::FlatBufferBuilder oSkelBuilder;
        SE::Skeleton oSkel("blend_mask_skel", 0,
                           SerializeSkeleton(oSkelBuilder, *pSkelData));

        InstanceRig oRig(MakeLayerGraph(
                MakeConstAllBones(1.0f), MakeConstAllBones(5.0f), "tail_only", 1.0f));

        SE::LocalPose oPose = SE::AllocatePose(oSkel.BoneCount(), Alloc());
        SE::InitBindPose(oPose, oSkel);
        oRig.inst.EvaluateBlendTree(1.0f, oPose, Alloc(), oSkel);
        SE::RenormalizeRotations(oPose);
        for (uint32_t i = 0; i < 3; ++i) {
                EXPECT_NEAR(oPose.pPos[i].x, 1.0f, kPoseEps);   // layer invisible
        }
        Alloc().reset();
}

} // namespace
