
#ifndef SE_TEST_PHYSICS_HARNESS_H
#define SE_TEST_PHYSICS_HARNESS_H

// ---------------------------------------------------------------------------
// PhysicsHarness — fixture + helpers for the physics functional suites.
//
// Composition: a REAL PhysicsSystem over the real Jolt runtime, plus a real
// TSceneTree for observability. Bodies created through MakeBody are
// node-registered, so simulated transforms reach the test exclusively through
// the production path — PhysicsSystem::Interpolate() writing scene-node
// transforms — exactly how samples/physics_demo observes its ball. The system
// API deliberately has no GetPosition; tests read node transforms (or raycast).
//
// House rules (see EngineFixture.h) apply unchanged: ONE test .cpp per binary
// (GlobalTypes.h ends with the SE_IMPL blocks incl. PhysicsSystem.tcc), no
// blanket Engine::Init(), run from repo root.
//
// Jolt runtime lifecycle: PhysicsSystem::Init/Shutdown pair the PROCESS-GLOBAL
// Jolt Factory (RegisterTypes/UnregisterTypes). Exactly ONE PhysicsSystem may
// be initialized at any moment — the per-test instance of PhysTestBase OR the
// engine singleton of EnginePhysTestBase, never both. Both fixtures therefore
// Init in SetUp and Shutdown in TearDown; a fresh factory per test also gives
// cross-test isolation for free.
// ---------------------------------------------------------------------------

#include <PhysicsTypes.h>
#include <PhysicsEvents.h>

#include "EngineFixture.h"
#include "LogCapture.h"

#include <algorithm>
#include <string>
#include <vector>

namespace se_test {

// Identity rotation for Teleport/MoveKinematic calls. NEVER pass glm::quat{}
// here: value-initialization zeroes all four components, and Jolt asserts
// (Debug) on the non-normalized quaternion (JPH_ASSERT(IsNormalized())).
inline const glm::quat kIdentityQuat{1.0f, 0.0f, 0.0f, 0.0f};

// ---- event sinks ------------------------------------------------------------

// RAII EventManager listener recording every event of E delivered while alive.
// EPhysics* events are TriggerEvent'ed synchronously inside PhysicsSystem::Update
// (core/PhysicsSystem.tcc DrainContacts) — sinks are populated the moment Update
// returns, no EventManager::Process() needed. Queued events (e.g. ETriggerFired
// from the TriggerVolume component) DO require Process() — the EnginePhysTestBase
// step helpers include it, mirroring common/application.tcc.
template <class E>
struct EventSink {

        std::vector<E> hits;

        EventSink() {
                SE::GetSystem<SE::EventManager>().template AddListener<E, &EventSink::On>(this);
        }
        ~EventSink() {
                SE::GetSystem<SE::EventManager>().template RemoveListener<E, &EventSink::On>(this);
        }

        EventSink(const EventSink&) = delete;
        EventSink& operator=(const EventSink&) = delete;

        void On(const SE::Event& e) { hits.push_back(e.template Get<E>()); }
        void Clear()                { hits.clear(); }
        size_t Size() const         { return hits.size(); }
};

// Contact pairs are reported in Jolt's order — compare unordered.
inline bool SamePair(SE::BodyHandle a1, SE::BodyHandle b1,
                     SE::BodyHandle a2, SE::BodyHandle b2) {
        return (a1 == a2 && b1 == b2) || (a1 == b2 && b1 == a2);
}

// ---- fixtures ---------------------------------------------------------------

// Tier C fixture: per-test PhysicsSystem + per-test TSceneTree.
class PhysTestBase : public FuncTestBase {

protected:
        SE::PhysicsConfig                   cfg;      // default 60 Hz; tests may Shutdown+re-Init
        SE::PhysicsSystem                   phys;     // fresh Jolt runtime per test
        std::unique_ptr<SE::TSceneTree>     scene;

        void SetUp() override {
                FuncTestBase::SetUp();
                scene = std::make_unique<SE::TSceneTree>("phys_test_scene", 0, true);
                phys.Init(cfg);
        }

