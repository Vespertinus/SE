
// ---------------------------------------------------------------------------
// PhysicsDeterminismTest — bitwise determinism tier. Physics positions are
// compared memcmp-exact between identical runs (same binary, same inputs) —
// the house rule allowing bitwise comparison ONLY for same-binary replays.
//
// Covered: replay identity of a mixed scene (kinematic carrier + cargo,
// falling stack, bouncer, walking character) across a full Jolt
// Init/Shutdown cycle, and independence from how game_dt is chunked into
// frames (the fixed-step accumulator contract).
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>

#include "PhysicsHarness.h"

#include <gtest/gtest.h>

#include <cstring>

using namespace se_test;
using SE::ColliderDesc;
using SE::RigidBodyDesc;

namespace {

constexpr float kDt = 1.0f / 60.0f;

// Byte buffer of selected node transforms after each exact step.
using Trace = std::vector<float>;

} // namespace

class PhysicsDeterminismTest : public PhysTestBase {

protected:
        // A scene with the interesting couplings — kinematic platform carrying
        // cargo, a falling stack, a bouncer, and a walking character — stepped
        // together for n_steps, recording transforms each step.
        Trace SimulateMixedScene(uint32_t n_steps) {
                RigidBodyDesc k;
                k.oCollider.type         = ColliderDesc::Box;
                k.oCollider.vHalfExtents = {1.0f, 0.5f, 1.0f};
                k.vInitialPosition       = {0.0f, 0.0f, 0.0f};
                k.is_kinematic           = true;
                auto platform = MakeBody(k, "platform");

                RigidBodyDesc cargo;
                cargo.oCollider.type         = ColliderDesc::Box;
                cargo.oCollider.vHalfExtents = {0.25f, 0.25f, 0.25f};
                cargo.vInitialPosition       = {0.0f, 0.751f, 0.0f};
                cargo.friction               = 1.0f;
                auto cargo_ref = MakeBody(cargo, "cargo");

                MakeBody(FloorDesc(-4.0f), "floor");

                auto stack0 = MakeSphere({3.0f, 1.0f, 0.0f}, 0.5f);
                RigidBodyDesc stack1;
                stack1.oCollider.type         = ColliderDesc::Box;
                stack1.oCollider.vHalfExtents = {0.4f, 0.4f, 0.4f};
                stack1.vInitialPosition       = {3.0f, 2.2f, 0.0f};
                stack1.friction               = 0.7f;
                auto stack1_ref = MakeBody(stack1, "stack1");

                RigidBodyDesc bouncy;
                bouncy.oCollider.type   = ColliderDesc::Sphere;
                bouncy.oCollider.radius = 0.3f;
                bouncy.vInitialPosition = {6.0f, 6.0f, 0.0f};
                bouncy.restitution      = 0.7f;
                auto bouncy_ref = MakeBody(bouncy, "bouncy");

                auto ch = MakeCharacter({-3.0f, 5.0f, 0.0f});
                phys.SetCharacterVelocity(ch.handle, {1.0f, -5.0f, 0.0f});

                Trace trace;
                trace.reserve(n_steps * 13);
                for (uint32_t i = 0; i < n_steps; ++i) {
                        // Kinematic driver: deterministic function of the step
                        // index (deferred — applied by the NEXT step).
                        phys.MoveKinematic(platform.handle,
                                           {0.02f * float(i), 0.0f, 0.0f}, se_test::kIdentityQuat);
                        StepExact();
                        for (const auto* ref :
                             {&cargo_ref, &stack0, &stack1_ref, &bouncy_ref}) {
                                const glm::vec3 p = Pos(*ref);
                                trace.push_back(p.x);
                                trace.push_back(p.y);
                                trace.push_back(p.z);
                        }
                        trace.push_back(CharY(ch));
                }
                return trace;
        }
};

TEST_F(PhysicsDeterminismTest, SameInputs_ProduceBitIdenticalReplay) {
        const Trace run_a = SimulateMixedScene(240);
        TearDown();          // full Jolt teardown…
        SetUp();             // …and re-init — the strongest reset the process offers
        const Trace run_b = SimulateMixedScene(240);

        ASSERT_EQ(run_a.size(), run_b.size());
        EXPECT_EQ(0, std::memcmp(run_a.data(), run_b.data(),
                                 run_a.size() * sizeof(Trace::value_type)))
                << "physics replay diverged between identical runs";
}

TEST_F(PhysicsDeterminismTest, ChunkedGameDt_SameFinalStateAsUniformFrames) {
        // Drive the same 240 fixed steps through Update(game_dt) instead of
        // StepExact: 240 uniform frames of 1·kDt vs 60 frames of 4·kDt. The
        // accumulator must map both onto the identical fixed-step sequence,
        // so the final cargo position agrees to the bit.
        auto run = [&](uint32_t frames, float frame_dt) {
                RigidBodyDesc cargo;
                cargo.oCollider.type         = ColliderDesc::Box;
                cargo.oCollider.vHalfExtents = {0.25f, 0.25f, 0.25f};
                cargo.vInitialPosition       = {0.0f, 0.751f, 0.0f};
                cargo.friction               = 1.0f;
                auto cargo_ref = MakeBody(cargo, "cargo");

                for (uint32_t i = 0; i < frames; ++i) {
                        phys.Update(frame_dt);
                        phys.Interpolate();
                }
                return Pos(cargo_ref);
        };

        const glm::vec3 a = run(240, kDt);
        TearDown();
        SetUp();
        const glm::vec3 b = run(60, 4.0f * kDt);

        EXPECT_FLOAT_EQ(a.x, b.x);
        EXPECT_FLOAT_EQ(a.y, b.y);
        EXPECT_FLOAT_EQ(a.z, b.z);
}

// Ordering sanity rather than bitwise: the mixed scene must actually evolve
// (guards against a vacuously-equal trivial trace).
TEST_F(PhysicsDeterminismTest, MixedScene_TraceActuallyEvolves) {
        const Trace trace = SimulateMixedScene(240);
        ASSERT_GE(trace.size(), 240u * 13u);
        int changed = 0;
        for (size_t i = 13; i < trace.size(); i += 13) {
                changed += (std::memcmp(&trace[i - 13], &trace[i],
                                        13 * sizeof(Trace::value_type)) != 0);
        }
        EXPECT_GT(changed, 200);   // nearly every step moves something
}
