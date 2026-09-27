
// ---------------------------------------------------------------------------
// StreamingDirectorTest — Tier B: the streaming planner (pure decisions over
// value snapshots) and the worker thread round-trip. No scene tree, no world
// system: snapshots are built directly.
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>
#include "EngineFixture.h"

#include <world/StreamingDirector.h>
#ifdef SE_IMPL
#include <world/StreamingDirector.tcc>
#endif

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <fstream>
#include <thread>
#include <vector>

using namespace SE;

namespace {

using EDir   = StreamingDirector::ELevelStatus;
using SPlan  = StreamingDirector::SPlan;

StreamingDirector::SParams MakeParams(const float load = 40.f, const float unload = 64.f,
                                      const uint32_t delay = 3) {
        StreamingDirector::SParams p;
        p.load_radius        = load;
        p.unload_radius      = unload;
        p.unload_frame_delay = delay;
        p.max_in_flight      = 8;
        return p;
}

// A small level: 4x4 box centered at (cx, 0, cz).
StreamingDirector::SLevelSnap MakeSnap(const float cx, const float cz) {
        StreamingDirector::SLevelSnap s;
        s.bounds_min = { cx - 2.f, 0.f, cz - 2.f };
        s.bounds_max = { cx + 2.f, 4.f, cz + 2.f };
        return s;
}

StreamingDirector::SFocus MakeFocus(const float x, const float z, const float radius = 0.f) {
        return { glm::vec3(x, 1.f, z), radius, 0.f };
}

} // namespace

// ---------------------------------------------------------------------------
// Wanted set: load radius / hysteresis band / beyond unload radius
// ---------------------------------------------------------------------------

TEST(StreamingDirectorPlanTest, WantedSet_RadiiAndBand) {

        StreamingDirector oDirector;
        oDirector.Reset(1);

        const auto params = MakeParams(/*load=*/40.f, /*unload=*/64.f);
        const auto snap   = MakeSnap(0.f, 0.f);

        // Focus 30 m away (distance to bounds): inside load radius => requested.
        const auto focus_near = MakeFocus(32.f, 0.f);   // dist 30 <= 40
        std::array<EDir, 1> st_unloaded = { EDir::Unloaded };

        SPlan plan;
        oDirector.Plan(params, &snap, st_unloaded.data(), 1, &focus_near, 1, 0, true, plan);
        EXPECT_EQ(plan.vRequests, std::vector<uint32_t>({ 0 }));
        EXPECT_EQ(plan.wanted, 1u);

        // In the hysteresis band (40 < dist <= 64): not requested, nothing evicted.
        const auto focus_band = MakeFocus(60.f, 0.f);   // dist 58
        oDirector.Plan(params, &snap, st_unloaded.data(), 1, &focus_band, 1, 0, true, plan);
        EXPECT_TRUE(plan.vRequests.empty());
        EXPECT_EQ(plan.wanted, 0u);

        // Active in the band: holds (no unload).
        std::array<EDir, 1> st_active = { EDir::Active };
        for (int i = 0; i < 10; ++i) {
                oDirector.Plan(params, &snap, st_active.data(), 1, &focus_band, 1, 0, true, plan);
                EXPECT_TRUE(plan.vUnloads.empty());
        }

        // Beyond unload radius: unloads after exactly unload_frame_delay calls.
        const auto focus_far = MakeFocus(200.f, 0.f);
        for (uint32_t i = 0; i < params.unload_frame_delay - 1; ++i) {
                oDirector.Plan(params, &snap, st_active.data(), 1, &focus_far, 1, 0, true, plan);
                EXPECT_TRUE(plan.vUnloads.empty()) << "unloaded early at call " << i;
        }
        oDirector.Plan(params, &snap, st_active.data(), 1, &focus_far, 1, 0, true, plan);
        EXPECT_EQ(plan.vUnloads, std::vector<uint32_t>({ 0 }));

        // Re-entry into the band resets the timer.
        SPlan ignore;
        std::array<EDir, 1> st_active2 = { EDir::Active };
        for (uint32_t i = 0; i < params.unload_frame_delay - 1; ++i) {
                oDirector.Plan(params, &snap, st_active2.data(), 1, &focus_far, 1, 0, true, ignore);
        }
        oDirector.Plan(params, &snap, st_active2.data(), 1, &focus_band, 1, 0, true, ignore);   // reset
        oDirector.Plan(params, &snap, st_active2.data(), 1, &focus_far, 1, 0, true, ignore);
        EXPECT_TRUE(ignore.vUnloads.empty()) << "timer not reset by band re-entry";
}

