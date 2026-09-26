
#ifndef __PHYSICS_SYSTEM_H__
#define __PHYSICS_SYSTEM_H__

#include <functional>
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
        /// Static heightfield body (terrain tiles). vSamples is row-major
        /// (z-row * count + x-col) of ABSOLUTE world heights; count = samples per side.
        /// vOffset = tile min corner (x, 0, z); vScale = (step_x, 1.0, step_z). The body
        /// is never registered with a scene node — terrain has none.
        BodyHandle CreateStaticHeightField(const std::vector<float>& vSamples, uint32_t count,
                                           const glm::vec3& vOffset, const glm::vec3& vScale,
                                           float friction,
                                           uint32_t collision_layer = CollisionLayers::DEFAULT);
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

        // Targeted sensor contact delivery. Without it, every TriggerVolume
        // subscribes to the raw EPhysicsTriggerEnter/Exit broadcasts and each
        // contact anywhere costs O(#triggers) listener invocations. The raw
        // events keep broadcasting for external sinks (tests, samples).
        using SensorContactFn = std::function<void(bool bEnter, BodyHandle hOther)>;
        void AttachSensorListener(BodyHandle hBody, SensorContactFn fn);
        void DetachSensorListener(BodyHandle hBody);

        // Called from game loop
        void  Update(float game_dt);    // accumulator + fixed steps + drain events
        void  Interpolate(); // sync interpolated transforms to scene nodes
        float GetInterpolationAlpha() const;

        // Character (CharacterVirtual) API
        CharHandle    CreateCharacter(const CharacterDesc& desc);
        void          DestroyCharacter(CharHandle h);
        void          RegisterCharacterNode(CharHandle h, void* pNode);
        void          UnregisterCharacterNode(CharHandle h);
        void          SetCharacterVelocity(CharHandle h, glm::vec3 vel);
        bool          IsCharacterGrounded(CharHandle h) const;
        glm::vec3     GetCharacterFloorNormal(CharHandle h) const;
        glm::vec3     GetCharacterGroundVelocity(CharHandle h) const;
        void          TeleportCharacter(CharHandle h, glm::vec3 pos, glm::quat rot);

        glm::vec3     GetGravity() const;

        bool Raycast(const PhysicsRay& ray, RaycastHit& out, QueryFilter filter = {}) const;

        // Debug
        void DrawDebug();
        void SetPaused(bool paused);
        void StepOnce();

        bool IsInitialized() const;

private:
        struct Impl;
        std::unique_ptr<Impl> pImpl;
};

} // namespace SE

#endif
