
// ---------------------------------------------------------------------------
// PhysicsQueryTest — Tier C. Raycast semantics: hit/miss, distance, point,
// normal, nearest-hit selection, what bodies are visible to queries, and the
// direction-length convention. Static geometry is asserted through the
// PhysicsSystem::Raycast API directly.
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>

#include "PhysicsHarness.h"

#include <gtest/gtest.h>

using namespace se_test;
using SE::ColliderDesc;
using SE::PhysicsRay;
using SE::RaycastHit;
using SE::RigidBodyDesc;

namespace {

constexpr float kDt = 1.0f / 60.0f;

PhysicsRay RayDown(glm::vec3 origin) {
        PhysicsRay r;
        r.vOrigin    = origin;
        r.vDirection = {0.0f, -1.0f, 0.0f};
        return r;
}

} // namespace

TEST_F(PhysTestBase, Raycast_HitsFloorWithDistancePointNormalAndHandle) {
        // Floor box: half (10, 0.5, 10) at pos y = -0.5 → top surface y = 0.
        auto floor = MakeBody(FloorDesc(0.0f));

        RaycastHit hit;
        EXPECT_TRUE(phys.Raycast(RayDown({2.0f, 5.0f, -1.0f}), hit));

        EXPECT_EQ(hit.hBody, floor.handle);
        EXPECT_NEAR(hit.distance, 5.0f, 1e-3f);
        EXPECT_NEAR(hit.vPoint.y, 0.0f, 1e-3f);
        EXPECT_NEAR(hit.vPoint.x, 2.0f, 1e-3f);
        EXPECT_NEAR(hit.vPoint.z, -1.0f, 1e-3f);
        // Floor normal points up.
        EXPECT_NEAR(hit.vNormal.y, 1.0f, 1e-3f);
        EXPECT_NEAR(hit.vNormal.x, 0.0f, 1e-3f);
}

TEST_F(PhysTestBase, Raycast_MissesEmptySky) {
        MakeBody(FloorDesc(0.0f));
        PhysicsRay up = RayDown({0.0f, 5.0f, 0.0f});
        up.vDirection = {0.0f, 1.0f, 0.0f};
        RaycastHit hit;
        EXPECT_FALSE(phys.Raycast(up, hit));
}

TEST_F(PhysTestBase, Raycast_NearestHitWins) {
        MakeBody(FloorDesc(0.0f));                          // top at y = 0
        auto upper = MakeStaticBox({0.0f, 1.5f, 0.0f}, {1.0f, 0.25f, 1.0f}); // top at 1.75

        RaycastHit hit;
        EXPECT_TRUE(phys.Raycast(RayDown({0.0f, 5.0f, 0.0f}), hit));
        EXPECT_EQ(hit.hBody, upper.handle);
        EXPECT_NEAR(hit.distance, 3.25f, 1e-3f);
}

TEST_F(PhysTestBase, Raycast_HitsDynamicBodyAtSimulatedPosition) {
        MakeBody(FloorDesc(0.0f));
        auto ball = MakeSphere({0.0f, 3.0f, 0.0f});
        StepExact(60);   // falls ≈ 2.45 m → center ≈ 0.55, still above floor contact
        // Pin it at an exact height so the expected hit distance is exact.
        phys.Teleport(ball.handle, {0.0f, 2.0f, 0.0f}, se_test::kIdentityQuat);
        phys.DeactivateBody(ball.handle);
        StepExact();

        RaycastHit hit;
        EXPECT_TRUE(phys.Raycast(RayDown({0.0f, 6.0f, 0.0f}), hit));
        EXPECT_EQ(hit.hBody, ball.handle);
        EXPECT_NEAR(hit.distance, 6.0f - 2.5f, 1e-3f);   // sphere top = center + r
        EXPECT_NEAR(hit.vNormal.y, 1.0f, 1e-3f);
}

TEST_F(PhysTestBase, Raycast_BodyVisibleImmediatelyAfterCreate) {
        // No fixed step has run since creation — the body is already in the
        // broadphase (CreateRigidBody adds it activated). Production relies on
        // this: a same-frame ray against a just-spawned body must hit.
        auto ball = MakeSphere({0.0f, 1.0f, 0.0f});
        ASSERT_TRUE(ball.handle.IsValid());

        RaycastHit hit;
        EXPECT_TRUE(phys.Raycast(RayDown({0.0f, 3.0f, 0.0f}), hit));
        EXPECT_EQ(hit.hBody, ball.handle);
        EXPECT_NEAR(hit.distance, 1.5f, 1e-3f);   // sphere top at 1.5

        // CHARACTERIZATION: a ray STARTING INSIDE the body reports an
        // immediate hit at distance 0 (Jolt treats the interior as the front
        // face for the origin sample) — not a miss.
        EXPECT_TRUE(phys.Raycast(RayDown({0.0f, 1.0f, 0.0f}), hit));
        EXPECT_EQ(hit.hBody, ball.handle);
        EXPECT_NEAR(hit.distance, 0.0f, 1e-4f);
}