TEST(StreamingDirectorPlanTest, Distance_IsToBounds_NotCenter) {

        StreamingDirector oDirector;
        oDirector.Reset(1);

        // A huge level centered 100 m away whose bounds start 10 m from the focus.
        StreamingDirector::SLevelSnap big;
        big.bounds_min = { 50.f, 0.f, -50.f };
        big.bounds_max = { 150.f, 4.f, 50.f };

        const auto params  = MakeParams(40.f, 64.f);
        const auto focus   = MakeFocus(40.f, 0.f);   // distance to bounds = 10 (not 100)
        std::array<EDir, 1> st = { EDir::Unloaded };

        SPlan plan;
        oDirector.Plan(params, &big, st.data(), 1, &focus, 1, 0, true, plan);
        EXPECT_EQ(plan.vRequests, std::vector<uint32_t>({ 0 }));
}

TEST(StreamingDirectorPlanTest, MultipleFocuses_UnionAndNearest) {

        StreamingDirector oDirector;
        oDirector.Reset(2);

        const auto params = MakeParams(40.f, 64.f);
        std::array<StreamingDirector::SLevelSnap, 2> snaps = {
                MakeSnap(0.f, 0.f), MakeSnap(100.f, 0.f) };
        std::array<EDir, 2> st = { EDir::Unloaded, EDir::Unloaded };

        // Level 0 wanted by the near focus; level 1 wanted by the far one.
        const std::array<StreamingDirector::SFocus, 2> focuses = {
                MakeFocus(30.f, 0.f),      // dist to L0 = 28; to L1 = 68
                MakeFocus(120.f, 0.f) };   // dist to L1 = 18

        SPlan plan;
        oDirector.Plan(params, snaps.data(), st.data(), 2, focuses.data(), 2, 0, true, plan);
        EXPECT_EQ(plan.wanted, 2u);
        ASSERT_EQ(plan.vRequests.size(), 2u);
        // Level 1 is nearer (18 < 28) => first despite the higher index.
        EXPECT_EQ(plan.vRequests[0], 1u);
        EXPECT_EQ(plan.vRequests[1], 0u);

        // Eviction requires being beyond EVERY focus's unload distance.
        std::array<EDir, 2> st_active = { EDir::Active, EDir::Active };
        for (int i = 0; i < 10; ++i) {
                oDirector.Plan(params, snaps.data(), st_active.data(), 2, focuses.data(), 2, 0, true, plan);
        }
        EXPECT_TRUE(plan.vUnloads.empty());   // each level still held by one focus
}

TEST(StreamingDirectorPlanTest, FocusRadius_OverridesLoadRadius_KeepsBand) {

        StreamingDirector oDirector;
        oDirector.Reset(1);

        const auto params = MakeParams(40.f, /*unload=*/64.f);   // band width 24
        const auto snap   = MakeSnap(0.f, 0.f);
        std::array<EDir, 1> st = { EDir::Unloaded };

        // Focus with radius 100: a level at distance 60 is wanted (40 would not be).
        const auto focus = MakeFocus(62.f, 0.f, /*radius=*/100.f);

        SPlan plan;
        oDirector.Plan(params, &snap, st.data(), 1, &focus, 1, 0, true, plan);
        EXPECT_EQ(plan.vRequests, std::vector<uint32_t>({ 0 }));

        // Effective unload distance = 100 + 24 = 124: distance 130 still evicts.
        std::array<EDir, 1> st_active = { EDir::Active };
        const auto focus_far = MakeFocus(132.f, 0.f, 100.f);
        for (uint32_t i = 0; i < params.unload_frame_delay; ++i) {
                oDirector.Plan(params, &snap, st_active.data(), 1, &focus_far, 1, 0, true, plan);
        }
        EXPECT_EQ(plan.vUnloads.size(), 1u);
}

// ---------------------------------------------------------------------------
// Ordering, caps, cancels, exclusions
// ---------------------------------------------------------------------------

TEST(StreamingDirectorPlanTest, Ordering_DeterministicTieBreak) {

        StreamingDirector oDirector;
        oDirector.Reset(3);

        const auto params = MakeParams(40.f, 64.f);
        // Three levels at identical distance from the focus.
        std::array<StreamingDirector::SLevelSnap, 3> snaps = {
                MakeSnap(0.f, 32.f), MakeSnap(32.f, 0.f), MakeSnap(0.f, -32.f) };
        std::array<EDir, 3> st = { EDir::Unloaded, EDir::Unloaded, EDir::Unloaded };
        const auto focus = MakeFocus(0.f, 0.f);

        SPlan plan;
        oDirector.Plan(params, snaps.data(), st.data(), 3, &focus, 1, 0, true, plan);
        EXPECT_EQ(plan.vRequests, std::vector<uint32_t>({ 0, 1, 2 }));   // index tie-break
}

