
// NOTE: AnimationGraph_generated.h must be generated before compiling this file:
//   flatc --cpp -o generated/ misc/AnimationGraph.fbs

#ifdef SE_IMPL

#include <GlobalTypes.h>
#include <AnimGraphRuntime.h>
#include <AnimGraph.h>
#include <Skeleton.h>
#include <AnimEvaluator.h>
#include <AnimationGraph_generated.h>
#include <CommonEvents.h>
#include <Logging.h>
#include <flatbuffers/flatbuffers.h>

#include <cmath>
#include <algorithm>

namespace SE {

// ============================================================
// ParameterStore
// ============================================================

void ParameterStore::SetFloat(StrID name, float value) {
        auto& e   = mEntries[name];
        e.type     = Type::Float;
        e.float_val = value;
}

void ParameterStore::SetBool(StrID name, bool value) {
        auto& e  = mEntries[name];
        e.type    = Type::Bool;
        e.bool_val = value;
}

void ParameterStore::SetInt(StrID name, int value) {
        auto& e = mEntries[name];
        e.type   = Type::Int;
        e.int_val = value;
}

void ParameterStore::SetTrigger(StrID name) {
        auto& e    = mEntries[name];
        e.type      = Type::Trigger;
        e.triggered = true;
}

float ParameterStore::GetFloat(StrID name) const {
        auto it = mEntries.find(name);
        if (it == mEntries.end()) return 0.0f;
        return it->second.float_val;
}

bool ParameterStore::GetBool(StrID name) const {
        auto it = mEntries.find(name);
        if (it == mEntries.end()) return false;
        return it->second.bool_val;
}

int ParameterStore::GetInt(StrID name) const {
        auto it = mEntries.find(name);
        if (it == mEntries.end()) return 0;
        return it->second.int_val;
}

bool ParameterStore::ConsumeTrigger(StrID name) {
        auto it = mEntries.find(name);
        if (it == mEntries.end()) return false;
        if (!it->second.triggered) return false;
        it->second.triggered = false;
        return true;
}

// ============================================================
// AnimGraphInstance::Init
// ============================================================

void AnimGraphInstance::Init(const AnimGraph& graph) {
        pGraphFB = graph.GetFB();
        if (!pGraphFB) {
                log_e("AnimGraphInstance::Init: AnimGraph has null FlatBuffer pointer");
                return;
        }

        // ---- Default parameters from FlatBuffer ----
        if (pGraphFB->params()) {
                using PT = SE::FlatBuffers::AnimParamType;
                for (uint32_t i = 0; i < pGraphFB->params()->size(); ++i) {
                        const auto* p = pGraphFB->params()->Get(i);
                        if (!p || !p->name()) continue;
                        StrID nameID(p->name()->c_str());
                        switch (p->type()) {
                                case PT::Float:
                                        oParams.SetFloat(nameID, p->float_val());
                                        break;
                                case PT::Bool:
                                        oParams.SetBool(nameID, p->bool_val());
                                        break;
                                case PT::Int:
                                        oParams.SetInt(nameID, p->int_val());
                                        break;
                                case PT::Trigger:
                                        // Register a trigger slot with triggered=false.
                                        // ConsumeTrigger() checks entry.triggered, not entry.type, so
                                        // creating a float=0 entry is sufficient. A later SetTrigger()
                                        // call will flip triggered=true.
                                        oParams.SetFloat(nameID, 0.0f);
                                        break;
                                default:
                                        break;
                        }
                }
        }

        // ---- Entry state ----
        if (!pGraphFB->entry_state()) {
                log_e("AnimGraphInstance::Init: graph has no entry_state");
                return;
        }
        current_state_name      = StrID(pGraphFB->entry_state()->c_str());
        transition_target_name  = StrID("");
        transition_progress  = 0.0f;
        current_time         = 0.0f;
        transition_time        = 0.0f;

        // ---- Collect clip paths from all states ----
        if (!pGraphFB->states()) {
                log_e("AnimGraphInstance::Init: graph has no states");
                return;
        }

        std::vector<std::string> clipPaths;
        for (uint32_t si = 0; si < pGraphFB->states()->size(); ++si) {
                const auto* state = pGraphFB->states()->Get(si);
                if (!state || !state->nodes()) continue;
                for (uint32_t ni = 0; ni < state->nodes()->size(); ++ni) {
                        const auto* node = state->nodes()->Get(ni);
                        if (!node || !node->data()) continue;
                        if (node->data_type() == SE::FlatBuffers::BlendNodeDataU::ClipNodeData) {
                                auto* n = node->data_as_ClipNodeData();
                                if (n && n->clip_path()) {
                                        clipPaths.push_back(n->clip_path()->c_str());
                                }
                        }
                }
        }

        // De-duplicate and create resources
        std::sort(clipPaths.begin(), clipPaths.end());
        clipPaths.erase(std::unique(clipPaths.begin(), clipPaths.end()), clipPaths.end());

        for (const auto& path : clipPaths) {
                StrID pathID(path);
                if (mClips.find(pathID) == mClips.end()) {
                        H<AnimClip> h = CreateResource<AnimClip>(path);
                        if (!h.IsValid()) {
                                log_e("AnimGraphInstance::Init: failed to create AnimClip resource '{}'", path);
                        }
                        mClips[pathID] = h;
                }
        }

        log_d("AnimGraphInstance::Init: entry='{}', {} state(s), {} clip(s)",
                        pGraphFB->entry_state()->c_str(),
                        pGraphFB->states()->size(),
                        mClips.size());
}

// ============================================================
// AnimGraphInstance::Update
// ============================================================

void AnimGraphInstance::Update(float dt) {
        if (!pGraphFB) return;
        if (paused) return;

        const bool transitioning = (transition_target_name != StrID(""));

        // ---- Advance times ----
        prev_time       = current_time;
        current_time    += dt;
        if (transitioning) {
                transition_time += dt;
                transition_progress += dt / (transition_duration > 1e-6f ? transition_duration : 1e-6f);
        }

        // ---- Fire AnimEvents for primary state clip before wrapping ----
        {
                const SE::FlatBuffers::AnimState* curState = FindState(current_state_name);
                if (curState && curState->nodes() && curState->nodes()->size() > 0) {
                        const auto* rootNode = curState->nodes()->Get(0);
                        if (rootNode && rootNode->data_type() == SE::FlatBuffers::BlendNodeDataU::ClipNodeData) {
                                const auto* clipData = static_cast<const SE::FlatBuffers::ClipNodeData*>(rootNode->data());
                                if (clipData && clipData->clip_path()) {
                                        StrID clipKey(clipData->clip_path()->c_str());
                                        auto itClip = mClips.find(clipKey);
                                        if (itClip != mClips.end()) {
                                                const AnimClip* pClip = GetResource(itClip->second);
                                                if (pClip) {
                                                        const StrID state_name = current_state_name;
                                                        CheckAnimEvents(*pClip, prev_time, current_time, pClip->Looping(),
                                                                        [&state_name](const AnimClip::AnimEvent& ev) {
                                                                        EAnimEvent oEvt;
                                                                        oEvt.name      = ev.name;
                                                                        oEvt.value     = ev.value;
                                                                        oEvt.state_name = state_name;
                                                                        GetSystem<EventManager>().TriggerEvent(oEvt);
                                                                        });
                                                }
                                        }
                                }
                        }
                }
        }

        // ---- Wrap current clip time if looping ----
        {
                const SE::FlatBuffers::AnimState* curState = FindState(current_state_name);
                float dur = GetStateDuration(curState);
                if (dur > 1e-6f) {
                        current_time = std::fmod(current_time, dur);
                }
        }

        // ---- Complete transition if done ----
        if (transitioning && transition_progress >= 1.0f) {
                current_state_name     = transition_target_name;
                transition_target_name = StrID("");
                current_time          = transition_time;
                transition_time       = 0.0f;
                transition_progress   = 0.0f;
                transition_duration   = 0.2f;

                // Wrap new current time
                const SE::FlatBuffers::AnimState* newState = FindState(current_state_name);
                float dur = GetStateDuration(newState);
                if (dur > 1e-6f) {
                        current_time = std::fmod(current_time, dur);
                }
                return;  // Don't start another transition in the same frame
        }

        // ---- Check for new transitions (only if not already transitioning) ----
        if (!transitioning && pGraphFB->transitions()) {
                const SE::FlatBuffers::AnimState* curState = FindState(current_state_name);
                float stateDuration = GetStateDuration(curState);

                for (uint32_t ti = 0; ti < pGraphFB->transitions()->size(); ++ti) {
                        const auto* tr = pGraphFB->transitions()->Get(ti);
                        if (!tr || !tr->from() || !tr->to()) continue;

                        StrID fromID(tr->from()->c_str());
                        if (fromID != current_state_name) continue;

                        // ---- Check exit_time ----
                        if (tr->has_exit_time()) {
                                float normalizedTime = (stateDuration > 1e-6f)
                                        ? (current_time / stateDuration)
                                        : 1.0f;
                                if (normalizedTime < tr->exit_time()) continue;
                        }

                        // ---- Check all conditions ----
                        bool allMet = true;
                        if (tr->conditions()) {
                                for (uint32_t ci = 0; ci < tr->conditions()->size(); ++ci) {
                                        const auto* cond = tr->conditions()->Get(ci);
                                        if (!cond) { allMet = false; break; }
                                        if (!EvaluateCondition(cond)) { allMet = false; break; }
                                }
                        }

                        if (allMet) {
                                // Begin transition
                                transition_target_name = StrID(tr->to()->c_str());
                                transition_duration   = tr->duration();
                                transition_progress   = 0.0f;
                                transition_time       = 0.0f;
                                break;  // Take first matching transition
                        }
                }
        }
}

// ============================================================
// AnimGraphInstance::EvaluateBlendTree
// ============================================================

void AnimGraphInstance::EvaluateBlendTree(float weight,
                LocalPose& oOutPose,
                FrameAllocator& alloc,
                const Skeleton& skeleton) {

        if (!pGraphFB) return;

        const bool transitioning = (transition_target_name != StrID(""));

        if (!transitioning) {
                const SE::FlatBuffers::AnimState* state = FindState(current_state_name);
                if (state) {
                        EvaluateState(state, current_time, weight, oOutPose, alloc, skeleton);
                }
        } else {
                // Evaluate each state to a full pose (weight=1), then lerp between them.
                // This avoids the "blend-to-zero" artifact that occurs when accumulating
                // partial-weight clips into a shared zero-initialized pose.
                const SE::FlatBuffers::AnimState* srcState = FindState(current_state_name);
                const SE::FlatBuffers::AnimState* dstState = FindState(transition_target_name);

                LocalPose srcPose = AllocatePose(skeleton.BoneCount(), alloc);
                InitBindPose(srcPose, skeleton);
                if (srcState) {
                        EvaluateState(srcState, current_time, 1.0f, srcPose, alloc, skeleton);
                }
                RenormalizeRotations(srcPose);

                LocalPose dstPose = AllocatePose(skeleton.BoneCount(), alloc);
                InitBindPose(dstPose, skeleton);
                if (dstState) {
                        EvaluateState(dstState, transition_time, 1.0f, dstPose, alloc, skeleton);
                }
                RenormalizeRotations(dstPose);

                float t = std::clamp(transition_progress, 0.0f, 1.0f);
                BlendPoses(srcPose, dstPose, t, oOutPose);
        }
}

// ============================================================
// AnimGraphInstance::GetActiveStates
// ============================================================

void AnimGraphInstance::GetActiveStates(std::vector<ActiveStateInfo>& out) const {
        out.clear();

        const bool transitioning = (transition_target_name != StrID(""));

        // Current (source) state
        {
                ActiveStateInfo info;
                info.state_name = current_state_name;
                info.local_time = current_time;
                info.weight    = transitioning ? (1.0f - transition_progress) : 1.0f;

                // Try to find a clip handle if the root node is a ClipNode
                if (pGraphFB && pGraphFB->states()) {
                        const SE::FlatBuffers::AnimState* state = FindState(current_state_name);
                        if (state && state->nodes() && state->nodes()->size() > 0) {
                                const auto* rootNode = state->nodes()->Get(0);
                                if (rootNode && rootNode->data_type() == SE::FlatBuffers::BlendNodeDataU::ClipNodeData) {
                                        auto* n = rootNode->data_as_ClipNodeData();
                                        if (n && n->clip_path()) {
                                                StrID pathID(n->clip_path()->c_str());
                                                auto it = mClips.find(pathID);
                                                if (it != mClips.end()) {
                                                        info.hClip = it->second;
                                                }
                                        }
                                }
                        }
                }
                out.push_back(info);
        }

        // Transitioning-in state
        if (transitioning) {
                ActiveStateInfo info;
                info.state_name = transition_target_name;
                info.local_time = transition_time;
                info.weight    = transition_progress;

                if (pGraphFB && pGraphFB->states()) {
                        const SE::FlatBuffers::AnimState* state = FindState(transition_target_name);
                        if (state && state->nodes() && state->nodes()->size() > 0) {
                                const auto* rootNode = state->nodes()->Get(0);
                                if (rootNode && rootNode->data_type() == SE::FlatBuffers::BlendNodeDataU::ClipNodeData) {
                                        auto* n = rootNode->data_as_ClipNodeData();
                                        if (n && n->clip_path()) {
                                                StrID pathID(n->clip_path()->c_str());
                                                auto it = mClips.find(pathID);
                                                if (it != mClips.end()) {
                                                        info.hClip = it->second;
                                                }
                                        }
                                }
                        }
                }
                out.push_back(info);
        }
}

// ============================================================
// Private helpers
// ============================================================

bool AnimGraphInstance::EvaluateCondition(const SE::FlatBuffers::AnimCondition* cond) {
        if (!cond || !cond->parameter()) return false;
        using Op = SE::FlatBuffers::ConditionOp;
        StrID paramID(cond->parameter()->c_str());
        float threshold = cond->threshold();
        switch (cond->op()) {
                case Op::Greater:   return oParams.GetFloat(paramID) > threshold;
                case Op::Less:      return oParams.GetFloat(paramID) < threshold;
                case Op::Equal:     return std::abs(oParams.GetFloat(paramID) - threshold) < 1e-4f;
                case Op::NotEqual:  return std::abs(oParams.GetFloat(paramID) - threshold) >= 1e-4f;
                case Op::IsTrue:    return oParams.GetBool(paramID);
                case Op::IsFalse:   return !oParams.GetBool(paramID);
                case Op::Triggered: return oParams.ConsumeTrigger(paramID);
                default:            return false;
        }
}

const SE::FlatBuffers::AnimState* AnimGraphInstance::FindState(StrID name) const {
        if (!pGraphFB || !pGraphFB->states()) return nullptr;
        for (uint32_t i = 0; i < pGraphFB->states()->size(); ++i) {
                const auto* state = pGraphFB->states()->Get(i);
                if (!state || !state->id()) continue;
                if (StrID(state->id()->c_str()) == name) return state;
        }
        return nullptr;
}

void AnimGraphInstance::GetStateNames(std::vector<std::string>& out) const {
        if (!pGraphFB) return;
        const auto* states = pGraphFB->states();
        if (!states) return;
        out.reserve(states->size());
        for (flatbuffers::uoffset_t i = 0; i < states->size(); ++i) {
                const auto* s = states->Get(i);
                if (s && s->id()) out.emplace_back(s->id()->str());
        }
}

void AnimGraphInstance::ForceSetState(StrID name) {

        if (!FindState(name)) return;
        current_state_name     = name;
        current_time          = 0.0f;
        prev_time             = 0.0f;
        transition_target_name = StrID("");
        transition_progress   = 0.0f;
        transition_time       = 0.0f;
}

void AnimGraphInstance::SetPaused(bool new_paused) { paused = new_paused; }

float AnimGraphInstance::GetStateDuration(const SE::FlatBuffers::AnimState* state) const {
        if (!state || !state->nodes() || state->nodes()->size() == 0) return 0.0f;
        const auto* rootNode = state->nodes()->Get(0);
        if (!rootNode) return 0.0f;
        if (rootNode->data_type() != SE::FlatBuffers::BlendNodeDataU::ClipNodeData) return 0.0f;
        auto* n = rootNode->data_as_ClipNodeData();
        if (!n || !n->clip_path()) return 0.0f;
        StrID pathID(n->clip_path()->c_str());
        auto it = mClips.find(pathID);
        if (it == mClips.end() || !it->second.IsValid()) return 0.0f;
        const AnimClip* pClip = GetResource(it->second);
        if (!pClip) return 0.0f;
        float rate = n->playback_rate();
        if (rate < 1e-6f) rate = 1.0f;
        return pClip->Duration() / rate;
}

void AnimGraphInstance::EvaluateState(const SE::FlatBuffers::AnimState* state,
                float local_time, float weight,
                LocalPose& oOutPose,
                FrameAllocator& alloc,
                const Skeleton& skeleton) {

        if (!state || !state->nodes() || state->nodes()->size() == 0) return;
        EvaluateNode(state, 0, local_time, weight, oOutPose, alloc, skeleton);
}

void AnimGraphInstance::EvaluateNode(const SE::FlatBuffers::AnimState* state,
                uint16_t nodeIdx,
                float local_time, float weight,
                LocalPose& oOutPose,
                FrameAllocator& alloc,
                const Skeleton& skeleton) {

        if (!state || !state->nodes()) return;
        if (nodeIdx >= static_cast<uint16_t>(state->nodes()->size())) return;

        const auto* node = state->nodes()->Get(nodeIdx);
        if (!node || !node->data()) return;

        using BNU = SE::FlatBuffers::BlendNodeDataU;

        switch (node->data_type()) {

                // ------------------------------------------------------------------
                case BNU::ClipNodeData: {
                                                auto* n = node->data_as_ClipNodeData();
                                                if (!n || !n->clip_path()) return;

                                                StrID pathID(n->clip_path()->c_str());
                                                auto it = mClips.find(pathID);
                                                if (it == mClips.end() || !it->second.IsValid()) return;

                                                const AnimClip* pClip = GetResource(it->second);
                                                if (!pClip) return;

                                                float rate = n->playback_rate();
                                                if (rate < 1e-6f) rate = 1.0f;
                                                float dur = pClip->Duration();
                                                float t   = (dur > 1e-6f) ? std::fmod(local_time * rate, dur) : 0.0f;

                                                SampleClip(*pClip, t, oOutPose);
                                                break;
                                        }

                                        // ------------------------------------------------------------------
                case BNU::Blend1DNodeData: {
                                                   auto* n = node->data_as_Blend1DNodeData();
                                                   if (!n || !n->thresholds() || !n->child_indices() || !n->parameter()) return;

                                                   float param             = oParams.GetFloat(StrID(n->parameter()->c_str()));
                                                   const auto* thresholds  = n->thresholds();
                                                   const auto* children    = n->child_indices();

                                                   uint32_t count = std::min(thresholds->size(), children->size());
                                                   if (count < 2) return;

                                                   // Clamp param to range and find bracket
                                                   float lo_thresh = thresholds->Get(0);
                                                   float hi_thresh = thresholds->Get(count - 1);
                                                   param = std::clamp(param, lo_thresh, hi_thresh);

                                                   for (uint32_t i = 0; i + 1 < count; ++i) {
                                                           float lo = thresholds->Get(i);
                                                           float hi = thresholds->Get(i + 1);
                                                           if (param >= lo && param <= hi) {
                                                                   float t = (hi > lo) ? (param - lo) / (hi - lo) : 0.0f;

                                                                   // Evaluate each child to a full pose, then blend — avoids
                                                                   // partial-weight blend-to-zero for partial-rig clips.
                                                                   LocalPose poseA = AllocatePose(skeleton.BoneCount(), alloc);
                                                                   InitBindPose(poseA, skeleton);
                                                                   EvaluateNode(state, children->Get(i), local_time, 1.0f, poseA, alloc, skeleton);
                                                                   RenormalizeRotations(poseA);

                                                                   LocalPose poseB = AllocatePose(skeleton.BoneCount(), alloc);
                                                                   InitBindPose(poseB, skeleton);
                                                                   EvaluateNode(state, children->Get(i + 1), local_time, 1.0f, poseB, alloc, skeleton);
                                                                   RenormalizeRotations(poseB);

                                                                   BlendPoses(poseA, poseB, t, oOutPose);
                                                                   break;
                                                           }
                                                   }
                                                   break;
                                           }

                                           // ------------------------------------------------------------------
                case BNU::Blend2DNodeData: {
                                                   // Simplified: use the child closest to the parameter point (nearest-neighbour)
                                                   auto* n = node->data_as_Blend2DNodeData();
                                                   if (!n || !n->positions() || !n->child_indices() || !n->param_x() || !n->param_y()) return;

                                                   float px = oParams.GetFloat(StrID(n->param_x()->c_str()));
                                                   float py = oParams.GetFloat(StrID(n->param_y()->c_str()));

                                                   const auto* positions = n->positions();
                                                   const auto* children  = n->child_indices();
                                                   uint32_t count = std::min(positions->size(), children->size());
                                                   if (count == 0) return;

                                                   // Find nearest point
                                                   uint32_t bestIdx = 0;
                                                   float bestDist2  = std::numeric_limits<float>::max();
                                                   for (uint32_t i = 0; i < count; ++i) {
                                                           const auto& pt = *positions->Get(i);
                                                           float dx = pt.x() - px;
                                                           float dy = pt.y() - py;
                                                           float d2 = dx * dx + dy * dy;
                                                           if (d2 < bestDist2) { bestDist2 = d2; bestIdx = i; }
                                                   }
                                                   EvaluateNode(state, children->Get(bestIdx), local_time, weight, oOutPose, alloc, skeleton);
                                                   break;
                                           }

                                           // ------------------------------------------------------------------
                case BNU::AdditiveNodeData: {
                                                    auto* n = node->data_as_AdditiveNodeData();
                                                    if (!n) return;

                                                    // Resolve additive weight
                                                    float w = n->weight();
                                                    if (n->weight_param() && n->weight_param()->size() > 0) {
                                                            w = oParams.GetFloat(StrID(n->weight_param()->c_str()));
                                                    }
                                                    w = std::clamp(w, 0.0f, 1.0f);

                                                    // Evaluate base normally into oOutPose
                                                    EvaluateNode(state, n->base_index(), local_time, weight, oOutPose, alloc, skeleton);

                                                    // Evaluate additive layer into a separate pose initialised to bind
                                                    LocalPose addPose = AllocatePose(skeleton.BoneCount(), alloc);
                                                    InitBindPose(addPose, skeleton);
                                                    EvaluateNode(state, n->additive_index(), local_time, weight, addPose, alloc, skeleton);
                                                    RenormalizeRotations(addPose);

                                                    // Additive blend: add delta from bind pose
                                                    const auto& bones = skeleton.Bones();
                                                    for (uint32_t i = 0; i < oOutPose.bone_count && i < static_cast<uint32_t>(bones.size()); ++i) {
                                                            glm::vec3 deltaPos = addPose.pPos[i] - bones[i].bindPos;
                                                            oOutPose.pPos[i] += deltaPos * w;

                                                            glm::quat bindRot = bones[i].bindRot;
                                                            glm::quat addRot  = addPose.pRot[i];
                                                            // Delta rotation in local space: delta = inv(bind) * addRot
                                                            glm::quat deltaRot = glm::inverse(bindRot) * addRot;
                                                            oOutPose.pRot[i] = oOutPose.pRot[i] * glm::slerp(glm::quat(1.0f, 0.0f, 0.0f, 0.0f), deltaRot, w);
                                                    }
                                                    break;
                                            }

                                            // ------------------------------------------------------------------
                case BNU::LayerNodeData: {
                                                 auto* n = node->data_as_LayerNodeData();
                                                 if (!n) return;

                                                 // Resolve layer weight
                                                 float w = n->weight();
                                                 if (n->weight_param() && n->weight_param()->size() > 0) {
                                                         w = oParams.GetFloat(StrID(n->weight_param()->c_str()));
                                                 }
                                                 w = std::clamp(w, 0.0f, 1.0f);

                                                 // Evaluate base into oOutPose
                                                 EvaluateNode(state, n->base_index(), local_time, weight, oOutPose, alloc, skeleton);

                                                 // Evaluate layer pose
                                                 LocalPose layerPose = AllocatePose(skeleton.BoneCount(), alloc);
                                                 InitBindPose(layerPose, skeleton);
                                                 EvaluateNode(state, n->layer_index(), local_time, weight, layerPose, alloc, skeleton);
                                                 RenormalizeRotations(layerPose);

                                                 // Bone mask lookup
                                                 const Skeleton::BoneMask* mask = nullptr;
                                                 if (n->mask_name() && n->mask_name()->size() > 0) {
                                                         mask = skeleton.FindMask(StrID(n->mask_name()->c_str()));
                                                 }

                                                 const bool additive = (n->blend_mode() == SE::FlatBuffers::LayerBlendMode::AdditiveLayer);

                                                 for (uint32_t i = 0; i < oOutPose.bone_count; ++i) {
                                                         float boneW = w;
                                                         if (mask) {
                                                                 if (i < static_cast<uint32_t>(mask->weights.size())) {
                                                                         boneW = w * mask->weights[i];
                                                                 } else {
                                                                         boneW = 0.0f;
                                                                 }
                                                         }
                                                         if (boneW < 1e-4f) continue;

                                                         if (additive) {
                                                                 oOutPose.pPos[i] += layerPose.pPos[i] * boneW;
                                                                 oOutPose.pRot[i]  = oOutPose.pRot[i] * glm::slerp(
                                                                                 glm::quat(1.0f, 0.0f, 0.0f, 0.0f), layerPose.pRot[i], boneW);
                                                         } else {
                                                                 oOutPose.pPos[i] = glm::mix(oOutPose.pPos[i], layerPose.pPos[i], boneW);
                                                                 oOutPose.pRot[i] = glm::slerp(oOutPose.pRot[i], layerPose.pRot[i], boneW);
                                                                 oOutPose.pScl[i] = glm::mix(oOutPose.pScl[i], layerPose.pScl[i], boneW);
                                                         }
                                                 }
                                                 break;
                                         }

                default:
                                         break;
        }
}

} // namespace SE

#endif // SE_IMPL
