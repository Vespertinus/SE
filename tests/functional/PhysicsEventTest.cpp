
// ---------------------------------------------------------------------------
// PhysicsEventTest — Tier C. The EPhysics* event surface delivered through the
// engine EventManager: contact enter/exit, trigger enter/exit, collision
// layer/mask filtering (bidirectional OR for body pairs, unidirectional for
// sensors), event behavior across sleep and destruction.
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>

#include "PhysicsHarness.h"

#include <gtest/gtest.h>

using namespace se_test;
using SE::BodyHandle;
using SE::ColliderDesc;
using SE::EPhysicsContactEnter;
using SE::EPhysicsContactExit;
using SE::EPhysicsTriggerEnter;
using SE::EPhysicsTriggerExit;
using SE::RigidBodyDesc;

namespace {

constexpr float kDt = 1.0f / 60.0f;

RigidBodyDesc DynamicSphere(glm::vec3 pos, float radius = 0.5f) {
        RigidBodyDesc d;
        d.oCollider.type   = ColliderDesc::Sphere;
        d.oCollider.radius = radius;
        d.vInitialPosition = pos;
        return d;
}

RigidBodyDesc SensorBox(glm::vec3 pos, glm::vec3 half = {0.75f, 0.75f, 0.75f}) {
        RigidBodyDesc d;
        d.oCollider.type         = ColliderDesc::Box;
        d.oCollider.vHalfExtents = half;
        d.vInitialPosition       = pos;
        d.is_trigger             = true;
        d.is_kinematic           = true;   // TriggerVolume-style sensor
        return d;
}

} // namespace

// ---------------------------------------------------------------------------
// Contacts
// ---------------------------------------------------------------------------

TEST_F(PhysTestBase, ContactEnter_FiresOnceWhenBallLands) {
        auto floor = MakeBody(FloorDesc(0.0f));
        auto ball  = MakeSphere({0.0f, 3.0f, 0.0f});

        EventSink<EPhysicsContactEnter> enters;
        EventSink<EPhysicsContactExit>  exits;

        StepExact(60);

        ASSERT_EQ(enters.Size(), 1u);
        EXPECT_TRUE(SamePair(enters.hits[0].hBodyA, enters.hits[0].hBodyB,
                             ball.handle, floor.handle));
        // Impact sits on the floor plane; normal is vertical (Jolt's sign
        // convention for which body is "1" is its pair order).
        EXPECT_NEAR(std::abs(enters.hits[0].vNormal.y), 1.0f, 1e-3f);
        EXPECT_NEAR(enters.hits[0].vPoint.y, 0.0f, 0.05f);
        // Still resting (awake, inside the sleep timeout): no exit yet.
        EXPECT_EQ(exits.Size(), 0u);

        // CHARACTERIZATION: when the resting ball falls asleep (after Jolt's
        // ~0.5 s time-before-sleep), its contacts are RELEASED — a contact
        // exit fires even though nothing moved. Game code reading exit as
        // "left the ground" misfires on sleep.
        StepExact(180);
        EXPECT_EQ(exits.Size(), 1u);
        EXPECT_EQ(enters.Size(), 1u);   // no new contact while asleep
}

TEST_F(PhysTestBase, ContactPersist_DoesNotRefireEnter) {
        MakeBody(FloorDesc(0.0f));
        RigidBodyDesc d = DynamicSphere({0.0f, 0.7f, 0.0f});
        d.restitution   = 0.0f;
        auto ball       = MakeBody(d);

        EventSink<EPhysicsContactEnter> enters;
        StepExact(1);    // spawn overlaps the floor → contact this step
        StepExact(150);  // resting, Jolt fires OnContactPersisted (not surfaced)
        EXPECT_EQ(enters.Size(), 1u);
}

TEST_F(PhysTestBase, ContactExit_FiresOnSeparation) {
        auto floor = MakeBody(FloorDesc(0.0f));
        RigidBodyDesc d = DynamicSphere({0.0f, 0.7f, 0.0f});
        d.restitution   = 0.0f;
        auto ball       = MakeBody(d);

        EventSink<EPhysicsContactEnter> enters;
        EventSink<EPhysicsContactExit>  exits;

        StepExact(60);
        ASSERT_EQ(enters.Size(), 1u);
        ASSERT_EQ(exits.Size(), 0u);

        // Teleport away → separation is registered at the next step.
        phys.Teleport(ball.handle, {0.0f, 20.0f, 0.0f}, se_test::kIdentityQuat);
        StepExact();
        EXPECT_EQ(exits.Size(), 1u);
        EXPECT_TRUE(SamePair(exits.hits[0].hBodyA, exits.hits[0].hBodyB,
                             ball.handle, floor.handle));
}

