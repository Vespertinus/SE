
// ---------------------------------------------------------------------------
// Tier B functional tests — determinism of the animation path.
//
// The animation evaluation is single-threaded, has no randomness and its time
// comes entirely from the caller, so two identical runs must be BITWISE
// identical (memcmp). Tolerance-based comparison would hide real divergence;
// the only bitwise-safe place is same-binary same-input runs.
// ---------------------------------------------------------------------------

#include <cstring>
#include <gtest/gtest.h>
#include <glm/glm.hpp>

#define SE_IMPL
#include <EngineFixture.h>
#include <FrameRunner.h>
#include <PoseAssert.h>

namespace {

using namespace se_test;

struct InstanceRig {
        flatbuffers::FlatBufferBuilder oBuilder;
        std::unique_ptr<SE::AnimGraph> pGraph;
        SE::AnimGraphInstance          inst;

        explicit InstanceRig(const GraphPtr& pNative) {
                pGraph = std::make_unique<SE::AnimGraph>(
                                "det_graph", 0, SerializeGraph(oBuilder, *pNative));
                inst.Init(*pGraph);
        }
};

struct SkelRig {
        SkelPtr                        pNative;
        flatbuffers::FlatBufferBuilder oBuilder;
        const SE::FlatBuffers::Skeleton* pFb = nullptr;
        SE::Skeleton                   oSkel;

        SkelRig()
                : pNative(MakeChainSkeleton(3)),
                  pFb(SerializeSkeleton(oBuilder, *pNative)),
                  oSkel("det_skel", 0, pFb) {}
};

ClipPtr MakeWalkClip(float duration) {
        auto pClip = MakeClip(duration, true);
        AddRamp(*pClip, 1, 0, duration, 0.0f, 1.0f);   // bone1 pos-x
        return pClip;
}

GraphPtr MakeWalkGraph() {
        // a ->(speed > 0.1, 0.2 s crossfade)-> b
        return MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("det_a", MakeWalkClip(1.0f)))}),
                 MakeState("b", {MakeClipNode(MakeClipLeaf("det_b", MakeWalkClip(2.0f)))})},
                {MakeTransition("a", "b", 0.2f,
                                {MakeCondition("speed", SE::FlatBuffers::ConditionOp::Greater, 0.1f)})},
                {MakeParam("speed", SE::FlatBuffers::AnimParamType::Float)});
}

// One scripted run: walk, raise speed (triggering the crossfade), keep walking.
// Captures the full pose bits each frame. Deterministic by construction: no
// wall-clock, no randomness, single thread.
std::vector<std::vector<uint8_t>> RunScriptedWalk(SE::FrameAllocator& oAlloc,
                                                  const SE::Skeleton& oSkel) {
        InstanceRig oRig(MakeWalkGraph());
        std::vector<std::vector<uint8_t>> vFrames;
        vFrames.reserve(200);

        auto capture = [&](const SE::LocalPose& oPose) {
                std::vector<uint8_t> v;
                v.reserve(oPose.bone_count * (12 + 16 + 12));
                auto append = [&v](const void* p, size_t n) {
                        const uint8_t* pBytes = static_cast<const uint8_t*>(p);
                        v.insert(v.end(), pBytes, pBytes + n);
                };
                append(oPose.pPos, oPose.bone_count * sizeof(glm::vec3));
                append(oPose.pRot, oPose.bone_count * sizeof(glm::quat));
                append(oPose.pScl, oPose.bone_count * sizeof(glm::vec3));
                vFrames.push_back(std::move(v));
        };

        for (int i = 0; i < 50; ++i) {   // walk in a
                capture(EvalFrame(oRig.inst, oSkel, oAlloc, 1.0f / 60.0f));
        }
        oRig.inst.SetFloat(SE::StrID("speed"), 1.0f);   // arm a -> b
        for (int i = 0; i < 150; ++i) {                 // crossfade + walk in b
                capture(EvalFrame(oRig.inst, oSkel, oAlloc, 1.0f / 60.0f));
        }
        oAlloc.reset();
        return vFrames;
}

// ===========================================================================
// Determinism
// ===========================================================================

TEST_F(FuncTestBase, DeterminismTest_TwoIdenticalRunsProduceBitwiseIdenticalPoses) {
        SkelRig oSkelRig;

        std::vector<std::vector<uint8_t>> vRunA = RunScriptedWalk(Alloc(), oSkelRig.oSkel);
        std::vector<std::vector<uint8_t>> vRunB = RunScriptedWalk(Alloc(), oSkelRig.oSkel);

        ASSERT_EQ(vRunA.size(), vRunB.size());
        for (size_t i = 0; i < vRunA.size(); ++i) {
                ASSERT_EQ(vRunA[i].size(), vRunB[i].size()) << "frame " << i;
                EXPECT_EQ(std::memcmp(vRunA[i].data(), vRunB[i].data(), vRunA[i].size()), 0)
                                << "frame " << i << " diverged";
        }
}

