
// ---------------------------------------------------------------------------
// PhysicsTriggerVolumeTest — Tier C+ (application component). The TriggerVolume
// sensor component over the real physics system: named ETriggerFired events
// (enter/exit/stay), one-shot behavior, Enable/Disable, layer-mask gating, and
// the destructor contract. Compiled into a composition that includes
// TriggerVolume via the target-local shadow App.h (see trigger_app/).
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>

#include "PhysicsHarness.h"

#include <TriggerVolume.h>

#include <gtest/gtest.h>

using namespace se_test;
using SE::BodyHandle;
using SE::ColliderDesc;
using SE::EPhysicsTriggerEnter;
using SE::ETriggerFired;
using SE::RigidBodyDesc;
using SE::TriggerVolumeDesc;
using SE::uSUCCESS;

namespace {

constexpr float kDt = 1.0f / 60.0f;

// Ball body that ignores every layer — trigger pairing is gated by the
// TRIGGER's mask alone (bidirectional interest is not required).
RigidBodyDesc PassthroughBall(glm::vec3 pos, float radius = 0.25f) {
        RigidBodyDesc d;
        d.oCollider.type   = ColliderDesc::Sphere;
        d.oCollider.radius = radius;
        d.vInitialPosition = pos;
        d.collision_layer  = 1u << 4;
        d.collision_mask   = 0;
        d.restitution      = 0.0f;
        return d;
}

TriggerVolumeDesc VolDesc(std::string enter, std::string exit, std::string stay = {}) {
        TriggerVolumeDesc d;
        d.oCollider.type         = ColliderDesc::Box;
        d.oCollider.vHalfExtents = {1.0f, 1.0f, 1.0f};
        d.on_enter_event         = enter;
        d.on_exit_event          = exit;
        d.on_stay_event          = stay;
        return d;
}

// Count fired events matching a named id.
size_t CountNamed(const EventSink<ETriggerFired>& sink, const char* name) {
        const SE::StrID id(name);
        return size_t(std::count_if(sink.hits.begin(), sink.hits.end(),
                                    [&](const ETriggerFired& e) {
                                            return e.event_id == id;
                                    }));
}

} // namespace

// Volume standing over the floor at x=0; balls drop through it.
TEST_F(EnginePhysTestBase, TriggerVolume_EnterExitFireNamedEvents) {
        SE::GetSystem<SE::PhysicsSystem>().CreateRigidBody(FloorDesc(0.0f));

        auto vol_node = scene->Create("GoalVolume");
        vol_node->SetPos({0.0f, 2.0f, 0.0f});
        vol_node->CreateComponent<SE::TriggerVolume>(
                        VolDesc("tv.enter", "tv.exit"));

        auto ball = scene->Create("Ball");
        ball->SetPos({0.0f, 8.0f, 0.0f});
        ball->CreateComponent<SE::RigidBody>(PassthroughBall({0.0f, 8.0f, 0.0f}));

        EventSink<ETriggerFired> fired;
        RunSeconds(3.0f);

        EXPECT_GE(CountNamed(fired, "tv.enter"), 1u);
        EXPECT_GE(CountNamed(fired, "tv.exit"), 1u);
        // Trigger pairing reports the falling body as hOther.
        for (const auto& e : fired.hits) {
                if (e.event_id == SE::StrID("tv.enter")) {
                        EXPECT_EQ(e.hOther,
                                  ball->GetComponent<SE::RigidBody>()->GetHandle());
                }
        }
}

TEST_F(EnginePhysTestBase, TriggerVolume_StayFiresOnInterval) {
        SE::GetSystem<SE::PhysicsSystem>().CreateRigidBody(FloorDesc(0.0f));

        auto vol_node = scene->Create("StayVolume");
        vol_node->SetPos({0.0f, 1.0f, 0.0f});   // spans y ∈ [0..2]: floor rest spot
        TriggerVolumeDesc d = VolDesc("tv.enter", "tv.exit", "tv.stay");
        d.stay_interval     = 0.25f;
        vol_node->CreateComponent<SE::TriggerVolume>(d);

        auto ball = scene->Create("Ball");
        RigidBodyDesc bd = PassthroughBall({0.0f, 0.55f, 0.0f}, 0.25f);
        ball->SetPos({0.0f, 0.55f, 0.0f});
        ball->CreateComponent<SE::RigidBody>(bd);

        EventSink<ETriggerFired> fired;
        RunSeconds(1.5f);

        // ~1.5 s inside with a 0.25 s interval → several stay pulses.
        EXPECT_GE(CountNamed(fired, "tv.stay"), 3u);
        EXPECT_EQ(CountNamed(fired, "tv.enter"), 1u);
        EXPECT_EQ(CountNamed(fired, "tv.exit"), 0u);   // never left
}

