
// ---------------------------------------------------------------------------
// Tier A functional tests — core/AnimEvaluator.h primitives.
//
// No engine systems: pure functions over stack-built in-memory assets and a
// local FrameAllocator. Expected values are hand-computed from the SUT
// (SampleCurve's Hermite basis, SampleClip target mapping, etc.).
// ---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/constants.hpp>

#define SE_IMPL
#include <Logging.h>
#include <Allocator.h>
#include <Allocator.tcc>
#include <AnimClip.tcc>
#include <Skeleton.tcc>
#include <AnimEvaluator.h>

#include <AnimationAssets.h>
#include <PoseAssert.h>

namespace {

using namespace se_test;

// A skeleton + bind-seeded pose over a local frame allocator.
struct PoseRig {
        SkelPtr                           pNative;
        flatbuffers::FlatBufferBuilder    oBuilder;
        const SE::FlatBuffers::Skeleton*  pFb = nullptr;
        SE::Skeleton                      oSkel;
        SE::FrameAllocator                oAlloc{64 * 1024};
        SE::LocalPose                     oPose;

        explicit PoseRig(uint16_t n_bones = 3)
                : pNative(MakeChainSkeleton(n_bones)),
                  pFb(SerializeSkeleton(oBuilder, *pNative)),
                  oSkel("evaluator_rig", 0, pFb),
                  oPose(SE::AllocatePose(n_bones, oAlloc)) {
                SE::InitBindPose(oPose, oSkel);
        }

        ~PoseRig() noexcept { oAlloc.reset(); }

