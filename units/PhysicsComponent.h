
#ifndef __PHYSICS_COMPONENT_H__
#define __PHYSICS_COMPONENT_H__

#include <string>
#include <PhysicsTypes.h>

namespace SE {

class PhysicsComponent {

        TSceneTree::TSceneNodeExact* pNode;
        BodyHandle                   hBody;
        ColliderDesc                 oCollider;
        bool                         is_kinematic = false;

public:
        PhysicsComponent(TSceneTree::TSceneNodeExact* pNode, const RigidBodyDesc& desc);
        ~PhysicsComponent() noexcept;

        void Enable();
        void Disable();

        void TargetTransformChanged(TSceneTree::TSceneNodeExact* pTargetNode);

        std::string Str() const;
        void Print(size_t indent);
        void DrawDebug() const;

        BodyHandle GetHandle() const { return hBody; }
};

} // namespace SE

#endif
