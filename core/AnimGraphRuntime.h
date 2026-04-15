
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

namespace SE::FlatBuffers {
        struct AnimState;
        struct BlendTreeNode;
        struct AnimCondition;
        struct AnimationGraph;
}

namespace SE {
        class AnimGraph;
        class Skeleton;
        class FrameAllocator;
}

namespace SE {

// ============================================================
// ParameterStore — runtime parameter values for a graph instance
// ============================================================

class ParameterStore {

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

        // Returns true and resets trigger to false; returns false if not found or not triggered.
        bool ConsumeTrigger(StrID name);

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
        // initialised (e.g. with initBindPose). Contributions are *added*; call
        // renormalizeRotations() after.
        void EvaluateBlendTree(float weight, LocalPose& oOutPose,
                        FrameAllocator& alloc, const Skeleton& skeleton);

        // ------------------------------------------------------------------
        // Debug / query
        // ------------------------------------------------------------------

        void GetActiveStates(std::vector<ActiveStateInfo>& out) const;

        // Enumerate all states defined in the loaded graph (by their id string).
        void GetStateNames(std::vector<std::string>& out) const;

        // Immediately jump to the named state; resets playback time and clears any
        // in-progress transition. No-op if the name is unknown.
        void ForceSetState(StrID name);

        // Pause / resume time advancement (Update becomes a no-op while paused).
        void SetPaused(bool new_paused);
        bool IsPaused() const { return paused; }

        // Current playback time in the active state (seconds).
        float GetCurrentTime() const { return current_time; }

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

        StrID CurrentStateName()    const { return current_state_name; }
        bool  IsTransitioning()     const { return transition_target_name != StrID(""); }
        float TransitionProgress()  const { return transition_progress; }

        ParameterStore& Params() { return oParams; }
        const ParameterStore& Params() const { return oParams; }

private:
        // ------------------------------------------------------------------
        // Internal helpers
        // ------------------------------------------------------------------

        bool EvaluateCondition(const SE::FlatBuffers::AnimCondition* cond);

        const SE::FlatBuffers::AnimState* FindState(StrID name) const;

        void EvaluateState(const SE::FlatBuffers::AnimState* state,
                        float local_time, float weight,
                        LocalPose& oOutPose,
                        FrameAllocator& alloc,
                        const Skeleton& skeleton);

        void EvaluateNode(const SE::FlatBuffers::AnimState* state,
                        uint16_t nodeIdx,
                        float local_time, float weight,
                        LocalPose& oOutPose,
                        FrameAllocator& alloc,
                        const Skeleton& skeleton);

        // Returns the duration of a state's root clip (0 if unknown/non-clip root).
        float GetStateDuration(const SE::FlatBuffers::AnimState* state) const;

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
        bool   paused               = false;

        // ------------------------------------------------------------------
        // Resources
        // ------------------------------------------------------------------

        std::unordered_map<StrID, H<AnimClip>> mClips;  // clip_path StrID → handle

        // Non-owning pointer; valid as long as the H<AnimGraph> that owns vRawData is alive.
        const SE::FlatBuffers::AnimationGraph* pGraphFB = nullptr;

        ParameterStore oParams;
};

} // namespace SE

#endif
