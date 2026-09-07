
#include <CharacterMovementSystem.h>
#include <CharacterController.h>
#include <InputState.h>
#include <EventManager.h>
#include <CommonEvents.h>
#include <Logging.h>

#include <glm/geometric.hpp>
#include <glm/common.hpp>

namespace SE {

CharacterMovementSystem::CharacterMovementSystem() {
        auto& em = GetSystem<EventManager>();
        em.AddListener<EUpdate,         &CharacterMovementSystem::OnUpdate>(this);
        em.AddListener<EAnimRootMotion, &CharacterMovementSystem::OnAnimRootMotion>(this);
}

CharacterMovementSystem::~CharacterMovementSystem() noexcept {
        auto& em = GetSystem<EventManager>();
        em.RemoveListener<EUpdate,         &CharacterMovementSystem::OnUpdate>(this);
        em.RemoveListener<EAnimRootMotion, &CharacterMovementSystem::OnAnimRootMotion>(this);
}

void CharacterMovementSystem::AddComponent(CharacterController* pComp) {
        
        if (!pComp || !pComp->pNode) return;
        mComponents.try_emplace(pComp->pNode->GetID(), pComp);
}

void CharacterMovementSystem::RemoveComponent(CharacterController* pComp) {

        if (!pComp || !pComp->pNode) return;
        mComponents.erase(pComp->pNode->GetID());
}

void CharacterMovementSystem::OnUpdate(const Event& e) {
        
        const float dt = e.Get<EUpdate>().last_frame_time;
        if (dt <= 0.f) return;
        for (auto& [id, pComp] : mComponents) {
                ProcessComponent(*pComp, dt);
        }
}

void CharacterMovementSystem::ProcessComponent(CharacterController& oCharCont, float dt) {

        if (!oCharCont.hChar.IsValid()) return;

        // RAGDOLL: physics owns the body. Skip controller integration entirely so the
        // Jolt character is not fought by SetCharacterVelocity. The animation layer's
        // pose-source seam handles the visual side (see CharacterAnimationSystem).
        if (oCharCont.mode == MovementMode::RAGDOLL) return;

        auto& oPhysics = GetSystem<PhysicsSystem>();

        // ---- 1. Read oPhysics state from prior fixed step -----------------------
        const bool was_grounded = oCharCont.is_grounded;
        const MovementMode prev_mode = oCharCont.mode;
        const bool was_moving = oCharCont.is_moving;

        oCharCont.is_grounded    = oPhysics.IsCharacterGrounded(oCharCont.hChar);
        oCharCont.v_ground_normal = oPhysics.GetCharacterFloorNormal(oCharCont.hChar);

        // ---- 2. Timers ---------------------------------------------------------
        if (oCharCont.is_grounded) {
                oCharCont.coyote_timer  = 0.f;
                oCharCont.time_airborne = 0.f;
        } else {
                if (was_grounded)
                        oCharCont.coyote_timer = oCharCont.coyote_time;   // start coyote window
                else
                        oCharCont.coyote_timer = glm::max(0.f, oCharCont.coyote_timer - dt);
                oCharCont.time_airborne += dt;
        }

        // ---- 3. Jump buffer ----------------------------------------------------
        // Resolved per call (compile-time-typed O(k) scan) — a cached pointer would
        // go stale if the component is destroyed and re-created on this living node.
        const InputState* pInput = oCharCont.pNode->template GetComponent<InputState>();

        if (pInput && pInput->jump_pressed) {
                oCharCont.jump_buffer_timer = oCharCont.jump_buffer_time;
        } else {
                oCharCont.jump_buffer_timer = glm::max(0.f, oCharCont.jump_buffer_timer - dt);
        }

        // ---- 4. Jump attempt ---------------------------------------------------
        const bool can_jump = (oCharCont.jump_buffer_timer > 0.f) &&
                              (oCharCont.is_grounded || oCharCont.coyote_timer > 0.f);
        bool just_jumped = false;

        if (can_jump) {
                oCharCont.v_velocity.y      = oCharCont.jump_impulse;
                oCharCont.jump_buffer_timer = 0.f;
                oCharCont.coyote_timer      = 0.f;
                just_jumped          = true;
        }

        // ---- 5-6. Horizontal velocity ------------------------------------------
        // When root motion is active, the animation system drives horizontal movement
        // via v_root_motion_vel (accumulated from EAnimRootMotion). Skip input blending.
        const bool is_sprinting = pInput && pInput->sprint_held;
        if (!oCharCont.use_root_motion) {
                const glm::vec2 move_axis = pInput ? pInput->move_axis : glm::vec2{0.f, 0.f};
                const float target_speed  = oCharCont.max_speed * (is_sprinting ? oCharCont.sprint_multiplier : 1.f);
                const float ctrl          = oCharCont.is_grounded ? 1.f : oCharCont.air_control;

                const float input_len = glm::length(move_axis);
                if (input_len > 1e-4f) {
                        // move_axis is world-relative XZ: x=right, y=forward
                        const glm::vec3 desired_dir = glm::normalize(
                                glm::vec3{move_axis.x, 0.f, move_axis.y});
                        const float speed = glm::min(input_len, 1.f) * target_speed;
                        const float accel = oCharCont.acceleration * ctrl * dt;
                        const float t     = (speed > 0.f) ? glm::min(accel / speed, 1.f) : 1.f;
                        oCharCont.v_velocity.x = glm::mix(oCharCont.v_velocity.x, desired_dir.x * speed, t);
                        oCharCont.v_velocity.z = glm::mix(oCharCont.v_velocity.z, desired_dir.z * speed, t);
                } else {
                        const float decel = oCharCont.deceleration * ctrl * dt;
                        const float h_spd = glm::length(glm::vec2{oCharCont.v_velocity.x, oCharCont.v_velocity.z});
                        if (h_spd > decel) {
                                const glm::vec3 h_dir = glm::normalize(
                                        glm::vec3{oCharCont.v_velocity.x, 0.f, oCharCont.v_velocity.z});
                                oCharCont.v_velocity.x -= h_dir.x * decel;
                                oCharCont.v_velocity.z -= h_dir.z * decel;
                        } else {
                                oCharCont.v_velocity.x = 0.f;
                                oCharCont.v_velocity.z = 0.f;
                        }
                }
        }

        // ---- 7. Gravity --------------------------------------------------------
        if (!oCharCont.is_grounded && !just_jumped) {
                const glm::vec3 vGravity = oPhysics.GetGravity();
                oCharCont.v_velocity.y += vGravity.y * oCharCont.gravity_scale * dt;
        } else if (oCharCont.is_grounded && oCharCont.v_velocity.y < 0.f) {
                // Small downward bias keeps the character pressed against the floor
                oCharCont.v_velocity.y = -0.5f;
        }

        // ---- 8. Moving platform + root motion → physics -------------------------
        const glm::vec3 vPlatform = oPhysics.GetCharacterGroundVelocity(oCharCont.hChar);
        const glm::vec3 vRM       = {oCharCont.v_root_motion_vel.x, 0.f, oCharCont.v_root_motion_vel.y};
        const glm::vec3 vFinal    = oCharCont.v_velocity + vPlatform + vRM;
        // Capture combined XZ speed before resetting the per-frame accumulator.
        // This is the authoritative horizontal speed used for is_moving (and read by
        // CharacterAnimationSystem via GetVelocity()).
        const float h_speed = glm::length(glm::vec2{oCharCont.v_velocity.x + vRM.x,
                                                     oCharCont.v_velocity.z + vRM.z});
        oCharCont.v_root_motion_vel = {0.f, 0.f};

        oPhysics.SetCharacterVelocity(oCharCont.hChar, vFinal);

        // ---- 9. Update mode ----------------------------------------------------
        // SWIMMING/CLIMBING stubs: trigger volume system will set those externally.
        // Here we only toggle GROUNDED ↔ AIRBORNE.
        if (oCharCont.mode != MovementMode::SWIMMING && oCharCont.mode != MovementMode::CLIMBING) {
                oCharCont.mode = oCharCont.is_grounded ? MovementMode::GROUNDED : MovementMode::AIRBORNE;
        }

        oCharCont.is_moving    = (h_speed > 0.2f);
        oCharCont.was_sprinting = is_sprinting;

        // ---- 10. Post events ---------------------------------------------------
        PostMovementEvents(oCharCont.pNode, oCharCont, was_grounded, just_jumped, was_moving, prev_mode);
}

// ---------------------------------------------------------------------------

glm::vec3 CharacterMovementSystem::ComputeDesiredVelocity(const CharacterController& oCharCont, const InputState& oInput, float dt) const {

        // Used externally; ProcessComponent inlines the same logic for efficiency.
        const float target_speed = oCharCont.max_speed * (oInput.sprint_held ? oCharCont.sprint_multiplier : 1.f);
        const float input_len = glm::length(oInput.move_axis);

        if (input_len < 1e-4f) return {0.f, 0.f, 0.f};

        const glm::vec3 dir = glm::normalize(glm::vec3{oInput.move_axis.x, 0.f, oInput.move_axis.y});
        return dir * glm::min(input_len, 1.f) * target_speed;
}

void CharacterMovementSystem::PostMovementEvents(
        TSceneTree::TSceneNodeExact* pOwner,
        CharacterController& oCharCont,
        bool was_grounded,
        bool just_jumped,
        bool was_moving,
        MovementMode prev_mode) {

        auto& em = GetSystem<EventManager>();
        const auto pWeak = pOwner->weak_from_this();

        // Clean movement events only — the animation layer (CharacterAnimationSystem)
        // consumes these to drive the AnimGraph. The locomotion layer no longer knows
        // about any state machine.
        if (just_jumped)
                em.QueueEvent(ECharacterJumped{pWeak});

        if (oCharCont.is_grounded && !was_grounded)
                em.QueueEvent(ECharacterLanded{pWeak, oCharCont.v_ground_normal});

        if (oCharCont.mode != prev_mode)
                em.QueueEvent(ECharacterModeChanged{pWeak, prev_mode, oCharCont.mode});

        if (oCharCont.is_moving && !was_moving)
                em.QueueEvent(ECharacterStartedMoving{pWeak});
        else if (!oCharCont.is_moving && was_moving)
                em.QueueEvent(ECharacterStoppedMoving{pWeak});
}

// ---------------------------------------------------------------------------
// Root motion integration
// ---------------------------------------------------------------------------

void CharacterMovementSystem::ApplyRootMotionDelta(CharacterController& oCharCont, const glm::vec3& delta, float dt) {

        if (dt <= 0.f || !oCharCont.use_root_motion) return;

        // Convert displacement to velocity and accumulate into the per-frame buffer.
        // ProcessComponent consumes and resets v_root_motion_vel at step 8.
        // Vertical delta is discarded — gravity and jump logic own Y.
        oCharCont.v_root_motion_vel.x += delta.x / dt;
        oCharCont.v_root_motion_vel.y += delta.z / dt;
}

void CharacterMovementSystem::OnAnimRootMotion(const Event& e) {

        const auto& oEvt = e.Get<EAnimRootMotion>();
        auto pNode = oEvt.pActor.lock();
        if (!pNode) return;

        auto it = mComponents.find(pNode->GetID());
        if (it == mComponents.end()) return;

        ApplyRootMotionDelta(*it->second, oEvt.v_delta, oEvt.dt);
}

} // namespace SE
