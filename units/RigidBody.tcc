
#include <GlobalTypes.h>
#include <glm/trigonometric.hpp>

namespace SE {

static RigidBodyDesc RigidBodyDescFromFB(
        const SE::FlatBuffers::RigidBody* pFB,
        TSceneTree::TSceneNodeExact* pNode)
{
    using namespace SE::FlatBuffers;
    RigidBodyDesc desc;

    // Collider
    switch (pFB->collider_type()) {
        case ColliderU::BoxCollider: {
            auto* p = pFB->collider_as_BoxCollider();
            desc.oCollider.type = ColliderDesc::Box;
            desc.oCollider.vHalfExtents = {
                p->half_extents()->x(),
                p->half_extents()->y(),
                p->half_extents()->z()
            };
            break;
        }
        case ColliderU::SphereCollider: {
            auto* p = pFB->collider_as_SphereCollider();
            desc.oCollider.type   = ColliderDesc::Sphere;
            desc.oCollider.radius = p->radius();
            break;
        }
        case ColliderU::CapsuleCollider: {
            auto* p = pFB->collider_as_CapsuleCollider();
            desc.oCollider.type        = ColliderDesc::Capsule;
            desc.oCollider.radius      = p->radius();
            desc.oCollider.half_height = p->half_height();
            break;
        }
        case ColliderU::MeshCollider: {
            auto* p = pFB->collider_as_MeshCollider();
            desc.oCollider.type = ColliderDesc::Mesh;
            if (p->vertices()) {
                const float* raw = p->vertices()->data();
                const size_t n   = p->vertices()->size() / 3;
                desc.oCollider.vMeshVertices.reserve(n);
                for (size_t i = 0; i < n; ++i)
                    desc.oCollider.vMeshVertices.emplace_back(raw[i*3], raw[i*3+1], raw[i*3+2]);
            }
            if (p->indices())
                desc.oCollider.vMeshIndices.assign(p->indices()->begin(), p->indices()->end());
            break;
        }
        default:
            throw std::runtime_error("RigidBody: unknown collider type in FlatBuffer");
    }

    // Scalars
    desc.is_static        = pFB->is_static();
    desc.is_kinematic     = pFB->is_kinematic();
    desc.is_trigger       = pFB->is_trigger();
    desc.friction         = pFB->friction();
    desc.restitution      = pFB->restitution();
    desc.linear_damping   = pFB->linear_damping();
    desc.angular_damping  = pFB->angular_damping();
    desc.gravity_scale    = pFB->gravity_scale();
    desc.mass             = pFB->mass();

    // Position from node (already set by scene loader)
    desc.vInitialPosition = pNode->GetTransform().GetWorldPos();

    // Rotation — Vec4 is [x,y,z,w] matching glm::quat layout
    if (pFB->initial_rotation()) {
        desc.qInitialRotation = *reinterpret_cast<const glm::quat*>(pFB->initial_rotation());
    }

    return desc;
}

RigidBody::RigidBody(TSceneTree::TSceneNodeExact* pNode,
                     const SE::FlatBuffers::RigidBody* pFB)
    : RigidBody(pNode, RigidBodyDescFromFB(pFB, pNode)) {}

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
