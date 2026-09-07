
// ---------------------------------------------------------------------------
// Tier B functional tests — the REAL shipped animation assets
// (resource/anim_graph/character.seag + resource/animation/*.seak + skeleton).
//
// These are the first tests in the repo to read baked asset files: they run
// with CWD = repo root (pinned in CMake) so Config::sResourceDir resolves.
// Every graph Init through the real TResourceManager decodes ~16 real clips.
// ---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <glm/glm.hpp>

#define SE_IMPL
#include <EngineFixture.h>
#include <FrameRunner.h>
#include <PoseAssert.h>
#include <LogCapture.h>

namespace {

using namespace se_test;

constexpr const char* kGraphPath  = "resource/anim_graph/character.seag";
constexpr const char* kSkelPath   = "resource/animation/ual1_skeleton.sesk";
constexpr const char* kIdlePath   = "resource/animation/ual1_Idle_Loop.seak";

// Character graph + 65-joint skeleton + live instance.
struct CharacterRig {
        SE::H<SE::AnimGraph> hGraph;
        SE::AnimGraph*  pGraph = nullptr;
        SE::AnimClip    oIdle;      // manual reference copy of the idle clip
        SE::Skeleton    oSkel;
        SE::AnimGraphInstance inst;

        explicit CharacterRig()
                : oIdle(kIdlePath, 0),
                  oSkel(kSkelPath, 0) {
                hGraph = SE::CreateResource<SE::AnimGraph>(kGraphPath);
                pGraph = SE::GetResource(hGraph);
                inst.Init(*pGraph);
        }
};

// Update until the machine settles in [target] (not transitioning), or fail.
bool StepUntilState(SE::AnimGraphInstance& oInst, const char* sTarget,
                    float dt = 0.05f, int max_frames = 600) {
        const SE::StrID target(sTarget);
        for (int i = 0; i < max_frames; ++i) {
                oInst.Update(dt);
                if (oInst.CurrentStateName() == target && !oInst.IsTransitioning()) {
                        return true;
                }
        }
        return false;
}

// ===========================================================================
// Smoke
// ===========================================================================

TEST_F(FuncTestBase, AssetSmokeTest_CharacterGraphLoads8States10Params) {
        LogCapture oCapture;
        CharacterRig oRig;

        std::vector<std::string> vStates;
        oRig.inst.GetStateNames(vStates);
        ASSERT_EQ(vStates.size(), 8u);

        std::vector<SE::ParamInfo> vParams;
        oRig.inst.GetParams(vParams);
        ASSERT_EQ(vParams.size(), 10u);

        EXPECT_TRUE(oRig.inst.CurrentStateName() == SE::StrID("locomotion"));
        EXPECT_FALSE(oRig.inst.IsTransitioning());
        oCapture.ExpectNoErrors();   // every referenced clip resolved
}

// ===========================================================================
// Locomotion Blend1D
// ===========================================================================

TEST_F(FuncTestBase, AssetWalkTest_SpeedBlendProducesDistinctPoses) {
        CharacterRig oRig;

        float x_idle = 0.0f, x_walk = 0.0f, x_sprint = 0.0f;
        for (auto& [speed, out] : {std::pair{0.0f, &x_idle}, {1.0f, &x_walk}, {2.0f, &x_sprint}}) {
                oRig.inst.SetFloat(SE::StrID("speed"), speed);
                SE::LocalPose oPose = EvalFrame(oRig.inst, oRig.oSkel, Alloc(), 1.0f / 60.0f);
                *out = oPose.pPos[1].z;   // pelvis depth differs across gaits
                Alloc().reset();
        }
        EXPECT_NE(x_idle, x_walk);
        EXPECT_NE(x_walk, x_sprint);
        EXPECT_NE(x_idle, x_sprint);
}

TEST_F(FuncTestBase, AssetSampleTest_GraphPoseMatchesManualSampleClip) {
        // At speed 0 the Blend1D is purely the idle child, sampled at the
        // shared phase == accumulated dt (no wrap yet) — so the graph pose must
        // equal a manual SampleClip of the same clip at the same time.
        CharacterRig oRig;
        oRig.inst.SetFloat(SE::StrID("speed"), 0.0f);

        SE::LocalPose oPose{};
        for (int i = 0; i < 10; ++i) {
                oPose = EvalFrame(oRig.inst, oRig.oSkel, Alloc(), 1.0f / 60.0f);
        }

        SE::LocalPose oRef = SE::AllocatePose(oRig.oSkel.BoneCount(), Alloc());
        SE::InitBindPose(oRef, oRig.oSkel);
        SE::SampleClip(oRig.oIdle, 10.0f / 60.0f, oRef);
        SE::RenormalizeRotations(oRef);

        EXPECT_POSE_NEAR(oPose, oRef);
        Alloc().reset();
}

// ===========================================================================
// Scripted state machine walk
// ===========================================================================

TEST_F(FuncTestBase, AssetWalkTest_CrouchRoundTrip) {
        CharacterRig oRig;
        oRig.inst.SetBool(SE::StrID("is_crouching"), true);
        EXPECT_TRUE(StepUntilState(oRig.inst, "crouch"));
        oRig.inst.SetBool(SE::StrID("is_crouching"), false);
        EXPECT_TRUE(StepUntilState(oRig.inst, "locomotion"));
}

TEST_F(FuncTestBase, AssetWalkTest_JumpChainThroughTriggers) {
        CharacterRig oRig;

        oRig.inst.SetTrigger(SE::StrID("jump"));
        EXPECT_TRUE(StepUntilState(oRig.inst, "jump_start"));
        EXPECT_TRUE(StepUntilState(oRig.inst, "jump_loop"));   // exit_time 0.85

        oRig.inst.SetTrigger(SE::StrID("land"));
        EXPECT_TRUE(StepUntilState(oRig.inst, "jump_land"));
        EXPECT_TRUE(StepUntilState(oRig.inst, "locomotion"));  // exit_time 0.9
}

// ---------------------------------------------------------------------------
// Landing/re-jump edges (fix_animation_system_002)
//
// The demo writes the 'grounded' BOOL every frame, but landing used to be
// gated ONLY on the one-shot 'land' trigger. On a ramp the flight is shorter
// than the ~1.28 s the graph needs to settle into jump_loop (0.1 s fade +
// 0.85 x 1.333 s exit gate + 0.15 s fade), so the trigger expired unconsumed
// and the machine looped Jump_Loop forever while physics moved on.
// The graph now carries grounded-gated landing edges as a level fallback.
// ---------------------------------------------------------------------------

TEST_F(FuncTestBase, AssetWalkTest_GroundedBoolLandsFromJumpLoop) {
        // The ramp repro, headless: land with NO trigger at all — grounded
        // flipping true must be enough to leave jump_loop.
        CharacterRig oRig;

        oRig.inst.SetTrigger(SE::StrID("jump"));
        ASSERT_TRUE(StepUntilState(oRig.inst, "jump_loop"));

        oRig.inst.SetBool(SE::StrID("grounded"), true);   // landing, trigger lost
        EXPECT_TRUE(StepUntilState(oRig.inst, "jump_land"));
        EXPECT_TRUE(StepUntilState(oRig.inst, "locomotion"));
}

TEST_F(FuncTestBase, AssetWalkTest_EarlyGroundedDuringJumpStartLandsDirectly) {
        // Landing while jump_start is still playing (short hop onto a step/ramp):
        // the grounded edge must take the machine straight to jump_land instead
        // of first riding out the 0.85 exit gate into jump_loop.
        CharacterRig oRig;

        oRig.inst.SetTrigger(SE::StrID("jump"));
        ASSERT_TRUE(StepUntilState(oRig.inst, "jump_start"));

        oRig.inst.SetBool(SE::StrID("grounded"), true);
        bool landed = false;
        for (int i = 0; i < 12 && !landed; ++i) {   // 0.6 s: direct edge only
                oRig.inst.Update(0.05f);
                landed = (oRig.inst.CurrentStateName() == SE::StrID("jump_land"))
                                && !oRig.inst.IsTransitioning();
        }
        EXPECT_TRUE(landed)
                << "early landing did not take jump_start -> jump_land directly";
        EXPECT_TRUE(StepUntilState(oRig.inst, "locomotion"));
}

TEST_F(FuncTestBase, AssetWalkTest_ReJumpFromJumpLoopRestartsChain) {
        // Jump pressed while the machine is in jump_loop (exactly the stuck
        // state after a lost landing): the physical jump still fires, so the
        // trigger must re-enter jump_start via the can_interrupt edge instead
        // of being dropped.
        CharacterRig oRig;

        oRig.inst.SetTrigger(SE::StrID("jump"));
        ASSERT_TRUE(StepUntilState(oRig.inst, "jump_loop"));

        oRig.inst.SetBool(SE::StrID("grounded"), false);   // airborne again
        oRig.inst.SetTrigger(SE::StrID("jump"));
        EXPECT_TRUE(StepUntilState(oRig.inst, "jump_start"));
        EXPECT_TRUE(StepUntilState(oRig.inst, "jump_loop"));
}

TEST_F(FuncTestBase, AssetWalkTest_ReJumpWinsOverLandingInSameUpdate) {
        // Jump buffering: landing and a buffered jump press can coincide in one
        // update. The re-jump edge is declared BEFORE the landing edges, so the
        // machine must go back to jump_start — playing the land anim while the
        // character is already rising again would be wrong.
        CharacterRig oRig;

        oRig.inst.SetTrigger(SE::StrID("jump"));
        ASSERT_TRUE(StepUntilState(oRig.inst, "jump_loop"));

        oRig.inst.SetBool(SE::StrID("grounded"), true);
        oRig.inst.SetTrigger(SE::StrID("jump"));
        oRig.inst.Update(0.05f);
        EXPECT_TRUE(oRig.inst.TransitionTargetName() == SE::StrID("jump_start"));
}

TEST_F(FuncTestBase, AssetWalkTest_ReJumpFromJumpLandRestartsChain) {
        // Jump pressed while the machine is settled in jump_land (the whole
        // 0.9 x clip + fade window right after a landing): the controller is
        // still grounded and jumps physically, so the trigger must re-enter
        // jump_start instead of being dropped while the body rises in
        // idle/walk.
        CharacterRig oRig;

        oRig.inst.SetTrigger(SE::StrID("jump"));
        ASSERT_TRUE(StepUntilState(oRig.inst, "jump_loop"));
        oRig.inst.SetTrigger(SE::StrID("land"));
        ASSERT_TRUE(StepUntilState(oRig.inst, "jump_land"));

        oRig.inst.SetBool(SE::StrID("grounded"), false);   // airborne again
        oRig.inst.SetTrigger(SE::StrID("jump"));
        EXPECT_TRUE(StepUntilState(oRig.inst, "jump_start"));
        EXPECT_TRUE(StepUntilState(oRig.inst, "jump_loop"));
}

TEST_F(FuncTestBase, AssetWalkTest_ReJumpDuringLandFadeInterrupts) {
        // Same press landing during the jump_land -> locomotion EXIT fade: the
        // scan still keys on jump_land and only honors can_interrupt edges, so
        // the re-jump edge must replace the in-flight fade instead of letting
        // the machine ride into locomotion (idle/walk) while the body rises.
        CharacterRig oRig;

        oRig.inst.SetTrigger(SE::StrID("jump"));
        ASSERT_TRUE(StepUntilState(oRig.inst, "jump_loop"));
        oRig.inst.SetTrigger(SE::StrID("land"));
        ASSERT_TRUE(StepUntilState(oRig.inst, "jump_land"));

        bool exiting = false;
        for (int i = 0; i < 600 && !exiting; ++i) {   // wait out 0.9 x clip
                oRig.inst.Update(0.05f);
                exiting = oRig.inst.IsTransitioning()
                                && oRig.inst.TransitionTargetName() == SE::StrID("locomotion");
        }
        ASSERT_TRUE(exiting) << "jump_land never started its exit fade";

        oRig.inst.SetBool(SE::StrID("grounded"), false);   // airborne again
        oRig.inst.SetTrigger(SE::StrID("jump"));
        oRig.inst.Update(0.05f);
        EXPECT_TRUE(oRig.inst.TransitionTargetName() == SE::StrID("jump_start"))
                << "jump press during the land exit fade was dropped";
        EXPECT_TRUE(StepUntilState(oRig.inst, "jump_start"));
        EXPECT_TRUE(StepUntilState(oRig.inst, "jump_loop"));
}

TEST_F(FuncTestBase, AssetWalkTest_HitReactionRoundTrip) {
        CharacterRig oRig;

        oRig.inst.SetTrigger(SE::StrID("hit"));
        EXPECT_TRUE(StepUntilState(oRig.inst, "hit_reaction"));
        EXPECT_TRUE(StepUntilState(oRig.inst, "locomotion"));  // exit_time 0.7
}

namespace {

// Angle between two quats in degrees (shortest arc).
float QuatAngleDeg(const glm::quat& a, const glm::quat& b) {
        const float d = glm::clamp(std::abs(glm::dot(a, b)), 0.0f, 1.0f);
        return glm::degrees(2.0f * std::acos(d));
}

} // namespace

TEST_F(FuncTestBase, AssetWalkTest_HitReactionAdditiveDeltaStartsNeutral) {
        // The scene_viewer repro (male_character.sesc selects hit_reaction):
        // the Additive node computed its delta against the skeleton BIND pose.
        // The bind is an A/T-pose while Hit_Chest's frame 0 is a neutral
        // stance, so both upper arms carried a constant ~70 deg inward offset
        // from the first frame (and the additive root reported duration 0, so
        // the exit gate opened immediately). Both are fixed: the delta is
        // reference-relative and the exit gate follows the base child, so the
        // state actually plays its flinch.
        //
        // Runs on the real combo scene_viewer shows: character.seag +
        // male_fullbody_skeleton.sesk.
        SE::Skeleton oMaleSkel("resource/model/male_fullbody_skeleton.sesk", 0);
        ASSERT_EQ(oMaleSkel.BoneCount(), 65u);

        SE::AnimClip oIdle(kIdlePath, 0);   // the additive node's base child

        SE::H<SE::AnimGraph> hGraph = SE::CreateResource<SE::AnimGraph>(kGraphPath);
        SE::AnimGraphInstance oInst;
        oInst.Init(*SE::GetResource(hGraph));

        const uint16_t vArmBones[] = {7, 8, 9, 31, 32, 33};   // clavicle/upperarm/lowerarm, l+r

        // t = 0: the additive contribution is exactly zero -> arms equal the
        // idle base (Hit_Chest frame 0 == the additive reference pose).
        oInst.ForceSetState(SE::StrID("hit_reaction"));
        SE::LocalPose oPose0 = SE::AllocatePose(oMaleSkel.BoneCount(), Alloc());
        SE::InitBindPose(oPose0, oMaleSkel);
        oInst.EvaluateBlendTree(1.0f, oPose0, Alloc(), oMaleSkel);
        SE::RenormalizeRotations(oPose0);

        SE::LocalPose oIdle0 = SE::AllocatePose(oMaleSkel.BoneCount(), Alloc());
        SE::InitBindPose(oIdle0, oMaleSkel);
        SE::SampleClip(oIdle, 0.0f, oIdle0);
        SE::RenormalizeRotations(oIdle0);

        for (uint16_t b : vArmBones) {
                EXPECT_LT(QuatAngleDeg(oPose0.pRot[b], oIdle0.pRot[b]), 1.0f) << "bone " << b;
        }
        Alloc().reset();

        // t = 0.15: the flinch delta is applied on top of idle — bounded
        // (measured ~10-13 deg locally on the upper arms), and the state must
        // still BE hit_reaction (with the old 0-duration exit gate the machine
        // was already back in locomotion here).
        oInst.ForceSetState(SE::StrID("hit_reaction"));
        SE::LocalPose oPose1 = EvalFrame(oInst, oMaleSkel, Alloc(), 0.15f);
        EXPECT_TRUE(oInst.CurrentStateName() == SE::StrID("hit_reaction"));

        SE::LocalPose oIdle1 = SE::AllocatePose(oMaleSkel.BoneCount(), Alloc());
        SE::InitBindPose(oIdle1, oMaleSkel);
        SE::SampleClip(oIdle, 0.15f, oIdle1);
        SE::RenormalizeRotations(oIdle1);

        for (uint16_t b : vArmBones) {
                const float a = QuatAngleDeg(oPose1.pRot[b], oIdle1.pRot[b]);
                EXPECT_LT(a, 60.0f) << "bone " << b;    // bounded — not the old ~70 deg bind offset
        }
        // The flinch moves the upper/lower arms measurably (the clavicles
        // barely move in this clip, so they only get the bounded check above).
        for (uint16_t b : {8, 9, 32, 33}) {   // upperarm/lowerarm, l+r
                const float a = QuatAngleDeg(oPose1.pRot[b], oIdle1.pRot[b]);
                EXPECT_GT(a, 0.5f) << "bone " << b;     // a flinch delta is applied
        }
        Alloc().reset();
}

TEST_F(FuncTestBase, AssetWalkTest_DeathIsStickyTerminal) {
        CharacterRig oRig;
        oRig.inst.SetBool(SE::StrID("is_dead"), true);
        EXPECT_TRUE(StepUntilState(oRig.inst, "death"));
        for (int i = 0; i < 300; ++i) {
                oRig.inst.Update(0.05f);   // ~15 s: no way out of death
        }
        EXPECT_TRUE(oRig.inst.CurrentStateName() == SE::StrID("death"));
        EXPECT_FALSE(oRig.inst.IsTransitioning());
}

TEST_F(FuncTestBase, AssetWalkTest_AimingTransitionCompletesAndMasksLegs) {
        // locomotion -> aiming is can_interrupt=true and gated by the BOOL
        // is_aiming. It used to re-arm every frame while the flag held and never
        // complete (fixed: the in-flight transition is excluded from re-arming).
        // Regression walk over the real graph + skeleton:
        //
        // 1. With the flag held, the crossfade completes and the machine lands
        //    in aiming.
        // 2. The aiming layer respects the shipped upper_body mask: spine/arm
        //    bones follow aiming, legs/root/pelvis keep the locomotion pose.
        CharacterRig oRig;
        oRig.inst.SetFloat(SE::StrID("speed"), 0.0f);
        SE::LocalPose oLoco = EvalFrame(oRig.inst, oRig.oSkel, Alloc(), 1.0f / 60.0f);
        std::vector<glm::quat> vLocoRot(oLoco.pRot, oLoco.pRot + oLoco.bone_count);
        Alloc().reset();

        oRig.inst.SetBool(SE::StrID("is_aiming"), true);
        bool landed = false;
        for (int i = 0; i < 120 && !landed; ++i) {   // 2 s budget: must arrive
                oRig.inst.Update(1.0f / 60.0f);
                landed = (oRig.inst.CurrentStateName() == SE::StrID("aiming"))
                                && !oRig.inst.IsTransitioning();
        }
        ASSERT_TRUE(landed) << "aiming transition never completed while is_aiming held";
        EXPECT_TRUE(oRig.inst.CurrentStateName() == SE::StrID("aiming"));

        SE::LocalPose oAim = EvalFrame(oRig.inst, oRig.oSkel, Alloc(), 1.0f / 60.0f);

        // aiming is a rotational pose: position deltas are ~0, compare quats
        const auto differs = [&](uint32_t i) {
                return std::abs(glm::dot(oAim.pRot[i], vLocoRot[i])) < 1.0f - 1e-3f;
        };
        // Masked-off bones (mask weight 0) must match locomotion exactly.
        for (uint32_t i : {0u, 1u, 55u, 56u, 57u, 58u, 59u, 60u, 61u, 62u, 63u, 64u}) {
                EXPECT_FALSE(differs(i)) << "masked bone " << i << " was overridden";
        }
        // Most of the 53 masked-in upper-body bones must follow aiming.
        uint32_t differing = 0;
        for (uint32_t i = 0; i < oAim.bone_count; ++i) {
                if (differs(i)) ++differing;
        }
        EXPECT_GT(differing, (oAim.bone_count - 12u) / 2u)
                << differing << "/" << oAim.bone_count << " bones differ";
        Alloc().reset();

        // Drop the flag: aiming -> locomotion brings the machine back.
        oRig.inst.SetBool(SE::StrID("is_aiming"), false);
        EXPECT_TRUE(StepUntilState(oRig.inst, "locomotion"));
}

// ===========================================================================
// Full scripted walk, log-verified
// ===========================================================================

TEST_F(FuncTestBase, AssetWalkTest_FullWalkScriptIsErrorAndWarningFree) {
        // ~600 frames across the whole state machine under one log capture:
        // speed ramps, crouch, jump chain, hit, recovery, death, back.
        LogCapture oCapture;
        CharacterRig oRig;

        // locomotion speed ramp 0 -> 2
        for (int i = 0; i < 120; ++i) {
                oRig.inst.SetFloat(SE::StrID("speed"), 2.0f * i / 120.0f);
                oRig.inst.Update(1.0f / 60.0f);
        }
        Alloc().reset();

        // crouch round trip
        oRig.inst.SetFloat(SE::StrID("speed"), 0.5f);
        oRig.inst.SetBool(SE::StrID("is_crouching"), true);
        for (int i = 0; i < 60; ++i) oRig.inst.Update(1.0f / 60.0f);
        oRig.inst.SetBool(SE::StrID("is_crouching"), false);
        for (int i = 0; i < 60; ++i) oRig.inst.Update(1.0f / 60.0f);
        Alloc().reset();

        // jump chain
        oRig.inst.SetTrigger(SE::StrID("jump"));
        for (int i = 0; i < 240; ++i) {
                oRig.inst.Update(1.0f / 60.0f);
                if (oRig.inst.CurrentStateName() == SE::StrID("jump_loop")
                                && !oRig.inst.IsTransitioning()) {
                        oRig.inst.SetTrigger(SE::StrID("land"));
                }
        }
        Alloc().reset();

        // hit + recovery
        oRig.inst.SetTrigger(SE::StrID("hit"));
        for (int i = 0; i < 120; ++i) oRig.inst.Update(1.0f / 60.0f);
        Alloc().reset();

        oCapture.ExpectNoErrors();
        oCapture.ExpectNoWarnings();
}

// ===========================================================================
// Frame allocator budget
// ===========================================================================

TEST_F(FuncTestBase, AssetSampleTest_PoseAllocationWithinFrameBudget) {
        // 65-bone character eval: ~3-4 poses per frame * 65 * 40 B ~= 10 KB.
        // A 600-frame run over the full graph must stay far below the 4 MB
        // engine default — proves per-frame reset discipline with no leak.
        CharacterRig oRig;
        SE::FrameAllocator oAlloc(1024 * 1024);
        oRig.inst.SetFloat(SE::StrID("speed"), 1.0f);

        for (int i = 0; i < 600; ++i) {
                oRig.inst.SetFloat(SE::StrID("speed"), 1.0f + static_cast<float>(i % 60) / 60.0f);
                EvalFrame(oRig.inst, oRig.oSkel, oAlloc, 1.0f / 60.0f);
                oAlloc.reset();
        }
        EXPECT_LT(oAlloc.high_water(), 256 * 1024);
}

} // namespace
