
// ---------------------------------------------------------------------------
// Tier C functional tests — the real Animator component driving real
// TSceneTree joint nodes.
//
// This target uses the shadow/AnimatedModel.h double (headless: no
// Material/UniformBlock/mesh GPU state) placed FIRST on this target's include
// path — see CMake and shadow/AnimatedModel.h. Everything else is production:
// real Animator, real AnimGraphInstance, real TSceneTree, real resources.
// ---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <glm/glm.hpp>

#define SE_IMPL
#include <EngineFixture.h>
#include <FrameRunner.h>
#include <PoseAssert.h>
#include <LogCapture.h>

// SE::Animator is fully declared+implemented through GlobalTypes.h →
// CoreComponents (impl section compiles units/Animator.tcc against the
// shadow AnimatedModel). Do not include Animator.tcc here twice.

namespace {

using namespace se_test;

// A 3-joint character: node "hero" with child joint nodes named after the
// chain-skeleton bones, a shadow AnimatedModel, and a real Animator.
struct HeroRig {
        SkelPtr                                  pSkelNative;
        flatbuffers::FlatBufferBuilder           oSkelBuilder;
        const SE::FlatBuffers::Skeleton*         pSkelFb = nullptr;
        SE::H<SE::Skeleton>                      hSkel;

        flatbuffers::FlatBufferBuilder           oGraphBuilder;
        SE::H<SE::AnimGraph>                     hGraph;

        std::unique_ptr<SE::TSceneTree>          pScene;
        SE::TSceneTree::TSceneNode               pHero;
        SE::AnimatedModel*                       pModel = nullptr;
        SE::Animator*                            pAnimator = nullptr;

