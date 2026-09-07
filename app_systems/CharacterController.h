
#ifndef APP_CHARACTER_CONTROLLER_H
#define APP_CHARACTER_CONTROLLER_H 1

#include <string>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
#include <PhysicsTypes.h>
#include <MovementEvents.h>

namespace SE {

class CharacterMovementSystem;
struct InputState;

// Physics-driven character controller component.
//
// Stores authored capsule + movement config and per-frame runtime state.
// All runtime fields are private and written exclusively by CharacterMovementSystem.
// Jolt CharacterVirtual is created in the constructor and destroyed in the destructor
// (same lifecycle pattern as RigidBody).
//
// Add to TCustomComponents in App.h. Requires CharacterMovementSystem in TCustomSystems.
class CharacterController {

        friend class CharacterMovementSystem;
public:
        struct Desc {
                float    capsule_radius      = 0.4f;
                float    capsule_half_height = 0.9f;    // half-height of cylindrical section
                float    step_height         = 0.35f;   // max stair riser
                float    max_slope_angle     = 45.0f;   // degrees
                float    max_speed           = 5.0f;    // m/s walking
                float    sprint_multiplier   = 1.8f;
                float    acceleration        = 20.0f;   // m/s²
                float    deceleration        = 30.0f;   // m/s²
                float    air_control         = 0.25f;   // [0,1] fraction of accel in air
                float    jump_impulse        = 6.5f;    // m/s upward on jump
                float    gravity_scale       = 1.0f;
                float    coyote_time         = 0.12f;   // s after leaving ground
                float    jump_buffer_time    = 0.10f;   // s a jump input is remembered
                uint32_t collision_layer     = 0x1u;
                uint32_t collision_mask      = 0xFFFFFFFFu;
        };

        explicit CharacterController(TSceneTree::TSceneNodeExact* pNode);
        CharacterController(TSceneTree::TSceneNodeExact* pNode, const Desc& desc);
        ~CharacterController() noexcept;

        void Enable();
        void Disable();

        // Config getters
        float GetCapsuleRadius()     const { return capsule_radius; }
        float GetCapsuleHalfHeight() const { return capsule_half_height; }
        float GetStepHeight()        const { return step_height; }
        float GetMaxSlopeAngle()     const { return max_slope_angle; }
        float GetMaxSpeed()          const { return max_speed; }
        float GetSprintMultiplier()  const { return sprint_multiplier; }
        float GetAcceleration()      const { return acceleration; }
        float GetDeceleration()      const { return deceleration; }
        float GetAirControl()        const { return air_control; }
        float GetJumpImpulse()       const { return jump_impulse; }
        float GetGravityScale()      const { return gravity_scale; }
        float GetCoyoteTime()        const { return coyote_time; }
        float GetJumpBufferTime()    const { return jump_buffer_time; }

        // Runtime state getters (written by CharacterMovementSystem)
        MovementMode  GetMovementMode()     const { return mode; }
        glm::vec3     GetVelocity()         const { return v_velocity; }
        glm::vec3     GetGroundNormal()     const { return v_ground_normal; }
        bool          IsGrounded()          const { return is_grounded; }
        bool          UsesRootMotion()      const { return use_root_motion; }
        glm::vec3     GetRootMotionVelocity() const { return {v_root_motion_vel.x, 0.f, v_root_motion_vel.y}; }
        float         GetTimeAirborne()     const { return time_airborne; }
        float         GetCoyoteTimer()      const { return coyote_timer; }
        float         GetJumpBufferTimer()  const { return jump_buffer_timer; }
        CharHandle    GetHandle()           const { return hChar; }

        // Enable/disable animation root-motion drive. When true, CharacterMovementSystem
        // skips input-based horizontal blending and uses v_root_motion_vel instead.
        void          SetUseRootMotion(bool enable) { use_root_motion = enable; }
        bool          UseRootMotion()         const { return use_root_motion; }

        // Ragdoll seam (locomotion side). When enabled, the controller stops driving
        // the body (CharacterMovementSystem early-outs) and the animation layer switches
        // its pose source to physics. Disabling restores normal GROUNDED/AIRBORNE control.
        void          SetRagdoll(bool enable) { mode = enable ? MovementMode::RAGDOLL
                                                              : MovementMode::AIRBORNE; }
        bool          IsRagdoll() const { return mode == MovementMode::RAGDOLL; }

        std::string Str()       const;
        void        DrawDebug() const;

private:
        TSceneTree::TSceneNodeExact* pNode;
        CharHandle hChar;

        // Authored config
        float    capsule_radius;
        float    capsule_half_height;
        float    step_height;
        float    max_slope_angle;
        float    max_speed;
        float    sprint_multiplier;
        float    acceleration;
        float    deceleration;
        float    air_control;
        float    jump_impulse;
        float    gravity_scale;
        float    coyote_time;
        float    jump_buffer_time;

        // Runtime state (written by CharacterMovementSystem each frame)
        MovementMode  mode              = MovementMode::GROUNDED;
        glm::vec3     v_velocity        {0.f, 0.f, 0.f};
        glm::vec3     v_ground_normal   {0.f, 1.f, 0.f};
        glm::vec2     v_root_motion_vel {0.f, 0.f};  // XZ; accumulated by AnimationSystem, consumed+reset each frame
        bool          is_grounded       = false;
        bool          is_moving         = false;  // horizontal speed above threshold
        bool          was_sprinting     = false;
        bool          use_root_motion   = false;  // when true, animation drives horizontal movement
        float         coyote_timer      = 0.f;
        float         jump_buffer_timer = 0.f;
        float         time_airborne     = 0.f;
};

} // namespace SE

#endif
