
#include <CharacterController.h>
#include <CharacterMovementSystem.h>
#include <Logging.h>

namespace SE {

CharacterController::CharacterController(TSceneTree::TSceneNodeExact* pNewNode)
        : CharacterController(pNewNode, Desc{}) {}

CharacterController::CharacterController(TSceneTree::TSceneNodeExact* pNewNode, const Desc& desc)
        : pNode(pNewNode)
        , capsule_radius      (desc.capsule_radius)
        , capsule_half_height (desc.capsule_half_height)
        , step_height         (desc.step_height)
        , max_slope_angle     (desc.max_slope_angle)
        , max_speed           (desc.max_speed)
        , sprint_multiplier   (desc.sprint_multiplier)
        , acceleration        (desc.acceleration)
        , deceleration        (desc.deceleration)
        , air_control         (desc.air_control)
        , jump_impulse        (desc.jump_impulse)
        , gravity_scale       (desc.gravity_scale)
        , coyote_time         (desc.coyote_time)
        , jump_buffer_time    (desc.jump_buffer_time) {

        CharacterDesc oPhysDesc;
        oPhysDesc.radius          = desc.capsule_radius;
        oPhysDesc.half_height     = desc.capsule_half_height;
        oPhysDesc.step_height     = desc.step_height;
        oPhysDesc.slope_angle     = desc.max_slope_angle;
        oPhysDesc.collision_layer = desc.collision_layer;
        oPhysDesc.collision_mask  = desc.collision_mask;
        oPhysDesc.vInitialPosition = pNewNode->GetTransform().GetWorldPos();

        hChar = GetSystem<PhysicsSystem>().CreateCharacter(oPhysDesc);
        if (hChar.IsValid()) {
                GetSystem<PhysicsSystem>().RegisterCharacterNode(hChar, pNode);
        } else {
                log_e("CharacterController: failed to create Jolt character");
        }
}

CharacterController::~CharacterController() noexcept {
        Disable();
        if (hChar.IsValid()) {
                GetSystem<PhysicsSystem>().UnregisterCharacterNode(hChar);
                GetSystem<PhysicsSystem>().DestroyCharacter(hChar);
        }
}

void CharacterController::Enable() {
        GetSystem<CharacterMovementSystem>().AddComponent(this);
}

void CharacterController::Disable() {
        GetSystem<CharacterMovementSystem>().RemoveComponent(this);
}

std::string CharacterController::Str() const {
        const char* mode_str =
                mode == MovementMode::GROUNDED  ? "GROUNDED"  :
                mode == MovementMode::AIRBORNE  ? "AIRBORNE"  :
                mode == MovementMode::SWIMMING  ? "SWIMMING"  :
                mode == MovementMode::CLIMBING  ? "CLIMBING"  :
                mode == MovementMode::FLYING    ? "FLYING"    :
                mode == MovementMode::RAGDOLL   ? "RAGDOLL"   : "CUSTOM";

        return fmt::format("CharacterController[{}]: mode={}, spd={:.2f}, vy={:.2f}, grounded={}",
                hChar.id, mode_str,
                glm::length(glm::vec2{v_velocity.x, v_velocity.z}),
                v_velocity.y,
                is_grounded);
}

void CharacterController::DrawDebug() const {

        if (!pNode || !hChar.IsValid()) return;

        auto& dr = GetSystem<DebugRenderer>();
        const glm::vec4 vColor{0.2f, 0.8f, 1.0f, 1.0f};
        const glm::vec3 vCenter = pNode->GetTransform().GetWorldPos()
                                + glm::vec3(0.f, capsule_half_height + capsule_radius, 0.f);

        // Draw ground normal
        if (is_grounded) {
                const glm::vec4 vNormColor{0.f, 1.f, 0.4f, 1.0f};
                dr.DrawLine(vCenter, vCenter + v_ground_normal * 0.5f, vNormColor);
        }

        // Draw capsule outline (horizontal ring)
        constexpr int N = 32;
        for (int i = 0; i < N; ++i) {
                float a0 = glm::radians(360.0f *  i      / N);
                float a1 = glm::radians(360.0f * (i + 1) / N);
                dr.DrawLine(vCenter + glm::vec3(capsule_radius * glm::cos(a0), 0.f, capsule_radius * glm::sin(a0)),
                            vCenter + glm::vec3(capsule_radius * glm::cos(a1), 0.f, capsule_radius * glm::sin(a1)),
                            vColor);
        }
}

} // namespace SE
