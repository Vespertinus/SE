
#ifndef APP_CHARACTER_MOVEMENT_SYSTEM_H
#define APP_CHARACTER_MOVEMENT_SYSTEM_H 1

#include <unordered_map>
#include <glm/vec3.hpp>
#include <MovementEvents.h>
#include <glm/vec2.hpp>

namespace SE {

class CharacterController;
class InputState;
class Event;

// Drives all registered CharacterControllers each EUpdate.
// Components self-register via Enable() / Disable().
//
// Update sequence per entity (runs before PhysicsSystem::Update):
//   1. Read grounded state from PhysicsSystem (prior fixed step result)
//   2. Manage coyote timer and time_airborne
//   3. Record jump intent into jump_buffer_timer
//   4. Fire jump if buffer active and grounded / coyote window open
//   5. Blend horizontal velocity (input-driven, skipped when use_root_motion is set)
//   6. Apply gravity to vertical velocity
//   7. PhysicsSystem::SetCharacterVelocity (adds root-motion accumulator, then resets it)
//   8. Update MovementMode
//   9. Post clean ECharacter* movement events (no state-machine coupling)
//
// Root-motion flow: AnimationSystem publishes EAnimRootMotion after pose evaluation.
// CharacterMovementSystem::OnAnimRootMotion accumulates into v_root_motion_vel.
// ProcessComponent consumes and resets it at step 7.
//
// Transform sync is automatic: PhysicsSystem::Interpolate() writes
// pNode->SetWorldPos(joltChar->GetPosition()) for all registered characters.
class CharacterMovementSystem {
public:
        CharacterMovementSystem();
        ~CharacterMovementSystem() noexcept;

        void AddComponent(CharacterController* pComp);
        void RemoveComponent(CharacterController* pComp);

        // Advance a single component directly — useful for tests.
        void ProcessComponent(CharacterController& oCharCont, float dt);

        // Accumulates animation root-motion delta into the character's v_root_motion_vel.
        // Typically called indirectly via EAnimRootMotion; also exposed for direct use in tests.
        static void ApplyRootMotionDelta(CharacterController& oCharCont,
                                         const glm::vec3& delta, float dt);

private:
        void OnUpdate(const Event& e);
        void OnAnimRootMotion(const Event& e);

        glm::vec3 ComputeDesiredVelocity(const CharacterController& oCharCont,
                                          const InputState& input,
                                          float dt) const;

        void PostMovementEvents(TSceneTree::TSceneNodeExact* pOwner,
                                CharacterController& oCharCont,
                                bool was_grounded,
                                bool just_jumped,
                                bool was_moving,
                                MovementMode prev_mode);

        std::unordered_map<uint32_t, CharacterController*> mComponents;
};

} // namespace SE

#endif