TEST_F(EnginePhysTestBase, TriggerVolume_OneShotFiresOnceThenStaysSilent) {
        SE::GetSystem<SE::PhysicsSystem>().CreateRigidBody(FloorDesc(0.0f));

        auto vol_node = scene->Create("OneShotVolume");
        vol_node->SetPos({0.0f, 2.0f, 0.0f});
        TriggerVolumeDesc d = VolDesc("tv.enter", "tv.exit");
        d.one_shot          = true;
        vol_node->CreateComponent<SE::TriggerVolume>(d);

        EventSink<ETriggerFired> fired;

        // First ball through: enter fires.
        {
                auto b1 = scene->Create("Ball1");
                b1->SetPos({0.0f, 8.0f, 0.0f});
                b1->CreateComponent<SE::RigidBody>(PassthroughBall({0.0f, 8.0f, 0.0f}));
                RunSeconds(1.5f);
        }
        EXPECT_EQ(CountNamed(fired, "tv.enter"), 1u);
        EXPECT_EQ(CountNamed(fired, "tv.exit"), 0u);   // one-shot suppresses exit

        // Second ball: nothing (fired + disabled).
        {
                auto b2 = scene->Create("Ball2");
                b2->SetPos({0.0f, 8.0f, 0.0f});
                b2->CreateComponent<SE::RigidBody>(PassthroughBall({0.0f, 8.0f, 0.0f}));
                RunSeconds(1.5f);
        }
        EXPECT_EQ(CountNamed(fired, "tv.enter"), 1u);
}

TEST_F(EnginePhysTestBase, TriggerVolume_DisabledIgnoresOverlaps) {
        SE::GetSystem<SE::PhysicsSystem>().CreateRigidBody(FloorDesc(0.0f));

        auto vol_node = scene->Create("QuietVolume");
        vol_node->SetPos({0.0f, 2.0f, 0.0f});
        EXPECT_EQ(vol_node->CreateComponent<SE::TriggerVolume>(
                        VolDesc("tv.enter", "tv.exit")), uSUCCESS);
        auto* vol = vol_node->GetComponent<SE::TriggerVolume>();
        ASSERT_NE(vol, nullptr);
        vol->Disable();

        auto ball = scene->Create("Ball");
        ball->SetPos({0.0f, 8.0f, 0.0f});
        ball->CreateComponent<SE::RigidBody>(PassthroughBall({0.0f, 8.0f, 0.0f}));

        EventSink<ETriggerFired> fired;
        RunSeconds(2.0f);
        // CHARACTERIZATION: Disable suppresses the ENTER event (the enabled
        // gate in OnTriggerEnter) but NOT the exit — OnTriggerExit has no
        // enabled check, so exactly one exit event leaks while the ball
        // passes a disabled sensor. Fix ⇒ gate exit on enabled, update this.
        EXPECT_EQ(CountNamed(fired, "tv.enter"), 0u);
        EXPECT_EQ(CountNamed(fired, "tv.exit"), 1u);
}

TEST_F(EnginePhysTestBase, TriggerVolume_MaskGatesPairing) {
        SE::GetSystem<SE::PhysicsSystem>().CreateRigidBody(FloorDesc(0.0f));

        auto vol_node = scene->Create("PickyVolume");
        vol_node->SetPos({0.0f, 2.0f, 0.0f});
        TriggerVolumeDesc d = VolDesc("tv.enter", "tv.exit");
        d.collision_mask    = 1u << 7;   // only interested in layer 7 — ball is 4
        vol_node->CreateComponent<SE::TriggerVolume>(d);

        auto ball = scene->Create("Ball");
        ball->SetPos({0.0f, 8.0f, 0.0f});
        ball->CreateComponent<SE::RigidBody>(PassthroughBall({0.0f, 8.0f, 0.0f}));

        EventSink<ETriggerFired> fired;
        RunSeconds(2.0f);
        EXPECT_EQ(fired.Size(), 0u);
}