        // A clip with a single linear ramp channel.
        static SE::AnimClip MakeRampClip(float duration, bool looping,
                                         uint16_t bone, uint8_t target,
                                         float v_from, float v_to,
                                         bool delta = false, float src_pelvis = 0.0f) {
                auto pClip = MakeClip(duration, looping);
                AddRamp(*pClip, bone, target, duration, v_from, v_to);
                pClip->delta_translations = delta;
                pClip->src_pelvis_scale   = src_pelvis;
                flatbuffers::FlatBufferBuilder oClipBuilder;
                const auto* pFbClip = SerializeClip(oClipBuilder, *pClip);
                return SE::AnimClip("ramp_clip", 0, pFbClip);
        }
};

// ---------------------------------------------------------------------------
// SampleCurve
// ---------------------------------------------------------------------------

TEST(SampleCurveTest, ConstantFormatIgnoresTime) {
        SE::AnimClip::CurveChannel oCh;
        oCh.format = SE::AnimClip::Format::ConstantF32;
        oCh.vTimes = {0.0f, 0.5f, 1.0f};
        oCh.vValues = {7.0f, 8.0f, 9.0f};
        float val = -1.0f;
        SE::SampleCurve(oCh, 0.75f, val);
        EXPECT_FLOAT_EQ(val, 7.0f);
}

TEST(SampleCurveTest, SingleKeyClampsToValue) {
        SE::AnimClip::CurveChannel oCh;
        oCh.format = SE::AnimClip::Format::LinearF32;
        oCh.vTimes = {0.25f};
        oCh.vValues = {3.5f};
        float val = 0.0f;
        SE::SampleCurve(oCh, 0.0f, val);
        EXPECT_FLOAT_EQ(val, 3.5f);
        SE::SampleCurve(oCh, 1.0f, val);
        EXPECT_FLOAT_EQ(val, 3.5f);
}

TEST(SampleCurveTest, LinearMidpointIsExactLerp) {
        SE::AnimClip::CurveChannel oCh;
        oCh.format = SE::AnimClip::Format::LinearF32;
        oCh.vTimes = {0.0f, 1.0f};
        oCh.vValues = {2.0f, 6.0f};
        float val = 0.0f;
        SE::SampleCurve(oCh, 0.5f, val);
        EXPECT_NEAR(val, 4.0f, kPoseEps);
        SE::SampleCurve(oCh, 0.25f, val);
        EXPECT_NEAR(val, 3.0f, kPoseEps);
}

TEST(SampleCurveTest, LinearClampsOutsideKeyRange) {
        SE::AnimClip::CurveChannel oCh;
        oCh.format = SE::AnimClip::Format::LinearF32;
        oCh.vTimes = {0.0f, 1.0f};
        oCh.vValues = {2.0f, 6.0f};
        float val = 0.0f;
        SE::SampleCurve(oCh, -1.0f, val);
        EXPECT_NEAR(val, 2.0f, kPoseEps);
        SE::SampleCurve(oCh, 99.0f, val);
        EXPECT_NEAR(val, 6.0f, kPoseEps);
}

TEST(SampleCurveTest, StepReturnsLeftKeyOfBracket) {
        SE::AnimClip::CurveChannel oCh;
        oCh.format = SE::AnimClip::Format::StepF32;
        oCh.vTimes = {0.0f, 1.0f, 2.0f};
        oCh.vValues = {10.0f, 20.0f, 30.0f};
        float val = 0.0f;
        SE::SampleCurve(oCh, 1.5f, val);
        EXPECT_FLOAT_EQ(val, 20.0f);
        SE::SampleCurve(oCh, 1.0f, val);   // exactly on a key: still the left bracket's value
        EXPECT_FLOAT_EQ(val, 20.0f);
}

TEST(SampleCurveTest, HermiteWithoutTangentsIsSmoothstep) {
        SE::AnimClip::CurveChannel oCh;
        oCh.format = SE::AnimClip::Format::HermiteF32;
        oCh.vTimes = {0.0f, 1.0f};
        oCh.vValues = {0.0f, 1.0f};
        float val = 0.0f;
        SE::SampleCurve(oCh, 0.5f, val);
        EXPECT_NEAR(val, 0.5f, kPoseEps);   // h00(0.5)*0 + h01(0.5)*1 = 0.5
}

TEST(SampleCurveTest, HermiteWithTangentsFollowsCubic) {
        SE::AnimClip::CurveChannel oCh;
        oCh.format = SE::AnimClip::Format::HermiteF32;
        oCh.vTimes = {0.0f, 1.0f};
        oCh.vValues = {0.0f, 1.0f};
        oCh.vTangents = {1.0f, 0.0f};
        float val = 0.0f;
        // out = h00*p0 + h10*tan0 + h01*p1 + h11*tan1 = 0 + 0.125 + 0.5 + 0
        SE::SampleCurve(oCh, 0.5f, val);
        EXPECT_NEAR(val, 0.625f, kPoseEps);
}

TEST(SampleCurveTest, EmptyChannelWritesZero) {
        SE::AnimClip::CurveChannel oCh;
        float val = 42.0f;
        SE::SampleCurve(oCh, 0.5f, val);
        EXPECT_FLOAT_EQ(val, 0.0f);
}

// ---------------------------------------------------------------------------
// SampleClip
// ---------------------------------------------------------------------------

TEST(SampleClipTest, WritesOnlyChannelTargets) {
        PoseRig oRig;
        auto oClip = PoseRig::MakeRampClip(1.0f, false, 0, 0, 0.0f, 1.0f);   // bone0 pos-x
        SE::SampleClip(oClip, 0.5f, oRig.oPose);

        EXPECT_NEAR(oRig.oPose.pPos[0].x, 0.5f, kPoseEps);
        EXPECT_NEAR(oRig.oPose.pPos[0].y, 0.0f, kPoseEps);   // untouched
        EXPECT_NEAR(oRig.oPose.pPos[1].x, 1.0f, kPoseEps);   // bone1 keeps bind
        EXPECT_QUAT_NEAR(oRig.oPose.pRot[0], glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        EXPECT_VEC_NEAR(oRig.oPose.pScl[0], glm::vec3(1.0f));
}

TEST(SampleClipTest, RotationTargetMapsToQuaternionComponents) {
        PoseRig oRig;
        auto oClip = PoseRig::MakeRampClip(1.0f, false, 1, 6, 0.0f, 1.0f);   // bone1 rot w
        SE::SampleClip(oClip, 1.0f, oRig.oPose);
        EXPECT_NEAR(oRig.oPose.pRot[1].w, 1.0f, kPoseEps);
        EXPECT_NEAR(oRig.oPose.pRot[1].x, 0.0f, kPoseEps);
        EXPECT_NEAR(oRig.oPose.pRot[1].y, 0.0f, kPoseEps);
        EXPECT_NEAR(oRig.oPose.pRot[1].z, 0.0f, kPoseEps);
}

TEST(SampleClipTest, ScaleTargetMapsToScaleComponents) {
        PoseRig oRig;
        auto oClip = PoseRig::MakeRampClip(1.0f, false, 0, 7, 1.0f, 2.0f);   // bone0 scl x
        SE::SampleClip(oClip, 1.0f, oRig.oPose);
        EXPECT_NEAR(oRig.oPose.pScl[0].x, 2.0f, kPoseEps);
        EXPECT_NEAR(oRig.oPose.pScl[0].y, 1.0f, kPoseEps);
}

TEST(SampleClipTest, ChannelBeyondPoseBoneCountIsSkipped) {
        PoseRig oRig;   // 3 bones
        auto oClip = PoseRig::MakeRampClip(1.0f, false, 5, 0, 0.0f, 99.0f);
        SE::SampleClip(oClip, 1.0f, oRig.oPose);   // must not write out of bounds
        EXPECT_VEC_NEAR(oRig.oPose.pPos[0], glm::vec3(0.0f));
}

TEST(SampleClipTest, DeltaTranslationAddsScaledByPelvisRatio) {
        PoseRig oRig;   // chain skeleton: bone1 bindPos = (1,0,0) -> |pelvis| = 1
        auto oClip = PoseRig::MakeRampClip(1.0f, false, 1, 0, 0.0f, 1.0f,
                                           /*delta=*/true, /*src_pelvis=*/2.0f);
        SE::SampleClip(oClip, 0.5f, oRig.oPose);
        // uniform_scale = 1 / 2 = 0.5; pPos[1].x = bind(1) + 0.5 * 0.5
        EXPECT_NEAR(oRig.oPose.pPos[1].x, 1.25f, kPoseEps);
}

TEST(SampleClipTest, AbsoluteTranslationReplacesBind) {
        PoseRig oRig;
        auto oClip = PoseRig::MakeRampClip(1.0f, false, 1, 0, 0.0f, 1.0f);
        SE::SampleClip(oClip, 0.5f, oRig.oPose);
        EXPECT_NEAR(oRig.oPose.pPos[1].x, 0.5f, kPoseEps);
}

// ---------------------------------------------------------------------------
// BlendPoses
// ---------------------------------------------------------------------------

TEST(BlendPosesTest, EndpointsAndMidpoint) {
        PoseRig oRig;
        SE::LocalPose oA = SE::AllocatePose(3, oRig.oAlloc);
        SE::InitBindPose(oA, oRig.oSkel);
        SE::LocalPose oB = SE::AllocatePose(3, oRig.oAlloc);
        SE::InitBindPose(oB, oRig.oSkel);

        const glm::quat qZ90 = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0.0f, 0.0f, 1.0f));
        for (uint32_t i = 0; i < 3; ++i) {
                oA.pPos[i] = glm::vec3(0.0f);
                oB.pPos[i] = glm::vec3(2.0f, 4.0f, 6.0f);
                oB.pRot[i] = qZ90;
        }

