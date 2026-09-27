
// ---------------------------------------------------------------------------
// StreamingTest — Tier C: the full streaming pipeline (director + worker +
// budgeted drain + hysteresis) over the world_app composition, driven through
// in-test built .sesc/.seworld fixtures like WorldSystemTest.
//
// The worker is a real thread: tests pump frames with a ~1 ms wall sleep per
// step and assert STATE PROGRESSION, never exact frame counts — except the
// budget/cap tests, which count dependency creation and instantiations
// (deterministic regardless of worker latency).
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>
#include "EngineFixture.h"
#include "PhysicsHarness.h"
#include "WorldAssets.h"

#include <world/WorldSystem.h>
#include <world/WorldAsset.h>
#include <EntityManager.h>
#include <hsm/StateMachine.h>

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>

#include <chrono>
#include <algorithm>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace SE;

namespace {

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class StreamingTest : public se_test::EnginePhysTestBase {

protected:
        void SetUp() override {
                EnginePhysTestBase::SetUp();

                static bool systems_constructed = []() {
                        auto & oEngine = TEngine::Instance();
                        oEngine.Init<EntityManager>();
                        oEngine.Init<StateMachineSystem>();
                        oEngine.Init<WorldSystem>();
                        return true;
                }();
                (void)systems_constructed;

                GetSystem<EntityManager>().SetSceneTree(scene.get());
                GetSystem<WorldSystem>().SetSceneTree(scene.get());

                boost::filesystem::create_directories("resource/world/gen");
                WriteFixtureScenes();
        }

        void TearDown() override {
                auto & oWorldSystem = GetSystem<WorldSystem>();

                oWorldSystem.UnloadWorld();
                Drain();

                oWorldSystem.SetSceneTree(nullptr);
                GetSystem<EntityManager>().SetSceneTree(nullptr);

                GetSystem<TResourceManager>().Clear<StateMachineAsset>();

                EnginePhysTestBase::TearDown();
        }

        void Drain(const uint32_t n = 2) {
                for (uint32_t i = 0; i < n; ++i) { StepFrame(1.f / 60.f); }
        }

        size_t CountNodes() {
                size_t n = 0;
                scene->GetRoot()->DepthFirstWalk([&n](auto &) { ++n; return true; });
                return n;
        }

        WorldSystem & Worlds() { return GetSystem<WorldSystem>(); }

        // Pump frames (with real-time sleeps so the worker can deliver) until
        // pred() holds. Returns pred() at exhaustion.
        template <class TPred>
        bool PumpUntil(TPred pred, const uint32_t max_frames = 2000) {
                for (uint32_t i = 0; i < max_frames; ++i) {
                        StepFrame(1.f / 60.f);
                        if (pred()) { return true; }
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                return pred();
        }

        // A registered streaming focus node; move it with SetPos.
        TSceneTree::TSceneNode MakeFocus(const glm::vec3 vPos) {
                auto pFocus = scene->Create("focus");
                pFocus->SetPos(vPos);
                Worlds().RegisterFocus(pFocus, 0.f, 0.f);
                return pFocus;
        }

        // World: N streamed copies of the shared cell scene placed at (cx, cz),
        // a 4x4 footprint level. focus stays wherever the test moves it.
        H<WorldAsset> BuildStreamWorld(const std::string & sName,
                        const std::vector<std::pair<float, float>> & vPositions,
                        const float load_radius, const float unload_radius,
                        const uint32_t unload_frame_delay,
                        const float main_budget_ms = 2.f,
                        const uint32_t max_activations = 1,
                        const uint32_t deps_level = ~0u) {

                auto pWorldT = se_test::MakeWorld(sName);
                se_test::SetStreaming(*pWorldT, /*enabled=*/true, load_radius, unload_radius,
                                unload_frame_delay, main_budget_ms, max_activations);

                for (uint32_t i = 0; i < vPositions.size(); ++i) {
                        const auto [cx, cz] = vPositions[i];
                        const std::string & sScene = (i == deps_level)
                                        ? sDepsScenePath : sCellScenePath;
                        se_test::AddSubLevel(*pWorldT, sScene,
                                        FlatBuffers::Vec3(cx - 2.f, 0.f, cz - 2.f),
                                        FlatBuffers::Vec3(cx + 2.f, 4.f, cz + 2.f));
                }

                auto vBytes = se_test::BakeWorld(*pWorldT);
                return CreateResource<WorldAsset>(sName, FlatBuffers::GetWorld(vBytes.data()));
        }

        // All four lifecycle events as "event#level" strings.
        struct StreamEventSink {
                std::vector<std::string> vLog;

                StreamEventSink() {
                        auto & oEM = GetSystem<EventManager>();
                        oEM.AddListener<ESubLevelActivating,   &StreamEventSink::OnActivating>(this);
                        oEM.AddListener<ESubLevelActivated,    &StreamEventSink::OnActivated>(this);
                        oEM.AddListener<ESubLevelDeactivating, &StreamEventSink::OnDeactivating>(this);
                        oEM.AddListener<ESubLevelDeactivated,  &StreamEventSink::OnDeactivated>(this);
                }
                ~StreamEventSink() {
                        auto & oEM = GetSystem<EventManager>();
                        oEM.RemoveListener<ESubLevelActivating,   &StreamEventSink::OnActivating>(this);
                        oEM.RemoveListener<ESubLevelActivated,    &StreamEventSink::OnActivated>(this);
                        oEM.RemoveListener<ESubLevelDeactivating, &StreamEventSink::OnDeactivating>(this);
                        oEM.RemoveListener<ESubLevelDeactivated,  &StreamEventSink::OnDeactivated>(this);
                }
                void OnActivating(const Event & e)   { vLog.push_back("activating#"   + std::to_string(e.Get<ESubLevelActivating>().sub_level)); }
                void OnActivated(const Event & e)    { vLog.push_back("activated#"    + std::to_string(e.Get<ESubLevelActivated>().sub_level)); }
                void OnDeactivating(const Event & e) { vLog.push_back("deactivating#" + std::to_string(e.Get<ESubLevelDeactivating>().sub_level)); }
                void OnDeactivated(const Event & e)  { vLog.push_back("deactivated#"  + std::to_string(e.Get<ESubLevelDeactivated>().sub_level)); }
        };

        static constexpr const char * sCellScenePath = "resource/world/gen/stream_cell.sesc";
        static constexpr const char * sDepsScenePath = "resource/world/gen/stream_deps.sesc";

private:
        void WriteFixtureScenes() {
                static bool written = []() {
                        // Plain cell: two nodes, no declared resources.
                        {
                                auto oRoot = se_test::MakeNode("Cell");
                                oRoot.children.push_back(std::make_unique<FlatBuffers::NodeT>(
                                                se_test::MakeNode("Prop")));
                                const auto vBytes = se_test::BakeScene(std::move(oRoot));
                                se_test::WriteBytes(sCellScenePath, vBytes);
                        }
                        // Dependency cell: three distinct StateMachine paths => three
                        // separately budgetable dependency chunks.
                        {
                                auto oRoot = se_test::MakeNode("DepsCell");
                                oRoot.children.push_back(std::make_unique<FlatBuffers::NodeT>(
                                                se_test::MakeNodeWithSM("Npc", "resource/hsm/npc_ai.sesm")));
                                oRoot.children.push_back(std::make_unique<FlatBuffers::NodeT>(
                                                se_test::MakeNodeWithSM("Walker", "resource/hsm/character_movement.sesm")));
                                oRoot.children.push_back(std::make_unique<FlatBuffers::NodeT>(
                                                se_test::MakeNodeWithSM("Player", "resource/hsm/player_behavior.sesm")));
                                const auto vBytes = se_test::BakeSceneWithStateMachineResources(std::move(oRoot));
                                se_test::WriteBytes(sDepsScenePath, vBytes);
                        }
                        return true;
                }();
                (void)written;
        }
};

} // namespace

// ---------------------------------------------------------------------------
// Distance-driven async activation
// ---------------------------------------------------------------------------

TEST_F(StreamingTest, AsyncActivation_ByDistance) {

        auto hWorld = BuildStreamWorld("stream_basic", { { 100.f, 0.f } },
                        /*load=*/40.f, /*unload=*/64.f, /*delay=*/60);
        ASSERT_TRUE(Worlds().LoadWorld(hWorld));

        StreamEventSink oSink;
        auto pFocus = MakeFocus(glm::vec3(0.f, 1.f, 0.f));

        // Far away: nothing streams in.
        Drain(5);
        EXPECT_EQ(Worlds().GetLevelState(0), WorldSystem::ELevelState::Unloaded);
        EXPECT_TRUE(oSink.vLog.empty());

        // Move inside the load radius: Requested -> Loading -> Active.
        pFocus->SetPos(glm::vec3(100.f, 1.f, 0.f));
        ASSERT_TRUE(PumpUntil([&] {
                        return Worlds().GetLevelState(0) == WorldSystem::ELevelState::Active; }))
                << "level never activated";

        EXPECT_EQ(oSink.vLog, std::vector<std::string>({ "activating#0", "activated#0" }));
        EXPECT_NE(scene->FindFullName(StrID("root|world|stream_cell.sesc#0|Cell|Prop")), nullptr);

        const auto oStats = Worlds().GetStreamingStats();
        EXPECT_EQ(oStats.active, 1u);
        EXPECT_EQ(oStats.wanted, 1u);
}

// A synchronous FlushStreaming must converge while a Manual level is up. The
// planner never counts Manual levels, so the stats must not either — the
// round-7 "FlushStreaming timed out (wanted 4 / active 6)" was exactly this
// asymmetry busy-waiting the full 5 s timeout on every crypt exit.
TEST_F(StreamingTest, FlushStreaming_ConvergesWhileManualLevelActive) {

        auto pWorldT = se_test::MakeWorld("stream_flush_manual");
        se_test::SetStreaming(*pWorldT, /*enabled=*/true, 40.f, 64.f, 60);
        se_test::AddSubLevel(*pWorldT, sCellScenePath,
                        FlatBuffers::Vec3(-2.f, 0.f, -2.f), FlatBuffers::Vec3(2.f, 4.f, 2.f));
        // the "interior": Manual (flag 2) — explicit game control only
        se_test::AddSubLevel(*pWorldT, sCellScenePath,
                        FlatBuffers::Vec3(8.f, 0.f, -2.f), FlatBuffers::Vec3(12.f, 4.f, 2.f),
                        /*flags=*/0x2u);
        auto vBytes = se_test::BakeWorld(*pWorldT);
        auto hWorld = CreateResource<WorldAsset>("stream_flush_manual",
                        FlatBuffers::GetWorld(vBytes.data()));
        ASSERT_TRUE(Worlds().LoadWorld(hWorld));

        auto pFocus = MakeFocus(glm::vec3(0.f, 1.f, 0.f));
        ASSERT_TRUE(PumpUntil([&] { return Worlds().IsLevelActive(0); }));

        // the game activates the interior explicitly (the crypt flow)
        ASSERT_TRUE(Worlds().ActivateLevel(1));
        EXPECT_TRUE(Worlds().IsLevelActive(1));

        EXPECT_TRUE(Worlds().FlushStreaming(2000.f))
                        << "flush never converged while a Manual level was active";

        const auto oStats = Worlds().GetStreamingStats();
        EXPECT_EQ(oStats.active, 1u);   // Manual excluded from the counts...
        EXPECT_EQ(oStats.wanted, 1u);
        EXPECT_TRUE(Worlds().IsLevelActive(1));   // ...yet the interior stays up
}

TEST_F(StreamingTest, Hysteresis_BandHoldsThenUnloadsAfterDelay) {

        // load 40 / unload 64 / delay 5, level at the origin.
        auto hWorld = BuildStreamWorld("stream_hysteresis", { { 0.f, 0.f } },
                        40.f, 64.f, /*delay=*/5);
        ASSERT_TRUE(Worlds().LoadWorld(hWorld));

        StreamEventSink oSink;
        auto pFocus = MakeFocus(glm::vec3(0.f, 1.f, 0.f));

        ASSERT_TRUE(PumpUntil([&] {
                        return Worlds().IsLevelActive(0); }));

        // Into the hysteresis band (distance 50): holds Active indefinitely.
        pFocus->SetPos(glm::vec3(50.f, 1.f, 0.f));
        PumpUntil([&] { return false; }, 40);
        EXPECT_TRUE(Worlds().IsLevelActive(0));
        EXPECT_TRUE(oSink.vLog.size() == 2);   // still just activating/activated

        // Beyond the unload radius: exactly `delay` frames of eviction pending.
        pFocus->SetPos(glm::vec3(100.f, 1.f, 0.f));   // distance 98 > 64
        StepFrame(1.f / 60.f);
        for (uint32_t i = 0; i < 3; ++i) {              // delay - 2 frames
                StepFrame(1.f / 60.f);
                EXPECT_EQ(Worlds().GetLevelState(0), WorldSystem::ELevelState::Active)
                                << "unloaded early at frame " << i;
        }
        // Frame `delay`: the director fires Deactivating, queues the deferred
        // destroy, and the SAME frame's EPostUpdate flush completes the teardown.
        StepFrame(1.f / 60.f);
        EXPECT_EQ(Worlds().GetLevelState(0), WorldSystem::ELevelState::Unloaded);
        EXPECT_TRUE(Worlds().GetLevelRoot(0).expired());

        ASSERT_EQ(oSink.vLog.size(), 4u);
        EXPECT_EQ(oSink.vLog[2], "deactivating#0");
        EXPECT_EQ(oSink.vLog[3], "deactivated#0");
}

TEST_F(StreamingTest, NoThrash_BoundaryOscillation) {

        auto hWorld = BuildStreamWorld("stream_oscillate", { { 0.f, 0.f } },
                        40.f, 64.f, /*delay=*/3);
        ASSERT_TRUE(Worlds().LoadWorld(hWorld));

        StreamEventSink oSink;
        auto pFocus = MakeFocus(glm::vec3(30.f, 1.f, 0.f));   // distance 28: wanted

        ASSERT_TRUE(PumpUntil([&] { return Worlds().IsLevelActive(0); }));

        // Oscillate across the load boundary, never leaving the band: the level
        // must neither unload nor refire any lifecycle event.
        for (int i = 0; i < 40; ++i) {
                pFocus->SetPos(glm::vec3((i % 2 == 0) ? 50.f : 30.f, 1.f, 0.f));
                StepFrame(1.f / 60.f);
        }

        EXPECT_TRUE(Worlds().IsLevelActive(0));
        ASSERT_EQ(oSink.vLog.size(), 2u);   // activating#0, activated#0 only
}

TEST_F(StreamingTest, NearestFirst_DeterministicOrder) {

        // Level 0 at distance 18, level 1 at distance 32 (both wanted).
        auto hWorld = BuildStreamWorld("stream_order",
                        { { 0.f, 20.f }, { 0.f, 34.f } },
                        40.f, 64.f, /*delay=*/60, /*budget=*/2.f,
                        /*max_activations=*/1);
        ASSERT_TRUE(Worlds().LoadWorld(hWorld));

        StreamEventSink oSink;
        MakeFocus(glm::vec3(0.f, 1.f, 0.f));

        ASSERT_TRUE(PumpUntil([&] { return Worlds().IsLevelActive(0) && Worlds().IsLevelActive(1); }));

        // activation order: nearest (0) first, index tie-break never needed here
        auto vActivated = std::vector<std::string>(oSink.vLog);
        vActivated.erase(std::remove_if(vActivated.begin(), vActivated.end(),
                        [](const std::string & s) { return s.find("activated") != 0; }),
                        vActivated.end());
        EXPECT_EQ(vActivated, std::vector<std::string>({ "activated#0", "activated#1" }));
}

// ---------------------------------------------------------------------------
// Cancellation
// ---------------------------------------------------------------------------

TEST_F(StreamingTest, CancelBeforeStart_NoEventsNoPins) {

        auto hWorld = BuildStreamWorld("stream_cancel", { { 100.f, 0.f } },
                        40.f, 64.f, 60);
        ASSERT_TRUE(Worlds().LoadWorld(hWorld));

        StreamEventSink oSink;
        auto pFocus = MakeFocus(glm::vec3(0.f, 1.f, 0.f));

        // Bring the level into range until it is staged (Requested or Loading).
        pFocus->SetPos(glm::vec3(100.f, 1.f, 0.f));
        ASSERT_TRUE(PumpUntil([&] {
                        const auto st = Worlds().GetLevelState(0);
                        return st == WorldSystem::ELevelState::Requested ||
                               st == WorldSystem::ELevelState::Loading; }))
                << "level never staged";

        // Out of range before activation: cancelled, gameplay never saw it.
        pFocus->SetPos(glm::vec3(0.f, 1.f, 0.f));
        ASSERT_TRUE(PumpUntil([&] {
                        return Worlds().GetLevelState(0) == WorldSystem::ELevelState::Unloaded; }));

        // Let any in-flight worker result arrive and be dropped.
        PumpUntil([&] { return false; }, 20);

        EXPECT_TRUE(oSink.vLog.empty());
        EXPECT_EQ(Worlds().GetStreamingStats().pinned_total, 0u);

        // The level is immediately re-requestable (cooldown only follows failures).
        pFocus->SetPos(glm::vec3(100.f, 1.f, 0.f));
        ASSERT_TRUE(PumpUntil([&] { return Worlds().IsLevelActive(0); }));
        EXPECT_EQ(oSink.vLog, std::vector<std::string>({ "activating#0", "activated#0" }));
}

// ---------------------------------------------------------------------------
// Budgets
// ---------------------------------------------------------------------------

TEST_F(StreamingTest, BudgetChunking_OneDependencyPerFrame) {

        // main_budget_ms = 0: exactly one declared dependency per pass (the
        // progress guarantee), so a 3-dependency level activates across frames.
        auto hWorld = BuildStreamWorld("stream_budget", { { 0.f, 0.f } },
                        40.f, 64.f, 60, /*budget=*/0.f, /*max_act=*/1,
                        /*deps_level=*/0);
        ASSERT_TRUE(Worlds().LoadWorld(hWorld));

        MakeFocus(glm::vec3(0.f, 1.f, 0.f));

        bool saw_one = false, saw_two = false;
        uint32_t loading_frames = 0;

        ASSERT_TRUE(PumpUntil([&] {
                        uint32_t done = 0, total = 0;
                        if (Worlds().GetLevelDepProgress(0, done, total)) {
                                if (total == 3 && done == 1) { saw_one = true; }
                                if (total == 3 && done == 2) { saw_two = true; }
                                ++loading_frames;
                        }
                        return Worlds().IsLevelActive(0);
                        }))
                << "level with budgeted dependencies never activated";

        EXPECT_TRUE(saw_one) << "never observed the 1/3 chunk";
        EXPECT_TRUE(saw_two) << "never observed the 2/3 chunk";
        EXPECT_GE(loading_frames, 3u);

        // All three state machines are live on the instantiated nodes.
        EXPECT_NE(scene->FindFullName(StrID("root|world|stream_deps.sesc#0|DepsCell|Npc")), nullptr);
        EXPECT_NE(scene->FindFullName(StrID("root|world|stream_deps.sesc#0|DepsCell|Walker")), nullptr);
        EXPECT_NE(scene->FindFullName(StrID("root|world|stream_deps.sesc#0|DepsCell|Player")), nullptr);
}

TEST_F(StreamingTest, MaxActivationsPerFrame_OnePerFrame) {

        // Two fully-drained levels, activation cap 1: their Activated events
        // must land on different frames.
        auto hWorld = BuildStreamWorld("stream_cap",
                        { { 0.f, 20.f }, { 0.f, 34.f } },
                        40.f, 64.f, 60, /*budget=*/2.f, /*max_act=*/1);
        ASSERT_TRUE(Worlds().LoadWorld(hWorld));

        StreamEventSink oSink;
        MakeFocus(glm::vec3(0.f, 1.f, 0.f));

        std::vector<std::pair<std::string, uint32_t>> vStamped;
        uint32_t frame = 0;
        uint32_t activated_seen = 0;

        while (activated_seen < 2 && frame < 2000) {
                const size_t before = oSink.vLog.size();
                StepFrame(1.f / 60.f);
                ++frame;
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                for (size_t k = before; k < oSink.vLog.size(); ++k) {
                        // activating/activated fire back-to-back; only Activated
                        // counts against the per-frame cap.
                        if (oSink.vLog[k].rfind("activated#", 0) == 0) {
                                vStamped.emplace_back(oSink.vLog[k], frame);
                                ++activated_seen;
                        }
                }
        }

        ASSERT_EQ(vStamped.size(), 2u);
        EXPECT_EQ(vStamped[0].first, "activated#0");
        EXPECT_EQ(vStamped[1].first, "activated#1");
        EXPECT_NE(vStamped[0].second, vStamped[1].second) << "two activations in one frame";
}

// ---------------------------------------------------------------------------
// Churn / convergence
// ---------------------------------------------------------------------------

TEST_F(StreamingTest, Churn_TeleportLoop_ConvergesAndCleansUp) {

        auto hWorld = BuildStreamWorld("stream_churn",
                        { { 0.f, 0.f }, { 300.f, 0.f } },
                        40.f, 64.f, /*delay=*/3);
        ASSERT_TRUE(Worlds().LoadWorld(hWorld));

        StreamEventSink oSink;
        auto pFocus = MakeFocus(glm::vec3(0.f, 1.f, 0.f));

        for (int i = 0; i < 6; ++i) {
                const bool bNearA = (i % 2 == 0);

                pFocus->SetPos(bNearA ? glm::vec3(0.f, 1.f, 0.f)
                                      : glm::vec3(300.f, 1.f, 0.f));

                const bool bOk = PumpUntil([&] {
                        const bool a = Worlds().IsLevelActive(0);
                        const bool b = Worlds().IsLevelActive(1);
                        return bNearA ? (a && !b) : (b && !a);
                });
                ASSERT_TRUE(bOk) << "churn iteration " << i << " did not converge";
        }

        // Settle near level 0 and verify teardown completeness.
        pFocus->SetPos(glm::vec3(0.f, 1.f, 0.f));
        ASSERT_TRUE(PumpUntil([&] {
                        return Worlds().IsLevelActive(0) &&
                               Worlds().GetLevelState(1) == WorldSystem::ELevelState::Unloaded; }));

        EXPECT_EQ(Worlds().GetStreamingStats().pinned_total, 0u);   // level 1 fully unpinned
        // root + world + dynamics + focus + (wrapper + Cell + Prop)
        EXPECT_EQ(CountNodes(), 7u);
}

// ---------------------------------------------------------------------------
// Flush (loading-screen mode)
// ---------------------------------------------------------------------------

TEST_F(StreamingTest, FlushStreaming_BudgetFreeBurst) {

        auto hWorld = BuildStreamWorld("stream_flush", { { 0.f, 0.f } },
                        40.f, 64.f, /*delay=*/60,
                        /*budget=*/0.f /*flush ignores it*/);
        ASSERT_TRUE(Worlds().LoadWorld(hWorld));

        StreamEventSink oSink;
        MakeFocus(glm::vec3(0.f, 1.f, 0.f));

        // No frames stepped: Flush drives the whole pipeline itself.
        EXPECT_TRUE(Worlds().FlushStreaming(3000.f));
        EXPECT_TRUE(Worlds().IsLevelActive(0));
        EXPECT_EQ(oSink.vLog, std::vector<std::string>({ "activating#0", "activated#0" }));
}

// ---------------------------------------------------------------------------
// Determinism
// ---------------------------------------------------------------------------

TEST_F(StreamingTest, Determinism_ScriptedPath_SameEventSequence) {

        const std::vector<std::pair<float, float>> vPositions = {
                { 0.f, 20.f }, { 0.f, 34.f }, { 300.f, 0.f } };

        std::vector<std::string> vFirstRun;

        for (int run = 0; run < 2; ++run) {

                // Fresh world each run (previous one fully drained by TearDown-like path).
                if (run == 1) {
                        Worlds().UnloadWorld();
                        PumpUntil([&] {
                                for (uint32_t i = 0; i < Worlds().GetSubLevelCount(); ++i) {
                                        if (Worlds().GetLevelState(i) != WorldSystem::ELevelState::Unloaded) {
                                                return false;
                                        }
                                }
                                return true;
                        });
                }

                auto hWorld = BuildStreamWorld("stream_determinism", vPositions,
                                40.f, 64.f, /*delay=*/5);
                ASSERT_TRUE(Worlds().LoadWorld(hWorld));

                StreamEventSink oSink;

                // Unique per run: run 0's focus node survives UnloadWorld (it
                // lives under the scene root, not the world root).
                auto pFocus = scene->Create("focus_" + std::to_string(run));
                pFocus->SetPos(glm::vec3(0.f, 1.f, 0.f));
                Worlds().RegisterFocus(pFocus, 0.f, 0.f);

                auto vScript = std::vector<glm::vec3>({
                        glm::vec3(0.f, 1.f, 0.f),    // both near levels wanted
                        glm::vec3(400.f, 1.f, 0.f),  // everything evicted
                        glm::vec3(0.f, 1.f, 34.f),   // near again (level 1 closest)
                });

                for (const auto & vPos : vScript) {
                        pFocus->SetPos(vPos);
                        PumpUntil([&] { return false; }, 90);   // let the director react
                }

                if (run == 0) {
                        vFirstRun = oSink.vLog;
                }
                else {
                        ASSERT_EQ(oSink.vLog, vFirstRun)
                                << "streaming event sequence differs between identical runs";
                }
        }
}
