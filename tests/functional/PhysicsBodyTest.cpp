
// ---------------------------------------------------------------------------
// PhysicsBodyTest — Tier C. Rigid body lifecycle and dynamics of the real
// PhysicsSystem (Jolt): collider shapes, gravity integration, material
// properties, deferred command semantics, pause/step, activation/sleep,
// handle generation. Observability is the production path: registered scene
// nodes synced by PhysicsSystem::Interpolate (see PhysicsHarness.h).
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>

#include "PhysicsHarness.h"

#include <gtest/gtest.h>

using namespace se_test;
using SE::BodyHandle;
using SE::ColliderDesc;
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

} // namespace

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

TEST_F(PhysTestBase, Init_ShutdownLifecycleAndGravity) {
        ASSERT_TRUE(phys.IsInitialized());
        EXPECT_EQ(phys.GetGravity(), glm::vec3(0.0f, -9.81f, 0.0f));

        phys.Shutdown();
        EXPECT_FALSE(phys.IsInitialized());
        // GetGravity falls back to the default vector when not initialized.
        EXPECT_EQ(phys.GetGravity(), glm::vec3(0.0f, -9.81f, 0.0f));

        phys.Init(SE::PhysicsConfig{});
        EXPECT_TRUE(phys.IsInitialized());
}

TEST_F(PhysTestBase, CreateRigidBody_AllColliderTypesProduceDistinctValidHandles) {
        RigidBodyDesc box;
        box.oCollider.type = ColliderDesc::Box;
        auto box_ref       = MakeBody(box);

        auto sphere = MakeSphere({3.0f, 0.0f, 0.0f});

        RigidBodyDesc capsule;
        capsule.oCollider.type        = ColliderDesc::Capsule;
        capsule.oCollider.radius      = 0.4f;
        capsule.oCollider.half_height = 0.6f;
        capsule.vInitialPosition      = {6.0f, 0.0f, 0.0f};
        auto capsule_ref              = MakeBody(capsule);

        std::vector<glm::vec3> verts;
        std::vector<uint32_t>  idx;
        MeshQuad(5.0f, -2.0f, verts, idx);
        RigidBodyDesc mesh;
        mesh.oCollider.type          = ColliderDesc::Mesh;
        mesh.oCollider.vMeshVertices = verts;
        mesh.oCollider.vMeshIndices  = idx;
        mesh.is_static               = true;
        mesh.vInitialPosition        = {0.0f, 0.0f, -50.0f};
        auto mesh_ref                = MakeBody(mesh);

        ASSERT_TRUE(box_ref.handle.IsValid());
        ASSERT_TRUE(sphere.handle.IsValid());
        ASSERT_TRUE(capsule_ref.handle.IsValid());
        ASSERT_TRUE(mesh_ref.handle.IsValid());
        EXPECT_NE(box_ref.handle, sphere.handle);
        EXPECT_NE(sphere.handle, capsule_ref.handle);
        EXPECT_NE(capsule_ref.handle, mesh_ref.handle);
}

TEST_F(PhysTestBase, CreateRigidBody_LogsErrorAndReturnsInvalidHandleAtBodyLimit) {
        phys.Shutdown();
        cfg.max_bodies = 4;
        phys.Init(cfg);

        LogCapture logs;
        std::vector<BodyHandle> handles;
        for (int i = 0; i < 5; ++i) {
                auto ref = MakeSphere({0.0f, float(i), 0.0f});
                handles.push_back(ref.handle);
        }
        // The degradation is soft: log_e + invalid handle, engine keeps ticking.
        logs.ExpectLine("CreateBody failed");

        EXPECT_TRUE(handles[0].IsValid());
        EXPECT_TRUE(handles[3].IsValid());
        EXPECT_FALSE(handles[4].IsValid());
}

// ---------------------------------------------------------------------------
// Gravity
// ---------------------------------------------------------------------------

