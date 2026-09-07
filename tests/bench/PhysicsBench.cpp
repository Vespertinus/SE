
// ---------------------------------------------------------------------------
// Physics subsystem benchmarks (google/benchmark).
//
// One TU for the SE_IMPL engine blocks (see tests/CMakeLists.txt). A single
// PhysicsSystem per process (the Jolt Factory is a process global — see
// tests/functional/PhysicsHarness.h) is shared by the benchmarks; each sets
// up its scene with timing paused and tears it down after. One iteration ==
// one 60 Hz frame of PhysicsSystem::Update.
//
//   ./phys_bench --benchmark_format=json --benchmark_out=phys_bench.json
//   ./phys_bench --benchmark_min_time=0.05s        (smoke)
// ---------------------------------------------------------------------------

#include <benchmark/benchmark.h>
#include <glm/glm.hpp>

#define SE_IMPL
#include <Logging.h>
#include <GlobalTypes.h>

#include <PhysicsTypes.h>

#include <cmath>
#include <random>
#include <vector>

namespace {

using namespace SE;

constexpr float kDt = 1.0f / 60.0f;

// ---------------------------------------------------------------------------
// Shared runtime: one PhysicsSystem for the whole process.
// ---------------------------------------------------------------------------

struct PhysRuntime {

        PhysicsSystem phys;

