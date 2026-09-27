
// ---------------------------------------------------------------------------
// CrowdTest — Stage 3 Step 8 (tests/functional/crowd_app composition).
//
// Tier B: the composed CrowdSystemT<DetourCrowdAvoidance> driven directly over
// a flat synthetic bake; follower nodes are integrated KINEMATICALLY along
// their InputState (no physics — the crowd pipeline's output is the contract,
// and the runs stay bitwise deterministic).
// Tier C: full stack — CharacterController + InputState + CharacterMovement-
// System + PhysicsSystem, so the avoidance-adjusted velocity moves a real
// Jolt CharacterVirtual through the unmodified player movement stack.
// Backend seam: the LocalAvoidance contract is exercised directly through a
// scripted backend (no Detour) to pin the CrowdSystemT <-> backend boundary.
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>
#include "EngineFixture.h"
#include "PhysicsHarness.h"
#include "NavAssets.h"

#include <nav/NavBake.h>
#include <nav/NavBake.tcc>
#include <nav/NavMeshAsset.h>
#include <nav/NavigationSystem.h>
#include <nav/NavAgent.h>
#include <nav/PathFollower.h>
#include <nav/CrowdSystem.h>
#include <nav/NavAgentEntityPlugin.h>
#include <nav/PathFollowerEntityPlugin.h>
#include <InputState.h>
#include <CharacterController.h>
#include <EntityManager.h>
#include <world/WorldSystem.h>
#include <EntityTemplateSystem.h>
#include <spawner/SpawnerSystem.h>
#include <nav/WorldNavBridge.h>

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <vector>

using namespace SE;

namespace {

// ---------------------------------------------------------------------------
// scripted LocalAvoidance backend — pins the CrowdSystemT <-> backend contract
// without Detour. "Avoidance" = halve the desired velocity every solve.
// ---------------------------------------------------------------------------

struct ScriptedBackend {
        bool     inited   = false;
        uint32_t maxAgents = 0;
        float    maxRadius = 0.f;
        int      boundClasses = 0;
        std::vector<SAvoidanceParams> slots;
        glm::vec3 lastPos{}, lastVel{}, lastDesired{};

        bool Init(const uint32_t n, const float r) {
                inited = true; maxAgents = n; maxRadius = r;
                return n > 0;
        }
        void Shutdown() { inited = false; slots.clear(); }
        bool BindClass(const StrID, dtNavMesh *, const dtQueryFilter *) { ++boundClasses; return true; }
        bool IsClassMeshCurrent(const StrID, const dtNavMesh *) const { return true; }
        void DropClass(const StrID) {}
        int  Enroll(const StrID, const glm::vec3 &, const SAvoidanceParams & o) {
                slots.push_back(o);
                return int(slots.size()) - 1;
        }
        void Unenroll(const int nSlot) {
                if (nSlot >= 0 && size_t(nSlot) < slots.size()) { slots[size_t(nSlot)].radius = -1.f; }
        }
        void SyncAgent(const int, const glm::vec3 & vP, const glm::vec3 & vV) { lastPos = vP; lastVel = vV; }
        void SetDesired(const int, const glm::vec3 & vD) { lastDesired = vD; }
        void Update(const float) { lastDesired = lastDesired * 0.5f; }
        glm::vec3 AdjustedVelocity(const int) { return lastDesired; }
        bool      IsAgentWalking(const int) const { return true; }
        uint32_t MaxAgents() const { return maxAgents; }
        size_t ActiveAgents() const {
                size_t n = 0;
                for (const SAvoidanceParams & s : slots) { n += s.radius >= 0.f ? 1u : 0u; }
                return n;
        }
};

// ---------------------------------------------------------------------------
// trivially-copyable event counter (EPathArrived / EPathFailed)
// ---------------------------------------------------------------------------

struct PathEventCounter {
        int nArrived = 0;
        int nFailed  = 0;
        void OnArrived(const Event &) { ++nArrived; }
        void OnFailed(const Event &)  { ++nFailed; }
};

} // namespace

// ---------------------------------------------------------------------------
// fixture
// ---------------------------------------------------------------------------