TEST_F(PhysTestBase, FreeFall_MatchesAnalyticDisplacement) {
        auto ball = MakeSphere({0.0f, 10.0f, 0.0f});

        // 60 steps ≈ 1 s of fall. Semi-implicit Euler lands slightly below the
        // closed-form ½·g·t²; 5% covers the discretization without hiding bugs.
        const float t = 1.0f;
        StepExact(60);
        const float expected = 10.0f - 0.5f * 9.81f * t * t;
        EXPECT_NEAR(Y(ball), expected, 0.5f);

        // Still descending.
        const float y = Y(ball);
        StepExact(1);
        EXPECT_LT(Y(ball), y);
}

TEST_F(PhysTestBase, FreeFall_GravityScaleZeroFloatsInPlace) {
        RigidBodyDesc d = DynamicSphere({0.0f, 5.0f, 0.0f});
        d.gravity_scale = 0.0f;
        auto floater    = MakeBody(d);

        StepExact(120);
        EXPECT_NEAR(Y(floater), 5.0f, 1e-4f);
}

TEST_F(PhysTestBase, FreeFall_GravityScaleScalesAcceleration) {
        auto normal = MakeSphere({0.0f, 10.0f, 0.0f});
        RigidBodyDesc d = DynamicSphere({5.0f, 10.0f, 0.0f});
        d.gravity_scale = 2.0f;
        auto double_g   = MakeBody(d);

        StepExact(30);
        const float drop_normal = 10.0f - Y(normal);
        const float drop_double = 10.0f - Y(double_g);
        EXPECT_GT(drop_normal, 0.1f);
        EXPECT_NEAR(drop_double / drop_normal, 2.0f, 0.05f);
}

TEST_F(PhysTestBase, FreeFall_MassDoesNotChangeFallRate) {
        RigidBodyDesc light = DynamicSphere({0.0f, 8.0f, 0.0f});
        light.mass          = 1.0f;
        RigidBodyDesc heavy = DynamicSphere({1.0f, 8.0f, 0.0f});
        heavy.mass          = 100.0f;

        auto l = MakeBody(light);
        auto h = MakeBody(heavy);
        StepExact(45);
        EXPECT_NEAR(Y(l), Y(h), 1e-3f);
}

// ---------------------------------------------------------------------------
// Deferred command semantics — every mutation API queues a command that the
// NEXT fixed step applies (game thread → physics step hand-off). Nothing moves
// between steps.
// ---------------------------------------------------------------------------

TEST_F(PhysTestBase, SetLinearVelocity_IsDeferredUntilNextFixedStep) {
        RigidBodyDesc d = DynamicSphere({0.0f, 5.0f, 0.0f});
        d.gravity_scale  = 0.0f;
        d.linear_damping = 0.0f;
        auto ball        = MakeBody(d);

        phys.SetLinearVelocity(ball.handle, {2.0f, 0.0f, 0.0f});
        // No step ran — the command is still queued, position untouched.
        phys.Interpolate();
        EXPECT_NEAR(Pos(ball).x, 0.0f, 1e-6f);

        phys.StepOnce();
        phys.Interpolate();
        EXPECT_NEAR(Pos(ball).x, 2.0f * kDt, 1e-5f);
}

TEST_F(PhysTestBase, ApplyImpulse_VelocityChangeEqualsImpulseOverMass) {
        RigidBodyDesc d = DynamicSphere({0.0f, 5.0f, 0.0f});
        d.gravity_scale  = 0.0f;
        d.linear_damping = 0.0f;
        d.mass           = 2.0f;
        auto ball        = MakeBody(d);

        phys.ApplyImpulse(ball.handle, {0.0f, 0.0f, 4.0f});   // Δv = 4/2 = 2 m/s
        StepExact();
        EXPECT_NEAR(Pos(ball).z, 2.0f * kDt, 1e-5f);
}

