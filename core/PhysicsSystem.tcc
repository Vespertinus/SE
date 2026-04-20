
#ifdef SE_IMPL

// Jolt headers — only included in implementation units
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/MeshShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>

#include <GlobalTypes.h>
#include <Logging.h>

#include <mutex>
#include <vector>
#include <unordered_map>
#include <thread>
#include <cassert>

JPH_SUPPRESS_WARNINGS

namespace SE {

// ---- object / broad-phase layer constants -----------------------------------

namespace PhysLayers {
        static constexpr JPH::ObjectLayer STATIC  = 0;
        static constexpr JPH::ObjectLayer DYNAMIC = 1;
        static constexpr JPH::ObjectLayer TRIGGER = 2;
        static constexpr JPH::ObjectLayer NUM     = 3;
}

namespace PhysBPLayers {
        static constexpr JPH::BroadPhaseLayer NON_MOVING{0};
        static constexpr JPH::BroadPhaseLayer MOVING{1};
        static constexpr JPH::uint NUM = 2;
}

// ---- broad-phase layer interface --------------------------------------------

class SEBPLayerInterface final : public JPH::BroadPhaseLayerInterface {

public:
        JPH::uint GetNumBroadPhaseLayers() const override {
                return PhysBPLayers::NUM;
        }
        JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
                switch (layer) {
                        case PhysLayers::STATIC:  return PhysBPLayers::NON_MOVING;
                        case PhysLayers::DYNAMIC: return PhysBPLayers::MOVING;
                        case PhysLayers::TRIGGER: return PhysBPLayers::MOVING;
                        default:                  return PhysBPLayers::NON_MOVING;
                }
        }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
        const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
                return layer == PhysBPLayers::NON_MOVING ? "NON_MOVING" : "MOVING";
        }
#endif
};

// ---- object vs broad-phase filter ------------------------------------------

class SEObjBPFilter final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
        bool ShouldCollide(JPH::ObjectLayer obj, JPH::BroadPhaseLayer bp) const override {
                switch (obj) {
                        case PhysLayers::STATIC:  return bp == PhysBPLayers::MOVING;
                        case PhysLayers::DYNAMIC: return true;
                        case PhysLayers::TRIGGER: return bp == PhysBPLayers::MOVING;
                        default:                  return false;
                }
        }
};

// ---- object layer pair filter -----------------------------------------------

class SEObjLayerFilter final : public JPH::ObjectLayerPairFilter {
public:
        bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
                switch (a) {
                        case PhysLayers::STATIC:
                                return b == PhysLayers::DYNAMIC || b == PhysLayers::TRIGGER;
                        case PhysLayers::DYNAMIC:
                                return b == PhysLayers::STATIC || b == PhysLayers::DYNAMIC || b == PhysLayers::TRIGGER;
                        case PhysLayers::TRIGGER:
                                return b == PhysLayers::STATIC || b == PhysLayers::DYNAMIC;
                        default:
                                return false;
                }
        }
};

// ---- contact listener -------------------------------------------------------

struct PendingContact {
        bool        is_enter;
        bool        a_trigger, b_trigger;
        JPH::BodyID hA, hB;
        glm::vec3   vNormal, vPoint;
};

class SEContactListener final : public JPH::ContactListener {