        void TearDown() override {
                phys.Shutdown();
                scene.reset();
                FuncTestBase::TearDown();
        }

        // ---- body + node builders ---------------------------------------

        struct BodyRef {
                SE::BodyHandle               handle;
                SE::TSceneTree::TSceneNodeExact* node = nullptr;
        };

        // Create a scene node at the body's initial transform and register it,
        // so Interpolate() syncs the simulated transform into the node.
        BodyRef MakeBody(const SE::RigidBodyDesc& desc, std::string_view name = {}) {

                auto pNode = scene->Create(name.empty()
                                ? "body_" + std::to_string(++node_counter)
                                : std::string(name));
                pNode->SetPos(desc.vInitialPosition);
                pNode->SetRotation(desc.qInitialRotation);

                BodyRef ref;
                ref.handle = phys.CreateRigidBody(desc);
                if (ref.handle.IsValid()) {
                        phys.RegisterNode(ref.handle, pNode.get());
                }
                ref.node = pNode.get();
                return ref;
        }

        BodyRef MakeStaticBox(glm::vec3 center, glm::vec3 half_extents, std::string_view name = {}) {
                SE::RigidBodyDesc d;
                d.oCollider.type            = SE::ColliderDesc::Box;
                d.oCollider.vHalfExtents    = half_extents;
                d.vInitialPosition          = center;
                d.is_static                 = true;
                return MakeBody(d, name.empty() ? "static_" + std::to_string(++node_counter) : name);
        }

        BodyRef MakeSphere(glm::vec3 center, float radius = 0.5f, std::string_view name = {}) {
                SE::RigidBodyDesc d;
                d.oCollider.type   = SE::ColliderDesc::Sphere;
                d.oCollider.radius = radius;
                d.vInitialPosition = center;
                return MakeBody(d, name.empty() ? "sphere_" + std::to_string(++node_counter) : name);
        }

        // ---- frame drivers -----------------------------------------------

        // Exactly n fixed steps + node sync. StepOnce sets alpha = 1, so after
        // this nodes hold the exact post-step transforms — the deterministic
        // unit of physics time for assertions.
        void StepExact(uint32_t n = 1) {
                for (uint32_t i = 0; i < n; ++i) {
                        phys.StepOnce();
                        phys.Interpolate();
                }
        }

        // One game frame in the real loop's phase order
        // (common/application.tcc Run): EUpdate → physics accumulator →
        // queued-event Process → EPostUpdate → interpolated node sync →
        // frame allocator reset.
        void StepFrame(float game_dt) {
                Events().TriggerEvent(SE::EUpdate{game_dt});
                phys.Update(game_dt);
                Events().Process();
                Events().TriggerEvent(SE::EPostUpdate{game_dt});
                phys.Interpolate();
                Alloc().reset();
        }

        void RunSeconds(float seconds) {
                const float frame_dt = 1.0f / 60.0f;
                const uint32_t n = static_cast<uint32_t>(seconds / frame_dt + 0.5f);
                for (uint32_t i = 0; i < n; ++i) StepFrame(frame_dt);
        }

        // ---- character builders -------------------------------------------

        struct CharRef {
                SE::CharHandle                     handle;
                SE::TSceneTree::TSceneNodeExact*   node = nullptr;
        };

        // Create a CharacterVirtual with a registered scene node (character
        // nodes sync unconditionally in Interpolate — no prev/curr blending,
        // the controller owns the velocity).
        CharRef MakeCharacter(glm::vec3 pos,
                              float radius = 0.3f, float half_height = 0.9f,
                              float step_height = 0.3f, float slope_deg = 50.0f) {
                SE::CharacterDesc d;
                d.vInitialPosition = pos;
                d.radius           = radius;
                d.half_height      = half_height;
                d.step_height      = step_height;
                d.slope_angle      = slope_deg;

                CharRef ref;
                ref.handle = phys.CreateCharacter(d);
                if (ref.handle.IsValid()) {
                        ref.node = scene->Create("char_" + std::to_string(ref.handle.id)).get();
                        phys.RegisterCharacterNode(ref.handle, ref.node);
                }
                return ref;
        }

