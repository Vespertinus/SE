
#ifndef __PHYSICS_TYPES_H__
#define __PHYSICS_TYPES_H__

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

namespace SE {

struct BodyHandle {
        uint32_t index      = UINT32_MAX;
        uint32_t generation = 0;
        bool IsValid() const { return index != UINT32_MAX; }
};

inline bool operator==(BodyHandle a, BodyHandle b) {
        return a.index == b.index && a.generation == b.generation;
}
inline bool operator!=(BodyHandle a, BodyHandle b) { return !(a == b); }

struct ColliderDesc {
        enum Type { Box, Sphere, Capsule, Mesh } type = Box;
        glm::vec3 vHalfExtents {0.5f, 0.5f, 0.5f};  // Box
        float     radius       = 0.5f;               // Sphere / Capsule
        float     half_height  = 0.5f;               // Capsule
        // Mesh type — populated only for ColliderDesc::Mesh
        std::vector<glm::vec3> vMeshVertices;
        std::vector<uint32_t>  vMeshIndices;
};

namespace CollisionLayers {
    constexpr uint32_t DEFAULT = 1u << 0;
    // Applications define their own bits: PLAYER = 1u<<1, ENEMY = 1u<<2, …
}

// SE-level mirror of the Jolt object layers (PhysicsSystem.tcc PhysLayers).
namespace BodyLayers {
        enum Type { Static = 0, Dynamic = 1, Trigger = 2, Player = 3, Kinematic = 4 };
}

struct RigidBodyDesc {
        ColliderDesc oCollider;
        glm::vec3    vInitialPosition  {0.0f, 0.0f, 0.0f};
        glm::quat    qInitialRotation  {1.0f, 0.0f, 0.0f, 0.0f};
        bool         is_static         = false;
        bool         is_kinematic      = false;
        bool         is_trigger        = false;
        // -1 = derive from is_static/is_trigger/is_kinematic; otherwise a
        // BodyLayers::Type override (e.g. a probe body that must fire sensors).
        int          object_layer      = -1;
        float        friction          = 0.5f;
        float        restitution       = 0.0f;
        float        linear_damping    = 0.05f;
        float        angular_damping   = 0.05f;
        float        gravity_scale     = 1.0f;
        float        mass              = 1.0f;
        uint32_t     collision_layer   = CollisionLayers::DEFAULT;
        uint32_t     collision_mask    = 0xFFFFFFFFu;

        std::string Str() const;
};

struct CharHandle {
        uint32_t id = UINT32_MAX;
        bool IsValid() const { return id != UINT32_MAX; }
};

inline bool operator==(CharHandle a, CharHandle b) { return a.id == b.id; }
inline bool operator!=(CharHandle a, CharHandle b) { return a.id != b.id; }

struct CharacterDesc {
        float    radius          = 0.3f;
        float    half_height     = 0.9f;   // half-height of the cylindrical section
        float    step_height     = 0.3f;   // max stair riser
        float    slope_angle     = 50.0f;  // max walkable slope in degrees
        float    mass            = 70.0f;
        float    max_strength    = 100.0f;
        uint32_t collision_layer = CollisionLayers::DEFAULT;
        uint32_t collision_mask  = 0xFFFFFFFFu;
        // Inner presence body (Jolt CharacterVirtual mInnerBodyShape): gives
        // the character contact presence — trigger sensors fire, rays hit it,
        // dynamic props are pushed. BodyLayers::Player by default; -1 disables
        // it (pure sweep character: no trigger events, ghosts through the
        // player and other characters).
        int      inner_body_layer = BodyLayers::Player;
        // Sweep against KINEMATIC-layer followers (door leaves, NPC blocking
        // capsules). True for player-class characters; NPC CharacterVirtuals
        // opt OUT — sweeping their own follower capsule is a depenetration
        // feedback loop that hurls the node to infinity.
        bool     collides_kinematic = true;
        // Character LOD: run this character's ExtendedUpdate every Nth fixed
        // step (dt scaled by N, so average speed/gravity are unchanged). 1 =
        // every step (player). NPCs profiled as the dominant CPU cost with
        // ~15 characters alive (each step sweeps the terrain heightfield and
        // static broadphase); 2 halves that at no visible cost.
        uint32_t update_divider = 1;
        glm::vec3 vInitialPosition {0.f, 0.f, 0.f};
        glm::quat qInitialRotation {1.f, 0.f, 0.f, 0.f};
};

struct RaycastHit {
        BodyHandle  hBody;
        float       distance  = 0.0f;
        glm::vec3   vNormal   {0.0f, 1.0f, 0.0f};
        glm::vec3   vPoint    {0.0f, 0.0f, 0.0f};
};

struct QueryFilter {
        uint32_t layer_mask = 0xFFFFFFFF;
};

struct PhysicsRay {
        glm::vec3 vOrigin    {0.0f, 0.0f, 0.0f};
        glm::vec3 vDirection {0.0f, 0.0f, -1.0f};
};

//TODO move to global config
struct PhysicsConfig {
        float    fixed_dt               = 1.0f / 60.0f;
        uint32_t temp_allocator_mb      = 32;
        int      collision_steps        = 2;
        int      max_bodies             = 10240;
        int      max_body_pairs         = 65536;
        int      max_contact_constraints = 10240;
};

} // namespace SE

namespace std {
template<> struct hash<SE::BodyHandle> {
        size_t operator()(SE::BodyHandle h) const noexcept {
                return std::hash<uint64_t>{}((uint64_t)h.index << 32 | h.generation);
        }
};
} // namespace std

#endif