        HeroRig() {
                using namespace std::string_view_literals;

                pSkelNative = MakeChainSkeleton(3);
                pSkelFb     = SerializeSkeleton(oSkelBuilder, *pSkelNative);
                hSkel       = SE::CreateResource<SE::Skeleton>("animator_test_skel", pSkelFb);

                // graph: single looping state with a pos-x ramp on bone 1
                auto pClip = MakeClip(1.0f, true);
                AddRamp(*pClip, 1, 0, 1.0f, 0.0f, 1.0f);
                auto pNativeGraph = MakeSingleStateGraph("a", "anim_hero_clip", std::move(pClip));
                hGraph = SE::CreateResource<SE::AnimGraph>(
                                "animator_test_graph",
                                SerializeGraph(oGraphBuilder, *pNativeGraph));

                pScene = std::make_unique<SE::TSceneTree>("animator_scene", 0, true);
                pHero  = pScene->Create("hero");
                // joint nodes named after the skeleton bones: bone_0..bone_2
                SE::TSceneTree::TSceneNode pParent = pHero;
                for (int i = 0; i < 3; ++i) {
                        pParent = pScene->Create(pParent, "bone_" + std::to_string(i));
                }

                pHero->CreateComponent<SE::AnimatedModel>();
                pModel = pHero->GetComponent<SE::AnimatedModel>();
                // The Animator ctor snapshots the model's skeleton handle, so
                // the bind MUST happen first — in production the same order is
                // guaranteed by AnimatedModel::PostLoad running before
                // Animator::PostLoad (see units/Animator.tcc).
                pModel->BindSkeleton(hSkel);
                pHero->CreateComponent<SE::Animator>(hGraph);
                pAnimator = pHero->GetComponent<SE::Animator>();
                if (!pModel || !pAnimator) {
                        // Surface component-creation failures (see log) as a
                        // hard error instead of a null deref below.
                        throw std::runtime_error("HeroRig: component creation failed");
                }
        }
};

TEST_F(FuncTestBase, AnimatorTest_ConstructionBindsGraphAndSkeleton) {
        HeroRig oRig;

        ASSERT_NE(oRig.pAnimator, nullptr);
        EXPECT_TRUE(oRig.pAnimator->GetInstance().CurrentStateName() == SE::StrID("a"));
        ASSERT_EQ(oRig.pModel->JointNodes().size(), 3u);
        EXPECT_NE(oRig.pModel->JointNodes()[0].get(), nullptr);
}

TEST_F(FuncTestBase, AnimatorTest_MissingAnimatedModelThrows) {
        // Animator's ctor derives the skeleton from a co-located AnimatedModel —
        // without one it must throw (soft-degradation would hide rig bugs).
        auto pSkelNative = MakeChainSkeleton(1);
        flatbuffers::FlatBufferBuilder oSkelBuilder;
        auto hSkel = SE::CreateResource<SE::Skeleton>(
                        "animator_bare_skel",
                        SerializeSkeleton(oSkelBuilder, *pSkelNative));

        flatbuffers::FlatBufferBuilder oGraphBuilder;
        auto pNativeGraph = MakeSingleStateGraph("a", "anim_bare_clip", MakeClip(1.0f, true));
        auto hGraph = SE::CreateResource<SE::AnimGraph>(
                        "animator_bare_graph",
                        SerializeGraph(oGraphBuilder, *pNativeGraph));

        SE::TSceneTree oScene("animator_bare_scene", 0, true);
        auto pBare = oScene.Create("bare");
        // The Animator ctor throws without an AnimatedModel; CreateComponent
        // catches it, logs, and reports failure — no half-built component.
        const SE::ret_code_t ret = pBare->CreateComponent<SE::Animator>(hGraph);
        EXPECT_NE(ret, SE::uSUCCESS);
        EXPECT_EQ(pBare->GetComponent<SE::Animator>(), nullptr);
}

TEST_F(FuncTestBase, AnimatorTest_JointNodeTransformsAfterNFixedSteps) {
        HeroRig oRig;

        // 30 fixed steps of 1/60: the ramp clip on bone 1 reaches pos-x 0.5.
        for (int i = 0; i < 30; ++i) {
                StepFrame(1.0f / 60.0f);   // fires EUpdate -> Animator::Evaluate
        }

        const auto& vJoints = oRig.pModel->JointNodes();
        // bone 1 (middle of the chain): pos-x driven 0 -> 1 over the 1 s clip
        EXPECT_NEAR(vJoints[1]->GetTransform().GetPos().x, 0.5f, 1e-3f);
        // bone 0 has no channels: stays at bind
        EXPECT_NEAR(vJoints[0]->GetTransform().GetPos().x, 0.0f, kPoseEps);
        // bone 2 keeps its bind offset
        EXPECT_NEAR(vJoints[2]->GetTransform().GetPos().x, 2.0f, kPoseEps);
}

TEST_F(FuncTestBase, AnimatorTest_BindPoseModeSkipsGraphEvaluation) {
        HeroRig oRig;
        oRig.pAnimator->SetShowBindPose(true);
        EXPECT_TRUE(oRig.pAnimator->IsShowingBindPose());

        StepFrame(1.0f / 60.0f);
        StepFrame(1.0f / 60.0f);

        const auto& vJoints = oRig.pModel->JointNodes();
        EXPECT_NEAR(vJoints[1]->GetTransform().GetPos().x, 1.0f, kPoseEps);   // bind, not 1/30
        // graph time never advanced
        EXPECT_NEAR(oRig.pAnimator->GetInstance().GetCurrentTime(), 0.0f, kTimeEps);
}

TEST_F(FuncTestBase, AnimatorTest_ExternalPoseSourceLeavesJointNodesUntouched) {
        HeroRig oRig;
        oRig.pAnimator->SetPoseSource(SE::Animator::PoseSource::EXTERNAL);

        // sentinel written by the "ragdoll" system
        const glm::vec3 vSentinel(42.0f, 41.0f, 40.0f);
        oRig.pModel->JointNodes()[1]->SetPos(vSentinel);

        for (int i = 0; i < 10; ++i) StepFrame(1.0f / 60.0f);

        EXPECT_VEC_NEAR(oRig.pModel->JointNodes()[1]->GetTransform().GetPos(), vSentinel);
        // graph is not advanced while external
        EXPECT_NEAR(oRig.pAnimator->GetInstance().GetCurrentTime(), 0.0f, kTimeEps);
}

TEST_F(FuncTestBase, AnimatorTest_ParameterFeedsGraphThroughComponent) {
        HeroRig oRig;
        oRig.pAnimator->SetTrigger(SE::StrID("no_such_trigger"));   // harmless API check
        oRig.pAnimator->SetFloat(SE::StrID("speed"), 1.0f);
        EXPECT_FLOAT_EQ(oRig.pAnimator->GetInstance().GetFloat(SE::StrID("speed")), 1.0f);
}

} // namespace