        std::mutex                  oMtx;
        std::vector<PendingContact> vPending;
public:
        JPH::ValidateResult OnContactValidate(
                        const JPH::Body& a, const JPH::Body& b,
                        JPH::RVec3Arg, const JPH::CollideShapeResult&) override {

                uint32_t a_layer = static_cast<uint32_t>(a.GetUserData());
                uint32_t a_mask  = static_cast<uint32_t>(a.GetUserData() >> 32);
                uint32_t b_layer = static_cast<uint32_t>(b.GetUserData());
                uint32_t b_mask  = static_cast<uint32_t>(b.GetUserData() >> 32);

                bool a_trig = (a.GetObjectLayer() == PhysLayers::TRIGGER);
                bool b_trig = (b.GetObjectLayer() == PhysLayers::TRIGGER);

                if (a_trig || b_trig) {
                        // Trigger contacts: only the trigger's mask is checked (unidirectional).
                        // The non-trigger body's mask is irrelevant — it doesn't "subscribe" to triggers.
                        uint32_t trig_mask   = a_trig ? a_mask  : b_mask;
                        uint32_t other_layer = a_trig ? b_layer : a_layer;
                        if (!(trig_mask & other_layer))
                                return JPH::ValidateResult::RejectAllContactsForThisBodyPair;
                } else {
                        // Body–body contacts: bidirectional OR — either side detecting the other suffices.
                        if (!(a_mask & b_layer) && !(b_mask & a_layer))
                                return JPH::ValidateResult::RejectAllContactsForThisBodyPair;
                }
                return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
        }

        void OnContactAdded(
                        const JPH::Body& a,
                        const JPH::Body& b,
                        const JPH::ContactManifold& manifold,
                        JPH::ContactSettings&) override {

                bool a_trig = (a.GetObjectLayer() == PhysLayers::TRIGGER);
                bool b_trig = (b.GetObjectLayer() == PhysLayers::TRIGGER);
                JPH::Vec3 pt = manifold.GetWorldSpaceContactPointOn1(0);

                std::lock_guard<std::mutex> lk(oMtx);

                vPending.push_back({true, a_trig, b_trig, a.GetID(), b.GetID(),
                                {manifold.mWorldSpaceNormal.GetX(), manifold.mWorldSpaceNormal.GetY(), manifold.mWorldSpaceNormal.GetZ()},
                                {pt.GetX(), pt.GetY(), pt.GetZ()}});
        }

        void OnContactRemoved(const JPH::SubShapeIDPair& pair) override {
                std::lock_guard<std::mutex> lk(oMtx);
                vPending.push_back({false, false, false, pair.GetBody1ID(), pair.GetBody2ID(), {}, {}});
        }

        std::vector<PendingContact> Drain() {
                std::lock_guard<std::mutex> lk(oMtx);
                return std::move(vPending);
        }
};


// ---- body state for interpolation ------------------------------------------

struct BodyState {
        BodyHandle hBody;
        glm::vec3  vPrevPos  {0.0f, 0.0f, 0.0f};
        glm::vec3  vCurrPos  {0.0f, 0.0f, 0.0f};
        glm::quat  qPrevRot  {1.0f, 0.0f, 0.0f, 0.0f};
        glm::quat  qCurrRot  {1.0f, 0.0f, 0.0f, 0.0f};
        ColliderDesc oCollider;
        void*      pNode       = nullptr;  // TSceneTree::TSceneNodeExact*
        bool       is_trigger  = false;
        bool       is_static   = false;
        bool       is_kinematic = false;

        std::string Str() const {
                return fmt::format(
                        "BodyState: prev: ({:.2f}, {:.2f}, {:.2f}), curr: ({:.2f}, {:.2f}, {:.2f}), "
                        "prevRot: ({:.2f}, {:.2f}, {:.2f}, {:.2f}), currRot: ({:.2f}, {:.2f}, {:.2f}, {:.2f}), "
                        "node: {:p}, static: {}, kinematic: {}, trigger: {}",
                        vPrevPos.x, vPrevPos.y, vPrevPos.z,
                        vCurrPos.x, vCurrPos.y, vCurrPos.z,
                        qPrevRot.w, qPrevRot.x, qPrevRot.y, qPrevRot.z,
                        qCurrRot.w, qCurrRot.x, qCurrRot.y, qCurrRot.z,
                        pNode, is_static, is_kinematic, is_trigger);
        }
};

// ---- deferred command -------------------------------------------------------

struct DeferredCmd {

        enum Type { Impulse, SetVelocity, Teleport, Destroy, MoveKinematic, Activate, Deactivate } type;
        JPH::BodyID hId;
        glm::vec3   vParam;
        glm::quat   qParam;
};

// ---- helpers ----------------------------------------------------------------

