
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
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

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
        // Inner presence body of player characters (CharacterVirtual). Sensors
        // pair ONLY with this layer — NPCs/props can't ring bells, and the
        // characters' own sweeps ignore it (SECharObjFilter) so movement
        // behavior is unchanged.
        static constexpr JPH::ObjectLayer PLAYER  = 3;
        // Kinematic followers (door leaves, NPC blocking capsules). Pairs with
        // NOTHING body-body; only "player-class" character sweeps collide with
        // it — NPC CharacterVirtuals ghost through their own and each other's
        // followers (a self-collision feedback loop otherwise).
        static constexpr JPH::ObjectLayer KINEMATIC = 4;
        static constexpr JPH::ObjectLayer NUM     = 5;
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
                        case PhysLayers::PLAYER:  return PhysBPLayers::MOVING;
                        case PhysLayers::KINEMATIC: return PhysBPLayers::MOVING;
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
                        // Both BP layers: presence bodies must pair with static
                        // geometry too (floors/walls are NON_MOVING).
                        case PhysLayers::PLAYER:  return true;
                        // Kinematic followers live in the MOVING broadphase so
                        // their contacts with DYNAMIC bodies are found (the
                        // pair filter below gates what actually resolves).
                        case PhysLayers::KINEMATIC: return bp == PhysBPLayers::MOVING;
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
                                return b == PhysLayers::DYNAMIC || b == PhysLayers::TRIGGER
                                        || b == PhysLayers::PLAYER;
                        case PhysLayers::DYNAMIC:
                                // No TRIGGER: only the player's presence body fires sensors.
                                // KINEMATIC: platforms carry crates, door leaves push props.
                                return b == PhysLayers::STATIC || b == PhysLayers::DYNAMIC
                                        || b == PhysLayers::PLAYER || b == PhysLayers::KINEMATIC;
                        case PhysLayers::TRIGGER:
                                return b == PhysLayers::PLAYER;
                        case PhysLayers::PLAYER:
                                return b == PhysLayers::STATIC || b == PhysLayers::DYNAMIC
                                        || b == PhysLayers::TRIGGER;
                        // KINEMATIC (MoveKinematic-driven: platforms, door leaves,
                        // NPC presence capsules) resolves contacts against DYNAMIC
                        // bodies only — never with statics, other kinematics, the
                        // player's presence body, or sensors (the PLAYER character
                        // still sweeps followers via SEPlayerCharObjFilter).
                        case PhysLayers::KINEMATIC:
                                return b == PhysLayers::DYNAMIC;
                        default:
                                return false;
                }
        }
};

// ---- character collision filters -------------------------------------------

class SECharBPFilter final : public JPH::BroadPhaseLayerFilter {
public:
        bool ShouldCollide(JPH::BroadPhaseLayer) const override { return true; }
};

class SECharObjFilter final : public JPH::ObjectLayerFilter {
public:
        bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
                // TRIGGER: sensors never block movement. PLAYER: presence bodies
                // (the character's own and other characters') — characters keep
                // ghosting through each other; only the sim sees the presence.
                // KINEMATIC: followers (door leaves, NPC blocking capsules) —
                // an NPC sweeping its own follower capsule degenerates into a
                // depenetration feedback loop.
                return inLayer != PhysLayers::TRIGGER && inLayer != PhysLayers::PLAYER
                        && inLayer != PhysLayers::KINEMATIC;
        }
};

