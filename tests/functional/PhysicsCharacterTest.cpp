
// ---------------------------------------------------------------------------
// PhysicsCharacterTest — Tier C. The CharacterVirtual API: capsule controller
// grounded state, user-owned velocity, walking, walls, stairs, slope limits,
// teleports, ground velocity on moving platforms, and the sensor-pass-through
// rule. Characters are stepped before the rigid simulation each fixed step
// (PhysicsSystem::Impl::RunFixedStep → StepCharacters).
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>

#include "PhysicsHarness.h"

#include <gtest/gtest.h>
#include <iostream>

using namespace se_test;
using SE::CharacterDesc;
using SE::ColliderDesc;
using SE::EPhysicsTriggerEnter;
using SE::RigidBodyDesc;

namespace {

constexpr float kDt = 1.0f / 60.0f;

// Feet height used to spawn ON a surface at y = 0 (a hair above to avoid
// initial penetration).
constexpr float kOnFloor = 1e-3f;

RigidBodyDesc SensorBox(glm::vec3 pos, glm::vec3 half = {0.75f, 0.75f, 0.75f}) {
        RigidBodyDesc d;
        d.oCollider.type         = ColliderDesc::Box;
        d.oCollider.vHalfExtents = half;
        d.vInitialPosition       = pos;
        d.is_trigger             = true;
        d.is_kinematic           = true;
        return d;
}

} // namespace

TEST_F(PhysTestBase, CreateCharacter_ValidHandleAndImmediateNodeSync) {
        auto ch = MakeCharacter({0.0f, 1.2f, 0.0f});
        ASSERT_TRUE(ch.handle.IsValid());

        // Character nodes sync unconditionally in Interpolate (no prev/curr
        // interpolation — the controller owns the velocity), no step required.
        phys.Interpolate();
        EXPECT_NEAR(CharY(ch), 1.2f, 1e-5f);
}

TEST_F(PhysTestBase, Character_FallsOntoFloorAndGrounded) {
        MakeBody(FloorDesc(0.0f));
        auto ch = MakeCharacter({0.0f, 3.0f, 0.0f});

        phys.SetCharacterVelocity(ch.handle, {0.0f, -5.0f, 0.0f});
        StepExact(60);

        EXPECT_TRUE(phys.IsCharacterGrounded(ch.handle));
        // CHARACTERIZATION (positional contract): CharacterVirtual position is
        // the FEET point — CreateCharacter wraps the capsule in a
        // RotatedTranslatedShape shifted up by radius+half_height, so a
        // character resting on a surface at height H reports y == H.
        EXPECT_NEAR(CharY(ch), 0.0f, 0.05f);
        const glm::vec3 n = phys.GetCharacterFloorNormal(ch.handle);
        EXPECT_NEAR(n.y, 1.0f, 1e-2f);
        EXPECT_LT(CharY(ch), 3.0f);   // synced scene node followed the fall
}

TEST_F(PhysTestBase, Character_AirborneNotGroundedUntilLanding) {
        MakeBody(FloorDesc(0.0f));
        auto ch = MakeCharacter({0.0f, 3.0f, 0.0f});
        phys.SetCharacterVelocity(ch.handle, {0.0f, -5.0f, 0.0f});

        StepExact(6);   // ≈ 0.1 s — still in the air
        EXPECT_FALSE(phys.IsCharacterGrounded(ch.handle));

        StepExact(60);
        EXPECT_TRUE(phys.IsCharacterGrounded(ch.handle));
}

TEST_F(PhysTestBase, Character_WalksHorizontallyWithUserVelocity) {
        MakeBody(FloorDesc(0.0f));
        auto ch = MakeCharacter({0.0f, kOnFloor, 0.0f});
        phys.SetCharacterVelocity(ch.handle, {2.0f, 0.0f, 0.0f});

        StepExact(60);
        // ≈ 2 m/s along x, staying grounded on the floor.
        EXPECT_NEAR(CharPos(ch).x, 2.0f, 0.2f);
        EXPECT_TRUE(phys.IsCharacterGrounded(ch.handle));
        EXPECT_NEAR(CharY(ch), 0.0f, 0.05f);
}