class CrowdTest : public se_test::EnginePhysTestBase {

protected:
        void SetUp() override {
                EnginePhysTestBase::SetUp();

                static bool systems_constructed = []() {
                        auto & oEngine = TEngine::Instance();
                        oEngine.Init<EntityManager>();
                        oEngine.Init<StateMachineSystem>();
                        oEngine.Init<WorldSystem>();
                        oEngine.Init<TerrainSystem>();
                        oEngine.Init<NavigationSystem>();
                        oEngine.Init<CharacterMovementSystem>();
                        oEngine.Init<CrowdSystem>();          // after NavigationSystem
                        oEngine.Init<TEntityTemplateSystem>();
                        oEngine.Init<SpawnerSystem>();
                        oEngine.Init<WorldNavBridge>();       // after WorldSystem + NavigationSystem
                        return true;
                }();
                (void)systems_constructed;

                GetSystem<EntityManager>().SetSceneTree(scene.get());
                GetSystem<WorldSystem>().SetSceneTree(scene.get());
                GetSystem<TEntityTemplateSystem>().SetSceneTree(scene.get());

                // Deterministic phases: every follower upkeep-checks every tick.
                CrowdSystem::SConfig oCfg;
                oCfg.repath_buckets       = 1;
                oCfg.max_repaths_per_tick = 4;
                Crowd().SetConfig(oCfg);

                Nav().SetRandomSeed(0x5EEDC0DEull);

                boost::filesystem::create_directories("resource/nav/gen");
        }

        void TearDown() override {
                Drain(4);

                // Component teardown (dtor -> Disable) drains the rosters; drop
                // the scene BEFORE the physics shutdown since CharacterController
                // dtors talk to PhysicsSystem.
                scene.reset();

                EXPECT_EQ(Crowd().GetStats().followers, 0u);
                EXPECT_EQ(Crowd().GetStats().avoided, 0u);

                Nav().UnregisterAll();
                Nav().SetDefaultClass("");
                EXPECT_EQ(Nav().GetStats().tiles_live, 0u);

                Crowd().SetConfig(CrowdSystem::SConfig{});

                GetSystem<WorldSystem>().SetSceneTree(nullptr);
                GetSystem<EntityManager>().SetSceneTree(nullptr);
                GetSystem<TEntityTemplateSystem>().SetSceneTree(nullptr);

                GetSystem<TResourceManager>().Clear<TerrainAsset>();
                GetSystem<TResourceManager>().Clear<WorldAsset>();
                GetSystem<TResourceManager>().Clear<StateMachineAsset>();
                GetSystem<TResourceManager>().Clear<NavMeshAsset>();

                EnginePhysTestBase::TearDown();
        }

        // ---- runners -------------------------------------------------------

        void Drain(const uint32_t n = 2) {
                for (uint32_t i = 0; i < n; ++i) { StepFrame(1.f / 60.f); }
        }

        /// One frame, returning the crowd ticks that fired this frame
        /// (EPostUpdate resets the counter afterwards).
        uint32_t StepFrameCountTicks(const float dt) {
                Events().TriggerEvent(EInputUpdate{dt});
                const uint32_t nTicks = Crowd().GetStats().solver_ticks;
                Events().TriggerEvent(EUpdate{dt});
                GetSystem<PhysicsSystem>().Update(dt);
                Events().Process();
                Events().TriggerEvent(EPostUpdate{dt});
                GetSystem<PhysicsSystem>().Interpolate();
                Alloc().reset();
                return nTicks;
        }

        NavigationSystem & Nav()   { return GetSystem<NavigationSystem>(); }
        CrowdSystem &    Crowd()   { return GetSystem<CrowdSystem>(); }

        // ---- bake helpers (NavRuntimeTest convention) -----------------------

        NavBake::SOutput BakeFlat(const float fSize = 16.f, const std::string & sGroup = "ground") {
                NavBake::SBakeInput in;
                NavBake::SGroup & g = in.vGroups.emplace_back();
                g.id = sGroup;
                g.bounds_min = glm::vec3(-1.f, -1.f, -1.f);
                g.bounds_max = glm::vec3(fSize + 1.f, 3.f, fSize + 1.f);
                se_nav_test::AddQuad(g.vMeshes.emplace_back(), 0.f, 0.f, fSize, fSize, 0.f);
                NavBake::SOutput out;
                EXPECT_TRUE(NavBake::Bake(in, se_nav_test::MakeConfig(8.f), out));
                return out;
        }

        H<NavMeshAsset> RegisterBake(const NavBake::SOutput & out, const std::string & sClass) {
                FlatBuffers::NavMeshAssetT a;
                a.agent_class = sClass;
                a.version     = 1;
                a.origin      = std::make_unique<FlatBuffers::Vec3>(out.origin.x, out.origin.y, out.origin.z);
                a.tile_dim    = out.tile_dim;
                a.voxel_size   = 0.2f;
                a.voxel_height = 0.2f;
                for (const NavBake::SNavTileBytes & t : out.vTiles) {
                        auto pTile  = std::make_unique<FlatBuffers::NavTileT>();
                        pTile->x    = t.x;
                        pTile->y    = t.y;
                        pTile->layer = t.layer;
                        pTile->group = t.group;
                        pTile->data = t.vBytes;
                        a.tiles.push_back(std::move(pTile));
                }
                flatbuffers::FlatBufferBuilder oBuilder;
                auto offset = FlatBuffers::NavMeshAsset::Pack(oBuilder, &a);
                FlatBuffers::FinishNavMeshAssetBuffer(oBuilder, offset);
                const uint8_t * p = oBuilder.GetBufferPointer();

                H<NavMeshAsset> h = CreateResource<NavMeshAsset>(
                                "crowd_" + sClass + "_" + std::to_string(++resourceSerial),
                                p, oBuilder.GetSize());
                EXPECT_TRUE(h.IsValid());
                EXPECT_TRUE(Nav().RegisterClass(sClass, h));
                return h;
        }