static inline BodyHandle ToHandle(JPH::BodyID id) {
        return { id.GetIndex(), uint32_t(id.GetSequenceNumber()) };
}

static inline JPH::BodyID ToJoltID(BodyHandle h) {
        return JPH::BodyID(h.index | (uint32_t(h.generation) << JPH::BodyID::cSequenceNumberShift));
}


static inline glm::vec3 ToGlm(JPH::Vec3Arg v) {
        return {v.GetX(), v.GetY(), v.GetZ()};
}
static inline glm::quat ToGlm(JPH::QuatArg q) {
        return {q.GetW(), q.GetX(), q.GetY(), q.GetZ()};
}
static inline JPH::Vec3  ToJolt (glm::vec3 v) { return {v.x, v.y, v.z}; }
static inline JPH::RVec3 ToJoltR(glm::vec3 v) { return {v.x, v.y, v.z}; }
static inline JPH::Quat  ToJolt (glm::quat q) { return JPH::Quat(q.x, q.y, q.z, q.w); }

// ============================================================================
// PhysicsSystem::Impl
// ============================================================================

struct PhysicsSystem::Impl {

        PhysicsConfig oCfg;
        bool          initialized = false;

        std::unique_ptr<JPH::TempAllocatorImpl>    pTempAlloc;
        std::unique_ptr<JPH::JobSystemThreadPool>  pJobSystem;

        SEBPLayerInterface                         oBPLayerInterface;
        SEObjBPFilter                              oObjBPFilter;
        SEObjLayerFilter                           oObjLayerFilter;
        SEContactListener                          oContactListener;
        std::unique_ptr<JPH::PhysicsSystem>        pPhysics;

        // Body states (keyed by body array index for fast lookup)
        std::unordered_map<uint32_t, BodyState>    mBodyStates;

        // Deferred commands (game thread → physics thread)
        std::mutex                                 oCmdMtx;
        std::vector<DeferredCmd>                   vPendingCmds;

        FixedClock oFixedClock;
        float      interpolation_alpha = 0.0f;
        bool       paused             = false;

