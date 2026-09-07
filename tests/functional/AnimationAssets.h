
#ifndef SE_TEST_ANIMATION_ASSETS_H
#define SE_TEST_ANIMATION_ASSETS_H

// ---------------------------------------------------------------------------
// AnimationAssets — in-memory animation asset builders (FlatBuffers Object API).
//
// Where test asset builders live (convention for all subsystems):
//
//   tests/functional/
//     EngineFixture.h   engine init + resource-manager fixture (shared)
//     FrameRunner.h     frame-phase replay helpers        (shared)
//     PoseAssert.h      pose/tolerance assertions         (shared)
//     LogCapture.h      spdlog capture assertions         (shared)
//     AnimationAssets.h clip/skeleton/graph builders      (animation)
//     PhysicsAssets.h   (future) bodies/collider builders (physics)
//     AudioAssets.h     (future) cues/clips builders      (audio)
//     ...
//
//   * One header per subsystem, named <Subsystem>Assets.h, same house style:
//     header-only builders returning unique_ptr native tables, serialized
//     through the Serialize* helpers with the real file identifiers.
//   * Subsystem-agnostic helpers never go into a subsystem header — they stay
//     in the shared files above so harness code is independent of what it
//     tests.
//   * tests/bench/BenchAssets.h mirrors the pattern for benchmark fixtures.
//
// Builds tiny, fully deterministic clips / skeletons / graphs so tests can
// assert analytic pose values on 2-3 bone rigs.  Serialized bytes are finished
// with the real file identifiers ("SEAK"/"SESK"/"SEAG"); nothing touches the
// filesystem — resources are constructed from live FlatBuffer memory the same
// way AnimGraphInstance::Init consumes inline clip data.
//
// Native tables (AnimationClipT etc.) hold unique_ptr members, so builder
// functions transfer ownership via unique_ptr and node makers take data by
// value (moved-from after wiring into a node).
//
// Rules that keep graphs loadable by the runtime:
//   * inline clip holders set `name` + `clip`, never `path` — Init takes the
//     inline branch only when path() is empty (AnimGraphRuntime.tcc);
//   * always set `looping` explicitly — the schema default is false while the
//     runtime treats a *missing* state clip as looping.
// ---------------------------------------------------------------------------

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <flatbuffers/flatbuffers.h>
#include <AnimationClip_generated.h>
#include <AnimationSkeleton_generated.h>
#include <AnimationGraph_generated.h>