TEST(StreamingDirectorPlanTest, InFlightCap_TruncatesRequests) {

        StreamingDirector oDirector;
        oDirector.Reset(3);

        auto params = MakeParams();
        params.max_in_flight = 2;

        std::array<StreamingDirector::SLevelSnap, 3> snaps = {
                MakeSnap(0.f, 20.f), MakeSnap(0.f, 30.f), MakeSnap(0.f, 40.f) };
        std::array<EDir, 3> st = { EDir::Unloaded, EDir::Unloaded, EDir::Unloaded };
        const auto focus = MakeFocus(0.f, 0.f);

        SPlan plan;
        // All three wanted; cap 2, none in flight => nearest two.
        oDirector.Plan(params, snaps.data(), st.data(), 3, &focus, 1, /*in_flight=*/0, true, plan);
        EXPECT_EQ(plan.vRequests, std::vector<uint32_t>({ 0, 1 }));

        // Two already in flight: no room at all.
        oDirector.Plan(params, snaps.data(), st.data(), 3, &focus, 1, /*in_flight=*/2, true, plan);
        EXPECT_TRUE(plan.vRequests.empty());
}

TEST(StreamingDirectorPlanTest, Loading_LeavingWantedSet_Cancels) {

        StreamingDirector oDirector;
        oDirector.Reset(1);

        const auto params = MakeParams();
        const auto snap   = MakeSnap(0.f, 0.f);
        std::array<EDir, 1> st_loading = { EDir::Loading };

        const auto focus_far = MakeFocus(200.f, 0.f);
        SPlan plan;
        oDirector.Plan(params, &snap, st_loading.data(), 1, &focus_far, 1, 0, true, plan);
        EXPECT_EQ(plan.vCancels, std::vector<uint32_t>({ 0 }));

        // Still loading AND still wanted: nothing happens.
        const auto focus_near = MakeFocus(0.f, 0.f);
        oDirector.Plan(params, &snap, st_loading.data(), 1, &focus_near, 1, 0, true, plan);
        EXPECT_TRUE(plan.vCancels.empty());
}

TEST(StreamingDirectorPlanTest, ManualAndAlwaysLoaded_Excluded) {

        StreamingDirector oDirector;
        oDirector.Reset(2);

        const auto params = MakeParams(1000.f, 2000.f);   // everything in range
        std::array<StreamingDirector::SLevelSnap, 2> snaps = { MakeSnap(0.f, 0.f),
                                                                MakeSnap(0.f, 0.f) };
        snaps[0].manual        = true;
        snaps[1].always_loaded = true;

        std::array<EDir, 2> st = { EDir::Unloaded, EDir::Unloaded };
        const auto focus = MakeFocus(0.f, 0.f);

        SPlan plan;
        oDirector.Plan(params, snaps.data(), st.data(), 2, &focus, 1, 0, true, plan);
        EXPECT_TRUE(plan.vRequests.empty());
        EXPECT_EQ(plan.wanted, 1u);   // always-loaded counts as wanted

        // Even beyond every radius, neither is ever unloaded by the director.
        std::array<EDir, 2> st_active = { EDir::Active, EDir::Active };
        const auto focus_far = MakeFocus(9999.f, 0.f);
        for (uint32_t i = 0; i < params.unload_frame_delay; ++i) {
                oDirector.Plan(params, snaps.data(), st_active.data(), 2, &focus_far, 1, 0, true, plan);
        }
        EXPECT_TRUE(plan.vUnloads.empty());
}

TEST(StreamingDirectorPlanTest, FailureCooldown_SuppressesRequests) {

        StreamingDirector oDirector;
        oDirector.Reset(1);
        oDirector.NotifyFailure(0);

        const auto params = MakeParams(1000.f, 2000.f);
        const auto snap   = MakeSnap(0.f, 0.f);
        std::array<EDir, 1> st = { EDir::Unloaded };
        const auto focus  = MakeFocus(0.f, 0.f);

        SPlan plan;
        for (uint32_t i = 0; i < 5; ++i) {
                oDirector.Plan(params, &snap, st.data(), 1, &focus, 1, 0, true, plan);
                EXPECT_TRUE(plan.vRequests.empty()) << "cooldown burned too early at " << i;
        }

        // Burn the rest of the cooldown, then the level is requestable again.
        for (uint32_t i = 5; i < StreamingDirector::kRetryCooldownFrames; ++i) {
                oDirector.Plan(params, &snap, st.data(), 1, &focus, 1, 0, true, plan);
        }
        oDirector.Plan(params, &snap, st.data(), 1, &focus, 1, 0, true, plan);
        EXPECT_EQ(plan.vRequests, std::vector<uint32_t>({ 0 }));
}