        // ---- shape creation -------------------------------------------------
        JPH::Ref<JPH::Shape> MakeShape(const ColliderDesc& c) {
                switch (c.type) {
                        case ColliderDesc::Box:
                                return new JPH::BoxShape(JPH::Vec3(c.vHalfExtents.x, c.vHalfExtents.y, c.vHalfExtents.z));
                        case ColliderDesc::Sphere:
                                return new JPH::SphereShape(c.radius);
                        case ColliderDesc::Capsule:
                                return new JPH::CapsuleShape(c.half_height, c.radius);
                        case ColliderDesc::Mesh: {
                                JPH::VertexList jVerts;
                                jVerts.reserve(c.vMeshVertices.size());
                                for (const auto & v : c.vMeshVertices)
                                        jVerts.push_back(JPH::Float3(v.x, v.y, v.z));

                                JPH::IndexedTriangleList jTris;
                                jTris.reserve(c.vMeshIndices.size() / 3);
                                for (uint32_t i = 0; i + 2 < c.vMeshIndices.size(); i += 3)
                                        jTris.push_back(JPH::IndexedTriangle(
                                                c.vMeshIndices[i], c.vMeshIndices[i+1], c.vMeshIndices[i+2]));

                                JPH::MeshShapeSettings settings(std::move(jVerts), std::move(jTris));
                                auto result = settings.Create();
                                if (result.IsValid()) return result.Get();
                                log_e("PhysicsSystem: MeshShape creation failed");
                                return new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f)); // fallback
                        }
                }
                return new JPH::SphereShape(0.5f); // fallback
        }

        // ---- flush deferred commands (call before each fixed step) ----------
        void FlushCommands() {
                std::vector<DeferredCmd> cmds;
                {
                        std::lock_guard<std::mutex> lk(oCmdMtx);
                        cmds = std::move(vPendingCmds);
                }
                auto& bi = pPhysics->GetBodyInterface();
                for (auto& cmd : cmds) {
                        if (cmd.type == DeferredCmd::Destroy) {
                                if (bi.IsAdded(cmd.hId)) bi.RemoveBody(cmd.hId);
                                bi.DestroyBody(cmd.hId);
                                mBodyStates.erase(cmd.hId.GetIndex());
                                continue;
                        }
                        if (!bi.IsAdded(cmd.hId)) continue;
                        switch (cmd.type) {
                                case DeferredCmd::Impulse:
                                        bi.AddImpulse(cmd.hId, ToJolt(cmd.vParam));
                                        break;
                                case DeferredCmd::SetVelocity:
                                        bi.SetLinearVelocity(cmd.hId, ToJolt(cmd.vParam));
                                        break;
                                case DeferredCmd::Teleport:
                                        bi.SetPositionAndRotation(cmd.hId, ToJoltR(cmd.vParam), ToJolt(cmd.qParam),
                                                        JPH::EActivation::Activate);
                                        break;
                                case DeferredCmd::MoveKinematic:
                                        bi.MoveKinematic(cmd.hId, ToJoltR(cmd.vParam), ToJolt(cmd.qParam),
                                                        oCfg.fixed_dt);
                                        break;
                                case DeferredCmd::Activate:
                                        bi.ActivateBody(cmd.hId);
                                        break;
                                case DeferredCmd::Deactivate:
                                        bi.DeactivateBody(cmd.hId);
                                        break;
                                default: break;
                        }
                }
        }

        // ---- snapshot current into prev before each fixed step --------------
        void SnapshotCurrentState() {
                auto& bi = pPhysics->GetBodyInterface();
                for (auto& [idx, state] : mBodyStates) {
                        state.vPrevPos = state.vCurrPos;
                        state.qPrevRot = state.qCurrRot;
                        JPH::RVec3 pos; JPH::Quat rot;
                        bi.GetPositionAndRotation(ToJoltID(state.hBody), pos, rot);
                        state.vCurrPos = ToGlm(pos);
                        state.qCurrRot = ToGlm(rot);
                }
        }

        // ---- after fixed step: read back new body positions -----------------
        void UpdateCurrentState() {
                auto& bi = pPhysics->GetBodyInterface();
                for (auto& [idx, state] : mBodyStates) {
                        JPH::RVec3 pos; JPH::Quat rot;
                        bi.GetPositionAndRotation(ToJoltID(state.hBody), pos, rot);
                        state.vCurrPos = ToGlm(pos);
                        state.qCurrRot = ToGlm(rot);
                }
        }

        // ---- fire contact/trigger events ------------------------------------
        void DrainContacts() {
                auto contacts = oContactListener.Drain();
                if (contacts.empty()) return;

                auto& em = GetSystem<EventManager>();
                for (auto& c : contacts) {
                        BodyHandle ha = ToHandle(c.hA);
                        BodyHandle hb = ToHandle(c.hB);
                        if (c.is_enter) {
                                if (c.a_trigger || c.b_trigger) {
                                        BodyHandle hTrigger = c.a_trigger ? ha : hb;
                                        BodyHandle hOther   = c.a_trigger ? hb : ha;
                                        em.TriggerEvent(EPhysicsTriggerEnter{hTrigger, hOther});
                                } else {
                                        em.TriggerEvent(EPhysicsContactEnter{ha, hb, c.vNormal, c.vPoint});
                                }
                        } else {
                                auto itA = mBodyStates.find(c.hA.GetIndex());
                                bool a_trig = (itA != mBodyStates.end()) && itA->second.is_trigger;
                                auto itB = mBodyStates.find(c.hB.GetIndex());
                                bool b_trig = (itB != mBodyStates.end()) && itB->second.is_trigger;
                                if (a_trig || b_trig) {
                                        BodyHandle hTrigger = a_trig ? ha : hb;
                                        BodyHandle hOther   = a_trig ? hb : ha;
                                        em.TriggerEvent(EPhysicsTriggerExit{hTrigger, hOther});
                                } else {
                                        em.TriggerEvent(EPhysicsContactExit{ha, hb});
                                }
                        }
                }
        }

        // ---- run a single fixed step (shared by Update and StepOnce) --------
        void RunFixedStep() {
                FlushCommands();
                SnapshotCurrentState();
                pPhysics->Update(
                        oCfg.fixed_dt,
                        oCfg.collision_steps,
                        pTempAlloc.get(),
                        pJobSystem.get());
                UpdateCurrentState();
                DrainContacts();
        }
};