TEST_F(PhysTestBase, Character_BlockedByWall) {
        MakeBody(FloorDesc(0.0f));
        // Wall spans x ∈ [-0.5 .. 0.5]: the char approaches its +x face.
        MakeStaticBox({0.0f, 1.0f, 0.0f}, {0.5f, 1.0f, 5.0f});
        auto ch = MakeCharacter({2.0f, kOnFloor, 0.0f});
        // Downward stick keeps the capsule grounded while walking.
        phys.SetCharacterVelocity(ch.handle, {-2.0f, -0.5f, 0.0f});

        StepExact(120);
        // Traveled 1.2 m, then the capsule radius (0.3) holds it ~0.3 m off
        // the face at x = 0.5.
        EXPECT_GT(CharPos(ch).x, 0.72f);
        EXPECT_LT(CharPos(ch).x, 0.9f);
        EXPECT_TRUE(phys.IsCharacterGrounded(ch.handle));
        EXPECT_NEAR(CharY(ch), 0.0f, 0.05f);
}

TEST_F(PhysTestBase, Character_ClimbsStairWithinStepHeight) {
        MakeBody(FloorDesc(0.0f));
        // One 0.25 m riser (≤ default step_height 0.3): top surface y = 0.25.
        MakeStaticBox({1.5f, 0.125f, 0.0f}, {0.5f, 0.125f, 2.0f});

        auto ch = MakeCharacter({0.0f, kOnFloor, 0.0f});
        // CHARACTERIZATION: the stick-down must be gravity-like. A strong
        // constant downward velocity (e.g. -5 m/s) defeats the walk-stairs
        // scan — ExtendedUpdate lifts the capsule onto the riser, and the
        // stick slams it back down into a 3-frame limit cycle at the riser
        // face. With a gentle stick the stair climb commits.
        phys.SetCharacterVelocity(ch.handle, {1.0f, -0.5f, 0.0f});

        StepExact(100);   // walk 1.67 m — middle of the riser (x ∈ [1.0 .. 2.0])
        // Feet ended up ON the riser surface.
        EXPECT_NEAR(CharY(ch), 0.25f, 0.05f);
        EXPECT_TRUE(phys.IsCharacterGrounded(ch.handle));
        EXPECT_GT(CharPos(ch).x, 1.0f);

        // Walking on, it descends the far side back to the floor.
        StepExact(50);
        EXPECT_NEAR(CharY(ch), 0.0f, 0.05f);
        EXPECT_TRUE(phys.IsCharacterGrounded(ch.handle));
}

// CHARACTERIZATION: ground state on a too-steep slope is not "OnGround" —
// IsCharacterGrounded reports false even though the capsule rests on it
// (Jolt's OnSteepGround state); the slope still blocks horizontal motion.
TEST_F(PhysTestBase, Character_RejectsSlopeBeyondLimit) {
        // 60° wedge: steeper than the 50° default limit, standing on the floor.
        RigidBodyDesc wedge;
        wedge.oCollider.type         = ColliderDesc::Box;
        wedge.oCollider.vHalfExtents = {2.0f, 0.5f, 1.0f};
        wedge.qInitialRotation       = glm::angleAxis(glm::radians(60.0f), glm::vec3(0, 0, 1));
        wedge.vInitialPosition       = {0.0f, 0.55f, 0.0f};
        wedge.is_static              = true;
        MakeBody(wedge, "steep");

        auto ch = MakeCharacter({-1.5f, 1.2f, 0.0f});   // feet above the wedge slope
        phys.SetCharacterVelocity(ch.handle, {2.0f, 0.0f, 0.0f});

        StepExact(90);
        EXPECT_FALSE(phys.IsCharacterGrounded(ch.handle));
        // It did not climb over the wedge.
        EXPECT_LT(CharPos(ch).x, 1.0f);
}

TEST_F(PhysTestBase, TeleportCharacter_MovesImmediately) {
        auto ch = MakeCharacter({0.0f, 1.2f, 0.0f});
        phys.TeleportCharacter(ch.handle, {5.0f, 4.0f, -2.0f}, se_test::kIdentityQuat);

        phys.Interpolate();   // no step needed
        const glm::vec3 p = CharPos(ch);
        EXPECT_NEAR(p.x, 5.0f, 1e-5f);
        EXPECT_NEAR(p.y, 4.0f, 1e-5f);
        EXPECT_NEAR(p.z, -2.0f, 1e-5f);
}