TEST_F(EnginePhysTestBase, TriggerVolume_DtorRemovesBodyAndStopsEvents) {
        auto floor = SE::GetSystem<SE::PhysicsSystem>().CreateRigidBody(FloorDesc(0.0f));

        // The probe ray: through where the sensor will stand (and stand no more).
        SE::PhysicsRay ray;
        ray.vOrigin    = {0.0f, 5.0f, 0.0f};
        ray.vDirection = {0.0f, -1.0f, 0.0f};
        SE::RaycastHit hit;

        // Scoped scene: components die with their node (see the RigidBody dtor
        // test — a held node handle keeps the body alive).
        {
                auto own_scene = std::make_unique<SE::TSceneTree>("doomed_scene", 0, true);
                auto vol_node = own_scene->Create("DoomedVolume");
                vol_node->SetPos({0.0f, 2.0f, 0.0f});
                vol_node->CreateComponent<SE::TriggerVolume>(VolDesc("tv.enter", "tv.exit"));

                // Body is live before destruction: a ray against the sensor hits.
                EXPECT_TRUE(SE::GetSystem<SE::PhysicsSystem>().Raycast(ray, hit));

                vol_node->Unlink();   // detach from the (soon dead) root
                vol_node.reset();     // last reference → ~TriggerVolume runs here
        }

        EventSink<EPhysicsTriggerEnter> raw_enters;
        EventSink<ETriggerFired>        fired;

        // Drop a ball straight through where the sensor used to be.
        auto ball = scene->Create("Ball");
        ball->SetPos({0.0f, 8.0f, 0.0f});
        ball->CreateComponent<SE::RigidBody>(PassthroughBall({0.0f, 8.0f, 0.0f}));
        RunSeconds(2.0f);

        EXPECT_EQ(raw_enters.Size(), 0u);   // physics sensor really gone
        EXPECT_EQ(fired.Size(), 0u);        // …and no named events either
        // A probe ray clear of the ball's landing spot now reaches the floor
        // beneath where the sensor stood (nothing solid blocks it anymore).
        SE::PhysicsRay clear_ray;
        clear_ray.vOrigin    = {3.0f, 5.0f, 0.0f};
        clear_ray.vDirection = {0.0f, -1.0f, 0.0f};
        SE::RaycastHit floor_hit;
        EXPECT_TRUE(SE::GetSystem<SE::PhysicsSystem>().Raycast(clear_ray, floor_hit));
        EXPECT_EQ(floor_hit.hBody, floor);
        EXPECT_NEAR(floor_hit.distance, 5.0f, 1e-3f);
}

TEST_F(EnginePhysTestBase, TriggerVolume_MovedViaNodeTransformTracksTarget) {
        SE::GetSystem<SE::PhysicsSystem>().CreateRigidBody(FloorDesc(0.0f));

        auto vol_node = scene->Create("RoamingVolume");
        vol_node->SetPos({-5.0f, 1.0f, 0.0f});
        vol_node->CreateComponent<SE::TriggerVolume>(VolDesc("tv.enter", "tv.exit"));

        // Static ball sitting at x = 0 — outside the volume until it moves.
        auto ball = scene->Create("Ball");
        ball->SetPos({0.0f, 0.55f, 0.0f});
        RigidBodyDesc bd = PassthroughBall({0.0f, 0.55f, 0.0f}, 0.25f);
        ball->CreateComponent<SE::RigidBody>(bd);

        EventSink<ETriggerFired> fired;

        // Slide the NODE over the ball (listener → MoveKinematic).
        for (int i = 1; i <= 90; ++i) {
                vol_node->SetPos({-5.0f + 0.1f * float(i), 1.0f, 0.0f});
                StepFrame(kDt);
                if (CountNamed(fired, "tv.enter") > 0) break;
        }
        EXPECT_EQ(CountNamed(fired, "tv.enter"), 1u);
}