        // ---- agent builder --------------------------------------------------

        struct AgentRefs {
                TSceneTree::TSceneNode node;       // strong ref (destroy ordering)
                InputState *          input    = nullptr;
                NavAgent *            agent    = nullptr;
                PathFollower *        follower = nullptr;
                CharacterController * cc       = nullptr;
        };

        AgentRefs MakeAgent(const std::string & sName, const glm::vec3 & vPos,
                            const bool bAvoidance = true,
                            const bool bWithCC = false,
                            const bool bWithFollower = true,
                            const PathFollowerDesc & oPF = PathFollowerDesc{}) {
                AgentRefs r;
                r.node = scene->Create(sName);
                r.node->SetPos(vPos);
                r.node->CreateComponent<InputState>();
                r.node->CreateComponent<NavAgent>(NavAgentDesc{"default", 0.5f, 2.f, bAvoidance});
                r.input    = r.node->GetComponent<InputState>();
                r.agent    = r.node->GetComponent<NavAgent>();
                EXPECT_NE(r.input, nullptr);
                EXPECT_NE(r.agent, nullptr);
                if (bWithCC) {
                        CharacterController::Desc d;
                        d.capsule_radius      = 0.4f;
                        d.capsule_half_height = 0.9f;
                        d.max_speed           = 4.f;
                        r.node->CreateComponent<CharacterController>(d);
                        r.cc = r.node->GetComponent<CharacterController>();
                        EXPECT_NE(r.cc, nullptr);
                }
                if (bWithFollower) {
                        r.node->CreateComponent<PathFollower>(oPF);
                        r.follower = r.node->GetComponent<PathFollower>();
                        EXPECT_NE(r.follower, nullptr);
                }
                return r;
        }

        /// Tier B kinematic walk: integrate the node along its InputState
        /// (the crowd pipeline's output) — no physics, fully deterministic.
        static void KinematicStep(const AgentRefs & a, const float fDt, const float fMaxSpeed) {
                const glm::vec3 vPos = a.node->GetTransform().GetWorldPos();
                a.node->SetPos(vPos + glm::vec3(a.input->move_axis.x, 0.f, a.input->move_axis.y)
                                        * (fMaxSpeed * fDt));
        }

        static float XZDist(const glm::vec3 & a, const glm::vec3 & b) {
                return glm::length(glm::vec2{a.x - b.x, a.z - b.z});
        }

        uint32_t resourceSerial = 0;
};

// ===========================================================================
// Tier B — follower pipeline (kinematic integration)
// ===========================================================================

