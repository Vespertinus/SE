
#define SE_IMPL
#include <gtest/gtest.h>
#include <AppClock.h>
#include <FixedClock.h>
#include <FpsTracker.h>

// ── AppClock ──────────────────────────────────────────────────────────────────

TEST(AppClock, DeltaAdvancesNormally) {
        SE::AppClock clock;
        clock.Tick(1.0 / 60.0);
        EXPECT_NEAR(clock.Delta(),    1.0f / 60.0f, 1e-5f);
        EXPECT_NEAR(clock.RawDelta(), 1.0f / 60.0f, 1e-5f);
}

TEST(AppClock, PauseStopsDeltaAndTotal) {
        SE::AppClock clock;
        clock.Tick(1.0 / 60.0);
        const float total_before = clock.Total();

        clock.Pause();
        clock.Tick(1.0 / 60.0);

        EXPECT_EQ(clock.Delta(), 0.0f);
        EXPECT_EQ(clock.Total(), total_before);
}

TEST(AppClock, ResumeRestoresDelta) {
        SE::AppClock clock;
        clock.Pause();
        clock.Tick(1.0 / 60.0);
        EXPECT_EQ(clock.Delta(), 0.0f);

        clock.Resume();
        clock.Tick(1.0 / 60.0);
        EXPECT_GT(clock.Delta(), 0.0f);
}

TEST(AppClock, ScaleHalvesAccumulatedTime) {
        SE::AppClock clock;
        clock.SetScale(0.5f);
        clock.Tick(1.0 / 60.0);
        clock.Tick(1.0 / 60.0);

        SE::AppClock ref;
        ref.Tick(1.0 / 60.0);
        ref.Tick(1.0 / 60.0);

        EXPECT_NEAR(clock.Total(), ref.Total() * 0.5f, 1e-5f);
}

TEST(AppClock, SpikeCappedAtMaxDelta) {
        SE::AppClock clock;
        clock.Tick(10.0);   // massive spike

        // Max raw delta is 1/15 ≈ 0.0667 s
        EXPECT_LE(clock.RawDelta(), 1.0f / 15.0f + 1e-6f);
}

TEST(AppClock, FrameCounterIncrementsEachTick) {
        SE::AppClock clock;
        EXPECT_EQ(clock.Frame(), 0u);
        clock.Tick(1.0 / 60.0);
        EXPECT_EQ(clock.Frame(), 1u);
        clock.Tick(1.0 / 60.0);
        EXPECT_EQ(clock.Frame(), 2u);
}

// ── FixedClock ────────────────────────────────────────────────────────────────

TEST(FixedClock, ExactlyOneStepAt60Hz) {
        SE::FixedClock fc(1.0f / 60.0f);
        int steps = 0;
        while (fc.Step(1.0f / 60.0f)) ++steps;
        EXPECT_EQ(steps, 1);
}

TEST(FixedClock, TwoStepsAt120HzInput) {
        SE::FixedClock fc(1.0f / 60.0f);
        int steps = 0;
        while (fc.Step(2.0f / 60.0f)) ++steps;
        EXPECT_EQ(steps, 2);
}

TEST(FixedClock, AlphaInRange) {
        SE::FixedClock fc(1.0f / 60.0f);
        while (fc.Step(1.0f / 90.0f)) { /* drain */ }
        EXPECT_GE(fc.Alpha(), 0.0f);
        EXPECT_LE(fc.Alpha(), 1.0f);
}

TEST(FixedClock, ZeroStepsWhenDeltaBelowFixed) {
        SE::FixedClock fc(1.0f / 60.0f);
        int steps = 0;
        while (fc.Step(1.0f / 120.0f)) ++steps;
        EXPECT_EQ(steps, 0);
}

TEST(FixedClock, ResetClearsAccumulator) {
        SE::FixedClock fc(1.0f / 60.0f);
        // accumulate across two frames
        while (fc.Step(1.0f / 60.0f)) {}
        while (fc.Step(1.0f / 60.0f)) {}
        fc.Reset();
        // after reset, a sub-fixed delta should produce 0 steps
        int steps = 0;
        while (fc.Step(1.0f / 120.0f)) ++steps;
        EXPECT_EQ(steps, 0);
}

// ── FpsTracker ────────────────────────────────────────────────────────────────

TEST(FpsTracker, FpsConvergesTo60) {
        SE::FpsTracker tracker;
        const float dt = 1.0f / 60.0f;
        for (int i = 0; i < 60; ++i)
                tracker.Update(dt);
        EXPECT_NEAR(tracker.Fps(), 60.0f, 0.5f);
}

TEST(FpsTracker, FrameMsMatchesLastDelta) {
        SE::FpsTracker tracker;
        tracker.Update(1.0f / 30.0f);   // 33.3 ms
        EXPECT_NEAR(tracker.FrameMs(), 1000.0f / 30.0f, 0.1f);
}

TEST(FpsTracker, MinMaxTrackedCorrectly) {
        SE::FpsTracker tracker;
        tracker.Update(1.0f / 120.0f);  // fast frame
        tracker.Update(1.0f / 30.0f);   // slow frame
        EXPECT_LT(tracker.MinFrameMs(), tracker.MaxFrameMs());
}