TEST_F(PhysTestBase, Teleport_MovesBodyAtNextStep) {
        auto ball = MakeSphere({0.0f, 1.0f, 0.0f});
        phys.Teleport(ball.handle, {5.0f, 8.0f, -3.0f}, se_test::kIdentityQuat);
        StepExact();
        const glm::vec3 p = Pos(ball);
        EXPECT_NEAR(p.x, 5.0f, 1e-4f);
        EXPECT_NEAR(p.z, -3.0f, 1e-4f);
        EXPECT_LE(p.y, 8.0f);   // gravity already pulled it a little
}

TEST_F(PhysTestBase, Commands_IgnoreInvalidHandles) {
        BodyHandle invalid;
        EXPECT_NO_FATAL_FAILURE(phys.SetLinearVelocity(invalid, {1, 1, 1}));
        EXPECT_NO_FATAL_FAILURE(phys.ApplyImpulse(invalid, {1, 1, 1}));
        EXPECT_NO_FATAL_FAILURE(phys.Teleport(invalid, {1, 1, 1}, {}));
        EXPECT_NO_FATAL_FAILURE(phys.MoveKinematic(invalid, {1, 1, 1}, {}));
        EXPECT_NO_FATAL_FAILURE(phys.ActivateBody(invalid));
        EXPECT_NO_FATAL_FAILURE(phys.DeactivateBody(invalid));
        EXPECT_NO_FATAL_FAILURE(phys.DestroyBody(invalid));
        StepExact(2);   // flushed commands are all no-ops
}

// ---------------------------------------------------------------------------
// Material properties
// ---------------------------------------------------------------------------

TEST_F(PhysTestBase, Restitution_BallReboundsWithEnergyLoss) {
        MakeBody(FloorDesc(0.0f));

        RigidBodyDesc d = DynamicSphere({0.0f, 2.0f, 0.0f});
        d.restitution   = 0.8f;
        auto ball       = MakeBody(d);

        // Impact speed ≈ √(2·g·1.5) ≈ 5.4 m/s (above Jolt's restitution
        // threshold), rebound apex ≈ e² over the 1.5 m fall height.
        const float contact_y = 0.5f;   // sphere center resting height
        StepExact(38);                  // ≈ time to fall 1.5 m
        const float apex = ApexY(ball, 90);
        const float expected_rebound = contact_y + 0.8f * 0.8f * 1.5f;
        EXPECT_NEAR(apex, expected_rebound, 0.25f);
        EXPECT_GT(apex, contact_y + 0.5f);   // it visibly rebounded
}

TEST_F(PhysTestBase, Restitution_ZeroBallLandsDead) {
        MakeBody(FloorDesc(0.0f));
        RigidBodyDesc d = DynamicSphere({0.0f, 2.0f, 0.0f});
        d.restitution   = 0.0f;
        auto ball       = MakeBody(d);

        StepExact(120);
        EXPECT_NEAR(Y(ball), 0.5f, 0.02f);   // resting height == radius
        // …and it stays there (settled/asleep).
        const float y = Y(ball);
        StepExact(60);
        EXPECT_FLOAT_EQ(Y(ball), y);
}

TEST_F(PhysTestBase, LinearDamping_DecaysVelocity) {
        // Two identical balls sliding at 6 m/s, gravity off, no ground
        // contact — isolates linear damping.
        RigidBodyDesc fast = DynamicSphere({0.0f, 5.0f, 0.0f});
        fast.gravity_scale  = 0.0f;
        fast.linear_damping = 0.0f;
        RigidBodyDesc damped = fast;
        damped.vInitialPosition = {0.0f, 5.0f, 1.0f};
        damped.linear_damping   = 8.0f;

        auto a = MakeBody(fast);
        auto b = MakeBody(damped);
        phys.SetLinearVelocity(a.handle, {6.0f, 0.0f, 0.0f});
        phys.SetLinearVelocity(b.handle, {6.0f, 0.0f, 0.0f});

        StepExact(60);
        const float dist_free   = Pos(a).x;
        const float dist_damped = Pos(b).x;

        // v0·t for the undamped twin.
        EXPECT_NEAR(dist_free, 6.0f, 0.15f);
        // Damped: per-step v *= 1/(1+d·dt) → ≈ 21% of the free distance after 1 s.
        EXPECT_GT(dist_damped, 0.05f);
        EXPECT_LT(dist_damped, 0.35f * dist_free);
}

