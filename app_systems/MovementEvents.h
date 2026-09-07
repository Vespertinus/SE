
#ifndef __MOVEMENT_EVENTS_H__
#define __MOVEMENT_EVENTS_H__

#include <SceneTree.h>
#include <glm/vec3.hpp>

namespace SE {

enum class MovementMode : uint8_t {
        GROUNDED,  // in contact with a walkable surface
        AIRBORNE,  // no ground contact — gravity applies
        SWIMMING,  // inside a water trigger volume (stub — falls back to AIRBORNE)
        CLIMBING,  // in contact with a climbable surface (stub — falls back to AIRBORNE)
        FLYING,    // no gravity — noclip / spectator
        RAGDOLL,   // physics owns the body; CharacterMovementSystem skips integration,
                   // and the animation pose-source switches to physics-driven joints
        CUSTOM,    // game-specific extension point
};

struct ECharacterLanded {
        TSceneTree::TSceneNodeWeak pActor;
        glm::vec3                  vFloorNormal {0.f, 1.f, 0.f};
};

struct ECharacterJumped {
        TSceneTree::TSceneNodeWeak pActor;
};

struct ECharacterModeChanged {
        TSceneTree::TSceneNodeWeak pActor;
        MovementMode               old_mode;
        MovementMode               new_mode;
};

struct ECharacterStartedMoving {
        TSceneTree::TSceneNodeWeak pActor;
};

struct ECharacterStoppedMoving {
        TSceneTree::TSceneNodeWeak pActor;
};

// Published by AnimationSystem after extracting per-frame root-motion from a clip.
// CharacterMovementSystem listens and accumulates the delta into v_root_motion_vel.
struct EAnimRootMotion {
        TSceneTree::TSceneNodeWeak pActor;
        glm::vec3                  v_delta;   // world-space displacement this frame
        float                      dt;        // frame time used to compute the delta
};

} // namespace SE

#endif