namespace se_test {

using FbClip      = SE::FlatBuffers::AnimationClipT;
using FbSkel      = SE::FlatBuffers::SkeletonT;
using FbGraph     = SE::FlatBuffers::AnimationGraphT;
using ClipPtr     = std::unique_ptr<FbClip>;
using SkelPtr     = std::unique_ptr<FbSkel>;
using GraphPtr    = std::unique_ptr<FbGraph>;

// ===========================================================================
// Clips
// ===========================================================================

inline ClipPtr MakeClip(float duration, bool looping) {
        auto pClip = std::make_unique<FbClip>();
        pClip->duration = duration;
        pClip->looping  = looping;
        return pClip;
}

// Generic channel: times/values must have equal length.
// target: 0-2 pos xyz, 3-6 rot xyzw, 7-9 scale xyz.
inline void AddChannel(FbClip& oClip, uint16_t bone_index, uint8_t target,
                       const std::vector<float>& vTimes, const std::vector<float>& vValues,
                       SE::FlatBuffers::CurveFormat eFormat = SE::FlatBuffers::CurveFormat::LinearF32,
                       const std::vector<float>& vTangents = {},
                       float quant_min = 0.0f, float quant_max = 1.0f) {
        auto pCh = std::make_unique<SE::FlatBuffers::CurveChannelT>();
        pCh->bone_index = bone_index;
        pCh->target     = target;
        pCh->format     = eFormat;
        pCh->times      = vTimes;
        pCh->values     = vValues;
        pCh->tangents   = vTangents;
        pCh->quant_min  = quant_min;
        pCh->quant_max  = quant_max;
        oClip.channels.push_back(std::move(pCh));
}

// Shorthand: single-component linear ramp v_from -> v_to over [0, duration].
inline void AddRamp(FbClip& oClip, uint16_t bone_index, uint8_t target,
                    float duration, float v_from, float v_to) {
        AddChannel(oClip, bone_index, target, {0.0f, duration}, {v_from, v_to});
}

// Quantized16 channel: raw stored values are dequantized at load via raw*(qmax-qmin)+qmin.
inline void AddQuantizedChannel(FbClip& oClip, uint16_t bone_index, uint8_t target,
                                const std::vector<float>& vTimes, const std::vector<float>& vRaw,
                                float quant_min, float quant_max) {
        AddChannel(oClip, bone_index, target, vTimes, vRaw,
                   SE::FlatBuffers::CurveFormat::Quantized16, {}, quant_min, quant_max);
}

inline void AddEvent(FbClip& oClip, float time, std::string sName, float value = 0.0f) {
        auto pEv = std::make_unique<SE::FlatBuffers::AnimEventT>();
        pEv->time  = time;
        pEv->name  = std::move(sName);
        pEv->value = value;
        oClip.events.push_back(std::move(pEv));
}

// ===========================================================================
// Skeletons
// ===========================================================================

// Chain of [n] bones: bone i parented to i-1, bindPos = (i, 0, 0),
// identity rotation and scale, identity inverse bind matrix.
inline SkelPtr MakeChainSkeleton(uint16_t n) {
        auto pSkel = std::make_unique<FbSkel>();
        for (uint16_t i = 0; i < n; ++i) {
                auto pBone = std::make_unique<SE::FlatBuffers::SkeletonBoneT>();
                pBone->name = "bone_" + std::to_string(i);
                pBone->parent_index = (i == 0) ? 65535 : static_cast<uint16_t>(i - 1);
                pBone->bind_pos = std::make_unique<SE::FlatBuffers::Vec3>(
                                static_cast<float>(i), 0.0f, 0.0f);
                pBone->bind_rot = std::make_unique<SE::FlatBuffers::Vec4>(
                                0.0f, 0.0f, 0.0f, 1.0f);   // xyzw, identity
                pBone->bind_scale = std::make_unique<SE::FlatBuffers::Vec3>(1.0f, 1.0f, 1.0f);
                const SE::FlatBuffers::Vec4 c0(1.0f, 0.0f, 0.0f, 0.0f);
                const SE::FlatBuffers::Vec4 c1(0.0f, 1.0f, 0.0f, 0.0f);
                const SE::FlatBuffers::Vec4 c2(0.0f, 0.0f, 1.0f, 0.0f);
                const SE::FlatBuffers::Vec4 c3(0.0f, 0.0f, 0.0f, 1.0f);
                pBone->inv_bind_matrix = std::make_unique<SE::FlatBuffers::ColMat4>(c0, c1, c2, c3);
                pSkel->bones.push_back(std::move(pBone));
        }
        return pSkel;
}

// Symmetric 3-bone rig for mirror tests: center "spine" root with children
// "hand_l" (bind -1,0,0) and "hand_r" (bind +1,0,0).
inline SkelPtr MakeMirrorSkeleton() {
        auto pSkel = std::make_unique<FbSkel>();
        const auto add_bone = [&](const char* pName, uint16_t parent, float x) {
                auto pBone = std::make_unique<SE::FlatBuffers::SkeletonBoneT>();
                pBone->name = pName;
                pBone->parent_index = parent;
                pBone->bind_pos = std::make_unique<SE::FlatBuffers::Vec3>(x, 0.0f, 0.0f);
                pBone->bind_rot = std::make_unique<SE::FlatBuffers::Vec4>(0.0f, 0.0f, 0.0f, 1.0f);
                pBone->bind_scale = std::make_unique<SE::FlatBuffers::Vec3>(1.0f, 1.0f, 1.0f);
                const SE::FlatBuffers::Vec4 c0(1.0f, 0.0f, 0.0f, 0.0f);
                const SE::FlatBuffers::Vec4 c1(0.0f, 1.0f, 0.0f, 0.0f);
                const SE::FlatBuffers::Vec4 c2(0.0f, 0.0f, 1.0f, 0.0f);
                const SE::FlatBuffers::Vec4 c3(0.0f, 0.0f, 0.0f, 1.0f);
                pBone->inv_bind_matrix = std::make_unique<SE::FlatBuffers::ColMat4>(c0, c1, c2, c3);
                pSkel->bones.push_back(std::move(pBone));
        };
        add_bone("spine",  65535,  0.0f);
        add_bone("hand_l", 0,     -1.0f);
        add_bone("hand_r", 0,      1.0f);
        return pSkel;
}

// Per-bone mask weights; missing bones in the map get weight 0 at lookup time
// (Skeleton pads weights to bone_count with zeros).
inline void AddMask(FbSkel& oSkel, const std::string& sName,
                    const std::vector<std::pair<uint16_t, float>>& vEntries) {
        auto pMask = std::make_unique<SE::FlatBuffers::BoneMaskT>();
        pMask->name = sName;
        for (const auto& [idx, weight] : vEntries) {
                auto pEntry = std::make_unique<SE::FlatBuffers::BoneMaskEntryT>();
                pEntry->bone_index = idx;
                pEntry->weight     = weight;
                pMask->entries.push_back(std::move(pEntry));
        }
        oSkel.masks.push_back(std::move(pMask));
}

// ===========================================================================
// Graph node makers
// ===========================================================================

// Inline clip leaf. Takes ownership of the clip; the holder name is the
// resource cache key — use distinct names for distinct clip content.
inline SE::FlatBuffers::ClipNodeDataT MakeClipLeaf(
                std::string sName, ClipPtr pClip, float playback_rate = 1.0f,
                bool mirror = false) {
        SE::FlatBuffers::ClipNodeDataT oData;
        auto pHolder = std::make_unique<SE::FlatBuffers::AnimClipHolderT>();
        pHolder->name = std::move(sName);
        pHolder->clip = std::move(pClip);
        oData.clip          = std::move(pHolder);
        oData.playback_rate = playback_rate;
        oData.mirror        = mirror;
        return oData;
}

inline SE::FlatBuffers::BlendTreeNodeT MakeClipNode(SE::FlatBuffers::ClipNodeDataT oData) {
        SE::FlatBuffers::BlendTreeNodeT oNode;
        oNode.data.Set(std::move(oData));
        return oNode;
}

inline SE::FlatBuffers::BlendTreeNodeT MakeBlend1DNode(
                std::string sParam, const std::vector<float>& vThresholds,
                const std::vector<uint16_t>& vChildIndices) {
        SE::FlatBuffers::Blend1DNodeDataT oData;
        oData.parameter     = std::move(sParam);
        oData.thresholds    = vThresholds;
        oData.child_indices = vChildIndices;
        SE::FlatBuffers::BlendTreeNodeT oNode;
        oNode.data.Set(std::move(oData));
        return oNode;
}

inline SE::FlatBuffers::BlendTreeNodeT MakeBlend2DNode(
                std::string sParamX, std::string sParamY,
                const std::vector<SE::FlatBuffers::BlendPoint2D>& vPositions,
                const std::vector<uint16_t>& vChildIndices,
                SE::FlatBuffers::Blend2DAlgorithm eAlgorithm =
                                SE::FlatBuffers::Blend2DAlgorithm::FreeformCartesian) {
        SE::FlatBuffers::Blend2DNodeDataT oData;
        oData.param_x        = std::move(sParamX);
        oData.param_y        = std::move(sParamY);
        oData.positions      = vPositions;
        oData.child_indices  = vChildIndices;
        oData.algorithm      = eAlgorithm;
        SE::FlatBuffers::BlendTreeNodeT oNode;
        oNode.data.Set(std::move(oData));
        return oNode;
}

inline SE::FlatBuffers::BlendTreeNodeT MakeAdditiveNode(
                uint16_t base_index, uint16_t additive_index,
                float weight, std::string sWeightParam = {}) {
        SE::FlatBuffers::AdditiveNodeDataT oData;
        oData.base_index     = base_index;
        oData.additive_index = additive_index;
        oData.weight         = weight;
        oData.weight_param   = std::move(sWeightParam);
        SE::FlatBuffers::BlendTreeNodeT oNode;
        oNode.data.Set(std::move(oData));
        return oNode;
}

inline SE::FlatBuffers::BlendTreeNodeT MakeLayerNode(
                uint16_t base_index, uint16_t layer_index, std::string sMaskName,
                float weight, std::string sWeightParam = {},
                SE::FlatBuffers::LayerBlendMode eMode = SE::FlatBuffers::LayerBlendMode::Override) {
        SE::FlatBuffers::LayerNodeDataT oData;
        oData.base_index   = base_index;
        oData.layer_index  = layer_index;
        oData.mask_name    = std::move(sMaskName);
        oData.weight       = weight;
        oData.weight_param = std::move(sWeightParam);
        oData.blend_mode   = eMode;
        SE::FlatBuffers::BlendTreeNodeT oNode;
        oNode.data.Set(std::move(oData));
        return oNode;
}

// ===========================================================================
// Graph makers
// ===========================================================================

inline SE::FlatBuffers::AnimStateT MakeState(
                std::string sId, std::vector<SE::FlatBuffers::BlendTreeNodeT>&& vNodes,
                float speed = 1.0f, bool mirror = false) {
        SE::FlatBuffers::AnimStateT oState;
        oState.id     = std::move(sId);
        oState.speed  = speed;
        oState.mirror = mirror;
        for (auto& oNode : vNodes) {
                oState.nodes.push_back(
                                std::make_unique<SE::FlatBuffers::BlendTreeNodeT>(std::move(oNode)));
        }
        return oState;
}

inline SE::FlatBuffers::AnimTransitionT MakeTransition(
                std::string sFrom, std::string sTo, float duration,
                std::vector<SE::FlatBuffers::AnimConditionT> vConds = {},
                bool has_exit_time = false, float exit_time = 1.0f,
                bool can_interrupt = false,
                SE::FlatBuffers::TransitionMode eMode = SE::FlatBuffers::TransitionMode::CrossFade) {
        SE::FlatBuffers::AnimTransitionT oTr;
        oTr.from          = std::move(sFrom);
        oTr.to            = std::move(sTo);
        oTr.duration      = duration;
        oTr.has_exit_time = has_exit_time;
        oTr.exit_time     = exit_time;
        oTr.can_interrupt = can_interrupt;
        oTr.mode          = eMode;
        for (auto& oCond : vConds) {
                oTr.conditions.push_back(
                                std::make_unique<SE::FlatBuffers::AnimConditionT>(std::move(oCond)));
        }
        return oTr;
}

inline SE::FlatBuffers::AnimConditionT MakeCondition(
                std::string sParam, SE::FlatBuffers::ConditionOp eOp, float threshold = 0.0f) {
        SE::FlatBuffers::AnimConditionT oCond;
        oCond.parameter = std::move(sParam);
        oCond.op        = eOp;
        oCond.threshold = threshold;
        return oCond;
}

inline SE::FlatBuffers::AnimParamT MakeParam(
                std::string sName, SE::FlatBuffers::AnimParamType eType,
                float fVal = 0.0f, bool bVal = false, int iVal = 0) {
        SE::FlatBuffers::AnimParamT oParam;
        oParam.name      = std::move(sName);
        oParam.type      = eType;
        oParam.float_val = fVal;
        oParam.bool_val  = bVal;
        oParam.int_val   = iVal;
        return oParam;
}

// Assembles the graph. States/transitions are moved in (their native tables
// are move-only).
inline GraphPtr MakeGraph(
                std::string sEntry,
                std::vector<SE::FlatBuffers::AnimStateT> vStates,
                std::vector<SE::FlatBuffers::AnimTransitionT> vTransitions = {},
                std::vector<SE::FlatBuffers::AnimParamT> vParams = {}) {
        auto pGraph = std::make_unique<FbGraph>();
        pGraph->entry_state = std::move(sEntry);
        for (auto& oState : vStates) {
                pGraph->states.push_back(
                                std::make_unique<SE::FlatBuffers::AnimStateT>(std::move(oState)));
        }
        for (auto& oTr : vTransitions) {
                pGraph->transitions.push_back(
                                std::make_unique<SE::FlatBuffers::AnimTransitionT>(std::move(oTr)));
        }
        for (auto& oParam : vParams) {
                pGraph->params.push_back(
                                std::make_unique<SE::FlatBuffers::AnimParamT>(std::move(oParam)));
        }
        return pGraph;
}

// One state, one inline clip leaf, entry = that state.
inline GraphPtr MakeSingleStateGraph(std::string sState, std::string sClipName,
                                     ClipPtr pClip, float playback_rate = 1.0f) {
        std::vector<SE::FlatBuffers::BlendTreeNodeT> vNodes;
        vNodes.push_back(MakeClipNode(MakeClipLeaf(std::move(sClipName), std::move(pClip),
                                                   playback_rate)));
        std::vector<SE::FlatBuffers::AnimStateT> vStates;
        vStates.push_back(MakeState(std::move(sState), std::move(vNodes)));
        const std::string sEntry = vStates[0].id;   // read before the vector is moved
        return MakeGraph(sEntry, std::move(vStates));
}

// Two states (one clip leaf each) + transition A -> B with duration + conds.
inline GraphPtr MakeTwoStateGraph(std::string sA, std::string sB,
                                  ClipPtr pClipA, ClipPtr pClipB,
                                  float transition_duration,
                                  std::vector<SE::FlatBuffers::AnimConditionT> vConds = {},
                                  bool has_exit_time = false, float exit_time = 1.0f,
                                  bool can_interrupt = false,
                                  SE::FlatBuffers::TransitionMode eMode =
                                        SE::FlatBuffers::TransitionMode::CrossFade) {
        std::vector<SE::FlatBuffers::AnimStateT> vStates;
        const std::string sClipAName = sA + "_clip";
        const std::string sClipBName = sB + "_clip";
        vStates.push_back(MakeState(sA, {MakeClipNode(MakeClipLeaf(sClipAName, std::move(pClipA)))}));
        vStates.push_back(MakeState(sB, {MakeClipNode(MakeClipLeaf(sClipBName, std::move(pClipB)))}));
        std::vector<SE::FlatBuffers::AnimTransitionT> vTransitions;
        vTransitions.push_back(MakeTransition(sA, sB, transition_duration, std::move(vConds),
                                              has_exit_time, exit_time, can_interrupt, eMode));
        return MakeGraph(sA, std::move(vStates), std::move(vTransitions));
}

// ===========================================================================
// Serialization
// ===========================================================================

// Finishes the builder and returns a root pointer valid while the builder is
// alive. Inline resource constructors copy/decode immediately, so callers may
// let the builder die right after constructing the runtime resource.
inline const SE::FlatBuffers::AnimationClip* SerializeClip(
                flatbuffers::FlatBufferBuilder& oBuilder, const FbClip& oClip) {
        auto off = SE::FlatBuffers::CreateAnimationClip(oBuilder, &oClip);
        oBuilder.Finish(off, SE::FlatBuffers::AnimationClipIdentifier());
        return SE::FlatBuffers::GetAnimationClip(oBuilder.GetBufferPointer());
}

inline const SE::FlatBuffers::Skeleton* SerializeSkeleton(
                flatbuffers::FlatBufferBuilder& oBuilder, const FbSkel& oSkel) {
        auto off = SE::FlatBuffers::CreateSkeleton(oBuilder, &oSkel);
        oBuilder.Finish(off, SE::FlatBuffers::SkeletonIdentifier());
        return SE::FlatBuffers::GetSkeleton(oBuilder.GetBufferPointer());
}

inline const SE::FlatBuffers::AnimationGraph* SerializeGraph(
                flatbuffers::FlatBufferBuilder& oBuilder, const FbGraph& oGraph) {
        auto off = SE::FlatBuffers::CreateAnimationGraph(oBuilder, &oGraph);
        oBuilder.Finish(off, SE::FlatBuffers::AnimationGraphIdentifier());
        return SE::FlatBuffers::GetAnimationGraph(oBuilder.GetBufferPointer());
}

} // namespace se_test

#endif // SE_TEST_TEST_ASSETS_H
