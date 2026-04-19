
#ifndef __RIGID_BODY_H__
#define __RIGID_BODY_H__

#include <string>
#include <string_view>
#include <PhysicsTypes.h>
#include <Component_generated.h>
#include <EntityTemplate_generated.h>

namespace SE {

class RigidBody {

        TSceneTree::TSceneNodeExact* pNode;
        BodyHandle                   hBody;
        ColliderDesc                 oCollider;
        bool                         is_kinematic = false;

public:
        using TSerialized = FlatBuffers::RigidBody;

        static void ApplyField(FlatBuffers::RigidBodyT& obj,
                               std::string_view path,
                               const FlatBuffers::FieldOverride& fo);

        RigidBody(TSceneTree::TSceneNodeExact* pNode, const RigidBodyDesc& desc);
        RigidBody(TSceneTree::TSceneNodeExact* pNode,
                  const SE::FlatBuffers::RigidBody* pFB);
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