        SE::LocalPose oOut = SE::AllocatePose(3, oRig.oAlloc);

        SE::BlendPoses(oA, oB, 0.0f, oOut);
        for (uint32_t i = 0; i < 3; ++i) {
                EXPECT_VEC_NEAR(oOut.pPos[i], glm::vec3(0.0f));
                EXPECT_QUAT_NEAR(oOut.pRot[i], glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        }

        SE::BlendPoses(oA, oB, 1.0f, oOut);
        for (uint32_t i = 0; i < 3; ++i) {
                EXPECT_VEC_NEAR(oOut.pPos[i], glm::vec3(2.0f, 4.0f, 6.0f));
                EXPECT_QUAT_NEAR(oOut.pRot[i], qZ90);
        }

        SE::BlendPoses(oA, oB, 0.5f, oOut);
        const glm::quat qZ45 = glm::angleAxis(glm::quarter_pi<float>(), glm::vec3(0.0f, 0.0f, 1.0f));
        for (uint32_t i = 0; i < 3; ++i) {
                EXPECT_VEC_NEAR(oOut.pPos[i], glm::vec3(1.0f, 2.0f, 3.0f));
                EXPECT_QUAT_NEAR(oOut.pRot[i], qZ45);
        }
        oRig.oAlloc.reset();
}

TEST(BlendPosesTest, SlerpTakesShortestArc) {
        PoseRig oRig;
        SE::LocalPose oA = SE::AllocatePose(1, oRig.oAlloc);
        SE::InitBindPose(oA, oRig.oSkel);
        SE::LocalPose oB = SE::AllocatePose(1, oRig.oAlloc);
        SE::InitBindPose(oB, oRig.oSkel);
        SE::LocalPose oOut = SE::AllocatePose(1, oRig.oAlloc);

        const glm::vec3 z(0.0f, 0.0f, 1.0f);
        oA.pRot[0] = glm::angleAxis(glm::radians(350.0f), z);   // == -10 deg
        oB.pRot[0] = glm::angleAxis(glm::radians(10.0f), z);

        SE::BlendPoses(oA, oB, 0.5f, oOut);
        // midpoint of -10 and +10 through the short arc is 0 deg, not 180
        const glm::quat qId = glm::angleAxis(0.0f, z);
        EXPECT_QUAT_NEAR(oOut.pRot[0], qId);
        oRig.oAlloc.reset();
}

// ---------------------------------------------------------------------------
// RenormalizeRotations
// ---------------------------------------------------------------------------

TEST(RenormalizeRotationsTest, RestoresUnitLength) {
        PoseRig oRig;
        const glm::quat qZ90 = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0.0f, 0.0f, 1.0f));
        for (uint32_t i = 0; i < 3; ++i) {
                oRig.oPose.pRot[i] = qZ90 * 3.0f;   // degenerate length 3
        }
        SE::RenormalizeRotations(oRig.oPose);
        for (uint32_t i = 0; i < 3; ++i) {
                EXPECT_QUAT_NEAR(oRig.oPose.pRot[i], qZ90);
        }
}

