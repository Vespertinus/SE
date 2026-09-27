// ---------------------------------------------------------------------------
// world_bench — headless soak for the world_demo world (Stage 3 Step 11):
// teleporting-focus churn over the streaming director, per-frame cost under
// activation churn, and the crowd tick at load. google/benchmark binary; ONE
// TU for the SE_IMPL blocks (AnimBench precedent), worlddemo_app composition
// via target include priority.
//
//   ./world_bench --benchmark_format=json --benchmark_out=world_bench.json
//   ./world_bench --benchmark_min_time=2s
// ---------------------------------------------------------------------------

#include <benchmark/benchmark.h>

#define SE_IMPL
#include <GlobalTypes.h>

#include "../functional/WorldDemoWorldBuilder.h"

#include <world/WorldSystem.h>
#include <world/WorldAsset.h>
#include <terrain/TerrainSystem.h>
#include <ai/BlackboardSystem.h>
#include <world/SpatialIndexSystem.h>
#include <nav/NavigationSystem.h>
#include <nav/WorldNavBridge.h>
#include <nav/CrowdSystem.h>
#include <CharacterMovementSystem.h>
#include <EntityTemplateSystem.h>
#include <spawner/SpawnerSystem.h>
#include <ai/BehaviorTreeSystem.h>
#include <EntityManager.h>
#include <hsm/StateMachineSystem.h>

#include <random>

namespace {

using namespace SE;

// ---------------------------------------------------------------------------
// Shared world state: engine + systems + the no-streaming demo world.
// ---------------------------------------------------------------------------

struct WorldBenchState {
        std::unique_ptr<TSceneTree> scene;
        bool bLoaded = false;

        static WorldBenchState & Get() {
                static WorldBenchState s;
                return s;
        }

        // Registered AFTER the first TEngine touch, so its dtor runs before the
        // atexit ~Engine (atexit LIFO): node/component teardown still sees live
        // systems here. WorldBenchState itself is constructed before the engine
        // and must not own anything that outlives it.
        struct Teardown {
                ~Teardown() {
                        auto & oBench = WorldBenchState::Get();
                        if (oBench.scene) {
                                GetSystem<EntityManager>().SetSceneTree(nullptr);
                                GetSystem<WorldSystem>().SetSceneTree(nullptr);
                                GetSystem<TEntityTemplateSystem>().SetSceneTree(nullptr);
                                oBench.scene.reset();
                        }
                        GetSystem<TResourceManager>().ProcessDeferred();
                        GetSystem<TResourceManager>().DestroyUnretained();
                        GetSystem<TResourceManager>().ClearAllRetains();
                        GetSystem<TResourceManager>().ProcessDeferred();
                }
        };

        void Ensure() {
                if (bLoaded) return;
                static bool systems_constructed = []() {
                        auto & oEngine = TEngine::Instance();
                        oEngine.Init<TResourceManager>();
                        oEngine.Init<Config>();
                        oEngine.Init<EventManager>();
                        oEngine.Init<FrameAllocator>();
                        GetSystem<Config>().sResourceDir = "resource/";
                        oEngine.Init<EntityManager>();
                        oEngine.Init<StateMachineSystem>();
                        oEngine.Init<WorldSystem>();
                        oEngine.Init<TerrainSystem>();
                        oEngine.Init<BlackboardSystem>();
                        oEngine.Init<SpatialIndexSystem>();
                        oEngine.Init<NavigationSystem>();
                        oEngine.Init<CharacterMovementSystem>();
                        oEngine.Init<WorldNavBridge>();
                        oEngine.Init<CrowdSystem>();
                        oEngine.Init<TEntityTemplateSystem>();
                        oEngine.Init<SpawnerSystem>();
                        oEngine.Init<BTSystem>();
                        static Teardown sTeardown;
                        return true;
                }();
                (void)systems_constructed;

                if (!scene) {
                        scene = std::make_unique<TSceneTree>("world_bench_scene", 0, true);
                        auto & oEM      = GetSystem<EntityManager>();
                        auto & oWorlds  = GetSystem<WorldSystem>();
                        auto & oETS     = GetSystem<TEntityTemplateSystem>();
                        auto & oSpawner = GetSystem<SpawnerSystem>();
                        oEM.SetSceneTree(scene.get());
                        oWorlds.SetSceneTree(scene.get());
                        oETS.SetSceneTree(scene.get());
                        oSpawner.SetIndex(&GetSystem<SpatialIndexSystem>());
                }

                if (!GetSystem<WorldSystem>().GetWorldAsset()) {
                        // the GL-free demo mirror: same topology, headless-safe.
                        // WorldAsset throws on any load problem — surface the
                        // reason in the log instead of an anonymous terminate.
                        try {
                                se_test::BuildDemoMirrorWorld();
                                GetSystem<WorldSystem>().LoadWorld(
                                                "resource/world/gen/worlddemo/world_demo_test.seworld");
                        }
                        catch (const std::exception & ex) {
                                log_e("world_bench: demo mirror world failed to load: '{}'", ex.what());
                                throw;
                        }
                }
                bLoaded = true;
        }