// CHARACTERIZATION: destroying the ball flushes its contact as a CONTACT exit,
// not a trigger exit — the destroyed body's state row (the is_trigger flag
// source) is erased before the step runs, so DrainContacts can no longer tell
// the pair was trigger-involved.
TEST_F(PhysTestBase, DestroyTriggerPartner_ReportsContactExit) {
        auto floor = MakeBody(FloorDesc(0.0f));
        auto ball  = MakeSphere({0.0f, 0.7f, 0.0f});
        StepExact(60);

        EventSink<EPhysicsContactExit>  contact_exits;
        EventSink<EPhysicsTriggerExit>  trigger_exits;

        phys.DestroyBody(ball.handle);
        StepExact();

        EXPECT_EQ(contact_exits.Size(), 1u);
        EXPECT_EQ(trigger_exits.Size(), 0u);
        EXPECT_TRUE(SamePair(contact_exits.hits[0].hBodyA, contact_exits.hits[0].hBodyB,
                             ball.handle, floor.handle));
}

// ---------------------------------------------------------------------------
// Triggers (sensor bodies)
// ---------------------------------------------------------------------------

TEST_F(PhysTestBase, TriggerEnterExit_FireAsBallPassesThrough) {
        auto sensor = MakeBody(SensorBox({0.0f, 4.0f, 0.0f}));
        auto ball   = MakeSphere({0.0f, 12.0f, 0.0f});

        EventSink<EPhysicsTriggerEnter> enters;
        EventSink<EPhysicsTriggerExit>  exits;
        EventSink<EPhysicsContactEnter> contact_enters;   // must stay silent

        StepExact(240);

        ASSERT_EQ(enters.Size(), 1u);
        EXPECT_EQ(enters.hits[0].hTrigger, sensor.handle);
        EXPECT_EQ(enters.hits[0].hOther, ball.handle);

        ASSERT_EQ(exits.Size(), 1u);
        EXPECT_EQ(exits.hits[0].hTrigger, sensor.handle);
        EXPECT_EQ(exits.hits[0].hOther, ball.handle);

        // Sensor: no contact response — the ball fell straight through.
        EXPECT_EQ(contact_enters.Size(), 0u);
        EXPECT_LT(Y(ball), 2.0f);
}

TEST_F(PhysTestBase, Trigger_TwoBallsProducePairwiseEvents) {
        auto sensor = MakeBody(SensorBox({0.0f, 4.0f, 0.0f}));
        auto a      = MakeSphere({0.0f, 12.0f, 0.0f});
        auto b      = MakeSphere({0.2f, 14.0f, 0.0f});

        EventSink<EPhysicsTriggerEnter> enters;
        EventSink<EPhysicsTriggerExit>  exits;

        StepExact(300);
        ASSERT_EQ(enters.Size(), 2u);
        ASSERT_EQ(exits.Size(), 2u);
        for (const auto& e : enters.hits) {
                EXPECT_EQ(e.hTrigger, sensor.handle);
                EXPECT_TRUE(e.hOther == a.handle || e.hOther == b.handle);
        }
        for (const auto& e : exits.hits) {
                EXPECT_EQ(e.hTrigger, sensor.handle);
                EXPECT_TRUE(e.hOther == a.handle || e.hOther == b.handle);
        }
}

// ---------------------------------------------------------------------------
// Layer/mask filtering. User data packs layer | mask<<32 per body:
//   body–body pairs collide if EITHER side's mask includes the other's layer;
//   trigger pairs fire if the TRIGGER's mask includes the other body's layer.
// ---------------------------------------------------------------------------