TEST_F(PhysTestBase, DestroyedCharacter_QueriesAreSafeDefaults) {
        MakeBody(FloorDesc(0.0f));
        auto ch = MakeCharacter({0.0f, CapsuleLift(), 0.0f});
        phys.SetCharacterVelocity(ch.handle, {0.0f, -5.0f, 0.0f});
        StepExact(30);
        ASSERT_TRUE(phys.IsCharacterGrounded(ch.handle));

        phys.DestroyCharacter(ch.handle);

        // All queries fall back to their documented defaults, no crash.
        EXPECT_FALSE(phys.IsCharacterGrounded(ch.handle));
        EXPECT_EQ(phys.GetCharacterFloorNormal(ch.handle), glm::vec3(0.0f, 1.0f, 0.0f));
        EXPECT_EQ(phys.GetCharacterGroundVelocity(ch.handle), glm::vec3(0.0f, 0.0f, 0.0f));
        phys.SetCharacterVelocity(ch.handle, {1.0f, 0.0f, 0.0f});   // no-op
        phys.TeleportCharacter(ch.handle, {9.0f, 9.0f, 9.0f}, se_test::kIdentityQuat);  // no-op
        StepExact(5);
}

TEST_F(PhysTestBase, Character_GroundVelocityAndCarriageOnKinematicPlatform) {
        RigidBodyDesc k;
        k.oCollider.type         = ColliderDesc::Box;
        k.oCollider.vHalfExtents = {2.0f, 0.5f, 2.0f};
        k.vInitialPosition       = {0.0f, -0.5f, 0.0f};   // top surface y = 0
        k.is_kinematic           = true;
        auto platform            = MakeBody(k);

        auto ch = MakeCharacter({0.0f, kOnFloor, 0.0f});
        phys.SetCharacterVelocity(ch.handle, {0.0f, -0.5f, 0.0f});   // gentle stick-down
        StepExact(30);
        ASSERT_TRUE(phys.IsCharacterGrounded(ch.handle));

        // Belt: MoveKinematic targets advancing 1 m per second.
        const float speed = 1.0f;
        bool saw_ground_velocity = false;
        for (uint32_t i = 0; i < 90; ++i) {
                phys.MoveKinematic(platform.handle,
                                   {speed * kDt * float(i + 1), 0.0f, 0.0f}, se_test::kIdentityQuat);
                StepExact();
                if (glm::length(phys.GetCharacterGroundVelocity(ch.handle)) > 0.2f) {
                        saw_ground_velocity = true;
                }
        }
        // The platform motion is reported through the ground query…
        EXPECT_TRUE(saw_ground_velocity);
        // CHARACTERIZATION: …but the character is NOT carried. CharacterVirtual
        // has no inertia transfer; adding the ground velocity to the desired
        // velocity is the controller's job (UE/Unity do it inside their
        // character movement components). The standing char stays in place
        // while 1.5 m of platform slides beneath it.
        EXPECT_LT(CharPos(ch).x, 0.5f);
}

// CHARACTERIZATION: characters deliberately skip the TRIGGER object layer
// (SECharObjFilter) — sensors neither report nor block the capsule.
TEST_F(PhysTestBase, Character_PassesThroughTriggerVolumes) {
        MakeBody(FloorDesc(0.0f));
        auto sensor = MakeBody(SensorBox({2.0f, 1.2f, 0.0f}));

        auto ch = MakeCharacter({0.0f, kOnFloor, 0.0f});
        phys.SetCharacterVelocity(ch.handle, {2.0f, 0.0f, 0.0f});

        EventSink<EPhysicsTriggerEnter> enters;
        StepExact(120);

        EXPECT_EQ(enters.Size(), 0u);                    // no trigger fired
        EXPECT_GT(CharPos(ch).x, 2.5f);                  // walked straight through
        EXPECT_TRUE(phys.IsCharacterGrounded(ch.handle));
}

TEST_F(PhysTestBase, TwoCharacters_Independent) {
        MakeBody(FloorDesc(0.0f));
        auto a = MakeCharacter({0.0f, kOnFloor, 0.0f});
        auto b = MakeCharacter({5.0f, kOnFloor, 0.0f});

        phys.SetCharacterVelocity(a.handle, {1.0f, 0.0f, 0.0f});
        phys.SetCharacterVelocity(b.handle, {0.0f, 0.0f, 0.0f});

        StepExact(60);
        EXPECT_NEAR(CharPos(a).x, 1.0f, 0.1f);
        EXPECT_NEAR(CharPos(b).x, 5.0f, 1e-4f);
        EXPECT_TRUE(phys.IsCharacterGrounded(a.handle));
        EXPECT_TRUE(phys.IsCharacterGrounded(b.handle));
}