TEST(StreamingDirectorPlanTest, FlushMode_ShedsImmediately_TimersAndCooldownsFrozen) {

        StreamingDirector oDirector;
        oDirector.Reset(1);
        oDirector.NotifyFailure(0);

        const auto params = MakeParams(1000.f, 2000.f, /*delay=*/3);
        const auto snap   = MakeSnap(0.f, 0.f);
        std::array<EDir, 1> st_active = { EDir::Active };
        const auto focus_near = MakeFocus(0.f, 0.f);
        const auto focus_far  = MakeFocus(9999.f, 0.f);

        SPlan plan;
        // In range: nothing evict-pending, flush sheds nothing.
        oDirector.Plan(params, &snap, st_active.data(), 1, &focus_near, 1, 0,
                       /*bAdvanceTimers=*/false, plan);
        EXPECT_TRUE(plan.vUnloads.empty());

        // Far out of range: flush mode sheds at once — waiting out the unload
        // delay would stall a synchronous transition forever (this is the
        // WorldSystem::FlushStreaming / loading-screen discipline).
        oDirector.Plan(params, &snap, st_active.data(), 1, &focus_far, 1, 0,
                       /*bAdvanceTimers=*/false, plan);
        EXPECT_EQ(plan.vUnloads, std::vector<uint32_t>({ 0 }));

        // Cooldowns stay frozen in flush mode: the failure cooldown set above
        // must still suppress the request once normal ticking resumes.
        std::array<EDir, 1> st_unloaded = { EDir::Unloaded };
        oDirector.Plan(params, &snap, st_unloaded.data(), 1, &focus_near, 1, 0,
                       /*bAdvanceTimers=*/true, plan);
        EXPECT_TRUE(plan.vRequests.empty());
}

// ---------------------------------------------------------------------------
// Worker round-trip
// ---------------------------------------------------------------------------

TEST(StreamingDirectorWorkerTest, ReadRoundTrip_CommittedFixture) {

        StreamingDirector oDirector;

        EXPECT_TRUE(oDirector.DispatchRead(/*serial=*/1, /*ticket=*/7, /*index=*/2,
                        "resource/world/sublevels/sub_room.sesc"));
        EXPECT_EQ(oDirector.GetInFlightCount(), 1u);

        StreamingDirector::SReadResult res;
        bool got = false;
        for (int i = 0; i < 5000 && !got; ++i) {          // worker poll (~1 ms cycle)
                while (oDirector.TryPopResult(res)) { got = true; }
                if (!got) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        }
        ASSERT_TRUE(got);

        EXPECT_TRUE(res.ok);
        EXPECT_EQ(res.world_serial, 1u);
        EXPECT_EQ(res.ticket, 7u);
        EXPECT_EQ(res.index, 2u);
        ASSERT_NE(res.pBytes, nullptr);
        EXPECT_GT(res.pBytes->size(), 0u);

        // The bytes are exactly the file's.
        std::ifstream f("resource/world/sublevels/sub_room.sesc", std::ios::binary);
        ASSERT_TRUE(f.is_open());
        std::vector<char> vExpected((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
        EXPECT_EQ(*res.pBytes, vExpected);

        EXPECT_EQ(oDirector.GetInFlightCount(), 0u);   // popped => accounted
}

TEST(StreamingDirectorWorkerTest, ReadFailure_ReportsNotOk) {

        StreamingDirector oDirector;

        EXPECT_TRUE(oDirector.DispatchRead(1, 1, 0, "no/such/file.sesc"));

        StreamingDirector::SReadResult res;
        bool got = false;
        for (int i = 0; i < 5000 && !got; ++i) {
                while (oDirector.TryPopResult(res)) { got = true; }
                if (!got) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        }
        ASSERT_TRUE(got);
        EXPECT_FALSE(res.ok);
        EXPECT_EQ(res.pBytes, nullptr);
        EXPECT_EQ(oDirector.GetInFlightCount(), 0u);
}