        PhysRuntime() {
                // DrainContacts delivers EPhysics* through the engine
                // EventManager singleton — the bench must construct it even
                // though it drives no other engine systems.
                TEngine::Instance().Init<EventManager>();

                PhysicsConfig cfg;
                cfg.max_bodies              = 32768;
                cfg.max_body_pairs          = 262144;
                cfg.max_contact_constraints = 32768;
                phys.Init(cfg);
        }
        ~PhysRuntime() { phys.Shutdown(); }
};

PhysRuntime& Runtime() {
        static PhysRuntime rt;
        return rt;
}

// ---------------------------------------------------------------------------
// Scene builders (call with timing paused)
// ---------------------------------------------------------------------------

BodyHandle MakeFloor(PhysicsSystem& phys, float half = 30.0f) {
        RigidBodyDesc d;
        d.oCollider.type         = ColliderDesc::Box;
        d.oCollider.vHalfExtents = {half, 0.5f, half};
        d.vInitialPosition       = {0.0f, -0.5f, 0.0f};
        d.is_static              = true;
        return phys.CreateRigidBody(d);
}

std::vector<BodyHandle> MakeSphereField(PhysicsSystem& phys, int n,
                                        float y0, float radius = 0.5f) {
        std::vector<BodyHandle> bodies;
        bodies.reserve(size_t(n));
        const int side = int(std::ceil(std::sqrt(float(n))));
        RigidBodyDesc d;
        d.oCollider.type   = ColliderDesc::Sphere;
        d.oCollider.radius = radius;
        d.mass             = 1.0f;
        for (int i = 0; i < n; ++i) {
                const int x = i % side, z = i / side;
                d.vInitialPosition = {x * 1.2f, y0 + 0.3f * float(i % 7), z * 1.2f};
                bodies.push_back(phys.CreateRigidBody(d));
        }
        return bodies;
}

std::vector<BodyHandle> MakeTower(PhysicsSystem& phys, int levels) {
        std::vector<BodyHandle> bodies;
        bodies.reserve(size_t(levels));
        RigidBodyDesc d;
        d.oCollider.type         = ColliderDesc::Box;
        d.oCollider.vHalfExtents = {0.4f, 0.4f, 0.4f};
        d.mass                   = 1.0f;
        d.friction               = 0.8f;
        d.restitution            = 0.0f;
        for (int i = 0; i < levels; ++i) {
                d.vInitialPosition = {0.0f, 0.4f + 0.8f * float(i), 0.0f};
                bodies.push_back(phys.CreateRigidBody(d));
        }
        return bodies;
}

std::vector<CharHandle> MakeCharacters(PhysicsSystem& phys, int n) {
        std::vector<CharHandle> chars;
        chars.reserve(size_t(n));
        CharacterDesc d;
        const int side = int(std::ceil(std::sqrt(float(n))));
        for (int i = 0; i < n; ++i) {
                const int x = i % side, z = i / side;
                d.vInitialPosition = {x * 2.0f, 1.3f, z * 2.0f};
                CharHandle h = phys.CreateCharacter(d);
                phys.SetCharacterVelocity(h, {0.5f, 0.0f, 0.0f});   // keeps them stepping
                chars.push_back(h);
        }
        return chars;
}

void DestroyAll(PhysicsSystem& phys, const std::vector<BodyHandle>& bodies) {
        for (const BodyHandle h : bodies) phys.DestroyBody(h);
}

void DestroyChars(PhysicsSystem& phys, const std::vector<CharHandle>& chars) {
        for (const CharHandle h : chars) phys.DestroyCharacter(h);
}

// ===========================================================================
// Creation / destruction
// ===========================================================================

static void BM_CreateDestroyBody(benchmark::State& state) {
        auto& phys = Runtime().phys;
        RigidBodyDesc d;
        d.oCollider.type   = ColliderDesc::Sphere;
        d.oCollider.radius = 0.5f;
        d.mass             = 1.0f;

        for (auto _ : state) {
                BodyHandle h = phys.CreateRigidBody(d);
                phys.DestroyBody(h);   // deferred — measured cost is the queue push
                benchmark::DoNotOptimize(h);
        }
        phys.Update(kDt);          // flush the destroys
}
BENCHMARK(BM_CreateDestroyBody);

// ===========================================================================
// Per-frame simulation cost (1 iteration == 1 frame)
// ===========================================================================

// Integration-heavy, contact-free: spheres in slow free fall (gravity_scale
// scaled so they stay airborne for the whole run).
static void BM_Update_FreeFallSphereField(benchmark::State& state) {
        auto& phys = Runtime().phys;
        const int n = state.range(0);
        RigidBodyDesc d;
        d.oCollider.type   = ColliderDesc::Sphere;
        d.oCollider.radius = 0.5f;
        d.mass             = 1.0f;
        d.gravity_scale    = 0.02f;   // ~0.2 m/s² — never lands mid-benchmark
        std::vector<BodyHandle> bodies;
        bodies.reserve(size_t(n));
        const int side = int(std::ceil(std::sqrt(float(n))));
        for (int i = 0; i < n; ++i) {
                const int x = i % side, z = i / side;
                d.vInitialPosition = {x * 1.2f, 10.0f, z * 1.2f};
                bodies.push_back(phys.CreateRigidBody(d));
        }

        for (auto _ : state) phys.Update(kDt);

        DestroyAll(phys, bodies);
        phys.Update(kDt);
        state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Update_FreeFallSphereField)->Arg(64)->Arg(256)->Arg(1024);

// Contact-heavy but settled: a tower of boxes that has come to rest — the
// realistic idle cost of a scene (broadphase + sleeping islands).
static void BM_Update_SettledBoxTower(benchmark::State& state) {
        auto& phys = Runtime().phys;
        const int levels = state.range(0);
        BodyHandle floor = MakeFloor(phys);
        auto tower = MakeTower(phys, levels);
        for (int i = 0; i < 120; ++i) phys.Update(kDt);   // settle (untimed)

        for (auto _ : state) phys.Update(kDt);

        DestroyAll(phys, tower);
        phys.DestroyBody(floor);
        phys.Update(kDt);
        state.SetItemsProcessed(state.iterations() * levels);
}
BENCHMARK(BM_Update_SettledBoxTower)->Arg(16)->Arg(64);

// Active contact churn: spheres in a pen, kicked every frame — solver +
// contact-listener + deferred-command cost per frame.
static void BM_Update_ChurningContacts(benchmark::State& state) {
        auto& phys = Runtime().phys;
        const int n = state.range(0);
        BodyHandle floor = MakeFloor(phys, 8.0f);
        RigidBodyDesc wall;
        wall.oCollider.type         = ColliderDesc::Box;
        wall.oCollider.vHalfExtents = {8.0f, 2.0f, 0.5f};
        wall.is_static              = true;
        std::vector<BodyHandle> walls;
        for (int i = 0; i < 4; ++i) {
                static const glm::quat kRot[4] = {
                        glm::angleAxis(0.0f, glm::vec3(0, 1, 0)),
                        glm::angleAxis(glm::radians(90.f), glm::vec3(0, 1, 0)),
                        glm::angleAxis(glm::radians(180.f), glm::vec3(0, 1, 0)),
                        glm::angleAxis(glm::radians(270.f), glm::vec3(0, 1, 0))};
                wall.qInitialRotation = kRot[i];
                wall.vInitialPosition = kRot[i] * glm::vec3(0, 0, -8.0f);
                wall.vInitialPosition.y = 2.0f;
                walls.push_back(phys.CreateRigidBody(wall));
        }
        std::vector<BodyHandle> balls = MakeSphereField(phys, n, 1.5f, 0.5f);
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> dir(-1.0f, 1.0f);

        for (auto _ : state) {
                for (const BodyHandle h : balls) {
                        phys.SetLinearVelocity(h, {2.0f * dir(rng), 2.0f * dir(rng),
                                                   2.0f * dir(rng)});
                }
                phys.Update(kDt);
        }

        DestroyAll(phys, balls);
        DestroyAll(phys, walls);
        phys.DestroyBody(floor);
        phys.Update(kDt);
        state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Update_ChurningContacts)->Arg(32)->Arg(128);

// CharacterVirtual stepping cost — capsules never sleep, always active.
static void BM_Update_CharacterField(benchmark::State& state) {
        auto& phys = Runtime().phys;
        const int n = state.range(0);
        BodyHandle floor = MakeFloor(phys);
        auto chars = MakeCharacters(phys, n);
        for (int i = 0; i < 30; ++i) phys.Update(kDt);   // settle onto floor

        for (auto _ : state) phys.Update(kDt);

        DestroyChars(phys, chars);
        phys.DestroyBody(floor);
        state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_Update_CharacterField)->Arg(1)->Arg(16)->Arg(64);

// ===========================================================================
// Queries
// ===========================================================================

// 64 downward rays per iteration against a large static broadphase.
static void BM_Raycast_StaticField(benchmark::State& state) {
        auto& phys = Runtime().phys;
        const int n = state.range(0);
        std::vector<BodyHandle> statics;
        RigidBodyDesc d;
        d.oCollider.type         = ColliderDesc::Box;
        d.oCollider.vHalfExtents = {0.5f, 0.5f, 0.5f};
        d.is_static              = true;
        const int side = int(std::ceil(std::sqrt(float(n))));
        const float half_span = 0.5f * float(side - 1) * 1.5f;
        for (int i = 0; i < n; ++i) {
                const int x = i % side, z = i / side;
                d.vInitialPosition = {x * 1.5f - half_span, 0.0f, z * 1.5f - half_span};
                statics.push_back(phys.CreateRigidBody(d));
        }
        phys.Update(kDt);   // optimize broadphase
        constexpr int kRaysPerIter = 64;
        std::vector<PhysicsRay> rays;
        rays.reserve(kRaysPerIter);
        std::mt19937 rng(7);
        const float span = 0.6f * float(side - 1) * 1.5f;   // over the field
        std::uniform_real_distribution<float> pos(-span, span);
        for (int i = 0; i < kRaysPerIter; ++i) {
                rays.push_back({{pos(rng), 6.0f, pos(rng)}, {0.0f, -1.0f, 0.0f}});
        }

        int hits = 0;
        for (auto _ : state) {
                for (const PhysicsRay& r : rays) {
                        RaycastHit hit;
                        hits += phys.Raycast(r, hit) ? 1 : 0;
                }
                benchmark::DoNotOptimize(hits);
        }

        DestroyAll(phys, statics);
        phys.Update(kDt);
        state.counters["hit_rate"] = benchmark::Counter(
                        float(hits) / float(state.iterations() * kRaysPerIter));
        state.SetItemsProcessed(state.iterations() * kRaysPerIter);
}
BENCHMARK(BM_Raycast_StaticField)->Arg(64)->Arg(1024);

// Sensor overlap churn: one large trigger + kicked spheres inside —
// trigger enter/exit event generation and drain cost per frame.
static void BM_TriggerOverlap_Churn(benchmark::State& state) {
        auto& phys = Runtime().phys;
        const int n = state.range(0);
        RigidBodyDesc sensor;
        sensor.oCollider.type         = ColliderDesc::Box;
        sensor.oCollider.vHalfExtents = {8.0f, 4.0f, 8.0f};
        sensor.vInitialPosition       = {0.0f, 4.0f, 0.0f};
        sensor.is_trigger             = true;
        sensor.is_kinematic           = true;
        BodyHandle hSensor = phys.CreateRigidBody(sensor);
        BodyHandle floor = MakeFloor(phys, 8.0f);
        std::vector<BodyHandle> balls = MakeSphereField(phys, n, 1.0f, 0.4f);
        std::mt19937 rng(11);
        std::uniform_real_distribution<float> dir(-1.0f, 1.0f);

        for (auto _ : state) {
                for (const BodyHandle h : balls) {
                        phys.SetLinearVelocity(h, {1.5f * dir(rng), 1.0f * dir(rng),
                                                   1.5f * dir(rng)});
                }
                phys.Update(kDt);
        }

        phys.DestroyBody(hSensor);
        DestroyAll(phys, balls);
        phys.DestroyBody(floor);
        phys.Update(kDt);
        state.SetItemsProcessed(state.iterations() * n);
}
BENCHMARK(BM_TriggerOverlap_Churn)->Arg(32)->Arg(128);

} // namespace
