
#ifndef APP_STATE_MACHINE_H
#define APP_STATE_MACHINE_H 1

#include <memory>
#include <array>
#include <stdexcept>
#include <variant>
#include <string_view>

#include <StrID.h>
#include <ResourceHandle.h>
#include <hsm/ParameterStore.h>

namespace SE::FlatBuffers { struct StateMachine; }

namespace SE {

class StateMachineAsset;
class StateMachineSystem;

// Posted by StateMachineSystem when a state is entered or exited.
// Listeners check event_name against their own on_enter_event / on_exit_event StrIDs.
struct EStateMachineEvent {
        StrID                      event_name;
        StrID                      state;
        StrID                      previous_state;
        TSceneTree::TSceneNodeWeak pNode;
};

// Targeted parameter-update event: external systems post this to drive a specific
// entity's state machine parameters without direct component references.
struct EHSMEvent {
        static constexpr uint32_t kMaxParams = 10;

        struct Param {
                StrID        name;
                TParamValue  value; // float, bool, int, or SMTrigger
        };

        TSceneTree::TSceneNodeWeak     pNode;
        uint8_t                        param_count = 0;
        std::array<Param, kMaxParams>  params;
};

// ---------------------------------------------------------------------------
// StateMachine — per-entity runtime state.
//
// Register the component with StateMachineSystem by calling Enable(); the system
// drives updates.  Disable() removes from the system.
//
// Add to TCustomComponents in the application's App.h when needed.
// ---------------------------------------------------------------------------
class StateMachine {

        friend class StateMachineSystem;

public:
        using TSerialized = FlatBuffers::StateMachine;

        struct Desc {
                const char* definition_path;
                float       tick_interval = 0.f;
        };

        StateMachine(TSceneTree::TSceneNodeExact* pNode, const Desc& desc);
        StateMachine(TSceneTree::TSceneNodeExact* pNode,
                              const FlatBuffers::StateMachine* pFB);
        ~StateMachine() noexcept;

        void Enable();
        void Disable();

        StrID   GetCurrentState()       const { return current_state; }
        StrID   GetPreviousState()      const { return previous_state; }
        float   GetTimeInState()        const { return time_in_state; }
        bool    IsInTransition()        const { return in_transition; }
        float   GetTransitionProgress() const { return transition_progress; }
        StrID   GetTransitionTarget()   const { return transition_target; }
        float   GetTickInterval()       const { return tick_interval; }
        float   GetTimeSinceLastTick()  const { return time_since_last_tick; }

        ParameterStore&       GetParams()       { return oParams; }
        const ParameterStore& GetParams() const { return oParams; }

        // Returns the loaded asset, whether from ResourceManager or owned inline.
        const StateMachineAsset* GetAsset() const;

        void DrawDebug() const {}

        std::string Str() const;

private:
        void Init(const char* definition_path,
                  const FlatBuffers::StateMachineDefinition* pInlineDef,
                  float tick_interval_val);

        TSceneTree::TSceneNodeExact*       pNode = nullptr;
        H<StateMachineAsset>               hDefinition;
        ParameterStore                     oParams;

        // Runtime state — written by StateMachineSystem
        StrID   current_state;
        StrID   previous_state;
        float   time_in_state       = 0.f;
        bool    in_transition       = false;
        float   transition_progress = 0.f;
        StrID   transition_target;

        float   tick_interval        = 0.f;
        float   time_since_last_tick = 0.f;
};

} // namespace SE

#endif
