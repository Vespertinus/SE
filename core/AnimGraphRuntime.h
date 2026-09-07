
#ifndef __ANIM_GRAPH_RUNTIME_H__
#define __ANIM_GRAPH_RUNTIME_H__ 1

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <StrID.h>
#include <ResourceHandle.h>
#include <AnimClip.h>
#include <AnimEvaluator.h>
#include <SkeletonPoser.h>

namespace SE::FlatBuffers {
        struct AnimState;
        struct BlendTreeNode;
        struct AnimCondition;
        struct AnimTransition;
        struct AnimationGraph;
        struct ClipNodeData;
}

namespace SE {
        class AnimGraph;
        class Skeleton;
        class FrameAllocator;
}

namespace SE {

// ============================================================
// AnimParamStore — runtime parameter values for a graph instance
// ============================================================

class AnimParamStore {

public:
        enum class Type : uint8_t { Float = 0, Bool = 1, Int = 2, Trigger = 3 };

        struct Entry {
                Type  type       = Type::Float;
                float float_val  = 0.0f;
                bool  bool_val   = false;
                int   int_val    = 0;
                bool  triggered  = false;
        };

        void SetFloat(StrID name, float value);
        void SetBool (StrID name, bool  value);
        void SetInt  (StrID name, int   value);
        void SetTrigger(StrID name);

        float GetFloat(StrID name) const;
        bool  GetBool (StrID name) const;
        int   GetInt  (StrID name) const;

        // Non-destructive trigger check for condition evaluation (see Update()).
        bool PeekTrigger(StrID name) const;

        // Returns true and resets trigger to false; returns false if not found or not triggered.
        bool ConsumeTrigger(StrID name);

        // Clear all armed triggers. Called at the end of each Update() pass:
        // triggers are edge events — an armed trigger survives exactly one Update.
        void ExpireTriggers();

        const std::unordered_map<StrID, Entry>& Entries() const { return mEntries; }

private:
        std::unordered_map<StrID, Entry> mEntries;
};

// ============================================================
// ActiveStateInfo — snapshot of a single active state for debug/query
// ============================================================

struct ActiveStateInfo {
        StrID        state_name;
        float        weight    = 1.0f;
        float        local_time = 0.0f;
        H<AnimClip>  hClip;   // valid only for leaf ClipNode states; invalid otherwise
};

struct ParamInfo {
        const char*          name;       // points into FlatBuffers data; valid while AnimGraph resource is alive
        AnimParamStore::Type type;
        float                float_val;
        bool                 bool_val;
        int                  int_val;
        bool                 triggered;
};

// ============================================================
// AnimGraphInstance — per-entity runtime state machine
// ============================================================

class AnimGraphInstance {

public:
        AnimGraphInstance()  = default;
        ~AnimGraphInstance() = default;

        AnimGraphInstance(const AnimGraphInstance&) = delete;
        AnimGraphInstance& operator=(const AnimGraphInstance&) = delete;

        // ------------------------------------------------------------------
        // Lifecycle
        // ------------------------------------------------------------------

        // Parse graph FlatBuffer, set entry state, load default params, create clip resources.
        void Init(const AnimGraph& graph);

        // Tick state machine, advance playback times, consume triggers on transitions.
        void Update(float dt);

        // ------------------------------------------------------------------
        // Pose evaluation
        // ------------------------------------------------------------------

        // Recursive blend tree evaluation. oOutPose must be pre-allocated and
        // initialised (e.g. with initBindPose). Clip leaves *replace* channel
        // values (absolute clips) or add deltas onto them (delta clips); blend
        // nodes combine child poses by weight. Call renormalizeRotations() after.
        void EvaluateBlendTree(float weight, LocalPose& oOutPose,
                        FrameAllocator& alloc, const Skeleton& skeleton);

        // ------------------------------------------------------------------
        // Debug / query
        // ------------------------------------------------------------------

        void GetActiveStates(std::vector<ActiveStateInfo>& out) const;

        // Enumerate all states defined in the loaded graph (by their id string).
        void GetStateNames(std::vector<std::string>& out) const;

        // Enumerate all parameters with their names (from FlatBuffers) and current runtime values.
        void GetParams(std::vector<ParamInfo>& out) const;

