
#include <GlobalTypes.h>

namespace SE {

namespace {

static void InitTriggerVolume(TriggerVolume* self,
                              TSceneTree::TSceneNodeExact* pNode,
                              BodyHandle& hBody,
                              const ColliderDesc& oCollider,
                              uint32_t collision_layer,
                              uint32_t collision_mask) {

        RigidBodyDesc desc;
        desc.oCollider        = oCollider;
        desc.is_trigger       = true;
        desc.is_kinematic     = true;
        desc.vInitialPosition = pNode->GetTransform().GetWorldPos();
        desc.collision_layer  = collision_layer;
        desc.collision_mask   = collision_mask;

        hBody = GetSystem<PhysicsSystem>().CreateRigidBody(desc);

        if (!hBody.IsValid()) {
                log_e("TriggerVolume: failed to create sensor body");
                return;
        }

        pNode->AddListener(self);

        auto& em = GetSystem<EventManager>();
        em.AddListener<EPhysicsTriggerEnter, &TriggerVolume::OnTriggerEnter>(self);
        em.AddListener<EPhysicsTriggerExit,  &TriggerVolume::OnTriggerExit>(self);
        em.AddListener<EUpdate,              &TriggerVolume::OnUpdate>(self);
}

static ColliderDesc ColliderDescFromFB(const FlatBuffers::TriggerVolume* pFB) {

        using namespace SE::FlatBuffers;
        ColliderDesc desc;

        switch (pFB->collider_type()) {
                case ColliderU::BoxCollider: {
                                                     auto* p = pFB->collider_as_BoxCollider();
                                                     desc.type = ColliderDesc::Box;
                                                     desc.vHalfExtents = {
                                                             p->half_extents()->x(),
                                                             p->half_extents()->y(),
                                                             p->half_extents()->z()
                                                     };
                                                     break;
                                             }
                case ColliderU::SphereCollider: {
                                                        auto* p = pFB->collider_as_SphereCollider();
                                                        desc.type   = ColliderDesc::Sphere;
                                                        desc.radius = p->radius();
                                                        break;
                                                }
                case ColliderU::CapsuleCollider: {
                                                         auto* p = pFB->collider_as_CapsuleCollider();
                                                         desc.type        = ColliderDesc::Capsule;
                                                         desc.radius      = p->radius();
                                                         desc.half_height = p->half_height();
                                                         break;
                                                 }
                default:
                                                 throw std::runtime_error("TriggerVolume: unsupported collider type");
        }

        return desc;
}

} // anonymous namespace

TriggerVolume::TriggerVolume(TSceneTree::TSceneNodeExact* pNewNode,
                const TriggerVolumeDesc& desc)
        : pNode(pNewNode)
        , oCollider(desc.oCollider)
        , stay_interval(desc.stay_interval)
          , one_shot(desc.one_shot) {

        if (!desc.on_enter_event.empty()) on_enter_id = StrID(desc.on_enter_event);
        if (!desc.on_exit_event.empty())  on_exit_id  = StrID(desc.on_exit_event);
        if (!desc.on_stay_event.empty())  on_stay_id  = StrID(desc.on_stay_event);

        InitTriggerVolume(this, pNode, hBody, oCollider, desc.collision_layer, desc.collision_mask);
}

TriggerVolume::TriggerVolume(TSceneTree::TSceneNodeExact* pNewNode,
                const FlatBuffers::TriggerVolume* pFB)
        : pNode(pNewNode)
        , oCollider(ColliderDescFromFB(pFB))
        , stay_interval(pFB->stay_interval())
          , one_shot(pFB->one_shot()) {

        if (auto* s = pFB->on_enter_event(); s && s->size() > 0) on_enter_id = StrID(s->str());
        if (auto* s = pFB->on_exit_event();  s && s->size() > 0) on_exit_id  = StrID(s->str());
        if (auto* s = pFB->on_stay_event();  s && s->size() > 0) on_stay_id  = StrID(s->str());

        InitTriggerVolume(this, pNode, hBody, oCollider, pFB->collision_layer(), pFB->collision_mask());
}

TriggerVolume::~TriggerVolume() noexcept {

        auto& em = GetSystem<EventManager>();
        em.RemoveListener<EUpdate,              &TriggerVolume::OnUpdate>(this);
        em.RemoveListener<EPhysicsTriggerExit,  &TriggerVolume::OnTriggerExit>(this);
        em.RemoveListener<EPhysicsTriggerEnter, &TriggerVolume::OnTriggerEnter>(this);

        if (hBody.IsValid()) {
                pNode->RemoveListener(this);
                GetSystem<PhysicsSystem>().DestroyBody(hBody);
        }
}

void TriggerVolume::TargetTransformChanged(TSceneTree::TSceneNodeExact* pTargetNode) {

        if (!hBody.IsValid() || !enabled) return;
        auto [vPos, qRot, vScale] = pTargetNode->GetTransform().GetWorldDecomposedQuat();
        GetSystem<PhysicsSystem>().MoveKinematic(hBody, vPos, qRot);
}

void TriggerVolume::OnTriggerEnter(const Event& e) {

        auto& ev = e.Get<EPhysicsTriggerEnter>();
        if (ev.hTrigger != hBody || !enabled || (one_shot && fired)) return;

        sActiveOverlaps.insert(ev.hOther);
        stay_timer = 0.0f;

        if (on_enter_id != StrID{})
                GetSystem<EventManager>().QueueEvent(ETriggerFired{on_enter_id, hBody, ev.hOther});

        if (one_shot) {
                fired = true;
                Disable();
        }
}

void TriggerVolume::OnTriggerExit(const Event& e) {

        auto& ev = e.Get<EPhysicsTriggerExit>();
        if (ev.hTrigger != hBody) return;

        sActiveOverlaps.erase(ev.hOther);

        if (!one_shot && on_exit_id != StrID{})
                GetSystem<EventManager>().QueueEvent(ETriggerFired{on_exit_id, hBody, ev.hOther});
}

void TriggerVolume::Enable() {
        enabled = true;
        if (hBody.IsValid())
                GetSystem<PhysicsSystem>().ActivateBody(hBody);
}

void TriggerVolume::Disable() {
        enabled = false;
        if (hBody.IsValid())
                GetSystem<PhysicsSystem>().DeactivateBody(hBody);
}

void TriggerVolume::OnUpdate(const Event& e) {

        if (!enabled || on_stay_id == StrID{} || sActiveOverlaps.empty()) return;

        stay_timer += e.Get<EUpdate>().last_frame_time;

        if (stay_interval <= 0.0f || stay_timer >= stay_interval) {
                for (const auto& hOther : sActiveOverlaps)
                        GetSystem<EventManager>().QueueEvent(ETriggerFired{on_stay_id, hBody, hOther});
                stay_timer = 0.0f;
        }
}

std::string TriggerVolume::Str() const {
        return fmt::format("TriggerVolume[{}]: collider type: {}",
                        hBody.index,
                        static_cast<int>(oCollider.type));
}

} // namespace SE