// ---------------------------------------------------------------------------
// Interaction with static geometry
// ---------------------------------------------------------------------------

TEST_F(PhysTestBase, StaticFloor_StopsFallingBallAndNeverMoves) {
        auto floor = MakeBody(FloorDesc(0.0f));
        auto ball  = MakeSphere({0.0f, 3.0f, 0.0f});

        StepExact(180);
        EXPECT_NEAR(Y(ball), 0.5f, 0.05f);
        EXPECT_NEAR(Y(floor), -0.5f, 1e-6f);   // static body: node untouched
}

TEST_F(PhysTestBase, MeshCollider_SupportsBallAndRay) {
        std::vector<glm::vec3> verts;
        std::vector<uint32_t>  idx;
        MeshQuad(5.0f, 0.0f, verts, idx);
        RigidBodyDesc mesh;
        mesh.oCollider.type          = ColliderDesc::Mesh;
        mesh.oCollider.vMeshVertices = verts;
        mesh.oCollider.vMeshIndices  = idx;
        mesh.is_static               = true;
        auto mesh_floor              = MakeBody(mesh);

        auto ball = MakeSphere({0.0f, 3.0f, 0.0f});
        StepExact(180);
        EXPECT_NEAR(Y(ball), 0.5f, 0.05f);

        SE::PhysicsRay ray;
        ray.vOrigin    = {3.0f, 3.0f, 0.0f};   // off to the side: only mesh below
        ray.vDirection = {0.0f, -1.0f, 0.0f};
        SE::RaycastHit hit;
        EXPECT_TRUE(phys.Raycast(ray, hit));
        EXPECT_NEAR(hit.distance, 3.0f, 1e-3f);
        EXPECT_EQ(hit.hBody, mesh_floor.handle);
}

// ---------------------------------------------------------------------------
// Activation / deactivation / sleep
// ---------------------------------------------------------------------------

TEST_F(PhysTestBase, DeactivateBody_FreezesMidAirAndActivateResumes) {
        auto ball = MakeSphere({0.0f, 6.0f, 0.0f});

        StepExact(30);
        phys.DeactivateBody(ball.handle);
        StepExact();   // command flushed
        const float frozen_y = Y(ball);

        StepExact(60);
        EXPECT_FLOAT_EQ(Y(ball), frozen_y);   // inactive: no integration

        phys.ActivateBody(ball.handle);
        StepExact();
        EXPECT_LT(Y(ball), frozen_y);         // resumed falling
}

TEST_F(PhysTestBase, SleepingBody_SettlesAndStaysPutUntilImpulse) {
        MakeBody(FloorDesc(0.0f));
        RigidBodyDesc d  = DynamicSphere({0.0f, 0.6f, 0.0f});
        d.restitution    = 0.0f;
        d.linear_damping = 0.05f;
        auto ball        = MakeBody(d);

        StepExact(180);   // > Jolt's 0.5 s time-before-sleep after settling
        const float y1 = Y(ball);
        StepExact(60);
        ASSERT_FLOAT_EQ(Y(ball), y1);   // asleep: bit-exact stillness

        phys.ApplyImpulse(ball.handle, {0.0f, 0.0f, 1.5f});
        StepExact();
        StepExact(10);
        EXPECT_GT(Pos(ball).z, 0.05f);   // impulse woke it up
}

// ---------------------------------------------------------------------------
// Destruction and handle generation
// ---------------------------------------------------------------------------