// CHARACTERIZATION: the QueryFilter argument is accepted but ignored — the
// narrow-phase cast runs unfiltered, and sensor (trigger) bodies are hit like
// any other body. Fix ⇒ pin the filter into CastRay and flip these to the
// filtering behavior.
TEST_F(PhysTestBase, Raycast_TriggerBodiesAreHitAndFilterArgIsIgnored) {
        RigidBodyDesc t;
        t.oCollider.type         = ColliderDesc::Box;
        t.oCollider.vHalfExtents = {1.0f, 0.5f, 1.0f};
        t.vInitialPosition       = {0.0f, 3.0f, 0.0f};
        t.is_trigger             = true;
        t.is_kinematic           = true;   // TriggerVolume-style sensor
        auto sensor              = MakeBody(t);

        RaycastHit hit;
        EXPECT_TRUE(phys.Raycast(RayDown({0.0f, 5.0f, 0.0f}), hit));
        EXPECT_EQ(hit.hBody, sensor.handle);
        EXPECT_NEAR(hit.distance, 1.5f, 1e-3f);   // sensor top at 3.5

        // layer_mask = 0 filters nothing out today.
        SE::QueryFilter filter;
        filter.layer_mask = 0;
        EXPECT_TRUE(phys.Raycast(RayDown({0.0f, 5.0f, 0.0f}), hit, filter));
        EXPECT_EQ(hit.hBody, sensor.handle);
}

// CHARACTERIZATION: `distance` is parametric along vDirection (world distance
// divided by |vDirection|), because Raycast scales the direction by the fixed
// 1000 m max and reports fraction·1000. Unit-length directions give meters.
TEST_F(PhysTestBase, Raycast_DistanceIsParametricAlongDirectionLength) {
        MakeBody(FloorDesc(0.0f));

        RaycastHit hit;
        EXPECT_TRUE(phys.Raycast(RayDown({0.0f, 5.0f, 0.0f}), hit));
        EXPECT_NEAR(hit.distance, 5.0f, 1e-3f);   // |dir| = 1 → meters

        PhysicsRay twice = RayDown({0.0f, 5.0f, 0.0f});
        twice.vDirection = {0.0f, -2.0f, 0.0f};   // |dir| = 2
        EXPECT_TRUE(phys.Raycast(twice, hit));
        EXPECT_NEAR(hit.distance, 2.5f, 1e-3f);   // same geometry → half the t

        // vPoint is still the true world-space hit position in both cases.
        EXPECT_NEAR(hit.vPoint.y, 0.0f, 1e-3f);
}

// Max ray distance is a fixed 1000 m (direction is scaled by kMaxDist).
TEST_F(PhysTestBase, Raycast_BeyondFixedMaxDistanceMisses) {
        MakeBody(FloorDesc(0.0f));
        PhysicsRay far_ray = RayDown({0.0f, 1500.0f, 0.0f});
        RaycastHit hit;
        EXPECT_FALSE(phys.Raycast(far_ray, hit));
}

TEST_F(PhysTestBase, Raycast_ObliqueRayHitsBoxSide) {
        auto wall = MakeStaticBox({0.0f, 1.0f, 0.0f}, {0.5f, 1.0f, 0.5f}); // -x face at x=-0.5

        PhysicsRay r;
        r.vOrigin    = {-5.0f, 1.0f, 0.0f};
        r.vDirection = {1.0f, 0.0f, 0.0f};
        RaycastHit hit;
        EXPECT_TRUE(phys.Raycast(r, hit));
        EXPECT_EQ(hit.hBody, wall.handle);
        EXPECT_NEAR(hit.distance, 4.5f, 1e-3f);
        EXPECT_NEAR(hit.vPoint.x, -0.5f, 1e-3f);
        // Side normal points back toward the ray origin.
        EXPECT_NEAR(hit.vNormal.x, -1.0f, 1e-3f);
}
