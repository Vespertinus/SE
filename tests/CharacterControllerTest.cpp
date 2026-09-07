
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#define SE_IMPL
#include <Logging.h>
#include <Global.h>
#include <ErrCode.h>
#include <StrID.h>
#include <ResourceHolder.h>
#include <ResourceHandle.h>
#include <ResourceManagerMock.h>
#include <Engine.h>
#include <DebugRendererMock.h>
#include <PhysicsSystemMock.h>
#include <EventManager.h>
#include <EventManager.tcc>
#include <CommonEvents.h>

#include <loki/Singleton.h>

// ---------------------------------------------------------------------------
// Minimal engine — EventManager + PhysicsSystemMock + CharacterMovementSystem.
// No graphics, no files, no real physics.
// ---------------------------------------------------------------------------

namespace SE {

class CharacterMovementSystem;
template <class TSystem> TSystem& GetSystem();

using ResourceManager = ResourceManagerMock;
using DebugRenderer   = DebugRendererMock;
using PhysicsSystem   = PhysicsSystemMock;

using EngineBase = Engine<ResourceManagerMock, DebugRendererMock,
                           EventManager, PhysicsSystemMock,
                           CharacterMovementSystem>;

using TEngine = typename Loki::SingletonHolder<
        EngineBase,
        Loki::CreateUsingNew,
        Loki::DeletableSingleton>;

template <class TSystem> TSystem& GetSystem() {
        return TEngine::Instance().Get<TSystem>();
}

template <class Resource, class... TArgs>
H<Resource> CreateResource(const std::string& name, const TArgs&... args) {
        return GetSystem<ResourceManager>().Create<Resource>(name, args...);
}
template <class Resource>
Resource* GetResource(H<Resource> h) {
        return GetSystem<ResourceManager>().Get<Resource>(h);
}
template <class Resource>
void DestroyResource(H<Resource> h) {
        GetSystem<ResourceManager>().Destroy<Resource>(h);
}

} // namespace SE

#include <AnimClip.tcc>
#include <SceneTree.h>

namespace SE {
class StaticModelMock;
class CharacterController;
class InputState;
// StaticModelMock provides the TSerialized/PostLoad stubs SceneTree::Load requires.
// CharacterController and InputState are the components under test.
using TSceneTree = SceneTree<StaticModelMock, CharacterController, InputState>;
} // namespace SE

#include <StaticModelMock.h>
#include <InputState.h>
#include <hsm/ParameterStore.h>
#include <hsm/StateMachine.h>       // EHSMEvent, SMTrigger
#include <CharacterController.h>
#include <CharacterController.tcc>
#include <CharacterMovementSystem.h>
#include <CharacterMovementSystem.tcc>

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

class CharacterControllerTest : public ::testing::Test {
protected:
        CharacterControllerTest() {
                SE::TEngine::Instance().Init();
        }
        ~CharacterControllerTest() noexcept {
                Loki::DeletableSingleton<SE::EngineBase>::GracefulDelete();
        }

        SE::PhysicsSystemMock& oPhysics() {
                return SE::TEngine::Instance().Get<SE::PhysicsSystemMock>();
        }

        // Create a scene with one node that has CharacterController + InputState.
        SE::CharacterController* MakeCharacter(std::unique_ptr<SE::TSceneTree>& pScene) {
                pScene = std::make_unique<SE::TSceneTree>("test_scene", 0, true);
                auto pNode = pScene->Create("hero");
                pNode->CreateComponent<SE::CharacterController>();
                pNode->CreateComponent<SE::InputState>();
                return pNode->GetComponent<SE::CharacterController>();
        }

        SE::CharacterController* MakeCharacter(std::unique_ptr<SE::TSceneTree>& pScene,
                                               const SE::CharacterController::Desc& desc) {
                pScene = std::make_unique<SE::TSceneTree>("test_scene", 0, true);
                auto pNode = pScene->Create("hero");
                pNode->CreateComponent<SE::CharacterController>(desc);
                pNode->CreateComponent<SE::InputState>();
                return pNode->GetComponent<SE::CharacterController>();
        }

        // Drive one ProcessComponent frame.
        void Step(SE::CharacterController& oCharCont, float dt = 0.016f) {
                SE::GetSystem<SE::CharacterMovementSystem>().ProcessComponent(oCharCont, dt);
        }
};

// ---------------------------------------------------------------------------
// Coyote timer
// ---------------------------------------------------------------------------

