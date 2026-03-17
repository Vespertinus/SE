
#ifndef __PHYSICS_TYPES_H__
#define __PHYSICS_TYPES_H__

#include <cstdint>
#include <functional>
#include <string>
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
        enum Type { Box, Sphere, Capsule } type = Box;
        glm::vec3 vHalfExtents {0.5f, 0.5f, 0.5f};  // Box
        float     radius       = 0.5f;               // Sphere / Capsule
        float     half_height  = 0.5f;               // Capsule
};

struct RigidBodyDesc {
        ColliderDesc oCollider;
        glm::vec3    vInitialPosition  {0.0f, 0.0f, 0.0f};
        glm::quat    qInitialRotation  {1.0f, 0.0f, 0.0f, 0.0f};
        bool         is_static         = false;
        bool         is_kinematic      = false;
        bool         is_trigger        = false;
        float        friction          = 0.5f;
        float        restitution       = 0.0f;
        float        linear_damping    = 0.05f;
        float        angular_damping   = 0.05f;
        float        gravity_scale     = 1.0f;
        float        mass              = 1.0f;

        std::string Str() const;
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