TEST_F(FuncTestBase, DeterminismTest_StateDeclarationOrderDoesNotAffectPoses) {
        // mStates/mTransitionsFrom are unordered maps; the FB state array order
        // must not influence evaluation. Same graph, states declared b,a vs a,b.
        SkelRig oSkelRig;

        InstanceRig oRigA(MakeGraph("a",
                {MakeState("a", {MakeClipNode(MakeClipLeaf("ord_a", MakeWalkClip(1.0f)))}),
                 MakeState("b", {MakeClipNode(MakeClipLeaf("ord_b", MakeWalkClip(2.0f)))})},
                {MakeTransition("a", "b", 0.2f,
                                {MakeCondition("speed", SE::FlatBuffers::ConditionOp::Greater, 0.1f)})}));
        InstanceRig oRigB(MakeGraph("a",
                {MakeState("b", {MakeClipNode(MakeClipLeaf("ord_b", MakeWalkClip(2.0f)))}),
                 MakeState("a", {MakeClipNode(MakeClipLeaf("ord_a", MakeWalkClip(1.0f)))})},
                {MakeTransition("a", "b", 0.2f,
                                {MakeCondition("speed", SE::FlatBuffers::ConditionOp::Greater, 0.1f)})}));

        for (int i = 0; i < 10; ++i) {
                EvalFrame(oRigA.inst, oSkelRig.oSkel, Alloc(), 1.0f / 60.0f);
                EvalFrame(oRigB.inst, oSkelRig.oSkel, Alloc(), 1.0f / 60.0f);
        }
        oRigA.inst.SetFloat(SE::StrID("speed"), 1.0f);
        oRigB.inst.SetFloat(SE::StrID("speed"), 1.0f);
        for (int i = 0; i < 40; ++i) {
                SE::LocalPose oPoseA = EvalFrame(oRigA.inst, oSkelRig.oSkel, Alloc(), 1.0f / 60.0f);
                std::vector<uint8_t> vA;
                vA.resize(oPoseA.bone_count * (12 + 16 + 12));
                std::memcpy(vA.data(), oPoseA.pPos, oPoseA.bone_count * (12 + 16 + 12));

                SE::LocalPose oPoseB = EvalFrame(oRigB.inst, oSkelRig.oSkel, Alloc(), 1.0f / 60.0f);
                EXPECT_EQ(std::memcmp(vA.data(), oPoseB.pPos,
                                      oPoseA.bone_count * (12 + 16 + 12)), 0)
                                << "frame " << i;
        }
        Alloc().reset();
}

TEST_F(FuncTestBase, DeterminismTest_FrameRateIndependenceOfStateProgress) {
        // CHARACTERIZATION: state time is frame-rate independent (0.1 s either
        // way), but transition PROGRESS lags by up to one dt: the transition
        // arms at the END of the matching frame, so the 1/30 run loses one
        // coarse step versus the 1/60 run. Tolerance = one coarse step.
        SkelRig oSkelRig;
        InstanceRig oRigA(MakeWalkGraph());
        InstanceRig oRigB(MakeWalkGraph());
        oRigA.inst.SetFloat(SE::StrID("speed"), 1.0f);
        oRigB.inst.SetFloat(SE::StrID("speed"), 1.0f);

        for (int i = 0; i < 6; ++i) oRigA.inst.Update(1.0f / 60.0f);
        for (int i = 0; i < 3; ++i) oRigB.inst.Update(1.0f / 30.0f);

        EXPECT_TRUE(oRigA.inst.IsTransitioning());
        EXPECT_TRUE(oRigB.inst.IsTransitioning());
        EXPECT_NEAR(oRigA.inst.GetCurrentTime(), oRigB.inst.GetCurrentTime(), kTimeEps);
        const float one_coarse_step = (1.0f / 30.0f) / 0.2f;
        EXPECT_NEAR(oRigA.inst.TransitionProgress(), oRigB.inst.TransitionProgress(),
                    one_coarse_step + kTimeEps);
}

TEST_F(FuncTestBase, DeterminismTest_LongRunStaysInLoopBounds) {
        // 10 000 frames (~2.8 min of sim): fmod-wrapped time must never escape
        // [0, dur) — a NaN or runaway accumulator fails both comparisons.
        InstanceRig oRig(MakeSingleStateGraph("a", "det_loop", MakeWalkClip(2.0f)));

        bool in_bounds = true;
        for (int i = 0; i < 10000; ++i) {
                oRig.inst.Update(1.0f / 60.0f);
                const float t = oRig.inst.GetCurrentTime();
                if (!(t >= 0.0f && t < 2.0f)) {
                        in_bounds = false;
                        break;
                }
        }
        EXPECT_TRUE(in_bounds);
        EXPECT_LT(oRig.inst.GetCurrentTime(), 2.0f);
}

} // namespace