// ============================================================================
// PhysicsSystem public implementation
// ============================================================================

PhysicsSystem::PhysicsSystem() : pImpl(std::make_unique<Impl>()) {}

PhysicsSystem::~PhysicsSystem() noexcept {
        if (pImpl->initialized) Shutdown();
}

void PhysicsSystem::Init(const PhysicsConfig& cfg) {
        assert(!pImpl->initialized);
        pImpl->oCfg = cfg;
        pImpl->oFixedClock = FixedClock(cfg.fixed_dt);

        JPH::RegisterDefaultAllocator();
        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();

        pImpl->pTempAlloc = std::make_unique<JPH::TempAllocatorImpl>(cfg.temp_allocator_mb * 1024u * 1024u);

        //TODO read from global conf
        int threads = std::max(1, (int)std::thread::hardware_concurrency() - 1);
        pImpl->pJobSystem = std::make_unique<JPH::JobSystemThreadPool>(
                        JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, threads);

        pImpl->pPhysics = std::make_unique<JPH::PhysicsSystem>();
        pImpl->pPhysics->Init(
                        (JPH::uint)cfg.max_bodies, 0 /*mutexes=auto*/,
                        (JPH::uint)cfg.max_body_pairs, (JPH::uint)cfg.max_contact_constraints,
                        pImpl->oBPLayerInterface,
                        pImpl->oObjBPFilter,
                        pImpl->oObjLayerFilter);

        pImpl->pPhysics->SetContactListener(&pImpl->oContactListener);
        pImpl->pPhysics->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));

        pImpl->initialized = true;
        log_i("PhysicsSystem: initialized (Jolt {}.{}.{})", JPH_VERSION_MAJOR, JPH_VERSION_MINOR, JPH_VERSION_PATCH);
}

void PhysicsSystem::Shutdown() {

        if (!pImpl->initialized) return;

        pImpl->mBodyStates.clear();
        pImpl->pPhysics.reset();
        pImpl->pJobSystem.reset();
        pImpl->pTempAlloc.reset();

        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;

        JPH::Factory::sInstance = nullptr;
        pImpl->initialized = false;

        log_i("PhysicsSystem: shutdown");
}