// Player-class characters DO collide with kinematic followers (closed doors,
// solid NPCs) — selected per character in StepCharacters.
class SEPlayerCharObjFilter final : public JPH::ObjectLayerFilter {
public:
        bool ShouldCollide(JPH::ObjectLayer inLayer) const override {
                return inLayer != PhysLayers::TRIGGER && inLayer != PhysLayers::PLAYER;
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
        // Slot in Impl::vSimStates (UINT32_MAX = never simulated — static).
        // Jolt recycles body indexes; generation is what makes a stale
        // handle fail the lookup instead of resolving to the new body.
        uint32_t   sim_slot    = UINT32_MAX;
        uint32_t   generation = 0;
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

        // Bodies that can actually move (dynamic + kinematic), referencing the
        // stable unordered_map nodes. The per-fixed-step snapshot/update/
        // interpolate sweeps walk THIS, not mBodyStates — with streamed terrain
        // tiles and level statics the map is ~95% statics that never move.
        std::vector<BodyState*>                    vSimStates;

        // Character presence bodies (Jolt body index → node): kept apart from
        // mBodyStates so the interpolation loops never see them — the node is
        // driven by the CharacterController, not by body interpolation.
        // Generation-checked: recycled inner-body indexes of destroyed
        // characters must not resolve a stale event handle to a new character.
        struct InnerBodyRef {
                uint32_t generation = 0;
                void*    pNode       = nullptr;
        };
        std::unordered_map<uint32_t, InnerBodyRef> mInnerBodyNodes;

        // Targeted sensor-contact delivery (see AttachSensorListener). Raw
        // EPhysicsTriggerEnter/Exit still broadcast; this map is what lets a
        // TriggerVolume receive only its own contacts.
        struct SensorEntry {
                uint32_t       generation = 0;
                SensorContactFn fn;
        };
        std::unordered_map<uint32_t, SensorEntry>  mSensorListeners;

        // Character states (keyed by CharHandle::id)
        struct CharState {
                JPH::Ref<JPH::CharacterVirtual> pChar;
                glm::vec3 vDesiredVelocity {0.f, 0.f, 0.f};
                float     step_height      = 0.3f;
                void*     pNode            = nullptr;
                bool      bCollidesKinematic = true;
                // update LOD: step every Nth fixed step, staggered by phase
                uint32_t  update_divider   = 1;
                uint32_t  update_phase     = 0;
                // presence body (for body→node lookups: sensors report it as
                // the event's hOther — trigger bindings resolve the actor)
                JPH::BodyID inner_body_id  {};
        };
        std::unordered_map<uint32_t, CharState> mCharacters;
        uint32_t next_char_id = 0;

        // Kinematic followers are node-driven: Jolt keeps the (delta/dt)
        // velocity MoveKinematic sets and keeps integrating it, so a body whose
        // node rested after a move would drift (or rocket) away. Bodies that
        // received no move this fixed step get their velocities zeroed.
        std::vector<JPH::BodyID>                 vKinematicBodies;
        std::unordered_map<JPH::BodyID, uint64_t> mKinematicMoved;
        uint64_t                                 step_counter = 0;

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
                                vKinematicBodies.erase(
                                                std::remove(vKinematicBodies.begin(), vKinematicBodies.end(), cmd.hId),
                                                vKinematicBodies.end());
                                mKinematicMoved.erase(cmd.hId);
                                const auto st_it = mBodyStates.find(cmd.hId.GetIndex());
                                if (st_it != mBodyStates.end()
                                    && st_it->second.generation == cmd.hId.GetSequenceNumber()) {
                                        // swap-remove from the dense sim list, fixing
                                        // the moved element's slot
                                        if (st_it->second.sim_slot != UINT32_MAX) {
                                                const size_t slot = st_it->second.sim_slot;
                                                vSimStates[slot]  = vSimStates.back();
                                                vSimStates[slot]->sim_slot = uint32_t(slot);
                                                vSimStates.pop_back();
                                        }
                                        mBodyStates.erase(st_it);
                                }
                                const auto sen_it = mSensorListeners.find(cmd.hId.GetIndex());
                                if (sen_it != mSensorListeners.end()
                                    && sen_it->second.generation == cmd.hId.GetSequenceNumber()) {
                                        mSensorListeners.erase(sen_it);
                                }
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
                                        mKinematicMoved[cmd.hId] = step_counter;
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
        // Simulated bodies only: statics (terrain tiles, level geometry) never
        // move, so reading them twice per step was pure per-body mutex cost.
        void SnapshotCurrentState() {
                auto& bi = pPhysics->GetBodyInterface();
                for (BodyState* pState : vSimStates) {
                        pState->vPrevPos = pState->vCurrPos;
                        pState->qPrevRot = pState->qCurrRot;
                        JPH::RVec3 pos; JPH::Quat rot;
                        bi.GetPositionAndRotation(ToJoltID(pState->hBody), pos, rot);
                        pState->vCurrPos = ToGlm(pos);
                        pState->qCurrRot = ToGlm(rot);
                }
        }

        // ---- after fixed step: read back new body positions -----------------
        void UpdateCurrentState() {
                auto& bi = pPhysics->GetBodyInterface();
                for (BodyState* pState : vSimStates) {
                        JPH::RVec3 pos; JPH::Quat rot;
                        bi.GetPositionAndRotation(ToJoltID(pState->hBody), pos, rot);
                        pState->vCurrPos = ToGlm(pos);
                        pState->qCurrRot = ToGlm(rot);
                }
        }

        // ---- fire contact/trigger events ------------------------------------
        void DrainContacts() {
                auto contacts = oContactListener.Drain();
                if (contacts.empty()) return;

                auto& em = GetSystem<EventManager>();
                // Targeted delivery first: the owner gets its contact directly,
                // then the raw event still broadcasts for external sinks (the
                // EventManager early-outs when no one subscribed).
                auto deliver = [this, &em](const BodyHandle hTrigger, const BodyHandle hOther,
                                           const bool is_enter) {
                        if (is_enter) {
                                em.TriggerEvent(EPhysicsTriggerEnter{hTrigger, hOther});
                        } else {
                                em.TriggerEvent(EPhysicsTriggerExit{hTrigger, hOther});
                        }
                        const auto lit = mSensorListeners.find(hTrigger.index);
                        if (lit != mSensorListeners.end()
                            && lit->second.generation == hTrigger.generation
                            && lit->second.fn) {
                                lit->second.fn(is_enter, hOther);
                        }
                };
                for (auto& c : contacts) {
                        BodyHandle ha = ToHandle(c.hA);
                        BodyHandle hb = ToHandle(c.hB);
                        if (c.is_enter) {
                                if (c.a_trigger || c.b_trigger) {
                                        deliver(c.a_trigger ? ha : hb, c.a_trigger ? hb : ha, true);
                                } else {
                                        em.TriggerEvent(EPhysicsContactEnter{ha, hb, c.vNormal, c.vPoint});
                                }
                        } else {
                                auto itA = mBodyStates.find(ha.index);
                                bool a_trig = (itA != mBodyStates.end())
                                              && itA->second.generation == ha.generation
                                              && itA->second.is_trigger;
                                auto itB = mBodyStates.find(hb.index);
                                bool b_trig = (itB != mBodyStates.end())
                                              && itB->second.generation == hb.generation
                                              && itB->second.is_trigger;
                                if (a_trig || b_trig) {
                                        deliver(a_trig ? ha : hb, a_trig ? hb : ha, false);
                                } else {
                                        em.TriggerEvent(EPhysicsContactExit{ha, hb});
                                }
                        }
                }
        }

        // ---- step all CharacterVirtual instances at fixed_dt ----------------
        void StepCharacters(float dt) {
                if (mCharacters.empty()) return;

                SECharBPFilter       oBPFilter;
                SECharObjFilter      oObjFilter;       // NPC flavor: ghosts followers
                SEPlayerCharObjFilter oPlayerFilter;   // player flavor: sweeps followers
                JPH::BodyFilter  oBodyFilter;
                JPH::ShapeFilter oShapeFilter;
                JPH::Vec3        vGravity = pPhysics->GetGravity();

                JPH::CharacterVirtual::ExtendedUpdateSettings oExtSettings;

                for (auto& [id, cs] : mCharacters) {
                        // Character LOD: staggered step division, dt scaled so
                        // average velocity and gravity are unchanged.
                        if (cs.update_divider > 1
                            && ((step_counter + cs.update_phase) % cs.update_divider) != 0u) {
                                continue;
                        }
                        const float char_dt = dt * float(cs.update_divider);

                        oExtSettings.mWalkStairsStepUp    = JPH::Vec3(0.f, cs.step_height, 0.f);
                        oExtSettings.mStickToFloorStepDown = JPH::Vec3(0.f, -cs.step_height, 0.f);

                        cs.pChar->SetLinearVelocity(ToJolt(cs.vDesiredVelocity));
                        JPH::ObjectLayerFilter & oLayerFilter =
                                        cs.bCollidesKinematic
                                        ? static_cast<JPH::ObjectLayerFilter &>(oPlayerFilter)
                                        : static_cast<JPH::ObjectLayerFilter &>(oObjFilter);
                        cs.pChar->ExtendedUpdate(char_dt, vGravity, oExtSettings,
                                        oBPFilter, oLayerFilter, oBodyFilter, oShapeFilter,
                                        *pTempAlloc);
                }
        }

        // ---- run a single fixed step (shared by Update and StepOnce) --------
        void RunFixedStep() {
                FlushCommands();
                StepCharacters(oCfg.fixed_dt);
                SnapshotCurrentState();
                pPhysics->Update(
                        oCfg.fixed_dt,
                        oCfg.collision_steps,
                        pTempAlloc.get(),
                        pJobSystem.get());
                UpdateCurrentState();
                ZeroStaleKinematicVelocities();
                DrainContacts();
        }

        /** Kinematic followers are node-driven and only receive a fresh
         *  (delta/dt) velocity when their node's transform changes — Jolt
         *  would otherwise keep integrating a stale velocity after the node
         *  comes to rest and the body flies away from it. */
        void ZeroStaleKinematicVelocities() {

                ++step_counter;

                JPH::BodyInterface & bi = pPhysics->GetBodyInterface();
                for (const JPH::BodyID & id : vKinematicBodies) {
                        const auto it = mKinematicMoved.find(id);
                        if (it != mKinematicMoved.end() && it->second == step_counter) { continue; }
                        bi.SetLinearVelocity(id, JPH::Vec3::sZero());
                        bi.SetAngularVelocity(id, JPH::Vec3::sZero());
                }
                mKinematicMoved.clear();
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
        // Dense sim references point into mBodyStates nodes — dead now.
        pImpl->vSimStates.clear();
        // Same recycle hazard as vPendingCmds below: every structure keyed by
        // Jolt body index must die with the world, or the next Init resolves
        // recycled indexes into dangling nodes / zeroes new bodies' velocities.
        pImpl->mInnerBodyNodes.clear();
        pImpl->vKinematicBodies.clear();
        pImpl->mKinematicMoved.clear();
        pImpl->mSensorListeners.clear();
        pImpl->step_counter = 0;
        // Characters hold JPH::Ref<CharacterVirtual> bound to the dying Jolt
        // system — leaving them here would resurrect dangling objects on the
        // next Init (found by the physics functional suite).
        pImpl->mCharacters.clear();
        pImpl->next_char_id = 0;
        // Deferred commands hold BodyIDs of the dying world; Jolt recycles
        // those IDs in the next Init, so a stale Destroy here would silently
        // kill a freshly created body (found by the crowd functional suite:
        // level N+1's floor was destroyed by level N's teardown command).
        {
                std::lock_guard<std::mutex> lk(pImpl->oCmdMtx);
                pImpl->vPendingCmds.clear();
        }
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

        JPH::ObjectLayer layer = desc.object_layer >= 0 ? JPH::ObjectLayer(desc.object_layer)
                : desc.is_static     ? PhysLayers::STATIC
                : desc.is_trigger    ? PhysLayers::TRIGGER
                : desc.is_kinematic  ? PhysLayers::KINEMATIC
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

        if (motion_type == JPH::EMotionType::Kinematic) {
                pImpl->vKinematicBodies.push_back(body->GetID());
        }

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

        // Fresh index: the previous owner's map entry was erased on destroy.
        BodyState & stored = pImpl->mBodyStates[body->GetID().GetIndex()];
        stored             = state;
        stored.generation = h.generation;

        // Only bodies that can move join the per-step simulation sweeps.
        if (!stored.is_static) {
                stored.sim_slot = uint32_t(pImpl->vSimStates.size());
                pImpl->vSimStates.push_back(&stored);
        }

        return h;
}

BodyHandle PhysicsSystem::CreateStaticHeightField(const std::vector<float>& vSamples, const uint32_t count,
                                                  const glm::vec3& vOffset, const glm::vec3& vScale,
                                                  const float friction, const uint32_t collision_layer) {

        assert(pImpl->initialized);
        if (count < 2 || vSamples.size() < size_t(count) * count) {
                log_e("PhysicsSystem: CreateStaticHeightField: need {}x{} samples (got {})",
                      count, count, vSamples.size());
                return BodyHandle{};
        }

        JPH::HeightFieldShapeSettings settings(vSamples.data(),
                        JPH::RVec3(vOffset.x, vOffset.y, vOffset.z),
                        JPH::Vec3(vScale.x, vScale.y, vScale.z), count);
        // Full 16-bit height quantization: the physics surface must agree with
        // TerrainSystem::TryGetHeight within the asset's own int16 quantization error.
        settings.mBitsPerSample = 16;

        auto result = settings.Create();
        if (!result.IsValid()) {
                log_e("PhysicsSystem: HeightFieldShape creation failed");
                return BodyHandle{};
        }

        JPH::BodyCreationSettings bcs(result.Get(),
                        JPH::RVec3(0.0, 0.0, 0.0),
                        JPH::Quat::sIdentity(),
                        JPH::EMotionType::Static,
                        PhysLayers::STATIC);
        bcs.mUserData = (static_cast<uint64_t>(0xFFFFFFFFu) << 32) | static_cast<uint64_t>(collision_layer);
        bcs.mFriction = friction;

        auto& bi = pImpl->pPhysics->GetBodyInterface();
        JPH::Body* body = bi.CreateBody(bcs);
        if (!body) {
                log_e("PhysicsSystem: heightfield CreateBody failed (body limit reached)");
                return BodyHandle{};
        }

        bi.AddBody(body->GetID(), JPH::EActivation::DontActivate);
        return ToHandle(body->GetID());
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
        // NaN-proof without isfinite (compiled out by -ffast-math): a garbage
        // target would become a garbage velocity on the kinematic body.
        if (pos.x != pos.x || pos.y != pos.y || pos.z != pos.z
            || pos.x > 1e6f || pos.x < -1e6f || pos.y > 1e6f || pos.y < -1e6f || pos.z > 1e6f || pos.z < -1e6f) {
                return;
        }

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
        if (it != pImpl->mBodyStates.end() && it->second.generation == h.generation) {
                it->second.pNode = pNode;
        }
}

void PhysicsSystem::UnregisterNode(BodyHandle h) {
        if (!h.IsValid()) return;

        auto it = pImpl->mBodyStates.find(h.index);
        if (it != pImpl->mBodyStates.end() && it->second.generation == h.generation) {
                it->second.pNode = nullptr;
        }
}

void* PhysicsSystem::GetNode(BodyHandle h) const {
        if (!h.IsValid()) return nullptr;

        auto it = pImpl->mBodyStates.find(h.index);
        if (it != pImpl->mBodyStates.end()) {
                // Recycled index + stale generation: the handle belongs to a
                // destroyed body, never to the body that now owns the slot.
                if (it->second.generation != h.generation) { return nullptr; }
                return it->second.pNode;
        }

        // character presence bodies: not regular bodies, mapped node-only
        auto inner = pImpl->mInnerBodyNodes.find(h.index);
        return (inner != pImpl->mInnerBodyNodes.end()
                && inner->second.generation == h.generation)
                ? inner->second.pNode : nullptr;
}

void PhysicsSystem::AttachSensorListener(BodyHandle hBody, SensorContactFn fn) {
        if (!hBody.IsValid() || !fn) return;
        pImpl->mSensorListeners[hBody.index] =
                Impl::SensorEntry{ hBody.generation, std::move(fn) };
}

void PhysicsSystem::DetachSensorListener(BodyHandle hBody) {
        if (!hBody.IsValid()) return;
        auto it = pImpl->mSensorListeners.find(hBody.index);
        if (it != pImpl->mSensorListeners.end()
            && it->second.generation == hBody.generation) {
                pImpl->mSensorListeners.erase(it);
        }
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

        // Simulated bodies only (statics never move and are not tracked);
        // kinematic followers are node-driven — their node is never re-synced.
        for (BodyState* pState : pImpl->vSimStates) {
                if (!pState->pNode || pState->is_kinematic) continue;

                glm::vec3 pos = glm::mix(pState->vPrevPos, pState->vCurrPos, alpha);
                glm::quat rot = glm::slerp(pState->qPrevRot, pState->qCurrRot, alpha);

                auto* pNode = static_cast<TSceneTree::TSceneNodeExact*>(pState->pNode);
                pNode->SetWorldPos(pos);
                pNode->SetRotation(rot);
        }

        // Sync character node positions (no interpolation — we own the velocity)
        for (auto& [id, cs] : pImpl->mCharacters) {
                if (!cs.pNode) continue;
                auto* pNode = static_cast<TSceneTree::TSceneNodeExact*>(cs.pNode);
                pNode->SetWorldPos(ToGlm(cs.pChar->GetPosition()));
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

// ============================================================================
// CharacterVirtual public API
// ============================================================================

CharHandle PhysicsSystem::CreateCharacter(const CharacterDesc& desc) {
        assert(pImpl->initialized);

        JPH::Ref<JPH::CapsuleShapeSettings> capsule_settings =
                new JPH::CapsuleShapeSettings(desc.half_height, desc.radius);
        JPH::Ref<JPH::RotatedTranslatedShapeSettings> rts_settings =
                new JPH::RotatedTranslatedShapeSettings(
                        JPH::Vec3(0.f, desc.half_height + desc.radius, 0.f),
                        JPH::Quat::sIdentity(),
                        capsule_settings);

        auto shape_result = rts_settings->Create();
        if (!shape_result.IsValid()) {
                log_e("PhysicsSystem: CreateCharacter failed to build shape");
                return CharHandle{};
        }

        JPH::CharacterVirtualSettings settings;
        settings.mMaxSlopeAngle       = JPH::DegreesToRadians(desc.slope_angle);
        settings.mShape               = shape_result.Get();
        settings.mMass                = desc.mass;
        settings.mMaxStrength         = desc.max_strength;
        if (desc.inner_body_layer >= 0) {
                // Presence body: the ctor adds it to the broadphase with
                // mAllowSleeping off so sensor contacts reach the contact
                // listener, and packs inUserData onto it — the same
                // (mask<<32)|layer encoding regular bodies carry, so the
                // listener's mask gating treats it like any body. The dtor
                // removes/destroys it; no manual release needed.
                settings.mInnerBodyShape = shape_result.Get();
                settings.mInnerBodyLayer = JPH::ObjectLayer(desc.inner_body_layer);
        }

        auto* pChar = new JPH::CharacterVirtual(
                &settings,
                ToJoltR(desc.vInitialPosition),
                ToJolt(desc.qInitialRotation),
                (static_cast<uint64_t>(desc.collision_mask) << 32)
                        | static_cast<uint64_t>(desc.collision_layer),
                pImpl->pPhysics.get());

        CharHandle h{ pImpl->next_char_id++ };

        Impl::CharState cs;
        cs.pChar       = pChar;
        cs.step_height = desc.step_height;
        cs.bCollidesKinematic = desc.collides_kinematic;
        cs.update_divider = std::max(1u, desc.update_divider);
        // Phase from the monotonic id: sizing from the live-map count drifts
        // its parity as despawns shrink it, clustering phases after churn.
        cs.update_phase   = uint32_t(h.id) % cs.update_divider;
        cs.inner_body_id  = pChar->GetInnerBodyID();
        pImpl->mCharacters.emplace(h.id, std::move(cs));

        return h;
}

void PhysicsSystem::DestroyCharacter(CharHandle h) {
        if (!h.IsValid()) return;
        auto it = pImpl->mCharacters.find(h.id);
        if (it == pImpl->mCharacters.end()) return;
        if (!it->second.inner_body_id.IsInvalid())
                pImpl->mInnerBodyNodes.erase(it->second.inner_body_id.GetIndex());
        pImpl->mCharacters.erase(it);
}

void PhysicsSystem::RegisterCharacterNode(CharHandle h, void* pNode) {
        auto it = pImpl->mCharacters.find(h.id);
        if (it == pImpl->mCharacters.end()) return;

        it->second.pNode = pNode;

        // presence body → node: sensors report the inner body as the event's
        // hOther; bindings resolve the actor through it (bell noise_pos etc.)
        if (!it->second.inner_body_id.IsInvalid())
                pImpl->mInnerBodyNodes[it->second.inner_body_id.GetIndex()] =
                        Impl::InnerBodyRef{ uint32_t(it->second.inner_body_id.GetSequenceNumber()), pNode };
}

void PhysicsSystem::UnregisterCharacterNode(CharHandle h) {
        auto it = pImpl->mCharacters.find(h.id);
        if (it == pImpl->mCharacters.end()) return;

        if (!it->second.inner_body_id.IsInvalid())
                pImpl->mInnerBodyNodes.erase(it->second.inner_body_id.GetIndex());
        it->second.pNode = nullptr;
}

void PhysicsSystem::SetCharacterVelocity(CharHandle h, glm::vec3 vel) {
        auto it = pImpl->mCharacters.find(h.id);
        if (it != pImpl->mCharacters.end())
                it->second.vDesiredVelocity = vel;
}

bool PhysicsSystem::IsCharacterGrounded(CharHandle h) const {
        auto it = pImpl->mCharacters.find(h.id);
        if (it == pImpl->mCharacters.end()) return false;
        return it->second.pChar->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
}

glm::vec3 PhysicsSystem::GetCharacterFloorNormal(CharHandle h) const {
        auto it = pImpl->mCharacters.find(h.id);
        if (it == pImpl->mCharacters.end()) return {0.f, 1.f, 0.f};
        return ToGlm(it->second.pChar->GetGroundNormal());
}

glm::vec3 PhysicsSystem::GetCharacterGroundVelocity(CharHandle h) const {
        auto it = pImpl->mCharacters.find(h.id);
        if (it == pImpl->mCharacters.end()) return {0.f, 0.f, 0.f};
        return ToGlm(it->second.pChar->GetGroundVelocity());
}

void PhysicsSystem::TeleportCharacter(CharHandle h, glm::vec3 pos, glm::quat rot) {
        auto it = pImpl->mCharacters.find(h.id);
        if (it == pImpl->mCharacters.end()) return;
        it->second.pChar->SetPosition(ToJoltR(pos));
        it->second.pChar->SetRotation(ToJolt(rot));
}

glm::vec3 PhysicsSystem::GetGravity() const {
        if (!pImpl->initialized) return {0.f, -9.81f, 0.f};
        return ToGlm(pImpl->pPhysics->GetGravity());
}

} // namespace SE

#endif // SE_IMPL
