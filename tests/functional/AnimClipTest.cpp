
// ---------------------------------------------------------------------------
// AnimClip / Skeleton decode tests — Tier A (no engine systems).
//
// In-memory FlatBuffers for decode semantics; real baked assets from
// resource/ for the smoke tier (the first tests in the repo to read asset
// files — the binary must run with CWD = repo root, which CMake pins).
// ---------------------------------------------------------------------------

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>

#include <gtest/gtest.h>
#include <flatbuffers/flatbuffers.h>

#define SE_IMPL
#include <Logging.h>
#include <AnimClip.tcc>
#include <Skeleton.tcc>

#include <AnimationAssets.h>
#include <PoseAssert.h>

namespace {

using namespace se_test;

// ===========================================================================
// Decode from in-memory FlatBuffers
// ===========================================================================

TEST(ClipDecodeTest, DurationAndLoopingFromFlatBuffer) {
        auto pClipData = MakeClip(2.5f, true);
        flatbuffers::FlatBufferBuilder oBuilder;
        SE::AnimClip oClip("clip", 0, SerializeClip(oBuilder, *pClipData));
        EXPECT_FLOAT_EQ(oClip.Duration(), 2.5f);
        EXPECT_TRUE(oClip.Looping());
}

TEST(ClipDecodeTest, ChannelsCopiedIntoRuntimeStruct) {
        auto pClipData = MakeClip(1.0f, false);
        AddRamp(*pClipData, 0, 0, 1.0f, 0.0f, 1.0f);
        AddChannel(*pClipData, 1, 5, {0.0f, 1.0f}, {0.0f, 1.0f},
                   SE::FlatBuffers::CurveFormat::StepF32);
        AddChannel(*pClipData, 2, 3, {0.0f, 1.0f}, {0.0f, 1.0f},
                   SE::FlatBuffers::CurveFormat::HermiteF32, {0.5f, -0.5f});
        flatbuffers::FlatBufferBuilder oBuilder;
        SE::AnimClip oClip("clip", 0, SerializeClip(oBuilder, *pClipData));

        const auto& vChannels = oClip.Channels();
        ASSERT_EQ(vChannels.size(), 3u);
        EXPECT_EQ(vChannels[0].bone_index, 0);
        EXPECT_EQ(vChannels[0].target, 0);
        EXPECT_EQ(vChannels[0].format, SE::AnimClip::Format::LinearF32);
        EXPECT_EQ(vChannels[1].format, SE::AnimClip::Format::StepF32);
        EXPECT_EQ(vChannels[2].format, SE::AnimClip::Format::HermiteF32);
        ASSERT_EQ(vChannels[2].vTangents.size(), 2u);
        EXPECT_NEAR(vChannels[2].vTangents[1], -0.5f, kPoseEps);
}

TEST(ClipDecodeTest, Quantized16DequantizedToHermite) {
        auto pClipData = MakeClip(1.0f, false);
        // raw values in [0,1], dequant range [-2, 2]: raw*4 + (-2)
        AddQuantizedChannel(*pClipData, 0, 0, {0.0f, 1.0f}, {0.0f, 1.0f}, -2.0f, 2.0f);
        flatbuffers::FlatBufferBuilder oBuilder;
        SE::AnimClip oClip("clip", 0, SerializeClip(oBuilder, *pClipData));

        const auto& oCh = oClip.Channels()[0];
        EXPECT_EQ(oCh.format, SE::AnimClip::Format::HermiteF32);
        ASSERT_EQ(oCh.vValues.size(), 2u);
        EXPECT_NEAR(oCh.vValues[0], -2.0f, kPoseEps);
        EXPECT_NEAR(oCh.vValues[1],  2.0f, kPoseEps);
        EXPECT_TRUE(oCh.vTangents.empty());
}

TEST(ClipDecodeTest, EventsSortedByTimeAndNameIdHashed) {
        auto pClipData = MakeClip(2.0f, true);
        AddEvent(*pClipData, 1.5f, "later", 1.0f);
        AddEvent(*pClipData, 0.25f, "earlier", 2.0f);
        flatbuffers::FlatBufferBuilder oBuilder;
        SE::AnimClip oClip("clip", 0, SerializeClip(oBuilder, *pClipData));

        const auto& vEvents = oClip.Events();
        ASSERT_EQ(vEvents.size(), 2u);
        EXPECT_NEAR(vEvents[0].time, 0.25f, kTimeEps);   // re-sorted at load
        EXPECT_EQ(vEvents[0].name, "earlier");
        EXPECT_TRUE(vEvents[0].nameID == SE::StrID("earlier"));
        EXPECT_FLOAT_EQ(vEvents[1].value, 1.0f);
}

// NOTE: the `!fbEv->name()` guard in AnimClip::LoadFromFB is defensive only —
// AnimEvent.name is a `required` schema field, so a null name cannot appear in
// a buffer that passes verification (flatbuffers asserts at build time).

TEST(ClipDecodeTest, DeltaTranslationFlagsPreserved) {
        auto pClipData = MakeClip(1.0f, false);
        AddRamp(*pClipData, 1, 0, 1.0f, 0.0f, 1.0f);
        pClipData->delta_translations = true;
        pClipData->src_pelvis_scale   = 1.75f;
        flatbuffers::FlatBufferBuilder oBuilder;
        SE::AnimClip oClip("clip", 0, SerializeClip(oBuilder, *pClipData));
        EXPECT_TRUE(oClip.DeltaTranslations());
        EXPECT_FLOAT_EQ(oClip.SrcPelvisScale(), 1.75f);
}

// ===========================================================================
// Real baked assets (CWD = repo root)
// ===========================================================================

TEST(ClipLoadTest, RealSeakFileLoads) {
        SE::AnimClip oClip("resource/animation/ual1_Idle_Loop.seak", 0);
        EXPECT_GT(oClip.Duration(), 0.0f);
        EXPECT_LT(oClip.Duration(), 10.0f);
        EXPECT_FALSE(oClip.Channels().empty());
        EXPECT_TRUE(oClip.Looping());
}

TEST(ClipLoadTest, LocomotionSetIsLoopingAndSane) {
        for (const char* name : {"resource/animation/ual1_Idle_Loop.seak",
                                 "resource/animation/ual1_Walk_Loop.seak",
                                 "resource/animation/ual1_Sprint_Loop.seak"}) {
                SCOPED_TRACE(name);
                SE::AnimClip oClip(name, 0);
                EXPECT_GT(oClip.Duration(), 0.0f);
                EXPECT_LT(oClip.Duration(), 10.0f);
                EXPECT_TRUE(oClip.Looping());
                EXPECT_FALSE(oClip.Channels().empty());
        }
}

TEST(ClipLoadTest, NonLoopingJumpStartClip) {
        // CHARACTERIZATION: ual1_Jump_Start is a one-shot clip in the shipped set.
        SE::AnimClip oClip("resource/animation/ual1_Jump_Start.seak", 0);
        EXPECT_GT(oClip.Duration(), 0.0f);
        EXPECT_FALSE(oClip.Looping());
}

TEST(ClipLoadTest, MissingFileThrows) {
        EXPECT_ANY_THROW(SE::AnimClip oClip("resource/animation/does_not_exist.seak", 0));
}

TEST(ClipLoadTest, CorruptFileFailsVerification) {
        const char* sTempPath = "/tmp/se_test_corrupt.seak";
        {
                std::ofstream oFile(sTempPath, std::ios::binary);
                const char sGarbage[64] = {};
                oFile.write(sGarbage, sizeof(sGarbage));
        }
        EXPECT_ANY_THROW(SE::AnimClip oClip(sTempPath, 0));
        std::remove(sTempPath);
}

TEST(SkeletonLoadTest, Ual1SkeletonLoads65Joints) {
        SE::Skeleton oSkel("resource/animation/ual1_skeleton.sesk", 0);
        EXPECT_EQ(oSkel.BoneCount(), 65u);
        for (uint32_t i = 0; i < oSkel.BoneCount(); ++i) {
                const uint16_t parent = oSkel.Bones()[i].parentIndex;
                if (parent != SE::Skeleton::kNoParent) {
                        ASSERT_LT(parent, i) << "topological order violated at bone " << i;
                }
        }
}

TEST(SkeletonLoadTest, CanonicalHumanoidLoads65Joints) {
        SE::Skeleton oSkel("resource/animation/canonical_humanoid.sesk", 0);
        EXPECT_EQ(oSkel.BoneCount(), 65u);
}

TEST(SkeletonLoadTest, ShippedSkeletonsDefineUpperBodyMask) {
        // The shipped `upper_body` mask (spine_01 and up; root/pelvis/legs at 0)
        // is required by character.seag's aiming layer — without it the layer
        // silently overrides every bone. Guard every skeleton used with the
        // character graph. Regenerate via scripts/process_animation.sh.
        for (const char* pPath : {"resource/animation/ual1_skeleton.sesk",
                                  "resource/animation/canonical_humanoid.sesk",
                                  "resource/model/male_fullbody_skeleton.sesk",
                                  "resource/model/female_fullbody_skeleton.sesk"}) {
                SCOPED_TRACE(pPath);
                SE::Skeleton oSkel(pPath, 0);
                const SE::Skeleton::BoneMask* pMask = oSkel.FindMask(SE::StrID("upper_body"));
                ASSERT_NE(pMask, nullptr);
                ASSERT_EQ(pMask->weights.size(), oSkel.BoneCount());

                // spine_01 / neck / head included...
                EXPECT_NEAR(pMask->weights[2], 1.0f, kWeightEps);   // spine_01
                EXPECT_NEAR(pMask->weights[6], 1.0f, kWeightEps);   // Head
                // ...legs and root chain excluded (locomotion keeps driving them)
                EXPECT_NEAR(pMask->weights[0],  0.0f, kWeightEps);  // root
                EXPECT_NEAR(pMask->weights[1],  0.0f, kWeightEps);  // pelvis
                EXPECT_NEAR(pMask->weights[55], 0.0f, kWeightEps);  // thigh_l
                EXPECT_NEAR(pMask->weights[60], 0.0f, kWeightEps);  // thigh_r
        }
}

TEST(SkeletonLoadTest, InMemoryMaskIsFound) {
        // Control for the characterization above: masks do load when present.
        auto pSkelData = MakeChainSkeleton(4);
        AddMask(*pSkelData, "upper_body", {{2, 1.0f}, {3, 0.5f}});
        flatbuffers::FlatBufferBuilder oBuilder;
        SE::Skeleton oSkel("skel", 0, SerializeSkeleton(oBuilder, *pSkelData));

        const SE::Skeleton::BoneMask* pMask = oSkel.FindMask(SE::StrID("upper_body"));
        ASSERT_NE(pMask, nullptr);
        ASSERT_EQ(pMask->weights.size(), 4u);   // padded to bone count
        EXPECT_NEAR(pMask->weights[0], 0.0f, kWeightEps);
        EXPECT_NEAR(pMask->weights[2], 1.0f, kWeightEps);
        EXPECT_NEAR(pMask->weights[3], 0.5f, kWeightEps);
        EXPECT_EQ(oSkel.FindMask(SE::StrID("missing")), nullptr);
}

// ===========================================================================
// Quaternion sign continuity (the "leg flips behind the head" fix).
//
// Rotation channels interpolate per component; a sign flip between adjacent
// keys (q and -q are the same rotation) sweeps the limb the long way round.
// LoadFromFB negates flipped keys so interpolation follows the short arc —
// the same fixup UE5/Unity importers apply at import time.
// ===========================================================================

namespace {

// 3-key rotation: identity -> 90° around Y -> the SAME rotation sign-flipped.
constexpr float kS2 = 0.7071067811865476f;
const std::vector<float> vTimes3 = {0.0f, 0.5f, 1.0f};
// per axis: x, y, z, w
const std::vector<float> vKeyValues[4] = {
        {0.0f, 0.0f,  0.0f},   // x — three keys
        {0.0f, kS2,  -kS2},    // y  (key 2 flipped)
        {0.0f, 0.0f,  0.0f},   // z
        {1.0f, kS2,  -kS2},    // w  (key 2 flipped)
};

ClipPtr MakeFlippedClip(SE::FlatBuffers::CurveFormat eFormat,
                        const std::vector<float>& vTangents = {}) {
        auto pClip = MakeClip(1.0f, true);
        for (uint8_t axis = 0; axis < 4; ++axis) {
                AddChannel(*pClip, 0, static_cast<uint8_t>(3 + axis), vTimes3,
                           vKeyValues[axis], eFormat, vTangents);
        }
        return pClip;
}

} // namespace

TEST(ClipContinuityTest, SignFlippedKeyNegatedAtLoad) {
        flatbuffers::FlatBufferBuilder oBuilder;
        SE::AnimClip oClip("clip", 0,
                        SerializeClip(oBuilder, *MakeFlippedClip(
                                        SE::FlatBuffers::CurveFormat::LinearF32)));

        // Collect the runtime rot channels (they keep bone/target order).
        const SE::AnimClip::CurveChannel* pCh[4] = {nullptr, nullptr, nullptr, nullptr};
        for (const auto& ch : oClip.Channels()) {
                if (ch.bone_index == 0 && ch.target >= 3 && ch.target <= 6) {
                        pCh[ch.target - 3] = &ch;
                }
        }
        ASSERT_NE(pCh[0], nullptr);

        // Key 2 (negated 90°Y) must have been flipped back into key 1's
        // hemisphere: values equal key 1, so adjacent dots are positive.
        for (int k = 0; k < 3; ++k) {
                EXPECT_NEAR(pCh[1]->vValues[k], vKeyValues[1][k == 2 ? 1 : k], kPoseEps) << "y key " << k;
                EXPECT_NEAR(pCh[3]->vValues[k], vKeyValues[3][k == 2 ? 1 : k], kPoseEps) << "w key " << k;
        }
        float dot = 0.0f;
        for (int axis = 0; axis < 4; ++axis) {
                dot += pCh[axis]->vValues[1] * pCh[axis]->vValues[2];
        }
        EXPECT_GT(dot, 0.0f);
}

TEST(ClipContinuityTest, ContinuousChannelsPassThroughUntouched) {
        // identity -> 90°Y (no flip): the pass must be a no-op.
        auto pClipData = MakeClip(1.0f, true);
        const std::vector<float> vY = {0.0f, kS2};
        const std::vector<float> vW = {1.0f, kS2};
        for (uint8_t axis = 0; axis < 4; ++axis) {
                AddChannel(*pClipData, 0, static_cast<uint8_t>(3 + axis), {0.0f, 1.0f},
                           axis == 1 ? vY : (axis == 3 ? vW : std::vector<float>{0.0f, 0.0f}),
                           SE::FlatBuffers::CurveFormat::LinearF32);
        }
        flatbuffers::FlatBufferBuilder oBuilder;
        SE::AnimClip oClip("clip", 0, SerializeClip(oBuilder, *pClipData));

        for (const auto& ch : oClip.Channels()) {
                if (ch.target == 4) {
                        EXPECT_NEAR(ch.vValues[1], kS2, kPoseEps);
                }
                if (ch.target == 6) {
                        EXPECT_NEAR(ch.vValues[1], kS2, kPoseEps);
                }
        }
}

TEST(ClipContinuityTest, HermiteTangentsFlipWithCorrectedKey) {
        const std::vector<float> vTans = {1.0f, 1.0f, 1.0f};
        flatbuffers::FlatBufferBuilder oBuilder;
        SE::AnimClip oClip("clip", 0,
                        SerializeClip(oBuilder, *MakeFlippedClip(
                                        SE::FlatBuffers::CurveFormat::HermiteF32, vTans)));

        for (const auto& ch : oClip.Channels()) {
                ASSERT_EQ(ch.vTangents.size(), 3u);
                // keys 0,1 unchanged; key 2 flipped -> its tangent flips too
                EXPECT_NEAR(ch.vTangents[0], 1.0f, kPoseEps);
                EXPECT_NEAR(ch.vTangents[1], 1.0f, kPoseEps);
                EXPECT_NEAR(ch.vTangents[2], -1.0f, kPoseEps) << "target " << ch.target;
        }
}

TEST(ClipContinuityTest, PartialRotChannelsLeftAsIs) {
        // Only 3 of the 4 rotation channels present: no grouping possible —
        // the pass must leave values alone (and not crash).
        auto pClipData = MakeClip(1.0f, true);
        for (uint8_t axis = 0; axis < 3; ++axis) {   // x, y, z only — no w
                AddChannel(*pClipData, 0, static_cast<uint8_t>(3 + axis), vTimes3,
                           vKeyValues[axis], SE::FlatBuffers::CurveFormat::LinearF32);
        }
        flatbuffers::FlatBufferBuilder oBuilder;
        SE::AnimClip oClip("clip", 0, SerializeClip(oBuilder, *pClipData));

        for (const auto& ch : oClip.Channels()) {
                if (ch.target == 4) {   // y axis: flipped key 2 still flipped
                        EXPECT_NEAR(ch.vValues[2], -kS2, kPoseEps);
                }
        }
}

TEST(ClipContinuityTest, ShippedLibraryHasNoSignFlips) {
        // Library-wide regression guard: every baked clip in resource/animation
        // must decode sign-continuous. Catches a future rebake from an unfixed
        // converter and exercises the runtime pass against real assets.
        const std::string sDir = "resource/animation";
        uint32_t clips_checked = 0;
        for (const auto& entry : std::filesystem::directory_iterator(sDir)) {
                if (entry.path().extension() != ".seak") continue;
                SCOPED_TRACE(entry.path().string());
                SE::AnimClip oClip(entry.path().string(), 0);

                // Gather rot channels per bone, then check adjacent-key dots.
                std::map<uint16_t, std::array<const std::vector<float>*, 4>> mBones;
                for (const auto& ch : oClip.Channels()) {
                        if (ch.target >= 3 && ch.target <= 6) {
                                mBones[ch.bone_index][ch.target - 3] = &ch.vValues;
                        }
                }
                for (const auto& [bone, vAxes] : mBones) {
                        if (!vAxes[0] || vAxes[0]->size() < 2) continue;
                        const size_t keys = vAxes[0]->size();
                        bool complete = true;
                        for (int axis = 0; axis < 4; ++axis) {
                                if (!vAxes[axis] || vAxes[axis]->size() != keys) complete = false;
                        }
                        if (!complete) continue;
                        for (size_t k = 0; k + 1 < keys; ++k) {
                                float dot = 0.0f;
                                for (int axis = 0; axis < 4; ++axis) {
                                        dot += (*vAxes[axis])[k] * (*vAxes[axis])[k + 1];
                                }
                                EXPECT_GE(dot, -1e-6f)
                                        << "sign flip at bone " << bone << ", key " << k;
                        }
                }
                ++clips_checked;
        }
        EXPECT_GT(clips_checked, 40u) << "expected the shipped UAL clip set";
}

} // namespace
