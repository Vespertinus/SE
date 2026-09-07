
#ifndef APP_AI_BRAIN_H
#define APP_AI_BRAIN_H 1

#include <string>
#include <glm/vec2.hpp>

namespace SE {

class StateMachine;
class InputState;

// ---------------------------------------------------------------------------
// AIBrain — the AI side of the behavior/intent layer.
//
// This is the NPC counterpart of InputMappingContext: it produces InputState
// (the shared intent contract) rather than driving animation or physics directly.
// It is the executor for a sibling StateMachine (the decision maker):
//   1. Perception — writes params into the StateMachine (e.g. dist_to_player),
//      plus the timer triggers that cycle Idle ↔ Wander.
//   2. Action — reads the StateMachine's current state and writes InputState
//      accordingly (Idle → still, Wander → roam, Chase → steer to target).
//
// The HSM owns *when* to switch behavior; AIBrain owns *what each behavior does*.
// No separate system needed — subscribes to EUpdate in the constructor.
// ---------------------------------------------------------------------------
class AIBrain {
public:
        explicit AIBrain(TSceneTree::TSceneNodeExact* pOwner);
        ~AIBrain() noexcept;

        void Enable()  {}
        void Disable() {}

        // The actor this brain perceives and chases (typically the player).
        void SetTarget(TSceneTree::TSceneNodeWeak pNewTarget) { pTarget = pNewTarget; }

        std::string Str()       const { return "AIBrain"; }
        void        DrawDebug() const {}

private:
        void OnUpdate(const Event& e);
        void PickNewDirection();

        TSceneTree::TSceneNodeExact* pNode;
        TSceneTree::TSceneNodeWeak   pTarget;

        glm::vec2 v_direction {0.f, 1.f};  // world-space XZ wander heading, normalized
        float wander_timer = 0.f;
        float idle_timer   = 0.f;
        float last_dist_to_player = -1.f;  // skip the param-store write while unchanged

        static constexpr float kWalkDuration   = 3.0f;   // s before firing wander_done
        static constexpr float kIdleDuration   = 1.2f;   // s before firing idle_done
        static constexpr float kChaseRange     = 5.0f;   // m — enter Chase below this
        static constexpr float kChaseSpeedMul  = 1.0f;   // wander/chase share base speed
};

} // namespace SE

#endif