        void StepFrame(float dt) {
                SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EInputUpdate{dt});
                SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EUpdate{dt});
                SE::GetSystem<SE::EventManager>().Process();
                SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EPostUpdate{dt});
                GetSystem<FrameAllocator>().reset();
        }
};

// ---------------------------------------------------------------------------
// Streaming churn: the focus teleports between the village and the west
// forest camp — the exact soak the stage plan §14 specifies (unload distance
// 85 m, so every hop evicts one cell and requests the other).
// ---------------------------------------------------------------------------

void FocusChurn(benchmark::State & oState) {
        auto & oBench = WorldBenchState::Get();
        try {
                oBench.Ensure();
        }
        catch (const std::exception & ex) {
                oState.SkipWithError(ex.what());
                return;
        }
        auto & oWorlds = GetSystem<WorldSystem>();

        static uint32_t sFocusSeq = 0;
        auto pFocus = oBench.scene->Create("BenchFocus_" + std::to_string(++sFocusSeq));
        auto hFocus = oWorlds.RegisterFocus(pFocus, 55.f, 0.f);

        std::mt19937 rng{20260917u};
        std::uniform_real_distribution<float> dist(-6.f, 6.f);
        const glm::vec3 vVillage{0.f, 3.f, 0.f};
        const glm::vec3 vCamp{-76.f, 3.f, 4.f};

        double max_frame_ms = 0.0;
        uint64_t activations = 0;
        uint32_t hops = 0;

        for (auto _ : oState) {
                // hop every ~30 frames (0.5 s at 60 Hz)
                if ((oState.iterations() % 30) == 0) {
                        const bool bToCamp = (hops % 2) == 1;
                        const glm::vec3 vTarget = (bToCamp ? vCamp : vVillage)
                                                + glm::vec3{dist(rng), 0.f, dist(rng)};
                        pFocus->SetPos(vTarget);
                        ++hops;
                }

                const auto t0 = std::chrono::steady_clock::now();
                oBench.StepFrame(1.f / 60.f);
                const auto t1 = std::chrono::steady_clock::now();

                const double frame_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                max_frame_ms = std::max(max_frame_ms, frame_ms);

                uint32_t active = 0;
                for (uint32_t i = 0; i < oWorlds.GetSubLevelCount(); ++i) {
                        if (oWorlds.GetLevelState(i) == WorldSystem::ELevelState::Active) { ++active; }
                }
                activations += active;
        }

        oState.counters["max_frame_ms"] = max_frame_ms;
        oState.counters["level_active_frames"] = activations;
        oState.counters["hops"] = hops;

        oWorlds.UnregisterFocus(hFocus);
        pFocus.reset();
        oWorlds.UnloadWorld();
        for (uint32_t i = 0; i < 8; ++i) { oBench.StepFrame(1.f / 60.f); }
        oBench.bLoaded = false;
}
BENCHMARK(FocusChurn)->Unit(benchmark::kMillisecond);

// ---------------------------------------------------------------------------
// Steady state: all levels up, fixed frame cost of the full Stage 3 tick.
// ---------------------------------------------------------------------------

void SteadyState(benchmark::State & oState) {
        auto & oBench = WorldBenchState::Get();
        try {
                oBench.Ensure();
                if (!GetSystem<WorldSystem>().GetWorldAsset()) {
                        GetSystem<WorldSystem>().LoadWorld(
                                        "resource/world/gen/worlddemo/world_demo_test.seworld");
                }
        }
        catch (const std::exception & ex) {
                oState.SkipWithError(ex.what());
                return;
        }

        auto & oWorlds = GetSystem<WorldSystem>();
        for (uint32_t i = 0; i < 30; ++i) { oBench.StepFrame(1.f / 60.f); }

        double max_frame_ms = 0.0;
        double sum_ms = 0.0;
        uint64_t frames = 0;

        for (auto _ : oState) {
                const auto t0 = std::chrono::steady_clock::now();
                oBench.StepFrame(1.f / 60.f);
                const auto t1 = std::chrono::steady_clock::now();

                const double frame_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
                max_frame_ms = std::max(max_frame_ms, frame_ms);
                sum_ms += frame_ms;
                ++frames;
        }

        oState.counters["avg_frame_ms"] = frames ? sum_ms / frames : 0.0;
        oState.counters["max_frame_ms"] = max_frame_ms;

        oWorlds.UnloadWorld();
        for (uint32_t i = 0; i < 8; ++i) { oBench.StepFrame(1.f / 60.f); }
        oBench.bLoaded = false;
}
BENCHMARK(SteadyState)->Unit(benchmark::kMillisecond);

} // namespace
