
#ifndef __RIGID_BODY_H__
#define __RIGID_BODY_H__

#include <string>
#include <PhysicsTypes.h>

namespace SE {

class RigidBody {

        TSceneTree::TSceneNodeExact* pNode;
        BodyHandle                   hBody;
        ColliderDesc                 oCollider;
        bool                         is_kinematic = false;

public:
        RigidBody(TSceneTree::TSceneNodeExact* pNode, const RigidBodyDesc& desc);
        ~RigidBody() noexcept;

        void Enable();
        void Disable();

        void TargetTransformChanged(TSceneTree::TSceneNodeExact* pTargetNode);

        std::string Str() const;
        void DrawDebug() const;

        BodyHandle GetHandle() const { return hBody; }
};

} // namespace SE

#endif