TEST_F(PhysTestBase, BodyFilter_BidirectionalOrDetectsOneSidedInterest) {
        // A (layer 1) ignores everything (mask 0); B (layer 2) looks for layer 1.
        // Either side detecting the other must produce a solid contact.
        RigidBodyDesc da = DynamicSphere({0.0f, 3.0f, 0.0f});
        da.collision_layer = 1u << 0;
        da.collision_mask  = 0;
        da.restitution     = 0.0f;
        auto a = MakeBody(da);

        RigidBodyDesc db = FloorDesc(0.0f);
        db.collision_layer = 1u << 1;
        db.collision_mask  = 1u << 0;   // interested in A
        auto b = MakeBody(db);

        EventSink<EPhysicsContactEnter> enters;
        StepExact(120);
        EXPECT_EQ(enters.Size(), 1u);
        EXPECT_NEAR(Y(a), 0.5f, 0.05f);   // B caught A
}

TEST_F(PhysTestBase, BodyFilter_MutualBlindnessPassesThrough) {
        // Both sides use disjoint layers and mask nothing → no collision, no
        // events; the ball falls straight through the "floor".
        RigidBodyDesc da = DynamicSphere({0.0f, 3.0f, 0.0f});
        da.collision_layer = 1u << 0;
        da.collision_mask  = 0;
        auto a = MakeBody(da);

        RigidBodyDesc db = FloorDesc(0.0f);
        db.collision_layer = 1u << 1;
        db.collision_mask  = 0;
        auto b = MakeBody(db);

        EventSink<EPhysicsContactEnter> enters;
        StepExact(180);
        EXPECT_EQ(enters.Size(), 0u);
        EXPECT_LT(Y(a), -3.0f);   // fell through
}

TEST_F(PhysTestBase, TriggerFilter_Unidirectional_MaskIgnoredOnOtherBody) {
        // Ball subscribes to nothing (mask 0) yet the trigger still fires:
        // only the trigger's mask gates the pair.
        RigidBodyDesc dt = SensorBox({0.0f, 4.0f, 0.0f});
        dt.collision_layer = 1u << 5;
        dt.collision_mask  = 1u << 0;    // interested in balls
        auto sensor = MakeBody(dt);

        RigidBodyDesc da = DynamicSphere({0.0f, 12.0f, 0.0f});
        da.collision_layer = 1u << 0;
        da.collision_mask  = 0;          // ball ignores triggers
        auto ball = MakeBody(da);

        EventSink<EPhysicsTriggerEnter> enters;
        StepExact(240);
        EXPECT_EQ(enters.Size(), 1u);
}

TEST_F(PhysTestBase, TriggerFilter_TriggerMaskRejectsLayer) {
        RigidBodyDesc dt = SensorBox({0.0f, 4.0f, 0.0f});
        dt.collision_layer = 1u << 5;
        dt.collision_mask  = 1u << 2;    // NOT interested in balls (layer 1<<0)
        auto sensor = MakeBody(dt);

        RigidBodyDesc da = DynamicSphere({0.0f, 12.0f, 0.0f});
        da.collision_layer = 1u << 0;
        da.collision_mask  = 0xFFFFFFFFu; // ball would love to see the trigger
        auto ball = MakeBody(da);

        EventSink<EPhysicsTriggerEnter> enters;
        StepExact(240);
        EXPECT_EQ(enters.Size(), 0u);
        EXPECT_LT(Y(ball), 2.0f);   // and it fell straight through
}

// ---------------------------------------------------------------------------
// Sleep interaction
// ---------------------------------------------------------------------------

TEST_F(PhysTestBase, SleepingBall_SleepReleasesContactAndWakeRefires) {
        MakeBody(FloorDesc(0.0f));
        RigidBodyDesc d = DynamicSphere({0.0f, 0.7f, 0.0f});
        d.restitution   = 0.0f;
        auto ball       = MakeBody(d);

        EventSink<EPhysicsContactEnter> enters;
        EventSink<EPhysicsContactExit>  exits;

        // Landed and asleep: the sleep release already fired the exit.
        StepExact(180);
        ASSERT_EQ(enters.Size(), 1u);
        ASSERT_EQ(exits.Size(), 1u);

        phys.ApplyImpulse(ball.handle, {0.5f, 0.0f, 0.0f});   // wake + small slide
        StepExact(60);
        // Waking and re-contacting re-fires enter (the pair was released on
        // sleep); the slide never separates, so no second exit.
        EXPECT_EQ(enters.Size(), 2u);
        EXPECT_EQ(exits.Size(), 1u);
        EXPECT_GT(Pos(ball).x, 0.005f);   // the impulse did act on the body
}
