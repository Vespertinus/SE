
#include <GlobalTypes.h>
#include <glm/trigonometric.hpp>

namespace SE {

RigidBody::RigidBody(TSceneTree::TSceneNodeExact* pNewNode, const RigidBodyDesc& desc)
        : pNode(pNewNode)
        , oCollider(desc.oCollider)
        , is_kinematic(desc.is_kinematic) {

        hBody = GetSystem<PhysicsSystem>().CreateRigidBody(desc);
        if (hBody.IsValid()) {
                GetSystem<PhysicsSystem>().RegisterNode(hBody, pNode);
                if (is_kinematic) {
                        pNode->AddListener(this);
                }
        }
}

RigidBody::~RigidBody() noexcept {

        if (hBody.IsValid()) {
                if (is_kinematic) {
                        pNode->RemoveListener(this);
                }
                GetSystem<PhysicsSystem>().UnregisterNode(hBody);
                GetSystem<PhysicsSystem>().DestroyBody(hBody);
        }
}

void RigidBody::Enable() {
        if (hBody.IsValid()) {
                GetSystem<PhysicsSystem>().ActivateBody(hBody);
        }
}

void RigidBody::Disable() {
        if (hBody.IsValid()) {
                GetSystem<PhysicsSystem>().DeactivateBody(hBody);
        }
}

void RigidBody::TargetTransformChanged(TSceneTree::TSceneNodeExact* pTargetNode) {
        if (!hBody.IsValid()) return;
        auto [oPos, oRot, oScale] = pTargetNode->GetTransform().GetWorldDecomposedQuat();
        GetSystem<PhysicsSystem>().MoveKinematic(hBody, oPos, oRot);
}

std::string RigidBody::Str() const {
        return fmt::format("RigidBody[{}]: is_kinematic: {}, collider type: {}",
                        hBody.index,
                        is_kinematic,
                        static_cast<int>(oCollider.type));
}

void RigidBody::DrawDebug() const {

        //FIXME component always inside pNode
        if (!pNode) return;
        auto& dr = GetSystem<DebugRenderer>();
        const glm::vec4 oColor{0.0f, 1.0f, 0.0f, 1.0f};

        switch (oCollider.type) {
                case ColliderDesc::Box: {
                        BoundingBox bbox(-oCollider.vHalfExtents, oCollider.vHalfExtents);
                        dr.DrawBBox(bbox, pNode->GetTransform());
                        break;
                }
                case ColliderDesc::Sphere:
                case ColliderDesc::Capsule: {
                        // Approximate with three axis-aligned circles (32 segments each)
                        const glm::vec3 oCenter = pNode->GetTransform().GetWorldPos();
                        const float r = oCollider.radius;
                        constexpr int N = 32;
                        for (int i = 0; i < N; ++i) {
                                float a0 = glm::radians(360.0f * i       / N);
                                float a1 = glm::radians(360.0f * (i + 1) / N);
                                float c0 = glm::cos(a0), s0 = glm::sin(a0);
                                float c1 = glm::cos(a1), s1 = glm::sin(a1);
                                // XY plane
                                dr.DrawLine(oCenter + glm::vec3(r*c0, r*s0, 0), oCenter + glm::vec3(r*c1, r*s1, 0), oColor);
                                // XZ plane
                                dr.DrawLine(oCenter + glm::vec3(r*c0, 0, r*s0), oCenter + glm::vec3(r*c1, 0, r*s1), oColor);
                                // YZ plane
                                dr.DrawLine(oCenter + glm::vec3(0, r*c0, r*s0), oCenter + glm::vec3(0, r*c1, r*s1), oColor);
                        }
                        break;
                }
                case ColliderDesc::Mesh:
                        break; // no debug draw for mesh colliders
        }
}

} // namespace SE