        // Center height above the surface for a capsule resting on it:
        // radius + half_height (cylindrical section).
        static float CapsuleLift(float radius = 0.3f, float half_height = 0.9f) {
                return radius + half_height;
        }

        static glm::vec3 CharPos(const CharRef& c) {
                return c.node->GetTransform().GetWorldPos();
        }
        static float CharY(const CharRef& c) { return CharPos(c).y; }

        // ---- node reads (the sanctioned observability path) ---------------

        static glm::vec3 Pos(const BodyRef& b) { return b.node->GetTransform().GetWorldPos(); }

        static glm::quat Quat(const BodyRef& b) {
                auto [pos, rot, scale] = b.node->GetTransform().GetWorldDecomposedQuat();
                (void)pos; (void)scale;
                return rot;
        }

        static float Y(const BodyRef& b) { return Pos(b).y; }

        // Highest node position reached over the next n steps (apex finder).
        template <typename Ref>
        float ApexY(const Ref& b, uint32_t n) {
                float best = Y(b);
                for (uint32_t i = 0; i < n; ++i) {
                        StepExact();
                        best = std::max(best, Y(b));
                }
                return best;
        }

        uint32_t node_counter = 0;
};

// Variant for component-level suites (RigidBody, TriggerVolume): those
// components construct their bodies via GetSystem<PhysicsSystem>() — the
// engine composition root — so the singleton system is initialized here
// instead of a test-local instance.
class EnginePhysTestBase : public FuncTestBase {

protected:
        std::unique_ptr<SE::TSceneTree> scene;

        void SetUp() override {
                FuncTestBase::SetUp();
                static bool system_constructed = []() {
                        SE::TEngine::Instance().Init<SE::PhysicsSystem>();
                        return true;
                }();
                (void)system_constructed;

                auto& oPhys = SE::GetSystem<SE::PhysicsSystem>();
                if (!oPhys.IsInitialized()) {
                        oPhys.Init(SE::PhysicsConfig{});
                }
                scene = std::make_unique<SE::TSceneTree>("engine_phys_test_scene", 0, true);
        }

        void TearDown() override {
                SE::GetSystem<SE::PhysicsSystem>().Shutdown();
                scene.reset();
                FuncTestBase::TearDown();
        }

        void StepFrame(float game_dt) {
                Events().TriggerEvent(SE::EUpdate{game_dt});
                SE::GetSystem<SE::PhysicsSystem>().Update(game_dt);
                Events().Process();
                Events().TriggerEvent(SE::EPostUpdate{game_dt});
                SE::GetSystem<SE::PhysicsSystem>().Interpolate();
                Alloc().reset();
        }

        void RunSeconds(float seconds) {
                const float frame_dt = 1.0f / 60.0f;
                const uint32_t n = static_cast<uint32_t>(seconds / frame_dt + 0.5f);
                for (uint32_t i = 0; i < n; ++i) StepFrame(frame_dt);
        }
};

// ---- geometry helpers ---------------------------------------------------------

// Floor box whose TOP surface sits exactly at y = top_y.
inline SE::RigidBodyDesc FloorDesc(float top_y = 0.0f, glm::vec3 half = {20.0f, 0.5f, 20.0f}) {
        SE::RigidBodyDesc d;
        d.oCollider.type         = SE::ColliderDesc::Box;
        d.oCollider.vHalfExtents = half;
        d.vInitialPosition       = {0.0f, top_y - half.y, 0.0f};
        d.is_static              = true;
        d.friction               = 0.5f;
        return d;
}

// Two-triangle horizontal mesh quad covering [-s..s]² at height y.
inline void MeshQuad(float s, float y,
                     std::vector<glm::vec3>& verts, std::vector<uint32_t>& idx) {
        verts = {{-s, y, -s}, {s, y, -s}, {s, y, s}, {-s, y, s}};
        idx   = {0, 2, 1,  0, 3, 2};
}

} // namespace se_test

#endif // SE_TEST_PHYSICS_HARNESS_H