TEST(RenormalizeRotationsTest, ZeroQuaternionFallsBackToIdentity) {
        PoseRig oRig;
        for (uint32_t i = 0; i < 3; ++i) {
                oRig.oPose.pRot[i] = glm::quat(0.0f, 0.0f, 0.0f, 0.0f);
        }
        SE::RenormalizeRotations(oRig.oPose);
        for (uint32_t i = 0; i < 3; ++i) {
                EXPECT_QUAT_NEAR(oRig.oPose.pRot[i], glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        }
}

// ---------------------------------------------------------------------------
// AllocatePose / FrameAllocator
// ---------------------------------------------------------------------------

TEST(AllocatePoseTest, UsesFrameAllocatorAndTracksHighWater) {
        SE::FrameAllocator oAlloc(4096);
        SE::LocalPose oPose = SE::AllocatePose(3, oAlloc);
        EXPECT_NE(oPose.pPos, nullptr);
        EXPECT_NE(oPose.pRot, nullptr);
        EXPECT_NE(oPose.pScl, nullptr);
        EXPECT_EQ(oPose.bone_count, 3u);
        // 3 bones * (12 + 16 + 12) bytes + alignment padding
        EXPECT_GE(oAlloc.used(), 3u * (12 + 16 + 12));
        EXPECT_EQ(oAlloc.high_water(), oAlloc.used());
}

TEST(AllocatePoseTest, ResetAllowsReallocation) {
        SE::FrameAllocator oAlloc(4096);
        SE::AllocatePose(65, oAlloc);
        const size_t used_first = oAlloc.used();
        oAlloc.reset();
        EXPECT_EQ(oAlloc.used(), 0u);
        EXPECT_EQ(oAlloc.high_water(), used_first);   // high water survives reset
        SE::AllocatePose(65, oAlloc);
        EXPECT_EQ(oAlloc.used(), used_first);
}

TEST(AllocatePoseTest, ExhaustedAllocatorThrowsBadAlloc) {
        SE::FrameAllocator oAlloc(64);
        EXPECT_THROW(SE::AllocatePose(3, oAlloc), std::bad_alloc);
}

// ---------------------------------------------------------------------------
// InitBindPose
// ---------------------------------------------------------------------------

TEST(InitBindPoseTest, FillsFromSkeletonBones) {
        PoseRig oRig;
        const auto& vBones = oRig.oSkel.Bones();
        for (uint32_t i = 0; i < oRig.oPose.bone_count; ++i) {
                EXPECT_VEC_NEAR(oRig.oPose.pPos[i], vBones[i].bindPos);
                EXPECT_QUAT_NEAR(oRig.oPose.pRot[i], vBones[i].bindRot);
                EXPECT_VEC_NEAR(oRig.oPose.pScl[i], vBones[i].bindScale);
        }
}

TEST(InitBindPoseTest, ChainSkeletonLayoutIsDeterministic) {
        PoseRig oRig;
        EXPECT_EQ(oRig.oSkel.BoneCount(), 3u);
        EXPECT_EQ(oRig.oSkel.Bones()[0].parentIndex, SE::Skeleton::kNoParent);
        EXPECT_EQ(oRig.oSkel.Bones()[1].parentIndex, 0);
        EXPECT_EQ(oRig.oSkel.Bones()[2].parentIndex, 1);
        EXPECT_VEC_NEAR(oRig.oSkel.Bones()[2].bindPos, glm::vec3(2.0f, 0.0f, 0.0f));
}

// ---------------------------------------------------------------------------
// CheckAnimEvents
// ---------------------------------------------------------------------------

TEST(CheckAnimEventsTest, FiresEventsInRange) {
        auto pClipData = MakeClip(1.0f, true);
        AddEvent(*pClipData, 0.25f, "a", 1.0f);
        AddEvent(*pClipData, 0.50f, "b", 2.0f);
        AddEvent(*pClipData, 0.75f, "c", 3.0f);
        flatbuffers::FlatBufferBuilder oBuilder;
        SE::AnimClip oClip("ev_clip", 0, SerializeClip(oBuilder, *pClipData));

        std::vector<std::string> vFired;
        SE::CheckAnimEvents(oClip, 0.1f, 0.6f, true, [&](const SE::AnimClip::AnimEvent& oEv) {
                vFired.push_back(oEv.name);
        });
        ASSERT_EQ(vFired.size(), 2u);
        EXPECT_EQ(vFired[0], "a");
        EXPECT_EQ(vFired[1], "b");
}

TEST(CheckAnimEventsTest, LoopWrapFiresTailThenHead) {
        auto pClipData = MakeClip(1.0f, true);
        AddEvent(*pClipData, 0.95f, "tail", 1.0f);
        AddEvent(*pClipData, 0.05f, "head", 2.0f);
        flatbuffers::FlatBufferBuilder oBuilder;
        SE::AnimClip oClip("ev_clip", 0, SerializeClip(oBuilder, *pClipData));

        std::vector<std::string> vFired;
        SE::CheckAnimEvents(oClip, 0.9f, 0.1f, true, [&](const SE::AnimClip::AnimEvent& oEv) {
                vFired.push_back(oEv.name);
        });
        ASSERT_EQ(vFired.size(), 2u);
        EXPECT_EQ(vFired[0], "tail");
        EXPECT_EQ(vFired[1], "head");
}

TEST(CheckAnimEventsTest, NonLoopingDoesNotWrap) {
        auto pClipData = MakeClip(1.0f, false);
        AddEvent(*pClipData, 0.95f, "tail", 1.0f);
        AddEvent(*pClipData, 0.05f, "head", 2.0f);
        flatbuffers::FlatBufferBuilder oBuilder;
        SE::AnimClip oClip("ev_clip", 0, SerializeClip(oBuilder, *pClipData));

        std::vector<std::string> vFired;
        SE::CheckAnimEvents(oClip, 0.9f, 0.1f, false, [&](const SE::AnimClip::AnimEvent& oEv) {
                vFired.push_back(oEv.name);
        });
        EXPECT_EQ(vFired.size(), 0u);
}

TEST(CheckAnimEventsTest, RangeIsHalfOpenStartExclusive) {
        auto pClipData = MakeClip(1.0f, true);
        AddEvent(*pClipData, 0.5f, "mid", 1.0f);
        flatbuffers::FlatBufferBuilder oBuilder;
        SE::AnimClip oClip("ev_clip", 0, SerializeClip(oBuilder, *pClipData));

        int fire_count = 0;
        // start is exclusive: an event exactly at prevTime is NOT re-fired
        SE::CheckAnimEvents(oClip, 0.5f, 0.6f, true, [&](const SE::AnimClip::AnimEvent&) { ++fire_count; });
        EXPECT_EQ(fire_count, 0);
        // end is inclusive: an event exactly at curTime IS fired
        SE::CheckAnimEvents(oClip, 0.4f, 0.5f, true, [&](const SE::AnimClip::AnimEvent&) { ++fire_count; });
        EXPECT_EQ(fire_count, 1);
}

} // namespace