TEST_F(PhysTestBase, DestroyBody_IsDeferredUntilNextStep) {
        auto platform = MakeStaticBox({0.0f, 0.0f, 0.0f}, {1.0f, 0.5f, 1.0f});
        auto ball     = MakeSphere({0.0f, 1.5f, 0.0f});
        StepExact(120);
        // Ball radius 0.5 resting on the platform top (y = 0.5) → center 1.0.
        ASSERT_NEAR(Y(ball), 1.0f, 0.02f);

        phys.DestroyBody(platform.handle);

        // Queued only — the platform is still castable this frame. The ray is
        // offset to x = 0.75 so the ball (x ∈ [-0.5..0.5]) stays out of it.
        SE::PhysicsRay ray;
        ray.vOrigin    = {0.75f, 3.0f, 0.0f};
        ray.vDirection = {0.0f, -1.0f, 0.0f};
        SE::RaycastHit hit;
        EXPECT_TRUE(phys.Raycast(ray, hit));
        EXPECT_EQ(hit.hBody, platform.handle);

        StepExact();   // flushed now — nothing supports x = 0.75 anymore
        EXPECT_FALSE(phys.Raycast(ray, hit));

        // CHARACTERIZATION: the ball went asleep on the platform, and destroying
        // the support does NOT wake it — a sleeper floats in the air until
        // something activates it. (Wake-on-support-loss is game code's job.)
        EXPECT_NEAR(Y(ball), 1.0f, 0.02f);
        phys.ActivateBody(ball.handle);
        StepExact(30);
        EXPECT_LT(Y(ball), 0.9f);
}

TEST_F(PhysTestBase, StaleHandle_IsInertAfterIndexReuse) {
        auto first = MakeSphere({0.0f, 2.0f, 0.0f});
        ASSERT_TRUE(first.handle.IsValid());
        const auto first_index = first.handle.index;

        phys.DestroyBody(first.handle);
        StepExact();

        // New body very likely reuses the freed slot…
        auto second = MakeSphere({1.0f, 2.0f, 0.0f});
        ASSERT_TRUE(second.handle.IsValid());
        // …but with a bumped generation, so handles never alias.
        EXPECT_NE(first.handle, second.handle);

        // A stale handle's commands must be inert no-ops: they encode the old
        // generation, which no live body carries.
        phys.SetLinearVelocity(first.handle, {0.0f, 100.0f, 0.0f});
        phys.Teleport(first.handle, {50.0f, 50.0f, 50.0f}, se_test::kIdentityQuat);
        phys.DeactivateBody(first.handle);
        StepExact();
        EXPECT_NEAR(Pos(second).y, 2.0f, 0.1f);        // untouched by stale velocity
        EXPECT_NEAR(Pos(second).x, 1.0f, 1e-4f);       // …and by the stale teleport
        EXPECT_GT(Y(second), 1.5f);                    // still active/integrated
}

// ---------------------------------------------------------------------------
// Fixed-clock semantics
// ---------------------------------------------------------------------------

TEST_F(PhysTestBase, Update_ChunksGameDtIntoFixedSteps) {
        RigidBodyDesc d = DynamicSphere({0.0f, 5.0f, 0.0f});
        d.gravity_scale  = 0.0f;
        d.linear_damping = 0.0f;
        auto ball        = MakeBody(d);
        phys.SetLinearVelocity(ball.handle, {2.0f, 0.0f, 0.0f});

        // Sub-step frames accumulate: 0.3·dt each, no fixed step runs while the
        // accumulator is below the threshold. Alpha tracks the remainder.
        for (float expected_alpha : {0.3f, 0.6f, 0.9f}) {
                phys.Update(kDt * 0.3f);
                phys.Interpolate();
                EXPECT_NEAR(phys.GetInterpolationAlpha(), expected_alpha, 1e-4f);
                EXPECT_FLOAT_EQ(Pos(ball).x, 0.0f);   // no step yet → not moved
        }

        // 4th frame crosses the threshold: exactly one fixed step executes and
        // the remainder (0.2) carries into alpha. The node renders 20% into
        // the new pose — the interpolation contract prev/curr + alpha.
        phys.Update(kDt * 0.3f);
        EXPECT_NEAR(phys.GetInterpolationAlpha(), 0.2f, 1e-3f);
        phys.Interpolate();
        EXPECT_NEAR(Pos(ball).x, 0.2f * 2.0f * kDt, 1e-5f);

        // StepOnce runs another full step and drives alpha to 1: the node
        // shows the exact current pose — two steps of travel total.
        phys.StepOnce();
        phys.Interpolate();
        EXPECT_NEAR(Pos(ball).x, 2.0f * 2.0f * kDt, 1e-5f);
}