BodyHandle PhysicsSystem::CreateRigidBody(const RigidBodyDesc& desc) {

        assert(pImpl->initialized);

        log_d("{}", desc.Str());

        JPH::Ref<JPH::Shape> shape = pImpl->MakeShape(desc.oCollider);

        JPH::ObjectLayer layer = desc.is_static     ? PhysLayers::STATIC
                : desc.is_trigger    ? PhysLayers::TRIGGER
                : PhysLayers::DYNAMIC;

        JPH::EMotionType motion_type = desc.is_static     ? JPH::EMotionType::Static
                : desc.is_kinematic  ? JPH::EMotionType::Kinematic
                : JPH::EMotionType::Dynamic;

        JPH::BodyCreationSettings settings(
                        shape.GetPtr(),
                        ToJoltR(desc.vInitialPosition),
                        ToJolt(desc.qInitialRotation),
                        motion_type,
                        layer);

        settings.mUserData       = (static_cast<uint64_t>(desc.collision_mask) << 32)
                                 | static_cast<uint64_t>(desc.collision_layer);
        settings.mFriction       = desc.friction;
        settings.mRestitution    = desc.restitution;
        settings.mLinearDamping  = desc.linear_damping;
        settings.mAngularDamping = desc.angular_damping;
        settings.mGravityFactor  = desc.gravity_scale;
        settings.mIsSensor       = desc.is_trigger;

        if (!desc.is_static && desc.mass > 0.0f) {
                settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
                settings.mMassPropertiesOverride.mMass = desc.mass;
        }

        auto& bi = pImpl->pPhysics->GetBodyInterface();
        JPH::Body* body = bi.CreateBody(settings);
        if (!body) {
                log_e("PhysicsSystem: CreateBody failed (body limit reached)");
                return BodyHandle{};
        }

        bi.AddBody(body->GetID(), JPH::EActivation::Activate);

        BodyHandle h = ToHandle(body->GetID());

        BodyState state;
        state.hBody       = h;
        state.vCurrPos    = desc.vInitialPosition;
        state.vPrevPos    = desc.vInitialPosition;
        state.qCurrRot    = desc.qInitialRotation;
        state.qPrevRot    = desc.qInitialRotation;
        state.oCollider   = desc.oCollider;
        state.oCollider.vMeshVertices.clear();
        state.oCollider.vMeshVertices.shrink_to_fit();
        state.oCollider.vMeshIndices.clear();
        state.oCollider.vMeshIndices.shrink_to_fit();
        state.is_trigger  = desc.is_trigger;
        state.is_static   = desc.is_static;
        state.is_kinematic = desc.is_kinematic;

        //log_d("{}", state.Str());

        pImpl->mBodyStates[body->GetID().GetIndex()] = state;

        return h;
}

void PhysicsSystem::DestroyBody(BodyHandle h) {
        if (!h.IsValid() || !pImpl->initialized) return;

        DeferredCmd cmd;
        cmd.type = DeferredCmd::Destroy;
        cmd.hId  = ToJoltID(h);
        std::lock_guard<std::mutex> lk(pImpl->oCmdMtx);
        pImpl->vPendingCmds.push_back(cmd);
}

void PhysicsSystem::ApplyImpulse(BodyHandle h, glm::vec3 impulse) {
        if (!h.IsValid() || !pImpl->initialized) return;

        DeferredCmd cmd;
        cmd.type   = DeferredCmd::Impulse;
        cmd.hId    = ToJoltID(h);
        cmd.vParam = impulse;
        std::lock_guard<std::mutex> lk(pImpl->oCmdMtx);
        pImpl->vPendingCmds.push_back(cmd);
}

void PhysicsSystem::SetLinearVelocity(BodyHandle h, glm::vec3 vel) {
        if (!h.IsValid() || !pImpl->initialized) return;

        DeferredCmd cmd;
        cmd.type   = DeferredCmd::SetVelocity;
        cmd.hId    = ToJoltID(h);
        cmd.vParam = vel;
        std::lock_guard<std::mutex> lk(pImpl->oCmdMtx);
        pImpl->vPendingCmds.push_back(cmd);
}

void PhysicsSystem::Teleport(BodyHandle h, glm::vec3 pos, glm::quat rot) {
        if (!h.IsValid() || !pImpl->initialized) return;

        DeferredCmd cmd;
        cmd.type   = DeferredCmd::Teleport;
        cmd.hId    = ToJoltID(h);
        cmd.vParam = pos;
        cmd.qParam = rot;
        std::lock_guard<std::mutex> lk(pImpl->oCmdMtx);
        pImpl->vPendingCmds.push_back(cmd);
}

void PhysicsSystem::MoveKinematic(BodyHandle h, glm::vec3 pos, glm::quat rot) {
        if (!h.IsValid() || !pImpl->initialized) return;

        DeferredCmd cmd;
        cmd.type   = DeferredCmd::MoveKinematic;
        cmd.hId    = ToJoltID(h);
        cmd.vParam = pos;
        cmd.qParam = rot;
        std::lock_guard<std::mutex> lk(pImpl->oCmdMtx);
        pImpl->vPendingCmds.push_back(cmd);
}