        // Name of the transition target state; empty StrID if not transitioning.
        StrID TransitionTargetName() const { return transition_target_name; }

        // Raw FlatBuffer graph definition — for tooling only; valid while the AnimGraph resource is alive.
        const SE::FlatBuffers::AnimationGraph* GetGraphFB() const { return pGraphFB; }

        // Immediately jump to the named state; resets playback time and clears any
        // in-progress transition. No-op if the name is unknown.
        void ForceSetState(StrID name);

        // Pause / resume time advancement (Update becomes a no-op while paused).
        void SetPaused(bool new_paused);
        bool IsPaused() const { return paused; }

        // Current playback time in the active state (seconds).
        float GetCurrentTime() const { return current_time; }

        // Root motion extraction (bone 0 of the primary state's root clip).
        // When enabled, Update() also produces a per-frame root delta; the
        // character animation layer feeds it to the locomotion system, and the
        // Animator strips the root translation from the pose.
        void SetUseRootMotion(bool enable) { use_root_motion = enable; }
        bool IsRootMotionEnabled() const   { return use_root_motion; }
        const RootMotionDelta& GetRootMotionDelta() const { return last_root_delta; }

        // ------------------------------------------------------------------
        // Parameter setters
        // ------------------------------------------------------------------

        void SetFloat  (StrID name, float value) { oParams.SetFloat(name, value); }
        void SetBool   (StrID name, bool  value) { oParams.SetBool (name, value); }
        void SetInt    (StrID name, int   value) { oParams.SetInt  (name, value); }
        void SetTrigger(StrID name)              { oParams.SetTrigger(name); }

        // ------------------------------------------------------------------
        // Parameter getters
        // ------------------------------------------------------------------

        float GetFloat(StrID name) const { return oParams.GetFloat(name); }
        bool  GetBool (StrID name) const { return oParams.GetBool (name); }
        int   GetInt  (StrID name) const { return oParams.GetInt  (name); }

        // ------------------------------------------------------------------
        // Public read-only state
        // ------------------------------------------------------------------

        /** Shared empty-name marker (hash of ""). Note: not equal to a default
         *  StrID{}, which is the 0xDEADBEEF sentinel, so use this — not {} —
         *  wherever "" semantics are intended. */
        static const StrID kEmptyName;

        StrID CurrentStateName()    const { return current_state_name; }
        bool  IsTransitioning()     const { return transition_target_name != kEmptyName; }
        float TransitionProgress()  const { return transition_progress; }

        AnimParamStore& Params() { return oParams; }
        const AnimParamStore& Params() const { return oParams; }

private:
        // ------------------------------------------------------------------
        // Internal helpers
        // ------------------------------------------------------------------

        // Pre-compiled condition: parameter name hashed once at Init, not per frame.
        struct CompiledCondition {
                const SE::FlatBuffers::AnimCondition* cond = nullptr;
                StrID param;
                bool  is_trigger = false;
        };

        // Pre-compiled transition: from-state key (map key below), pre-hashed target,
        // interrupt flag, mode and condition list. Mirrors the FlatBuffers transition.
        // Mode: CrossFade and Frozen are honored; AdditiveBlend falls back to
        // CrossFade (documented limitation).
        struct CompiledTransition {
                const SE::FlatBuffers::AnimTransition* tr = nullptr;
                StrID to;
                bool  can_interrupt = false;
                bool  frozen        = false;   // TransitionMode::Frozen
                std::vector<CompiledCondition> vConds;
        };

        // Pre-compiled blend-node parameter/mask names: hashed once in Init so the
        // evaluate path does no string hashing. Only the fields relevant to the
        // node's type are populated; unused ones stay default (sentinel StrID{}).
        struct CompiledNodeParams {
                StrID param;     // Blend1D
                StrID param_x;   // Blend2D
                StrID param_y;   // Blend2D
                StrID weight;    // Additive / Layer
                StrID mask;      // Layer
        };

        bool EvaluateCondition(const CompiledCondition& cond);

        const SE::FlatBuffers::AnimState* FindState(StrID name) const;

        void EvaluateState(const SE::FlatBuffers::AnimState* state,
                        float local_time, float weight,
                        LocalPose& oOutPose,
                        FrameAllocator& alloc,
                        const Skeleton& skeleton);

