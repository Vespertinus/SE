
// ---------------------------------------------------------------------------
// PhysicsSceneTest — Tier C+. The physics/scene integration through the REAL
// composition root: the RigidBody core component creating bodies via
// GetSystem<PhysicsSystem>(), node ↔ body synchronization in both directions
// (Interpolate → node for dynamic bodies, node listener → MoveKinematic for
// kinematic ones), Enable/Disable lifecycle, and destructor cleanup.
// Uses EnginePhysTestBase — components resolve PhysicsSystem through the
// engine singleton, exactly as common/application.tcc wires it.
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>

#include "PhysicsHarness.h"

#include <gtest/gtest.h>

using namespace se_test;
using SE::BodyHandle;
using SE::ColliderDesc;
using SE::RigidBody;
using SE::RigidBodyDesc;
using SE::uSUCCESS;

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

TEST_F(EnginePhysTestBase, RigidBodyComponent_CreatesBodyAndNodeFollowsSimulation) {
        // Floor via the raw system (no component needed).
        SE::GetSystem<SE::PhysicsSystem>().CreateRigidBody(FloorDesc(0.0f));

        auto node = scene->Create("Ball");
        node->SetPos({0.0f, 3.0f, 0.0f});
        ASSERT_EQ(node->CreateComponent<RigidBody>(DynamicSphere({0.0f, 3.0f, 0.0f})),
                  uSUCCESS);
        auto* comp = node->GetComponent<RigidBody>();
        ASSERT_NE(comp, nullptr);
        EXPECT_TRUE(comp->GetHandle().IsValid());

        RunSeconds(3.0f);
        // The registered node was driven by Interpolate into the resting pose.
        EXPECT_NEAR(node->GetTransform().GetWorldPos().y, 0.5f, 0.05f);
}

// CHARACTERIZATION: there is no RemoveComponent API, and ~SceneTree is an
// empty TODO (core/SceneTree.tcc) — but node destruction cascades through the
// root shared_ptr, so components die when their node does. The test must drop
// its own node handle: a held shared_ptr keeps the node (and its body) alive
// even after the scene object is gone.
TEST_F(EnginePhysTestBase, RigidBodyComponent_DtorDestroysBodyAndUnregisters) {
        BodyHandle handle;
        {
                auto own_scene = std::make_unique<SE::TSceneTree>("ghost_scene", 0, true);
                auto node = own_scene->Create("Ghost");
                node->SetPos({0.0f, 1.0f, 0.0f});
                node->CreateComponent<RigidBody>(DynamicSphere({0.0f, 1.0f, 0.0f}));
                handle = node->GetComponent<RigidBody>()->GetHandle();
                ASSERT_TRUE(handle.IsValid());

                node->Unlink();   // detach from the (soon dead) root
                node.reset();     // last reference → ~RigidBody runs here
        }

        StepFrame(kDt);           // flush the deferred destroy
        // Destroyed body casts no shadow: no ray hit, node registration gone.
        SE::PhysicsRay ray;
        ray.vOrigin    = {0.0f, 1.0f, 0.0f};
        ray.vDirection = {1.0f, 0.0f, 0.0f};
        SE::RaycastHit hit;
        EXPECT_FALSE(SE::GetSystem<SE::PhysicsSystem>().Raycast(ray, hit));
        EXPECT_EQ(SE::GetSystem<SE::PhysicsSystem>().GetNode(handle), nullptr);
}