void PhysicsSystem::ActivateBody(BodyHandle h) {
        if (!h.IsValid() || !pImpl->initialized) return;

        DeferredCmd cmd;
        cmd.type = DeferredCmd::Activate;
        cmd.hId  = ToJoltID(h);
        std::lock_guard<std::mutex> lk(pImpl->oCmdMtx);
        pImpl->vPendingCmds.push_back(cmd);
}

void PhysicsSystem::DeactivateBody(BodyHandle h) {
        if (!h.IsValid() || !pImpl->initialized) return;

        DeferredCmd cmd;
        cmd.type = DeferredCmd::Deactivate;
        cmd.hId  = ToJoltID(h);
        std::lock_guard<std::mutex> lk(pImpl->oCmdMtx);
        pImpl->vPendingCmds.push_back(cmd);
}

void PhysicsSystem::RegisterNode(BodyHandle h, void* pNode) {
        if (!h.IsValid()) return;

        auto it = pImpl->mBodyStates.find(h.index);
        if (it != pImpl->mBodyStates.end()) {
                it->second.pNode = pNode;
        }
}

void PhysicsSystem::UnregisterNode(BodyHandle h) {
        if (!h.IsValid()) return;

        auto it = pImpl->mBodyStates.find(h.index);
        if (it != pImpl->mBodyStates.end()) {
                it->second.pNode = nullptr;
        }
}

void* PhysicsSystem::GetNode(BodyHandle h) const {
        if (!h.IsValid()) return nullptr;

        auto it = pImpl->mBodyStates.find(h.index);
        if (it == pImpl->mBodyStates.end()) return nullptr;
        return it->second.pNode;
}

std::string RigidBodyDesc::Str() const {
        const char* collider_type = oCollider.type == ColliderDesc::Box     ? "Box"
                                  : oCollider.type == ColliderDesc::Sphere ? "Sphere"
                                  : oCollider.type == ColliderDesc::Mesh   ? "Mesh"
                                                                           : "Capsule";
        return fmt::format(
                "RigidBodyDesc: pos: ({:.2f}, {:.2f}, {:.2f}), rot: ({:.2f}, {:.2f}, {:.2f}, {:.2f}), "
                "collider: {}, half: ({:.2f}, {:.2f}, {:.2f}), radius: {:.2f}, "
                "static: {}, kinematic: {}, trigger: {}, "
                "mass: {:.2f}, friction: {:.2f}, restitution: {:.2f}, "
                "ldamp: {:.2f}, adamp: {:.2f}, gravity_scale: {:.2f}",
                vInitialPosition.x, vInitialPosition.y, vInitialPosition.z,
                qInitialRotation.w, qInitialRotation.x, qInitialRotation.y, qInitialRotation.z,
                collider_type,
                oCollider.vHalfExtents.x, oCollider.vHalfExtents.y, oCollider.vHalfExtents.z,
                oCollider.radius,
                is_static, is_kinematic, is_trigger,
                mass, friction, restitution,
                linear_damping, angular_damping, gravity_scale);
}

void PhysicsSystem::Update(float game_dt) {
        if (!pImpl->initialized) return;
        if (pImpl->paused) {
                pImpl->interpolation_alpha = 1.0f;
                return;
        }

        while (pImpl->oFixedClock.Step(game_dt)) {
                pImpl->RunFixedStep();
        }

        pImpl->interpolation_alpha = pImpl->oFixedClock.Alpha();
}

void PhysicsSystem::SetPaused(bool paused) {
        pImpl->paused = paused;
        if (paused) pImpl->oFixedClock.Reset();
}

void PhysicsSystem::StepOnce() {
        if (!pImpl->initialized) return;
        pImpl->RunFixedStep();
        pImpl->interpolation_alpha = 1.0f;
}