        // Evaluates one blend-tree node into oOutPose. Additive contributions
        // (AdditiveNodeData, LayerNodeData in AdditiveLayer mode) are deltas
        // relative to the subtree's own frame-0 pose — the engine re-evaluates
        // the additive subtree at local time 0 as its reference. Keep additive
        // subtrees to clip nodes: a Blend1D inside one would double-advance its
        // phase sync under the reference evaluation.
        void EvaluateNode(const SE::FlatBuffers::AnimState* state,
                        uint16_t nodeIdx,
                        float local_time, float weight,
                        LocalPose& oOutPose,
                        FrameAllocator& alloc,
                        const Skeleton& skeleton);

        // Duration of the state's driving clip: the root clip itself, or for an
        // Additive/Layer root the BASE child's duration (normalized progress,
        // wrap and the exit-time gate follow the continuing motion). 0 for
        // Blend1D/Blend2D roots and unknown clips.
        float GetStateDuration(const SE::FlatBuffers::AnimState* state) const;

        // Rate/speed-scaled duration of an arbitrary node (0 if not a clip node).
        // Includes the state's speed multiplier and the clip node's playback_rate.
        float GetNodeDuration(const SE::FlatBuffers::AnimState* state, uint16_t nodeIdx) const;

        // Returns true if the root clip of the state is marked as looping (defaults to true on error).
        // Looping of the state's driving clip; composite (Additive/Layer) roots
        // follow their BASE child, matching GetStateDuration.
        bool IsStateLooping(const SE::FlatBuffers::AnimState* state) const;
        bool IsNodeLooping(const SE::FlatBuffers::AnimState* state, uint16_t nodeIdx) const;

        // Effective (speed >= eps) speed multiplier of [state]; 1.0 on absence.
        static float GetStateSpeed(const SE::FlatBuffers::AnimState* state);

        // L/R partner table for [skeleton] — built once per skeleton, cached.
        const std::vector<uint16_t>& MirrorPairs(const Skeleton& skeleton);

        // ------------------------------------------------------------------
        // State machine fields
        // ------------------------------------------------------------------

        StrID  current_state_name;       // name of the current (source) state
        StrID  transition_target_name;   // name of the destination state; "empty" StrID if none
        float  transition_duration  = 0.2f;
        float  transition_progress  = 0.0f;  // 0 → 1
        float  current_time         = 0.0f;  // playback time in current state
        float  prev_time            = 0.0f;  // current_time at start of previous Update()
        float  transition_time      = 0.0f;  // playback time in transitioning-in state
        bool   transition_frozen    = false; // Frozen mode: source clock holds during the fade
        bool   paused               = false;

        // ------------------------------------------------------------------
        // Resources
        // ------------------------------------------------------------------

        std::unordered_map<const SE::FlatBuffers::ClipNodeData*, H<AnimClip>> mClips;

        // Hot-path lookup caches, built once in Init. Keyed by raw FlatBuffers
        // pointers/StrIDs — valid while the AnimGraph resource is alive.
        std::unordered_map<StrID, const SE::FlatBuffers::AnimState*> mStates;
        std::unordered_map<StrID, std::vector<CompiledTransition>>   mTransitionsFrom;
        std::unordered_map<const SE::FlatBuffers::AnimState*,
                        std::vector<CompiledNodeParams>>             mNodeParams;

        // Normalized phase [0,1) per Blend1D node (keyed by node pointer) —
        // phase-syncs the blended children, see EvaluateNode's Blend1D branch.
        std::unordered_map<const void*, float> mNodePhase;
        float                                  last_dt = 0.0f;  // dt of the last Update()

        // L/R mirror partner tables per skeleton (see MirrorPose/BuildMirrorPairs).
        // Keyed by skeleton pointer — valid while the resource is alive.
        std::unordered_map<const Skeleton*, std::vector<uint16_t>> mMirrorPairs;

        // Root motion extraction state (see SetUseRootMotion).
        bool             use_root_motion = false;
        RootMotionConfig oRMConfig;
        RootMotionDelta  last_root_delta;

        // Non-owning pointer; valid as long as the H<AnimGraph> that owns vRawData is alive.
        const SE::FlatBuffers::AnimationGraph* pGraphFB = nullptr;

        AnimParamStore oParams;
};

} // namespace SE

#endif