TEST_F(CrowdTest, FollowerProgressAndArrival) {
        RegisterBake(BakeFlat(), "default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        AgentRefs a = MakeAgent("npc_a", glm::vec3(2.f, 0.f, 2.f));

        PathEventCounter oEvents;
        auto & em = GetSystem<EventManager>();
        ASSERT_TRUE((em.AddListener<EPathArrived, &PathEventCounter::OnArrived>(&oEvents)));
        ASSERT_TRUE((em.AddListener<EPathFailed,  &PathEventCounter::OnFailed>(&oEvents)));

        const glm::vec3 vGoal(14.f, 0.f, 14.f);
        a.follower->MoveTo(vGoal);
        EXPECT_EQ(a.follower->GetStatus(), PathStatus::Following);

        const float fDt = 1.f / 30.f;
        bool bArrived = false;
        for (int i = 0; i < 900 && !bArrived; ++i) {
                Crowd().RunCrowdStep(fDt);
                KinematicStep(a, fDt, 5.f);
                bArrived = a.follower->HasArrived();
        }

        ASSERT_TRUE(bArrived);
        EXPECT_EQ(a.follower->GetStatus(), PathStatus::Arrived);
        EXPECT_LT(XZDist(a.node->GetTransform().GetWorldPos(), vGoal), 0.35f + 0.05f);
        EXPECT_EQ(oEvents.nArrived, 1);
        EXPECT_EQ(oEvents.nFailed, 0);

        em.RemoveListener<EPathArrived, &PathEventCounter::OnArrived>(&oEvents);
        em.RemoveListener<EPathFailed,  &PathEventCounter::OnFailed>(&oEvents);
}

TEST_F(CrowdTest, SlowRadiusAnalogSpeed) {
        RegisterBake(BakeFlat(), "default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        PathFollowerDesc oDesc;
        oDesc.arrival_radius = 0.35f;
        oDesc.slow_radius    = 2.5f;
        AgentRefs a = MakeAgent("npc_a", glm::vec3(8.f, 0.f, 8.f), true, false, true, oDesc);

        // far goal -> full-speed request (analog magnitude ~1; the adaptive
        // sampler returns a velocity at/near the desired speed with no
        // obstacles around)
        a.follower->MoveTo(glm::vec3(14.f, 0.f, 14.f));
        Crowd().RunCrowdStep(1.f / 30.f);
        EXPECT_NEAR(glm::length(a.input->move_axis), 1.f, 0.15f);

        // walk into the slow band: within slow_radius the magnitude must drop
        a.node->SetPos(glm::vec3(13.f, 0.f, 13.f));   // ~0.87 m from the goal
        Crowd().RunCrowdStep(1.f / 30.f);
        const float fNearMag = glm::length(a.input->move_axis);
        EXPECT_GT(fNearMag, 0.05f);
        EXPECT_LT(fNearMag, 0.6f);   // slowFactor ~ (0.87-0.35)/2.15 ~ 0.24
}

TEST_F(CrowdTest, RepathOnTimerTeleportAndGoalChange) {
        RegisterBake(BakeFlat(), "default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        PathFollowerDesc oDesc;
        oDesc.repath_interval = 0.2f;   // every 6 ticks at 30 Hz
        AgentRefs a = MakeAgent("npc_a", glm::vec3(2.f, 0.f, 2.f), true, false, true, oDesc);
        a.follower->MoveTo(glm::vec3(14.f, 0.f, 14.f));
        Crowd().RunCrowdStep(1.f / 30.f);
        EXPECT_EQ(Crowd().GetStats().repaths, 1u);      // the MoveTo request

        // walk a while -> the timer alone must produce re-paths
        const float fDt = 1.f / 30.f;
        for (int i = 0; i < 12; ++i) {
                Crowd().RunCrowdStep(fDt);
                KinematicStep(a, fDt, 5.f);
        }
        EXPECT_GE(Crowd().GetStats().repaths, 2u);

        // teleport away from the corridor -> the distance check must repath
        uint32_t nBefore = Crowd().GetStats().repaths;
        a.node->SetPos(glm::vec3(1.f, 0.f, 14.f));
        Crowd().RunCrowdStep(fDt);
        EXPECT_GT(Crowd().GetStats().repaths, nBefore);

        // a fresh MoveTo bypasses the phase gate even with no timer due
        nBefore = Crowd().GetStats().repaths;
        a.follower->MoveTo(glm::vec3(14.f, 0.f, 2.f));
        Crowd().RunCrowdStep(fDt);
        EXPECT_GT(Crowd().GetStats().repaths, nBefore);
}

// Off-mesh spawn: enrollment must NOT create an INVALID dtCrowd agent (the
// solver never computes velocity for those — the round-7 "NPCs appear but
// stand still" deadlock). The enrollment projection skips, and the crowd-less
// follower steers on its raw desired velocity instead of freezing.
TEST_F(CrowdTest, OffMeshAgentStillSteersToGoal) {
        RegisterBake(BakeFlat(), "default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        const glm::vec3 vStart(500.f, 0.f, 500.f);   // far off the 16 m bake
        const glm::vec3 vGoal(8.f, 0.f, 8.f);
        AgentRefs a = MakeAgent("npc_off", vStart);
        a.follower->MoveTo(vGoal);

        const float fDt = 1.f / 30.f;
        for (int i = 0; i < 120; ++i) {
                Crowd().RunCrowdStep(fDt);
                KinematicStep(a, fDt, 5.f);
        }

        const glm::vec3 vPos = a.node->GetTransform().GetWorldPos();
        EXPECT_LT(XZDist(vPos, vStart), XZDist(vStart, vGoal) - 1.f)
                        << "off-mesh agent never moved toward the goal";
}

// A failed re-path latches Failed AND starts a backoff: a BT re-issuing MoveTo
// on its tick_interval must not burn a budgeted findPath until the window
// drains (the round-7 alarm report: silent 5 Hz-per-NPC re-paths, uncapped).
TEST_F(CrowdTest, PathFailureBacksOffBeforeRetrying) {
        RegisterBake(BakeFlat(), "default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        AgentRefs a = MakeAgent("npc_fail", glm::vec3(2.f, 0.f, 2.f));

        PathEventCounter oEvents;
        auto & em = GetSystem<EventManager>();
        ASSERT_TRUE((em.AddListener<EPathFailed, &PathEventCounter::OnFailed>(&oEvents)));

        // goal far off the bake: no goal poly -> the re-path fails
        a.follower->MoveTo(glm::vec3(500.f, 0.f, 500.f));
        Crowd().RunCrowdStep(1.f / 30.f);
        EXPECT_EQ(Crowd().GetStats().repaths, 1u);
        EXPECT_EQ(a.follower->GetStatus(), PathStatus::Failed);
        EXPECT_EQ(oEvents.nFailed, 1);

        // inside the backoff window (repath_interval 0.5 s): re-issued MoveTos
        // stay pending, no new findPath
        const float fDt = 1.f / 30.f;
        for (int i = 0; i < 12; ++i) {          // ~0.4 s
                a.follower->MoveTo(glm::vec3(500.f, 0.f, 500.f));
                Crowd().RunCrowdStep(fDt);
        }
        EXPECT_EQ(Crowd().GetStats().repaths, 1u)
                        << "backoff ignored — failed findPath storm";

        // past the window the retry goes through (pending request or timer)
        for (int i = 0; i < 18; ++i) {
                Crowd().RunCrowdStep(fDt);
        }
        EXPECT_GE(Crowd().GetStats().repaths, 2u);

        em.RemoveListener<EPathFailed, &PathEventCounter::OnFailed>(&oEvents);
}

TEST_F(CrowdTest, StopAndIdleZeroInput) {
        RegisterBake(BakeFlat(), "default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        AgentRefs a = MakeAgent("npc_a", glm::vec3(2.f, 0.f, 2.f));
        a.follower->MoveTo(glm::vec3(14.f, 0.f, 14.f));
        Crowd().RunCrowdStep(1.f / 30.f);
        EXPECT_GT(glm::length(a.input->move_axis), 0.5f);

        a.follower->Stop();
        EXPECT_EQ(a.follower->GetStatus(), PathStatus::Idle);
        Crowd().RunCrowdStep(1.f / 30.f);
        EXPECT_FLOAT_EQ(glm::length(a.input->move_axis), 0.f);

        // idling holds: zero is written every tick, no drift from the old goal
        for (int i = 0; i < 10; ++i) { Crowd().RunCrowdStep(1.f / 30.f); }
        EXPECT_FLOAT_EQ(glm::length(a.input->move_axis), 0.f);
}

TEST_F(CrowdTest, EnrollmentLifecycle) {
        RegisterBake(BakeFlat(), "default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        // avoidance=false: the follower still walks, but stays out of the pool
        AgentRefs a = MakeAgent("npc_calm", glm::vec3(2.f, 0.f, 2.f), /*avoidance=*/false);
        Crowd().RunCrowdStep(1.f / 30.f);
        EXPECT_EQ(Crowd().GetStats().avoided, 0u);
        EXPECT_EQ(Crowd().GetStats().followers, 1u);
        a.follower->MoveTo(glm::vec3(14.f, 0.f, 14.f));
        Crowd().RunCrowdStep(1.f / 30.f);
        EXPECT_GT(glm::length(a.input->move_axis), 0.5f);

        // avoidance=true (default): enrolled — with no follower it is a stable
        // idle obstacle (zero requested velocity clamps its solved velocity).
        AgentRefs b = MakeAgent("npc_idle", glm::vec3(12.f, 0.f, 12.f), true, false, false);
        Crowd().RunCrowdStep(1.f / 30.f);
        EXPECT_EQ(Crowd().GetStats().avoided, 1u);
        EXPECT_EQ(Crowd().Backend().ActiveAgents(), 1u);

        // deferred destroy deregisters both (no stale slots / followers)
        TSceneTree::TSceneNodeWeak hA = a.node;
        TSceneTree::TSceneNodeWeak hB = b.node;
        a.node.reset();
        b.node.reset();
        GetSystem<EntityManager>().Destroy(hA);
        GetSystem<EntityManager>().Destroy(hB);
        Drain(4);
        EXPECT_TRUE(hA.expired());
        EXPECT_TRUE(hB.expired());
        EXPECT_EQ(Crowd().GetStats().avoided, 0u);
        EXPECT_EQ(Crowd().GetStats().followers, 0u);
        EXPECT_EQ(Crowd().Backend().ActiveAgents(), 0u);
}

TEST_F(CrowdTest, PoolExhaustionRunsUnavioded) {
        RegisterBake(BakeFlat(), "default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        CrowdSystem::SConfig oCfg = Crowd().GetConfig();
        oCfg.max_agents = 1;
        Crowd().SetConfig(oCfg);

        AgentRefs a = MakeAgent("npc_a", glm::vec3(2.f, 0.f, 2.f));
        AgentRefs b = MakeAgent("npc_b", glm::vec3(3.f, 0.f, 2.f));
        a.follower->MoveTo(glm::vec3(14.f, 0.f, 14.f));
        b.follower->MoveTo(glm::vec3(14.f, 0.f, 14.f));
        Crowd().RunCrowdStep(1.f / 30.f);

        // one slot only: the loser runs unavioded but still steers
        EXPECT_EQ(Crowd().Backend().ActiveAgents(), 1u);
        EXPECT_GT(glm::length(a.input->move_axis), 0.5f);
        EXPECT_GT(glm::length(b.input->move_axis), 0.5f);
}

TEST_F(CrowdTest, TickCadenceFixedClock) {
        RegisterBake(BakeFlat(), "default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        AgentRefs a = MakeAgent("npc_a", glm::vec3(2.f, 0.f, 2.f));

        // 60 frames at 60 Hz against a 30 Hz crowd clock: ~30 solver ticks,
        // regardless of frame time jitter (the accumulator catches up).
        uint32_t nTotal = 0;
        for (uint32_t i = 0; i < 60; ++i) { nTotal += StepFrameCountTicks(1.f / 60.f); }
        EXPECT_NEAR(nTotal, 30u, 1u);

        // a long frame catches up with multiple ticks in one frame
        const uint32_t nCatchup = StepFrameCountTicks(0.25f);   // ~7 ticks
        EXPECT_GE(nCatchup, 5u);
}

TEST_F(CrowdTest, RunToRunDeterminism) {
        RegisterBake(BakeFlat(), "default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        glm::vec3 vFinalA, vFinalB;
        for (int run = 0; run < 2; ++run) {
                AgentRefs a = MakeAgent("npc_det" + std::to_string(run),
                                        glm::vec3(2.f, 0.f, 2.f));
                a.follower->MoveTo(glm::vec3(14.f, 0.f, 14.f));
                const float fDt = 1.f / 30.f;
                for (int i = 0; i < 120 && !a.follower->HasArrived(); ++i) {
                        Crowd().RunCrowdStep(fDt);
                        KinematicStep(a, fDt, 5.f);
                }
                (run == 0 ? vFinalA : vFinalB) = a.node->GetTransform().GetWorldPos();

                // drop the agent so the next run sees an identical world (the
                // previous agent would otherwise stand on the goal as an obstacle)
                TSceneTree::TSceneNodeWeak hNode = a.node;
                a.node.reset();
                GetSystem<EntityManager>().Destroy(hNode);
                Drain(4);
        }

        EXPECT_FLOAT_EQ(vFinalA.x, vFinalB.x);
        EXPECT_FLOAT_EQ(vFinalA.z, vFinalB.z);
}

TEST_F(CrowdTest, PairwiseHeadOnKinematicSeparation) {
        RegisterBake(BakeFlat(), "default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        AgentRefs a = MakeAgent("npc_a", glm::vec3(3.f, 0.f, 8.f));
        AgentRefs b = MakeAgent("npc_b", glm::vec3(13.f, 0.f, 8.f));
        a.follower->MoveTo(glm::vec3(13.f, 0.f, 8.f));
        b.follower->MoveTo(glm::vec3(3.f, 0.f, 8.f));

        const float fDt = 1.f / 30.f;
        float fMinDist = 1e9f;
        bool bBothArrived = false;
        for (int i = 0; i < 600; ++i) {
                Crowd().RunCrowdStep(fDt);
                KinematicStep(a, fDt, 5.f);
                KinematicStep(b, fDt, 5.f);
                fMinDist = glm::min(fMinDist,
                                    XZDist(a.node->GetTransform().GetWorldPos(),
                                           b.node->GetTransform().GetWorldPos()));
                bBothArrived = a.follower->HasArrived() && b.follower->HasArrived();
                if (bBothArrived) { break; }
        }

        EXPECT_TRUE(bBothArrived);
        // radii 0.5 + 0.5: avoidance must keep them apart as they pass
        EXPECT_GT(fMinDist, 0.7f);
}

TEST_F(CrowdTest, SolverBenchmark64Agents) {
        RegisterBake(BakeFlat(40.f), "default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        constexpr int kN = 64;
        std::vector<AgentRefs> vAgents;
        for (int i = 0; i < kN; ++i) {
                const float x = 3.f + float(i % 8) * 4.5f;
                const float z = 3.f + float(i / 8) * 4.5f;
                vAgents.push_back(MakeAgent("swarm_" + std::to_string(i), glm::vec3(x, 0.f, z)));
                vAgents.back().follower->MoveTo(glm::vec3(40.f - x, 0.f, 40.f - z));
        }

        const float fDt = 1.f / 30.f;
        double fTotalMs = 0.0;
        for (int tick = 0; tick < 30; ++tick) {
                const auto t0 = std::chrono::steady_clock::now();
                Crowd().RunCrowdStep(fDt);
                fTotalMs += std::chrono::duration<double, std::milli>(
                        std::chrono::steady_clock::now() - t0).count();
                for (const AgentRefs & a : vAgents) { KinematicStep(a, fDt, 5.f); }
        }

        const double fAvgMs = fTotalMs / 30.0;
        GTEST_LOG_(INFO) << "64-agent crowd solver tick avg: " << fAvgMs << " ms (target < 1 ms @ 30 Hz)";
        EXPECT_LT(fAvgMs, 2.0);   // CI-loose: 2x the documented desktop target

        // and they make progress across the map (the solver did not deadlock)
        Crowd().RunCrowdStep(fDt);
        EXPECT_GT(glm::length(vAgents[0].input->move_axis), 0.f);
}

// ===========================================================================
// Tier C — full stack with physics bodies
// ===========================================================================

TEST_F(CrowdTest, FullStackWalkArrival) {
        RegisterBake(BakeFlat(), "default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        // physics floor at y = 0 (the navmesh plane)
        auto pFloor = scene->Create("floor");
        pFloor->CreateComponent<RigidBody>(se_test::FloorDesc(0.0f));

        AgentRefs a = MakeAgent("walker", glm::vec3(3.f, (0.4f + 0.9f), 8.f),
                                true, /*withCC=*/true);
        ASSERT_NE(a.cc, nullptr);

        PathEventCounter oEvents;
        auto & em = GetSystem<EventManager>();
        ASSERT_TRUE((em.AddListener<EPathArrived, &PathEventCounter::OnArrived>(&oEvents)));

        const glm::vec3 vGoal(13.f, 0.f, 8.f);
        a.follower->MoveTo(vGoal);

        bool bArrived = false;
        for (int frame = 0; frame < 60 * 8 && !bArrived; ++frame) {
                StepFrame(1.f / 60.f);   // EInputUpdate drives the 30 Hz crowd tick
                bArrived = a.follower->HasArrived();
        }

        ASSERT_TRUE(bArrived);
        EXPECT_LT(XZDist(a.node->GetTransform().GetWorldPos(), vGoal), 0.5f + 0.1f);
        EXPECT_EQ(oEvents.nArrived, 1);
        EXPECT_TRUE(a.cc->IsGrounded());

        em.RemoveListener<EPathArrived, &PathEventCounter::OnArrived>(&oEvents);
}

TEST_F(CrowdTest, HeadOnCharactersStayApart) {
        RegisterBake(BakeFlat(), "default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        auto pFloor = scene->Create("floor");
        pFloor->CreateComponent<RigidBody>(se_test::FloorDesc(0.0f));

        const float fLift = (0.4f + 0.9f);
        AgentRefs a = MakeAgent("npc_a", glm::vec3(3.f, fLift, 8.f), true, true);
        AgentRefs b = MakeAgent("npc_b", glm::vec3(13.f, fLift, 8.f), true, true);
        a.follower->MoveTo(glm::vec3(13.f, 0.f, 8.f));
        b.follower->MoveTo(glm::vec3(3.f, 0.f, 8.f));

        float fMinDist = 1e9f;
        bool bBothArrived = false;
        for (int frame = 0; frame < 60 * 12 && !bBothArrived; ++frame) {
                StepFrame(1.f / 60.f);
                fMinDist = glm::min(fMinDist,
                                    XZDist(a.node->GetTransform().GetWorldPos(),
                                           b.node->GetTransform().GetWorldPos()));
                bBothArrived = a.follower->HasArrived() && b.follower->HasArrived();
        }

        EXPECT_TRUE(bBothArrived);
        EXPECT_GT(fMinDist, 0.7f);   // capsule radii 0.4 + 0.4
}

// ===========================================================================
// schema / serialization / backend contract
// ===========================================================================

TEST_F(CrowdTest, SchemaAndPluginRoundTrip) {
        // PathFollower component round-trips its baked table
        flatbuffers::FlatBufferBuilder fbb;
        FlatBuffers::PathFollowerT oT;
        oT.arrival_radius       = 0.5f;
        oT.slow_radius          = 3.f;
        oT.repath_interval      = 0.25f;
        oT.accept_partial_paths = false;
        auto off = FlatBuffers::PathFollower::Pack(fbb, &oT);
        fbb.Finish(off);
        auto * pFB = flatbuffers::GetRoot<FlatBuffers::PathFollower>(fbb.GetBufferPointer());

        auto pFollower = scene->Create("pf_node");
        pFollower->CreateComponent<PathFollower>(pFB);
        const PathFollowerDesc & oDesc = pFollower->GetComponent<PathFollower>()->Desc();
        EXPECT_FLOAT_EQ(oDesc.arrival_radius, 0.5f);
        EXPECT_FLOAT_EQ(oDesc.slow_radius, 3.f);
        EXPECT_FLOAT_EQ(oDesc.repath_interval, 0.25f);
        EXPECT_FALSE(oDesc.accept_partial_paths);

        // NavAgent: old buffers (field absent) default to avoidance = true ...
        {
                flatbuffers::FlatBufferBuilder b2;
                auto off2 = FlatBuffers::CreateNavAgent(b2, b2.CreateString("default").o,
                                                        0.5f, 2.f, true);
                b2.Finish(off2);
                auto * pAgentFB = flatbuffers::GetRoot<FlatBuffers::NavAgent>(b2.GetBufferPointer());
                auto pNode = scene->Create("na_old");
                pNode->CreateComponent<NavAgent>(pAgentFB);
                EXPECT_TRUE(pNode->GetComponent<NavAgent>()->Avoidance());
        }
        // ... and authored avoidance=false survives the round-trip.
        {
                flatbuffers::FlatBufferBuilder b3;
                auto off3 = FlatBuffers::CreateNavAgent(b3, b3.CreateString("default").o,
                                                        0.5f, 2.f, false);
                b3.Finish(off3);
                auto * pAgentFB = flatbuffers::GetRoot<FlatBuffers::NavAgent>(b3.GetBufferPointer());
                auto pNode = scene->Create("na_off");
                pNode->CreateComponent<NavAgent>(pAgentFB);
                EXPECT_FALSE(pNode->GetComponent<NavAgent>()->Avoidance());
        }

        // plugin field overrides (the .setm authoring path)
        {
                flatbuffers::FlatBufferBuilder fbbPF;
                FlatBuffers::FieldOverrideT foPF;
                foPF.value.Set(FlatBuffers::Float(1.25f));
                auto offPF = FlatBuffers::FieldOverride::Pack(fbbPF, &foPF);
                fbbPF.Finish(offPF);
                auto * pFO = flatbuffers::GetRoot<FlatBuffers::FieldOverride>(fbbPF.GetBufferPointer());

                FlatBuffers::PathFollowerT oPF;
                EntityTemplatePlugin<PathFollower>::ApplyField(oPF, "slow_radius", *pFO);
                EXPECT_FLOAT_EQ(oPF.slow_radius, 1.25f);

                flatbuffers::FlatBufferBuilder fbbB;
                FlatBuffers::FieldOverrideT foB;
                foB.value.Set(FlatBuffers::Bool(false));
                auto offB = FlatBuffers::FieldOverride::Pack(fbbB, &foB);
                fbbB.Finish(offB);
                auto * pFOB = flatbuffers::GetRoot<FlatBuffers::FieldOverride>(fbbB.GetBufferPointer());

                EntityTemplatePlugin<PathFollower>::ApplyField(oPF, "accept_partial_paths", *pFOB);
                EXPECT_FALSE(oPF.accept_partial_paths);

                FlatBuffers::NavAgentT oNA;
                EntityTemplatePlugin<NavAgent>::ApplyField(oNA, "avoidance", *pFOB);
                EXPECT_FALSE(oNA.avoidance);
        }
}

TEST_F(CrowdTest, ScriptedBackendContract) {
        // CrowdSystemT over a scripted backend (no Detour): the shared pipeline
        // drives ONLY the LocalAvoidance contract, so any conforming backend
        // slots in (the Stage 6 ORCA seam, proven at the type level here).

        CrowdSystemT<ScriptedBackend> oSys;
        CrowdSystemT<ScriptedBackend>::SConfig oCfg;
        oCfg.max_agents = 4;
        oSys.SetConfig(oCfg);

        ScriptedBackend & oBackend = oSys.Backend();
        ASSERT_TRUE(oBackend.Init(oCfg.max_agents, oCfg.max_agent_radius));
        EXPECT_EQ(oBackend.MaxAgents(), 4u);

        SAvoidanceParams oP;
        oP.radius    = 0.5f;
        oP.max_speed = 5.f;
        const int nSlot = oBackend.Enroll(StrID("default"), glm::vec3(1.f, 0.f, 1.f), oP);
        ASSERT_GE(nSlot, 0);
        EXPECT_EQ(oBackend.ActiveAgents(), 1u);

        oBackend.SyncAgent(nSlot, glm::vec3(1.f, 0.f, 2.f), glm::vec3(0.f, 0.f, 3.f));
        EXPECT_FLOAT_EQ(oBackend.lastPos.z, 2.f);
        EXPECT_FLOAT_EQ(oBackend.lastVel.z, 3.f);

        oBackend.SetDesired(nSlot, glm::vec3(4.f, 0.f, 0.f));
        oBackend.Update(1.f / 30.f);
        const glm::vec3 vAdj = oBackend.AdjustedVelocity(nSlot);
        EXPECT_FLOAT_EQ(vAdj.x, 2.f);   // the scripted "avoidance" halves

        oBackend.Unenroll(nSlot);
        EXPECT_EQ(oBackend.ActiveAgents(), 0u);
        oBackend.Shutdown();
        EXPECT_FALSE(oBackend.inited);
}