void PhysicsSystem::DrawDebug() {
        if (!pImpl->initialized) return;

        auto & dr = GetSystem<DebugRenderer>();

        for (auto & [idx, state] : pImpl->mBodyStates) {

                const glm::vec4 vColor = state.is_trigger  ? glm::vec4(0.0f, 1.0f, 1.0f, 1.0f)  // cyan
                                       : state.is_static   ? glm::vec4(0.4f, 0.4f, 1.0f, 1.0f)  // blue
                                       : state.is_kinematic? glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)  // yellow
                                       :                     glm::vec4(1.0f, 0.5f, 0.0f, 1.0f);  // orange

                Transform oT;
                oT.SetPos(state.vCurrPos);
                oT.SetRotation(state.qCurrRot);

                switch (state.oCollider.type) {
                        case ColliderDesc::Box: {
                                BoundingBox bbox(-state.oCollider.vHalfExtents, state.oCollider.vHalfExtents);
                                dr.DrawBBox(bbox, oT, vColor);
                                break;
                        }
                        case ColliderDesc::Sphere:
                        case ColliderDesc::Capsule: {
                                const float r = state.oCollider.radius;
                                constexpr int N = 32;
                                for (int i = 0; i < N; ++i) {
                                        float a0 = glm::radians(360.0f * i       / N);
                                        float a1 = glm::radians(360.0f * (i + 1) / N);
                                        float c0 = glm::cos(a0), s0 = glm::sin(a0);
                                        float c1 = glm::cos(a1), s1 = glm::sin(a1);
                                        dr.DrawLine(state.vCurrPos + glm::vec3(r*c0, r*s0, 0), state.vCurrPos + glm::vec3(r*c1, r*s1, 0), vColor);
                                        dr.DrawLine(state.vCurrPos + glm::vec3(r*c0, 0, r*s0), state.vCurrPos + glm::vec3(r*c1, 0, r*s1), vColor);
                                        dr.DrawLine(state.vCurrPos + glm::vec3(0, r*c0, r*s0), state.vCurrPos + glm::vec3(0, r*c1, r*s1), vColor);
                                }
                                break;
                        }
                        case ColliderDesc::Mesh:
                                break; // mesh data cleared after creation — no debug draw
                }
        }
}

void PhysicsSystem::Interpolate() {
        const float alpha = pImpl->interpolation_alpha;
        if (!pImpl->initialized) return;

        for (auto& [idx, state] : pImpl->mBodyStates) {
                if (!state.pNode || state.is_static || state.is_kinematic) continue;

                glm::vec3 pos = glm::mix(state.vPrevPos, state.vCurrPos, alpha);
                glm::quat rot = glm::slerp(state.qPrevRot, state.qCurrRot, alpha);

                //log_d("BodyState[{}]: {}", idx, state.Str());

                auto* pNode = static_cast<TSceneTree::TSceneNodeExact*>(state.pNode);
                pNode->SetWorldPos(pos);
                pNode->SetRotation(rot);
        }
}

float PhysicsSystem::GetInterpolationAlpha() const {
        return pImpl->interpolation_alpha;
}

bool PhysicsSystem::Raycast(const PhysicsRay& ray, RaycastHit& out, QueryFilter) const {
        if (!pImpl->initialized) return false;

        constexpr float kMaxDist = 1000.0f;
        JPH::RRayCast jray{
                ToJoltR(ray.vOrigin),
                        ToJolt(ray.vDirection * kMaxDist)
        };

        JPH::RayCastResult result;
        bool hit = pImpl->pPhysics->GetNarrowPhaseQuery().CastRay(jray, result);

        if (!hit) return false;

        JPH::BodyID hit_id = result.mBodyID;
        out.hBody    = ToHandle(hit_id);
        out.distance = result.mFraction * kMaxDist;
        out.vPoint   = ray.vOrigin + ray.vDirection * out.distance;

        JPH::BodyLockRead lock(pImpl->pPhysics->GetBodyLockInterface(), hit_id);
        if (lock.Succeeded()) {
                const JPH::Body& body = lock.GetBody();
                JPH::Vec3 n = body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, ToJoltR(out.vPoint));
                out.vNormal = ToGlm(n);
        }

        return true;
}

bool PhysicsSystem::IsInitialized() const {
        return pImpl->initialized;
}

} // namespace SE

#endif // SE_IMPL
