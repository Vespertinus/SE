
#ifndef __PHYSICS_SYSTEM_H__
#define __PHYSICS_SYSTEM_H__

#include <memory>
#include <PhysicsTypes.h>
#include <PhysicsEvents.h>

namespace SE {

class PhysicsSystem {
public:
        PhysicsSystem();
        ~PhysicsSystem() noexcept;

        void Init(const PhysicsConfig& cfg);
        void Shutdown();

        BodyHandle CreateRigidBody(const RigidBodyDesc& desc);
        void       DestroyBody(BodyHandle h);

        void ApplyImpulse(BodyHandle h, glm::vec3 impulse);
        void SetLinearVelocity(BodyHandle h, glm::vec3 vel);
        void Teleport(BodyHandle h, glm::vec3 pos, glm::quat rot);
        void MoveKinematic(BodyHandle h, glm::vec3 pos, glm::quat rot);

        void ActivateBody(BodyHandle h);
        void DeactivateBody(BodyHandle h);

        // Register a node so Interpolate() can sync its transform each frame.
        // pNode must be a TSceneTree::TSceneNodeExact pointer.
        void RegisterNode(BodyHandle h, void* pNode);
        void UnregisterNode(BodyHandle h);

        // Returns the registered TSceneTree::TSceneNodeExact* as void* (cast at call site).
        void* GetNode(BodyHandle h) const;

        // Called from game loop
        void  Update(float game_dt);    // accumulator + fixed steps + drain events
        void  Interpolate(float alpha); // sync interpolated transforms to scene nodes
        float GetInterpolationAlpha() const;

        bool Raycast(const PhysicsRay& ray, RaycastHit& out, QueryFilter filter = {}) const;

        bool IsInitialized() const;

private:
        struct Impl;
        std::unique_ptr<Impl> pImpl;
};

} // namespace SE

#endif
