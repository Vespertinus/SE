
#ifndef SE_TEST_FRAME_RUNNER_H
#define SE_TEST_FRAME_RUNNER_H

// ---------------------------------------------------------------------------
// FrameRunner — deterministic frame-phase replay for Tier B/C suites.
//
// Application::Run() cannot be used in tests (its dt comes from a
// function-local static wall clock with no injection seam), so the harness
// replays the subset of the frame phases the system under test depends on,
// in the same order the real loop uses (common/application.tcc).
//
// Animation depends on:
//   EUpdate        — Animator/AnimGraphInstance tick
//   EPostUpdate    — end-of-simulation phase (parity with the real loop)
//   FrameAllocator reset — poses are frame-lifetime bump allocations
// EAnimEvent fires synchronously inside AnimGraphInstance::Update, so no
// EventManager::Process() is required. EPreRenderUpdate (skinning bake) is
// deliberately skipped in the headless tier.
// ---------------------------------------------------------------------------

#include <EngineFixture.h>

namespace se_test {

inline void StepFrame(float dt) {
        SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EUpdate{dt});
        SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EPostUpdate{dt});
        SE::GetSystem<SE::FrameAllocator>().reset();
}

inline void StepFrames(uint32_t n, float dt) {
        for (uint32_t i = 0; i < n; ++i) {
                StepFrame(dt);
        }
}

// Advance a graph instance and evaluate its blend tree into a fresh
// bind-seeded pose allocated from oAlloc.
// FrameAllocator discipline: the pose is valid until the caller resets oAlloc.
inline SE::LocalPose EvalFrame(SE::AnimGraphInstance& oInst, const SE::Skeleton& oSkel,
                               SE::FrameAllocator& oAlloc, float dt) {
        oInst.Update(dt);
        SE::LocalPose oPose = SE::AllocatePose(oSkel.BoneCount(), oAlloc);
        SE::InitBindPose(oPose, oSkel);
        oInst.EvaluateBlendTree(1.0f, oPose, oAlloc, oSkel);
        SE::RenormalizeRotations(oPose);
        return oPose;
}

} // namespace se_test

#endif // SE_TEST_FRAME_RUNNER_H