TEST_F(PhysTestBase, Pause_StepOnceInteraction) {
        auto ball = MakeSphere({0.0f, 5.0f, 0.0f});
        phys.SetPaused(true);

        phys.Update(0.5f);   // long dt — still nothing while paused
        phys.Interpolate();
        EXPECT_FLOAT_EQ(Y(ball), 5.0f);
        EXPECT_FLOAT_EQ(phys.GetInterpolationAlpha(), 1.0f);

        // CHARACTERIZATION: StepOnce bypasses the pause flag (debug-stepping
        // affordance — P/N controls in samples/physics_demo).
        phys.StepOnce();
        phys.Interpolate();
        EXPECT_LT(Y(ball), 5.0f);

        phys.SetPaused(false);
        const float y = Y(ball);
        phys.Update(kDt);
        phys.Interpolate();
        // CHARACTERIZATION: the resumed frame's step lands at alpha = 0, and
        // Interpolate renders the PREVIOUS (pre-step) transform — the node
        // freezes for exactly one frame when simulation resumes.
        EXPECT_FLOAT_EQ(Y(ball), y);
        StepExact();
        EXPECT_LT(Y(ball), y);
}

// ---------------------------------------------------------------------------
// Kinematic bodies (system-level; the node-driven path is covered in
// PhysicsSceneTest via the RigidBody component)
// ---------------------------------------------------------------------------

TEST_F(PhysTestBase, KinematicPlatform_CarriesDynamicBox) {
        RigidBodyDesc k;
        k.oCollider.type         = ColliderDesc::Box;
        k.oCollider.vHalfExtents = {1.0f, 0.5f, 1.0f};
        k.vInitialPosition       = {0.0f, 0.0f, 0.0f};
        k.is_kinematic           = true;
        auto platform            = MakeBody(k);

        RigidBodyDesc box;
        box.oCollider.type         = ColliderDesc::Box;
        box.oCollider.vHalfExtents = {0.25f, 0.25f, 0.25f};
        box.vInitialPosition       = {0.0f, 0.751f, 0.0f};   // on platform top (y=0.5)
        box.friction               = 1.0f;
        box.restitution            = 0.0f;
        auto cargo                 = MakeBody(box);

        StepExact(60);
        ASSERT_NEAR(Y(cargo), 0.75f, 0.05f);   // resting on the kinematic slab

        // Slide the platform +x at 1 m/s via per-step MoveKinematic targets.
        const float speed = 1.0f;
        for (uint32_t i = 0; i < 60; ++i) {
                phys.MoveKinematic(platform.handle,
                                   {speed * kDt * float(i + 1), 0.0f, 0.0f}, se_test::kIdentityQuat);
                StepExact();
        }
        // Cargo was carried along (friction, no sliding).
        EXPECT_NEAR(Pos(cargo).x, speed, 0.15f);
        EXPECT_NEAR(Y(cargo), 0.75f, 0.05f);

        // Platform's own position is observable via raycast (Interpolate skips
        // kinematic nodes — the node stays the authored source of truth).
        SE::PhysicsRay ray;
        ray.vOrigin    = {0.5f, 3.0f, 0.0f};
        ray.vDirection = {0.0f, -1.0f, 0.0f};
        SE::RaycastHit hit;
        ASSERT_TRUE(phys.Raycast(ray, hit));
        EXPECT_NEAR(hit.distance, 2.5f, 0.05f);
}
