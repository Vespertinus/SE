
// NOTE: AnimationGraph_generated.h must be generated before compiling this file:
//   flatc --cpp -o generated/ misc/AnimationGraph.fbs

#ifdef SE_IMPL

#include <GlobalTypes.h>
#include <AnimGraphRuntime.h>
#include <AnimGraph.h>
#include <Skeleton.h>
#include <AnimEvaluator.h>
#include <cstring>
#include <AnimationGraph_generated.h>
#include <CommonEvents.h>
#include <Logging.h>
#include <flatbuffers/flatbuffers.h>

#include <cmath>
#include <algorithm>

namespace SE {

// Hash of "" — computed once (see kEmptyName in the header)
const StrID AnimGraphInstance::kEmptyName("");

// ============================================================
// AnimParamStore
// ============================================================

void AnimParamStore::SetFloat(StrID name, float value) {
        auto& e   = mEntries[name];
        e.type     = Type::Float;
        e.float_val = value;
}

void AnimParamStore::SetBool(StrID name, bool value) {
        auto& e  = mEntries[name];
        e.type    = Type::Bool;
        e.bool_val = value;
}

void AnimParamStore::SetInt(StrID name, int value) {
        auto& e = mEntries[name];
        e.type   = Type::Int;
        e.int_val = value;
}

void AnimParamStore::SetTrigger(StrID name) {
        auto& e    = mEntries[name];
        e.type      = Type::Trigger;
        e.triggered = true;
}

float AnimParamStore::GetFloat(StrID name) const {
        auto it = mEntries.find(name);
        if (it == mEntries.end()) return 0.0f;
        return it->second.float_val;
}

bool AnimParamStore::GetBool(StrID name) const {
        auto it = mEntries.find(name);
        if (it == mEntries.end()) return false;
        return it->second.bool_val;
}

int AnimParamStore::GetInt(StrID name) const {
        auto it = mEntries.find(name);
        if (it == mEntries.end()) return 0;
        return it->second.int_val;
}

bool AnimParamStore::PeekTrigger(StrID name) const {
        auto it = mEntries.find(name);
        if (it == mEntries.end()) return false;
        return it->second.triggered;
}

bool AnimParamStore::ConsumeTrigger(StrID name) {
        auto it = mEntries.find(name);
        if (it == mEntries.end()) return false;
        if (!it->second.triggered) return false;
        it->second.triggered = false;
        return true;
}

void AnimParamStore::ExpireTriggers() {
        for (auto& [name, entry] : mEntries) {
                if (entry.type == Type::Trigger || entry.triggered) {
                        entry.triggered = false;
                }
        }
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
        transition_target_name  = kEmptyName;
        transition_progress  = 0.0f;
        current_time         = 0.0f;
        transition_time        = 0.0f;

        // ---- Collect clips from all states ----
        if (!pGraphFB->states()) {
                log_e("AnimGraphInstance::Init: graph has no states");
                return;
        }

        const auto& oConfig = GetSystem<Config>();
        
        for (uint32_t si = 0; si < pGraphFB->states()->size(); ++si) {
                const auto* state = pGraphFB->states()->Get(si);
                if (!state || !state->nodes()) continue;
                for (uint32_t ni = 0; ni < state->nodes()->size(); ++ni) {
                        const auto* node = state->nodes()->Get(ni);
                        if (!node || !node->data()) continue;
                        if (node->data_type() != SE::FlatBuffers::BlendNodeDataU::ClipNodeData) continue;
                        const auto* n = node->data_as_ClipNodeData();
                        if (!n || !n->clip()) continue;
                        const auto* holder = n->clip();
                        H<AnimClip> h;
                        if (holder->path() && holder->path()->size() > 0) {
                                h = CreateResource<AnimClip>(oConfig.sResourceDir + holder->path()->c_str());
                        } else if (holder->name() && holder->clip()) {
                                h = CreateResource<AnimClip>(holder->name()->c_str(), holder->clip());
                        }
                        if (!h.IsValid()) {
                                const char* id = (holder->path() && holder->path()->size() > 0)
                                        ? holder->path()->c_str() : holder->name()->c_str();
                                log_e("AnimGraphInstance::Init: failed to create AnimClip resource '{}'", id);
                        }
                        mClips[n] = h;
                }
        }

        log_d("AnimGraphInstance::Init: entry='{}', {} state(s), {} clip(s)",
                        pGraphFB->entry_state()->c_str(),
                        pGraphFB->states()->size(),
                        mClips.size());

        // ---- Compile hot-path lookups: state map + per-source transition table ----
        // All string hashing happens once here; Update() runs hash-free.
        for (uint32_t si = 0; si < pGraphFB->states()->size(); ++si) {
                const auto* state = pGraphFB->states()->Get(si);
                if (!state || !state->id()) continue;
                mStates[StrID(state->id()->c_str())] = state;
        }
        if (pGraphFB->transitions()) {
                for (uint32_t ti = 0; ti < pGraphFB->transitions()->size(); ++ti) {
                        const auto* tr = pGraphFB->transitions()->Get(ti);
                        if (!tr || !tr->from() || !tr->to()) continue;

                        CompiledTransition ct;
                        ct.tr            = tr;
                        ct.to            = StrID(tr->to()->c_str());
                        ct.can_interrupt = tr->can_interrupt();
                        ct.frozen        = (tr->mode() == SE::FlatBuffers::TransitionMode::Frozen);
                        if (tr->conditions()) {
                                ct.vConds.reserve(tr->conditions()->size());
                                for (uint32_t ci = 0; ci < tr->conditions()->size(); ++ci) {
                                        const auto* cond = tr->conditions()->Get(ci);
                                        // Null/paramless conditions are kept as-is:
                                        // EvaluateCondition returns false for them,
                                        // disabling the transition (import-time guard).
                                        CompiledCondition cc;
                                        cc.cond      = cond;
                                        cc.is_trigger = (cond && cond->parameter()
                                                        && cond->op() == SE::FlatBuffers::ConditionOp::Triggered);
                                        if (cond && cond->parameter()) {
                                                cc.param = StrID(cond->parameter()->c_str());
                                        }
                                        ct.vConds.push_back(cc);
                                }
                        }
                        mTransitionsFrom[StrID(tr->from()->c_str())].push_back(std::move(ct));
                }
        }

        // ---- Compile blend-node parameter/mask names (per state, indexed by node) ----
        // The evaluate path reads these instead of hashing FB strings per frame.
        for (uint32_t si = 0; si < pGraphFB->states()->size(); ++si) {
                const auto* state = pGraphFB->states()->Get(si);
                if (!state || !state->nodes()) continue;

                auto & vParams = mNodeParams[state];
                vParams.resize(state->nodes()->size());

                for (uint32_t ni = 0; ni < state->nodes()->size(); ++ni) {
                        const auto* node = state->nodes()->Get(ni);
                        if (!node || !node->data()) continue;

                        CompiledNodeParams & oCP = vParams[ni];
                        switch (node->data_type()) {
                        case SE::FlatBuffers::BlendNodeDataU::Blend1DNodeData: {
                                const auto* n = node->data_as_Blend1DNodeData();
                                if (n && n->parameter()) oCP.param = StrID(n->parameter()->c_str());
                                break;
                        }
                        case SE::FlatBuffers::BlendNodeDataU::Blend2DNodeData: {
                                const auto* n = node->data_as_Blend2DNodeData();
                                if (n && n->param_x()) oCP.param_x = StrID(n->param_x()->c_str());
                                if (n && n->param_y()) oCP.param_y = StrID(n->param_y()->c_str());
                                break;
                        }
                        case SE::FlatBuffers::BlendNodeDataU::AdditiveNodeData: {
                                const auto* n = node->data_as_AdditiveNodeData();
                                if (n && n->weight_param() && n->weight_param()->size() > 0)
                                        oCP.weight = StrID(n->weight_param()->c_str());
                                break;
                        }
                        case SE::FlatBuffers::BlendNodeDataU::LayerNodeData: {
                                const auto* n = node->data_as_LayerNodeData();
                                if (n && n->weight_param() && n->weight_param()->size() > 0)
                                        oCP.weight = StrID(n->weight_param()->c_str());
                                if (n && n->mask_name() && n->mask_name()->size() > 0)
                                        oCP.mask = StrID(n->mask_name()->c_str());
                                break;
                        }
                        default: break;
                        }
                }
        }
}

// ============================================================
// AnimGraphInstance::Update
// ============================================================

void AnimGraphInstance::Update(float dt) {
        if (!pGraphFB) return;
        if (paused) { last_dt = 0.0f; return; }
        last_dt = dt;

        const bool transitioning = (transition_target_name != kEmptyName);

        // ---- Advance times ----
        prev_time = current_time;
        // Frozen mode: the source state's clock holds while the destination fades
        // in (the source pose stays frozen at the frame the transition armed).
        if (!(transitioning && transition_frozen)) {
                current_time += dt;
        }
        if (transitioning) {
                transition_time += dt;
                transition_progress += dt / (transition_duration > 1e-6f ? transition_duration : 1e-6f);
        }

        // ---- Fire AnimEvents for primary state clip before wrapping ----
        // Also extracts the root-motion delta (bone 0) when enabled. prev/current
        // times here are pre-wrap; extractRootMotionDelta handles the loop boundary.
        if (use_root_motion) { last_root_delta = RootMotionDelta{}; }
        {
                const SE::FlatBuffers::AnimState* curState = FindState(current_state_name);
                if (curState && curState->nodes() && curState->nodes()->size() > 0) {
                        const auto* rootNode = curState->nodes()->Get(0);
                        if (rootNode && rootNode->data_type() == SE::FlatBuffers::BlendNodeDataU::ClipNodeData) {
                                const auto* clipData = static_cast<const SE::FlatBuffers::ClipNodeData*>(rootNode->data());
                                if (clipData) {
                                        auto itClip = mClips.find(clipData);
                                        if (itClip != mClips.end()) {
                                                const AnimClip* pClip = GetResource(itClip->second);
                                                if (pClip) {
                                                        const StrID state_name = current_state_name;
                                                        // current_time runs in state-time: map to clip-time
                                                        // (playback_rate × state speed) for event sampling.
                                                        const float clip_rate  = clipData->playback_rate();
                                                        const float clip_speed = GetStateSpeed(curState);
                                                        const float clip_prev  = prev_time    * clip_rate * clip_speed;
                                                        const float clip_cur   = current_time * clip_rate * clip_speed;
                                                        CheckAnimEvents(*pClip, clip_prev, clip_cur, pClip->Looping(),
                                                                        [&state_name](const AnimClip::AnimEvent& ev) {
                                                                        EAnimEvent oEvt;
                                                                        // bounded copy into the fixed buffer
                                                                        std::strncpy(oEvt.name, ev.name.c_str(), sizeof(oEvt.name) - 1);
                                                                        oEvt.name[sizeof(oEvt.name) - 1] = '\0';
                                                                        oEvt.name_id    = ev.nameID;
                                                                        oEvt.value      = ev.value;
                                                                        oEvt.state_name = state_name;
                                                                        GetSystem<EventManager>().TriggerEvent(oEvt);
                                                                        });
                                                        if (use_root_motion) {
                                                                last_root_delta = extractRootMotionDelta(
                                                                                *pClip, clip_prev, clip_cur,
                                                                                pClip->Looping(), oRMConfig);
                                                        }
                                                }
                                        }
                                }
                        }
                }
        }

        // ---- Wrap or clamp current clip time based on looping flag ----
        {
                const SE::FlatBuffers::AnimState* curState = FindState(current_state_name);
                float dur = GetStateDuration(curState);
                if (dur > 1e-6f) {
                        if (IsStateLooping(curState))
                                current_time = std::fmod(current_time, dur);
                        else
                                current_time = std::min(current_time, dur);
                }
        }

        // ---- Complete transition if done ----
        bool transition_completed = false;
        if (transitioning && transition_progress >= 1.0f) {
                current_state_name     = transition_target_name;
                transition_target_name = kEmptyName;
                current_time          = transition_time;
                transition_time       = 0.0f;
                transition_progress   = 0.0f;
                transition_duration   = 0.2f;
                transition_frozen     = false;

                // Wrap or clamp new current time based on looping flag
                const SE::FlatBuffers::AnimState* newState = FindState(current_state_name);
                float dur = GetStateDuration(newState);
                if (dur > 1e-6f) {
                        if (IsStateLooping(newState))
                                current_time = std::fmod(current_time, dur);
                        else
                                current_time = std::min(current_time, dur);
                }
                transition_completed = true;  // Don't start another transition in the same frame
        }

        // ---- Check for new transitions ----
        // While a transition is in flight, only can_interrupt transitions leaving the
        // current (source) state are considered — they cancel and replace it.
        // The in-flight transition itself is excluded: its conditions still hold by
        // definition (it is what started the fade), so re-taking it would reset
        // progress every frame and the fade could never complete.
        if (!transition_completed) {
                const bool interrupting = (transition_target_name != kEmptyName);
                const SE::FlatBuffers::AnimState* curState = FindState(current_state_name);
                float stateDuration = GetStateDuration(curState);

                auto itFrom = mTransitionsFrom.find(current_state_name);
                if (itFrom != mTransitionsFrom.end()) {
                        for (const CompiledTransition& ct : itFrom->second) {
                                const auto* tr = ct.tr;
                                if (interrupting && !ct.can_interrupt) continue;
                                if (interrupting && ct.to == transition_target_name) continue;

                                // ---- Check exit_time ----
                                if (tr->has_exit_time()) {
                                        float normalizedTime = (stateDuration > 1e-6f)
                                                ? (current_time / stateDuration)
                                                : 1.0f;
                                        if (normalizedTime < tr->exit_time()) continue;
                                }

                                // ---- Check all conditions (non-destructive for triggers) ----
                                bool allMet = true;
                                for (const CompiledCondition& cc : ct.vConds) {
                                        if (!EvaluateCondition(cc)) { allMet = false; break; }
                                }

                                if (allMet) {
                                        // The transition is taken — now consume its trigger params.
                                        for (const CompiledCondition& cc : ct.vConds) {
                                                if (cc.is_trigger) oParams.ConsumeTrigger(cc.param);
                                        }

                                        // Begin (or restart, on interrupt) the transition
                                        transition_target_name = ct.to;
                                        transition_duration   = tr->duration();
                                        transition_progress   = 0.0f;
                                        transition_time       = 0.0f;
                                        transition_frozen     = ct.frozen;
                                        break;  // Take first matching transition
                                }
                        }
                }
        }

        // ---- Expire unconsumed triggers (edge-event semantics) ----
        // A trigger armed after the previous Update() had this pass to act on it;
        // anything still armed now is dropped so stale triggers cannot fire late.
        oParams.ExpireTriggers();
}

// ============================================================
// AnimGraphInstance::EvaluateBlendTree
// ============================================================

void AnimGraphInstance::EvaluateBlendTree(float weight,
                LocalPose& oOutPose,
                FrameAllocator& alloc,
                const Skeleton& skeleton) {

        if (!pGraphFB) return;

        const bool transitioning = (transition_target_name != kEmptyName);

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

        const bool transitioning = (transition_target_name != kEmptyName);

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
                                        const auto* n = rootNode->data_as_ClipNodeData();
                                        if (n) {
                                                auto it = mClips.find(n);
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
                                        const auto* n = rootNode->data_as_ClipNodeData();
                                        if (n) {
                                                auto it = mClips.find(n);
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

bool AnimGraphInstance::EvaluateCondition(const CompiledCondition& cc) {
        const auto* cond = cc.cond;
        if (!cond || !cond->parameter()) return false;
        using Op = SE::FlatBuffers::ConditionOp;
        const StrID& paramID = cc.param;
        float threshold = cond->threshold();
        switch (cond->op()) {
                case Op::Greater:   return oParams.GetFloat(paramID) > threshold;
                case Op::Less:      return oParams.GetFloat(paramID) < threshold;
                case Op::Equal:     return std::abs(oParams.GetFloat(paramID) - threshold) < 1e-4f;
                case Op::NotEqual:  return std::abs(oParams.GetFloat(paramID) - threshold) >= 1e-4f;
                case Op::IsTrue:    return oParams.GetBool(paramID);
                case Op::IsFalse:   return !oParams.GetBool(paramID);
                // Non-destructive: the trigger is consumed only when the owning
                // transition is actually taken (see Update), so a failing sibling
                // condition can no longer silently destroy it.
                case Op::Triggered: return oParams.PeekTrigger(paramID);
                default:            return false;
        }
}

const SE::FlatBuffers::AnimState* AnimGraphInstance::FindState(StrID name) const {
        auto it = mStates.find(name);
        return (it != mStates.end()) ? it->second : nullptr;
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

void AnimGraphInstance::GetParams(std::vector<ParamInfo>& out) const {
        if (!pGraphFB) return;
        const auto* fb_params = pGraphFB->params();
        if (!fb_params) return;
        out.reserve(fb_params->size());
        const auto& entries = oParams.Entries();
        for (flatbuffers::uoffset_t i = 0; i < fb_params->size(); ++i) {
                const auto* p = fb_params->Get(i);
                if (!p || !p->name()) continue;
                ParamInfo info{};
                info.name = p->name()->c_str();
                auto it = entries.find(StrID(info.name));
                if (it != entries.end()) {
                        const auto& e  = it->second;
                        info.type      = e.type;
                        info.float_val = e.float_val;
                        info.bool_val  = e.bool_val;
                        info.int_val   = e.int_val;
                        info.triggered = e.triggered;
                } else {
                        info.type      = static_cast<AnimParamStore::Type>(p->type());
                        info.float_val = p->float_val();
                        info.bool_val  = p->bool_val();
                        info.int_val   = p->int_val();
                }
                out.push_back(info);
        }
}

void AnimGraphInstance::ForceSetState(StrID name) {

        if (!FindState(name)) return;
        current_state_name     = name;
        current_time          = 0.0f;
        prev_time             = 0.0f;
        transition_target_name = kEmptyName;
        transition_progress   = 0.0f;
        transition_time       = 0.0f;
        transition_frozen     = false;
        mNodePhase.clear();  // fresh state entry — restart blend-node phases
}

void AnimGraphInstance::SetPaused(bool new_paused) { paused = new_paused; }

float AnimGraphInstance::GetStateDuration(const SE::FlatBuffers::AnimState* state) const {
        return GetNodeDuration(state, 0);
}

float AnimGraphInstance::GetStateSpeed(const SE::FlatBuffers::AnimState* state) {
        // Unity-style state speed: scales the state clock (0 clamps to eps so a
        // misauthored 0-speed state degrades to near-frozen instead of NaNs).
        if (!state) return 1.0f;
        const float speed = state->speed();
        return (speed > 1e-6f) ? speed : 1e-6f;
}

float AnimGraphInstance::GetNodeDuration(const SE::FlatBuffers::AnimState* state, uint16_t nodeIdx) const {
        if (!state || !state->nodes() || nodeIdx >= state->nodes()->size()) return 0.0f;
        const auto* node = state->nodes()->Get(nodeIdx);
        if (!node) return 0.0f;
        if (node->data_type() == SE::FlatBuffers::BlendNodeDataU::ClipNodeData) {
                const auto* n = node->data_as_ClipNodeData();
                if (!n) return 0.0f;
                auto it = mClips.find(n);
                if (it == mClips.end() || !it->second.IsValid()) return 0.0f;
                const AnimClip* pClip = GetResource(it->second);
                if (!pClip) return 0.0f;
                float rate = n->playback_rate();
                if (rate < 1e-6f) rate = 1.0f;
                return pClip->Duration() / (rate * GetStateSpeed(state));
        }
        // Composite roots: normalized state progress (exit-time gate, wrap)
        // follows the BASE child — the continuing motion — not the additive or
        // layered contribution on top of it. Without this, an additive/layer
        // root reports duration 0, the exit gate reads progress 1.0 and the
        // state exits on the very next Update.
        if (node->data_type() == SE::FlatBuffers::BlendNodeDataU::AdditiveNodeData) {
                const auto* n = node->data_as_AdditiveNodeData();
                if (n && n->base_index() != nodeIdx) {
                        return GetNodeDuration(state, n->base_index());
                }
                return 0.0f;
        }
        if (node->data_type() == SE::FlatBuffers::BlendNodeDataU::LayerNodeData) {
                const auto* n = node->data_as_LayerNodeData();
                if (n && n->base_index() != nodeIdx) {
                        return GetNodeDuration(state, n->base_index());
                }
                return 0.0f;
        }
        return 0.0f;
}

const std::vector<uint16_t>& AnimGraphInstance::MirrorPairs(const Skeleton& skeleton) {
        auto it = mMirrorPairs.find(&skeleton);
        if (it == mMirrorPairs.end()) {
                it = mMirrorPairs.emplace(&skeleton, BuildMirrorPairs(skeleton)).first;
        }
        return it->second;
}

bool AnimGraphInstance::IsStateLooping(const SE::FlatBuffers::AnimState* state) const {
        return IsNodeLooping(state, 0);
}

bool AnimGraphInstance::IsNodeLooping(const SE::FlatBuffers::AnimState* state, uint16_t nodeIdx) const {
        if (!state || !state->nodes() || nodeIdx >= state->nodes()->size()) return true;
        const auto* node = state->nodes()->Get(nodeIdx);
        if (!node) return true;
        if (node->data_type() == SE::FlatBuffers::BlendNodeDataU::ClipNodeData) {
                const auto* n = node->data_as_ClipNodeData();
                if (!n) return true;
                auto it = mClips.find(n);
                if (it == mClips.end() || !it->second.IsValid()) return true;
                const AnimClip* pClip = GetResource(it->second);
                if (!pClip) return true;
                return pClip->Looping();
        }
        // Composite roots loop like their base child — same convention as
        // GetNodeDuration, so the state clock wraps/clamps consistently with
        // the duration the exit-time gate normalizes against.
        if (node->data_type() == SE::FlatBuffers::BlendNodeDataU::AdditiveNodeData) {
                const auto* n = node->data_as_AdditiveNodeData();
                return (n && n->base_index() != nodeIdx) ? IsNodeLooping(state, n->base_index()) : true;
        }
        if (node->data_type() == SE::FlatBuffers::BlendNodeDataU::LayerNodeData) {
                const auto* n = node->data_as_LayerNodeData();
                return (n && n->base_index() != nodeIdx) ? IsNodeLooping(state, n->base_index()) : true;
        }
        return true;
}

void AnimGraphInstance::EvaluateState(const SE::FlatBuffers::AnimState* state,
                float local_time, float weight,
                LocalPose& oOutPose,
                FrameAllocator& alloc,
                const Skeleton& skeleton) {

        if (!state || !state->nodes() || state->nodes()->size() == 0) return;
        EvaluateNode(state, 0, local_time, weight, oOutPose, alloc, skeleton);

        // State-level mirror: reflect the whole evaluated tree (UE-style mirrored
        // state). Node-level mirror (ClipNodeData.mirror) already applied inside
        // the clip node — the two compose (double reflection cancels for that
        // node's contribution), which lets a single mirrored clip serve both.
        if (state->mirror()) {
                MirrorPose(oOutPose, MirrorPairs(skeleton));
                RenormalizeRotations(oOutPose);
        }
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

        // Compiled parameter/mask StrIDs (hashed once in Init) — no per-frame hashing.
        static const CompiledNodeParams oNoParams {};
        const CompiledNodeParams * pCP = &oNoParams;
        if (auto it = mNodeParams.find(state);
                        it != mNodeParams.end() && nodeIdx < it->second.size()) {
                pCP = &it->second[nodeIdx];
        }

        switch (node->data_type()) {

                // ------------------------------------------------------------------
                case BNU::ClipNodeData: {
                                                const auto* n = node->data_as_ClipNodeData();
                                                if (!n) return;

                                                auto it = mClips.find(n);
                                                if (it == mClips.end() || !it->second.IsValid()) return;

                                                const AnimClip* pClip = GetResource(it->second);
                                                if (!pClip) return;

                                                float rate = n->playback_rate();
                                                if (rate < 1e-6f) rate = 1.0f;
                                                const float speed = GetStateSpeed(state);
                                                float dur = pClip->Duration();
                                                float scaled_time = local_time * rate * speed;
                                                float t = (dur > 1e-6f)
                                                        ? (pClip->Looping() ? std::fmod(scaled_time, dur) : std::clamp(scaled_time, 0.0f, dur))
                                                        : 0.0f;

                                                if (n->mirror()) {
                                                        // Mirrored clip node: sample into scratch, reflect with
                                                        // L/R swap, then copy over (mirrored blend trees blend
                                                        // mirrored contributions — reflection composes linearly).
                                                        LocalPose scratch = AllocatePose(oOutPose.bone_count, alloc);
                                                        InitBindPose(scratch, skeleton);
                                                        SampleClip(*pClip, t, scratch);
                                                        RenormalizeRotations(scratch);
                                                        MirrorPose(scratch, MirrorPairs(skeleton));

                                                        const uint32_t limit = std::min(scratch.bone_count, oOutPose.bone_count);
                                                        for (uint32_t bi = 0; bi < limit; ++bi) {
                                                                oOutPose.pPos[bi] = scratch.pPos[bi];
                                                                oOutPose.pRot[bi] = scratch.pRot[bi];
                                                                oOutPose.pScl[bi] = scratch.pScl[bi];
                                                        }
                                                } else {
                                                        SampleClip(*pClip, t, oOutPose);
                                                }
                                                break;
                                        }

                                        // ------------------------------------------------------------------
                case BNU::Blend1DNodeData: {
                                                   auto* n = node->data_as_Blend1DNodeData();
                                                   if (!n || !n->thresholds() || !n->child_indices() || !n->parameter()) return;

                                                   float param             = oParams.GetFloat(pCP->param);
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

                                                                   // Normalized-phase sync (Unity/UE-style): the node owns one
                                                                   // shared phase, advanced against the dominant child's
                                                                   // duration; every child is sampled at phase × its own
                                                                   // duration, so clips of different lengths stay
                                                                   // foot-phase-locked (no foot sliding). Non-clip children
                                                                   // fall back to the unsynced state time.
                                                                   const float durA = GetNodeDuration(state, children->Get(i));
                                                                   const float durB = GetNodeDuration(state, children->Get(i + 1));
                                                                   const float anchor_dur = (t < 0.5f) ? durA : durB;

                                                                   float& phase = mNodePhase[node];
                                                                   phase += (anchor_dur > 1e-6f)
                                                                           ? (last_dt / anchor_dur)
                                                                           : last_dt;
                                                                   phase = std::fmod(phase, 1.0f);

                                                                   const float timeA = (durA > 1e-6f) ? phase * durA : local_time;
                                                                   const float timeB = (durB > 1e-6f) ? phase * durB : local_time;

                                                                   // Evaluate each child to a full pose, then blend — avoids
                                                                   // partial-weight blend-to-zero for partial-rig clips.
                                                                   LocalPose poseA = AllocatePose(skeleton.BoneCount(), alloc);
                                                                   InitBindPose(poseA, skeleton);
                                                                   EvaluateNode(state, children->Get(i), timeA, 1.0f, poseA, alloc, skeleton);
                                                                   RenormalizeRotations(poseA);

                                                                   LocalPose poseB = AllocatePose(skeleton.BoneCount(), alloc);
                                                                   InitBindPose(poseB, skeleton);
                                                                   EvaluateNode(state, children->Get(i + 1), timeB, 1.0f, poseB, alloc, skeleton);
                                                                   RenormalizeRotations(poseB);

                                                                   BlendPoses(poseA, poseB, t, oOutPose);
                                                                   break;
                                                           }
                                                   }
                                                   break;
                                           }

                                           // ------------------------------------------------------------------
                case BNU::Blend2DNodeData: {
                                                   auto* n = node->data_as_Blend2DNodeData();
                                                   if (!n || !n->positions() || !n->child_indices() || !n->param_x() || !n->param_y()) return;

                                                   float px = oParams.GetFloat(pCP->param_x);
                                                   float py = oParams.GetFloat(pCP->param_y);

                                                   const auto* positions = n->positions();
                                                   const auto* children  = n->child_indices();
                                                   uint32_t count = std::min(positions->size(), children->size());
                                                   if (count == 0) return;

                                                   if (n->algorithm() == SE::FlatBuffers::Blend2DAlgorithm::SimpleDirectional) {
                                                           // SimpleDirectional: nearest-neighbour — the child whose
                                                           // blend point is closest to the parameter point.
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
                                                   } else {
                                                           // FreeformCartesian: inverse-distance-squared weights over
                                                           // all children (Shepard interpolation) — smooth freeform
                                                           // blends without a triangulation pass. Sequential blending
                                                           // (each next child blended by its normalized weight share)
                                                           // keeps rotations on proper slerp arcs.
                                                           float total_inv = 0.0f;
                                                           for (uint32_t i = 0; i < count; ++i) {
                                                                   const auto& pt = *positions->Get(i);
                                                                   float dx = pt.x() - px;
                                                                   float dy = pt.y() - py;
                                                                   total_inv += 1.0f / (dx * dx + dy * dy + 1e-6f);
                                                           }
                                                           if (total_inv <= 0.0f) return;

                                                           float acc = 0.0f;
                                                           bool  first = true;
                                                           for (uint32_t i = 0; i < count; ++i) {
                                                                   const auto& pt = *positions->Get(i);
                                                                   float dx = pt.x() - px;
                                                                   float dy = pt.y() - py;
                                                                   const float w = (1.0f / (dx * dx + dy * dy + 1e-6f)) / total_inv;
                                                                   if (w < 1e-5f && !first) continue;

                                                                   acc += w;
                                                                   const float norm_w = w / acc;

                                                                   LocalPose childPose = AllocatePose(skeleton.BoneCount(), alloc);
                                                                   InitBindPose(childPose, skeleton);
                                                                   EvaluateNode(state, children->Get(i), local_time, 1.0f, childPose, alloc, skeleton);
                                                                   RenormalizeRotations(childPose);

                                                                   if (first) {
                                                                           const uint32_t limit = std::min(childPose.bone_count, oOutPose.bone_count);
                                                                           for (uint32_t bi = 0; bi < limit; ++bi) {
                                                                                   oOutPose.pPos[bi] = childPose.pPos[bi];
                                                                                   oOutPose.pRot[bi] = childPose.pRot[bi];
                                                                                   oOutPose.pScl[bi] = childPose.pScl[bi];
                                                                           }
                                                                           first = false;
                                                                   } else {
                                                                           BlendPoses(oOutPose, childPose, norm_w, oOutPose);
                                                                   }
                                                           }
                                                   }
                                                   break;
                                           }

                                           // ------------------------------------------------------------------
                case BNU::AdditiveNodeData: {
                                                    auto* n = node->data_as_AdditiveNodeData();
                                                    if (!n) return;

                                                    // Resolve additive weight
                                                    float w = n->weight();
                                                    if (n->weight_param() && n->weight_param()->size() > 0) {
                                                            w = oParams.GetFloat(pCP->weight);
                                                    }
                                                    w = std::clamp(w, 0.0f, 1.0f);

                                                    // Evaluate base normally into oOutPose
                                                    EvaluateNode(state, n->base_index(), local_time, weight, oOutPose, alloc, skeleton);

                                                    // Evaluate additive layer into a separate pose initialised to bind
                                                    LocalPose addPose = AllocatePose(skeleton.BoneCount(), alloc);
                                                    InitBindPose(addPose, skeleton);
                                                    EvaluateNode(state, n->additive_index(), local_time, weight, addPose, alloc, skeleton);
                                                    RenormalizeRotations(addPose);

                                                    // Reference = the same additive subtree sampled at its local
                                                    // time 0 (Unity-style default additive reference pose).
                                                    // Deltas are relative to it, NOT to the skeleton bind pose —
                                                    // bind only seeds bones the subtree does not animate, where
                                                    // add == ref and the delta is exactly zero. NOTE: the subtree
                                                    // is evaluated a second time; phase-synced nodes (Blend1D)
                                                    // inside an additive subtree would double-advance, so keep
                                                    // additive subtrees to clip nodes.
                                                    LocalPose refPose = AllocatePose(skeleton.BoneCount(), alloc);
                                                    InitBindPose(refPose, skeleton);
                                                    EvaluateNode(state, n->additive_index(), 0.0f, weight, refPose, alloc, skeleton);
                                                    RenormalizeRotations(refPose);

                                                    // Additive blend: add the reference-relative delta
                                                    for (uint32_t i = 0; i < oOutPose.bone_count && i < static_cast<uint32_t>(skeleton.BoneCount()); ++i) {
                                                            glm::vec3 deltaPos = addPose.pPos[i] - refPose.pPos[i];
                                                            oOutPose.pPos[i] += deltaPos * w;

                                                            // Delta rotation in local space: delta = inv(ref) * addRot
                                                            glm::quat deltaRot = glm::inverse(refPose.pRot[i]) * addPose.pRot[i];
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
                                                         w = oParams.GetFloat(pCP->weight);
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
                                                         mask = skeleton.FindMask(pCP->mask);
                                                         if (!mask) {
                                                                 // Silent all-bones fallback hides rig/asset bugs — say so.
                                                                 log_w("AnimGraphInstance: bone mask '{}' not found in skeleton '{}' "
                                                                       "(layer node) — applying layer weight to ALL bones",
                                                                       n->mask_name()->c_str(), skeleton.Str());
                                                         }
                                                 }

                                                 const bool additive = (n->blend_mode() == SE::FlatBuffers::LayerBlendMode::AdditiveLayer);

                                                 // Additive layers blend their delta from the layer's own
                                                 // frame-0 reference pose (same convention as the additive
                                                 // node); Override mode ignores it.
                                                 LocalPose layerRefPose;
                                                 if (additive) {
                                                         layerRefPose = AllocatePose(skeleton.BoneCount(), alloc);
                                                         InitBindPose(layerRefPose, skeleton);
                                                         EvaluateNode(state, n->layer_index(), 0.0f, weight, layerRefPose, alloc, skeleton);
                                                         RenormalizeRotations(layerRefPose);
                                                 }

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
                                                                 oOutPose.pPos[i] += (layerPose.pPos[i] - layerRefPose.pPos[i]) * boneW;
                                                                 glm::quat deltaRot = glm::inverse(layerRefPose.pRot[i]) * layerPose.pRot[i];
                                                                 oOutPose.pRot[i]  = oOutPose.pRot[i] * glm::slerp(
                                                                                 glm::quat(1.0f, 0.0f, 0.0f, 0.0f), deltaRot, boneW);
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