TEST_F(CharacterControllerTest, CoyoteTimerStartsOnLeaveGround) {

        std::unique_ptr<SE::TSceneTree> pScene;
        SE::CharacterController* comp = MakeCharacter(pScene);
        ASSERT_NE(comp, nullptr);

        // Frame 1: grounded
        oPhysics().grounded_result = true;
        Step(*comp);
        EXPECT_TRUE(comp->IsGrounded());
        EXPECT_NEAR(comp->GetCoyoteTimer(), 0.f, 1e-5f);

        // Frame 2: leaves ground — coyote window opens
        oPhysics().grounded_result = false;
        Step(*comp);
        EXPECT_FALSE(comp->IsGrounded());
        EXPECT_NEAR(comp->GetCoyoteTimer(), comp->GetCoyoteTime(), 1e-5f);
}

TEST_F(CharacterControllerTest, CoyoteTimerDecaysToZero) {
        std::unique_ptr<SE::TSceneTree> pScene;
        SE::CharacterController* comp = MakeCharacter(pScene);
        ASSERT_NE(comp, nullptr);

        // Establish grounded state, then leave
        oPhysics().grounded_result = true;
        Step(*comp);
        oPhysics().grounded_result = false;
        Step(*comp);
        EXPECT_GT(comp->GetCoyoteTimer(), 0.f);

        // Drain coyote window with large dt
        Step(*comp, comp->GetCoyoteTime() + 0.1f);
        EXPECT_NEAR(comp->GetCoyoteTimer(), 0.f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Jump
// ---------------------------------------------------------------------------

TEST_F(CharacterControllerTest, JumpFiresWithinCoyoteWindow) {
        std::unique_ptr<SE::TSceneTree> pScene;
        SE::CharacterController* comp = MakeCharacter(pScene);
        ASSERT_NE(comp, nullptr);

        // Step 1: establish grounded
        oPhysics().grounded_result = true;
        Step(*comp);

        // Step 2: leave ground — coyote window opens
        oPhysics().grounded_result = false;
        Step(*comp);
        EXPECT_GT(comp->GetCoyoteTimer(), 0.f);

        // Step 3: press jump within coyote window
        auto pNode = pScene->GetRoot()->FindChild("hero");
        ASSERT_NE(pNode, nullptr);
        auto* pInput = pNode->GetComponent<SE::InputState>();
        ASSERT_NE(pInput, nullptr);
        pInput->jump_pressed = true;

        Step(*comp, 0.016f);

        EXPECT_NEAR(comp->GetVelocity().y, comp->GetJumpImpulse(), 1e-4f);
        // Coyote timer should be consumed
        EXPECT_NEAR(comp->GetCoyoteTimer(), 0.f, 1e-5f);
}

TEST_F(CharacterControllerTest, JumpBufferFiresOnLand) {
        std::unique_ptr<SE::TSceneTree> pScene;
        SE::CharacterController* comp = MakeCharacter(pScene);
        ASSERT_NE(comp, nullptr);

        auto pNode = pScene->GetRoot()->FindChild("hero");
        auto* pInput = pNode->GetComponent<SE::InputState>();

        // Airborne — press jump (buffered, can't fire yet)
        oPhysics().grounded_result = false;
        pInput->jump_pressed = true;
        Step(*comp);
        EXPECT_GT(comp->GetJumpBufferTimer(), 0.f);
        // No jump yet (not grounded, no coyote)
        EXPECT_LT(comp->GetVelocity().y, comp->GetJumpImpulse() - 1.f);

        // Land — buffered jump should fire
        pInput->jump_pressed = false;
        oPhysics().grounded_result = true;
        Step(*comp);

        EXPECT_NEAR(comp->GetVelocity().y, comp->GetJumpImpulse(), 1e-4f);
}

TEST_F(CharacterControllerTest, NoDoubleJump) {
        std::unique_ptr<SE::TSceneTree> pScene;
        SE::CharacterController* comp = MakeCharacter(pScene);
        ASSERT_NE(comp, nullptr);

        auto pNode = pScene->GetRoot()->FindChild("hero");
        auto* pInput = pNode->GetComponent<SE::InputState>();

        // Frame 1: grounded + jump → fires once
        oPhysics().grounded_result = true;
        pInput->jump_pressed = true;
        Step(*comp);
        const float vy_after_jump = comp->GetVelocity().y;
        EXPECT_NEAR(vy_after_jump, comp->GetJumpImpulse(), 1e-4f);

        // Frame 2: still jump_pressed, now airborne — no re-fire
        oPhysics().grounded_result = false;
        pInput->jump_pressed = true;
        Step(*comp);
        // velocity.y should have dropped (gravity) or stayed, but NOT jumped again
        EXPECT_LT(comp->GetVelocity().y, vy_after_jump + 0.1f); // not doubled
}

// ---------------------------------------------------------------------------
// Physics
// ---------------------------------------------------------------------------

TEST_F(CharacterControllerTest, GravityAccumulatesWhenAirborne) {
        std::unique_ptr<SE::TSceneTree> pScene;
        SE::CharacterController* comp = MakeCharacter(pScene);
        ASSERT_NE(comp, nullptr);

        oPhysics().grounded_result = false;
        const float dt = 0.1f;

        Step(*comp, dt);
        const float vy1 = comp->GetVelocity().y;

        Step(*comp, dt);
        const float vy2 = comp->GetVelocity().y;

        // Each airborne frame should decrease y velocity
        EXPECT_LT(vy1, 0.f);
        EXPECT_LT(vy2, vy1);
}

TEST_F(CharacterControllerTest, GroundedDownwardBias) {
        std::unique_ptr<SE::TSceneTree> pScene;
        SE::CharacterController* comp = MakeCharacter(pScene);
        ASSERT_NE(comp, nullptr);

        // Accumulate negative y-velocity in the air
        oPhysics().grounded_result = false;
        Step(*comp, 0.5f);
        EXPECT_LT(comp->GetVelocity().y, 0.f);

        // Land: y-velocity should be clamped to -0.5
        oPhysics().grounded_result = true;
        Step(*comp);
        EXPECT_NEAR(comp->GetVelocity().y, -0.5f, 1e-4f);
}

// ---------------------------------------------------------------------------
// Horizontal movement
// ---------------------------------------------------------------------------

TEST_F(CharacterControllerTest, HorizontalVelocityBlendsTowardTarget) {
        std::unique_ptr<SE::TSceneTree> pScene;
        SE::CharacterController* comp = MakeCharacter(pScene);
        ASSERT_NE(comp, nullptr);

        auto pNode = pScene->GetRoot()->FindChild("hero");
        auto* pInput = pNode->GetComponent<SE::InputState>();

        // Push forward on grounded surface
        oPhysics().grounded_result = true;
        pInput->move_axis = {0.f, 1.f}; // full forward

        float prev_z = comp->GetVelocity().z;
        for (int i = 0; i < 50; ++i) {
                Step(*comp);
                EXPECT_GT(comp->GetVelocity().z, prev_z - 1e-4f); // non-decreasing
                prev_z = comp->GetVelocity().z;
        }
        // Should be approaching max_speed (50 frames of exponential blending reaches ~95%)
        EXPECT_GT(comp->GetVelocity().z, comp->GetMaxSpeed() * 0.9f);
}

// ---------------------------------------------------------------------------
// Root motion
// ---------------------------------------------------------------------------

TEST_F(CharacterControllerTest, RootMotionDeltaAppliedToVelocity) {
        std::unique_ptr<SE::TSceneTree> pScene;
        SE::CharacterController* comp = MakeCharacter(pScene);
        ASSERT_NE(comp, nullptr);

        // Root motion only accumulates when use_root_motion is enabled.
        comp->SetUseRootMotion(true);

        const float dt = 1.f;
        const glm::vec3 delta{2.f, 0.f, 0.f};

        SE::CharacterMovementSystem::ApplyRootMotionDelta(*comp, delta, dt);

        // delta.x / dt = 2.0 accumulated into root-motion buffer, not v_velocity
        EXPECT_NEAR(comp->GetRootMotionVelocity().x, delta.x / dt, 1e-5f);
        // Vertical delta is discarded
        EXPECT_NEAR(comp->GetRootMotionVelocity().y, 0.f, 1e-5f);
        // v_velocity is unaffected; root motion is applied during ProcessComponent
        EXPECT_NEAR(comp->GetVelocity().x, 0.f, 1e-5f);
}

TEST_F(CharacterControllerTest, RootMotionIgnoredWhenDisabled) {
        std::unique_ptr<SE::TSceneTree> pScene;
        SE::CharacterController* comp = MakeCharacter(pScene);
        ASSERT_NE(comp, nullptr);

        // use_root_motion defaults to false — delta should be ignored
        SE::CharacterMovementSystem::ApplyRootMotionDelta(*comp, {5.f, 0.f, 0.f}, 1.f);

        EXPECT_NEAR(comp->GetRootMotionVelocity().x, 0.f, 1e-5f);
        EXPECT_NEAR(comp->GetVelocity().x, 0.f, 1e-5f);
}

// ---------------------------------------------------------------------------
// Velocity passed to oPhysics
// ---------------------------------------------------------------------------

TEST_F(CharacterControllerTest, VelocitySubmittedToPhysicsEachFrame) {
        std::unique_ptr<SE::TSceneTree> pScene;
        SE::CharacterController* comp = MakeCharacter(pScene);
        ASSERT_NE(comp, nullptr);

        oPhysics().grounded_result = false;
        Step(*comp);
        EXPECT_EQ(oPhysics().set_velocity_calls, 1);

        Step(*comp);
        EXPECT_EQ(oPhysics().set_velocity_calls, 2);
}