TEST_F(EnginePhysTestBase, RigidBodyComponent_KinematicFollowsNodeTransform) {
        SE::GetSystem<SE::PhysicsSystem>().CreateRigidBody(FloorDesc(0.0f));

        // Kinematic platform driven by its scene node (the node-listener path
        // RigidBody::TargetTransformChanged → MoveKinematic).
        auto plat = scene->Create("Platform");
        plat->SetPos({0.0f, 0.0f, 0.0f});
        RigidBodyDesc k;
        k.oCollider.type         = ColliderDesc::Box;
        k.oCollider.vHalfExtents = {1.0f, 0.5f, 1.0f};
        k.vInitialPosition       = {0.0f, 0.0f, 0.0f};
        k.is_kinematic           = true;
        plat->CreateComponent<RigidBody>(k);

        // Cargo resting on the platform top (y = 0.5). A box on purpose: a
        // sphere rolls and slips instead of being carried rigidly.
        auto cargo = scene->Create("Cargo");
        RigidBodyDesc c;
        c.oCollider.type         = ColliderDesc::Box;
        c.oCollider.vHalfExtents = {0.25f, 0.25f, 0.25f};
        c.vInitialPosition       = {0.0f, 0.751f, 0.0f};
        c.friction               = 1.0f;
        c.restitution            = 0.0f;
        cargo->SetPos({0.0f, 0.751f, 0.0f});
        cargo->CreateComponent<RigidBody>(c);

        RunSeconds(1.0f);
        ASSERT_NEAR(cargo->GetTransform().GetWorldPos().y, 0.75f, 0.05f);

        // Slide the NODE — the kinematic body must follow and carry the cargo.
        for (int i = 1; i <= 60; ++i) {
                plat->SetPos({float(i) / 60.0f, 0.0f, 0.0f});
                StepFrame(kDt);
        }
        EXPECT_NEAR(cargo->GetTransform().GetWorldPos().x, 1.0f, 0.15f);
        EXPECT_NEAR(cargo->GetTransform().GetWorldPos().y, 0.75f, 0.05f);
}

TEST_F(EnginePhysTestBase, RigidBodyComponent_EnableDisableActivatesBody) {
        auto node = scene->Create("Ball");
        node->SetPos({0.0f, 5.0f, 0.0f});
        node->CreateComponent<RigidBody>(DynamicSphere({0.0f, 5.0f, 0.0f}));
        auto* comp = node->GetComponent<RigidBody>();

        StepFrame(kDt);   // flush initial state
        comp->Disable();  // → DeactivateBody
        StepFrame(kDt);
        const float frozen = node->GetTransform().GetWorldPos().y;

        RunSeconds(1.0f);
        EXPECT_FLOAT_EQ(node->GetTransform().GetWorldPos().y, frozen);

        comp->Enable();   // → ActivateBody
        RunSeconds(0.5f);
        EXPECT_LT(node->GetTransform().GetWorldPos().y, frozen);
}

TEST_F(EnginePhysTestBase, Interpolate_WritesDynamicNodeAndSkipsStaticNode) {
        auto& oPhys = SE::GetSystem<SE::PhysicsSystem>();

        auto static_node = scene->Create("Static");
        static_node->SetPos({0.0f, 0.0f, 0.0f});
        RigidBodyDesc s;
        s.oCollider.type         = ColliderDesc::Box;
        s.oCollider.vHalfExtents = {1.0f, 0.5f, 1.0f};
        s.vInitialPosition       = {0.0f, -0.5f, 0.0f};   // top at y=0
        s.is_static              = true;
        oPhys.CreateRigidBody(s);
        // Simulate an authoring edit AFTER body creation: physics must not
        // stomp the node (Interpolate skips static bodies).
        static_node->SetPos({7.0f, 0.0f, 0.0f});

        auto ball = scene->Create("Ball");
        ball->SetPos({0.0f, 4.0f, 0.0f});
        ball->CreateComponent<RigidBody>(DynamicSphere({0.0f, 4.0f, 0.0f}));

        RunSeconds(1.0f);
        EXPECT_NEAR(static_node->GetTransform().GetWorldPos().x, 7.0f, 1e-6f);
        EXPECT_NEAR(static_node->GetTransform().GetWorldPos().y, 0.0f, 1e-6f);
        EXPECT_LT(ball->GetTransform().GetWorldPos().y, 3.0f);   // fell onto the slab
}

TEST_F(EnginePhysTestBase, InterpolationAlpha_MatchesAccumulatorAfterUpdate) {
        auto& oPhys = SE::GetSystem<SE::PhysicsSystem>();
        auto node   = scene->Create("Ball");
        node->SetPos({0.0f, 5.0f, 0.0f});
        node->CreateComponent<RigidBody>(DynamicSphere({0.0f, 5.0f, 0.0f}));

        // Half a frame → 0 fixed steps → alpha at the accumulator fraction.
        oPhys.Update(kDt * 0.5f);
        EXPECT_NEAR(oPhys.GetInterpolationAlpha(), 0.5f, 1e-4f);

        // A full extra frame → 1 step → alpha back to the sub-step remainder.
        oPhys.Update(kDt);
        EXPECT_NEAR(oPhys.GetInterpolationAlpha(), 0.5f, 1e-4f);
        EXPECT_NEAR(node->GetTransform().GetWorldPos().y, 5.0f, 1e-4f);
}
