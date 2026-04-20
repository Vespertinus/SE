
#include <GlobalTypes.h>
#include <glm/trigonometric.hpp>

namespace SE {

static RigidBodyDesc RigidBodyDescFromFB(
                const SE::FlatBuffers::RigidBody* pFB,
                TSceneTree::TSceneNodeExact* pNode) {

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
        desc.gravity_scale      = pFB->gravity_scale();
        desc.mass               = pFB->mass();
        desc.collision_layer    = pFB->collision_layer();
        desc.collision_mask     = pFB->collision_mask();

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
        auto [vPos, qRot, vScale] = pTargetNode->GetTransform().GetWorldDecomposedQuat();
        GetSystem<PhysicsSystem>().MoveKinematic(hBody, vPos, qRot);
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
        const glm::vec4 vColor{0.0f, 1.0f, 0.0f, 1.0f};

        switch (oCollider.type) {
                case ColliderDesc::Box: {
                        BoundingBox bbox(-oCollider.vHalfExtents, oCollider.vHalfExtents);
                        dr.DrawBBox(bbox, pNode->GetTransform());
                        break;
                }
                case ColliderDesc::Sphere:
                case ColliderDesc::Capsule: {
                        // Approximate with three axis-aligned circles (32 segments each)
                        const glm::vec3 vCenter = pNode->GetTransform().GetWorldPos();
                        const float r = oCollider.radius;
                        constexpr int N = 32;
                        for (int i = 0; i < N; ++i) {
                                float a0 = glm::radians(360.0f * i       / N);
                                float a1 = glm::radians(360.0f * (i + 1) / N);
                                float c0 = glm::cos(a0), s0 = glm::sin(a0);
                                float c1 = glm::cos(a1), s1 = glm::sin(a1);
                                // XY plane
                                dr.DrawLine(vCenter + glm::vec3(r*c0, r*s0, 0), vCenter + glm::vec3(r*c1, r*s1, 0), vColor);
                                // XZ plane
                                dr.DrawLine(vCenter + glm::vec3(r*c0, 0, r*s0), vCenter + glm::vec3(r*c1, 0, r*s1), vColor);
                                // YZ plane
                                dr.DrawLine(vCenter + glm::vec3(0, r*c0, r*s0), vCenter + glm::vec3(0, r*c1, r*s1), vColor);
                        }
                        break;
                }
                case ColliderDesc::Mesh:
                        break; // no debug draw for mesh colliders
        }
}

// static
void RigidBody::ApplyField(FlatBuffers::RigidBodyT& obj,
                            std::string_view path,
                            const FlatBuffers::FieldOverride& fo) {

        auto get_float = [&]() -> float {
                if (auto* p = fo.value_as_Float()) return p->value();
                log_e("RigidBody::ApplyField: '{}' expects Float value", path);
                return 0.f;
        };
        auto get_bool = [&]() -> bool {
                if (auto* p = fo.value_as_Bool()) return p->value() != 0;
                log_e("RigidBody::ApplyField: '{}' expects Bool value", path);
                return false;
        };

        if (path == "mass")            { obj.mass            = get_float(); return; }
        if (path == "friction")        { obj.friction        = get_float(); return; }
        if (path == "restitution")     { obj.restitution     = get_float(); return; }
        if (path == "linear_damping")  { obj.linear_damping  = get_float(); return; }
        if (path == "angular_damping") { obj.angular_damping = get_float(); return; }
        if (path == "gravity_scale")   { obj.gravity_scale   = get_float(); return; }
        if (path == "is_static")       { obj.is_static       = get_bool();  return; }
        if (path == "is_kinematic")    { obj.is_kinematic    = get_bool();  return; }
        if (path == "is_trigger")      { obj.is_trigger      = get_bool();  return; }

        auto get_uint = [&]() -> uint32_t {
                if (auto* p = fo.value_as_Int()) return static_cast<uint32_t>(p->value());
                log_e("RigidBody::ApplyField: '{}' expects Int value", path);
                return 0u;
        };

        if (path == "collision_layer") { obj.collision_layer = get_uint(); return; }
        if (path == "collision_mask")  { obj.collision_mask  = get_uint(); return; }

        log_e("RigidBody::ApplyField: unknown field '{}'", path);
}

} // namespace SE
