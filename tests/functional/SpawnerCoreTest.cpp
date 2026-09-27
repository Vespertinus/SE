// ---------------------------------------------------------------------------
// SpawnerCoreTest — Tier B: the pure spawner cores. No scene, no engine systems
// (SpatialIndexTest pattern): DeterministicRng / SpawnPlacement / SpawnConditions
// / PopulationRuntime are constructed directly. Determinism tests compare full
// output sequences; the runtime state machine is driven with manual clocks.
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>
#include "EngineFixture.h"

#include <spawner/DeterministicRng.h>
#include <spawner/SpawnPlacement.h>
#include <spawner/SpawnConditions.h>
#ifdef SE_IMPL
#include <spawner/SpawnPlacement.tcc>
#include <spawner/SpawnConditions.tcc>
#include <spawner/PopulationRuntime.tcc>
#include <world/SpatialIndex.tcc>
// Step 9: BlackboardCore definitions arrive transitively via
// SpawnConditions.tcc (WorldFlag conditions); pin-free in this composition
#endif

#include "BlackboardAssets.h"

#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include <set>
#include <vector>

using namespace SE;

namespace {

std::vector<PopulationRuntime::SSpawnCommand> TickRuntime(PopulationRuntime::Runtime & oRuntime,
                                                          float t, bool conditions_ok) {
        std::vector<PopulationRuntime::SSpawnCommand> vOut;
        oRuntime.Tick(t, conditions_ok, [&](const PopulationRuntime::SSpawnCommand & c) {
                vOut.push_back(c);
        });
        return vOut;
}

} // namespace

// ---------------------------------------------------------------------------
// DeterministicRng
// ---------------------------------------------------------------------------

TEST(SpawnerCoreTest, RngSequenceIsDeterministic) {
        DeterministicRng a(12345), b(12345);
        for (int i = 0; i < 64; ++i) {
                ASSERT_EQ(a.NextU64(), b.NextU64());
        }
}

TEST(SpawnerCoreTest, RngUniform01InRange) {
        DeterministicRng rng(777);
        for (int i = 0; i < 1000; ++i) {
                const float f = rng.Uniform01();
                ASSERT_GE(f, 0.f);
                ASSERT_LT(f, 1.f);
        }
}

TEST(SpawnerCoreTest, Hash64IsStable) {
        EXPECT_EQ(DeterministicRng::Hash64("wolf_pack"),
                  DeterministicRng::Hash64("wolf_pack"));
        EXPECT_NE(DeterministicRng::Hash64("wolf_pack"),
                  DeterministicRng::Hash64("wolf_pack_b"));
}

// ---------------------------------------------------------------------------
// SpawnPlacement
// ---------------------------------------------------------------------------

TEST(SpawnerCoreTest, UniformVolumeStaysInBox) {
        const SpawnPlacement::SVolumeDesc desc{ glm::vec3(6.f, 4.f, 8.f),
                        SpawnPlacement::EDistribution::Uniform, 8 };
        DeterministicRng rng(42);
        const glm::vec3 origin(10.f, 5.f, -3.f);

        for (int i = 0; i < 500; ++i) {
                const glm::vec3 p = SpawnPlacement::SampleVolume(desc, rng, origin);
                EXPECT_NEAR(p.x, 10.f, 3.f);
                EXPECT_NEAR(p.y, 5.f, 2.f);
                EXPECT_NEAR(p.z, -3.f, 4.f);
        }
}

TEST(SpawnerCoreTest, PlacementIsDeterministic) {
        const SpawnPlacement::SVolumeDesc desc{ glm::vec3(6.f, 4.f, 8.f),
                        SpawnPlacement::EDistribution::BestCandidate, 8 };
        DeterministicRng a(100), b(100);
        std::vector<glm::vec3> vA, vB, vHistA, vHistB;

        for (int i = 0; i < 32; ++i) {
                const glm::vec3 pa = SpawnPlacement::SampleVolume(desc, a, glm::vec3(0.f),
                                vHistA.data(), (uint32_t)vHistA.size());
                vA.push_back(pa);
                vHistA.push_back(pa);

                const glm::vec3 pb = SpawnPlacement::SampleVolume(desc, b, glm::vec3(0.f),
                                vHistB.data(), (uint32_t)vHistB.size());
                vB.push_back(pb);
                vHistB.push_back(pb);
        }
        ASSERT_EQ(vA.size(), vB.size());
        for (size_t i = 0; i < vA.size(); ++i) {
                EXPECT_EQ(vA[i], vB[i]);
        }
}

TEST(SpawnerCoreTest, BestCandidateSpreadsBetterThanUniform) {
        // min pairwise distance over N picks: best-candidate should beat uniform
        // for the same seed family (statistical, but with a fixed seed: stable).
        auto min_pairwise = [](const std::vector<glm::vec3> & v) {
                float m = 3.4e38f;
                for (size_t i = 0; i < v.size(); ++i) {
                        for (size_t j = i + 1; j < v.size(); ++j) {
                                const float d = glm::length(v[i] - v[j]);
                                if (d < m) { m = d; }
                        }
                }
                return m;
        };

        const SpawnPlacement::SVolumeDesc uni { glm::vec3(20.f, 0.f, 20.f),
                        SpawnPlacement::EDistribution::Uniform, 8 };
        const SpawnPlacement::SVolumeDesc best{ glm::vec3(20.f, 0.f, 20.f),
                        SpawnPlacement::EDistribution::BestCandidate, 8 };

        std::vector<glm::vec3> vU, vB, vHist;
        DeterministicRng rngU(2024), rngB(2024);
        for (int i = 0; i < 24; ++i) {
                vU.push_back(SpawnPlacement::SampleVolume(uni, rngU, glm::vec3(0.f)));
                const glm::vec3 p = SpawnPlacement::SampleVolume(best, rngB, glm::vec3(0.f),
                                vHist.data(), (uint32_t)vHist.size());
                vB.push_back(p);
                vHist.push_back(p);
        }
        EXPECT_GT(min_pairwise(vB), min_pairwise(vU));
}

TEST(SpawnerCoreTest, PointJitterRespectsRadius) {
        const SpawnPlacement::SPointDesc desc{ 5.f };
        DeterministicRng rng(9);
        const glm::vec3 origin(1.f, 2.f, 3.f);
        for (int i = 0; i < 200; ++i) {
                const glm::vec3 p = SpawnPlacement::SamplePoint(desc, rng, origin);
                EXPECT_LE(glm::length(glm::vec2(p.x - 1.f, p.z - 3.f)), 5.001f);
                EXPECT_FLOAT_EQ(p.y, 2.f);
        }
        // zero jitter: exact
        const SpawnPlacement::SPointDesc none{ 0.f };
        EXPECT_EQ(SpawnPlacement::SamplePoint(none, rng, origin), origin);
}

// ---------------------------------------------------------------------------
// SpawnConditions
// ---------------------------------------------------------------------------

TEST(SpawnerCoreTest, ParseRejectsBadAuthoring) {
        EXPECT_THROW(SpawnConditions::Parse(SpawnConditions::EKind::FocusNear, 0.f, 0.f, {}),
                     std::runtime_error);
        EXPECT_THROW(SpawnConditions::Parse(SpawnConditions::EKind::TagNear, 1.f, 0.f, {}),
                     std::runtime_error);
        EXPECT_THROW(SpawnConditions::Parse(SpawnConditions::EKind::AtTime, 0.f, -1.f, {}),
                     std::runtime_error);
        EXPECT_NO_THROW(SpawnConditions::Parse(SpawnConditions::EKind::FocusNear, 1.f, 0.f, {}));
}

TEST(SpawnerCoreTest, FocusNearMatchesNearestFocus) {
        std::vector<glm::vec3> vFocuses = { glm::vec3(10.f, 0.f, 0.f) };

        auto cond = SpawnConditions::Parse(SpawnConditions::EKind::FocusNear, 15.f, 0.f, {});

        SpawnConditions::SEvalContext ctx;
        ctx.origin      = glm::vec3(0.f);
        ctx.t_since_arm = 0.f;
        ctx.vFocuses    = vFocuses.data();
        ctx.focus_count = 1;

        EXPECT_TRUE(SpawnConditions::Evaluate(cond, ctx));

        // beyond the radius: false
        ctx.vFocuses = nullptr; ctx.focus_count = 0;
        EXPECT_FALSE(SpawnConditions::Evaluate(cond, ctx));

        ctx.vFocuses = vFocuses.data(); ctx.focus_count = 1;
        vFocuses[0] = glm::vec3(100.f, 0.f, 0.f);
        EXPECT_FALSE(SpawnConditions::Evaluate(cond, ctx));
}

TEST(SpawnerCoreTest, AtTimeWindow) {
        auto cond = SpawnConditions::Parse(SpawnConditions::EKind::AtTime, 0.f, 2.5f, {});
        SpawnConditions::SEvalContext ctx;
        ctx.t_since_arm = 2.49f;
        EXPECT_FALSE(SpawnConditions::Evaluate(cond, ctx));
        ctx.t_since_arm = 2.5f;
        EXPECT_TRUE(SpawnConditions::Evaluate(cond, ctx));
        ctx.t_since_arm = 100.f;
        EXPECT_TRUE(SpawnConditions::Evaluate(cond, ctx));
}

TEST(SpawnerCoreTest, TagNearViaSpatialIndex) {
        SpatialIndex oIndex;
        const uint32_t bit = oIndex.TagBit(StrID("noise_source"));
        ASSERT_LT(bit, SpatialIndex::kMaxTags);

        auto cond = SpawnConditions::Parse(SpawnConditions::EKind::TagNear, 30.f, 0.f, "noise_source");
        SpawnConditions::ResolveTag(cond, oIndex);
        ASSERT_NE(cond.tag_mask, 0ull);

        SpatialIndex::SEntryDesc d;
        d.vPos = glm::vec3(5.f, 0.f, 5.f);
        d.tag  = StrID("noise_source");
        oIndex.Add(d);

        SpawnConditions::SEvalContext ctx;
        ctx.origin = glm::vec3(0.f);
        ctx.pIndex = &oIndex;
        EXPECT_TRUE(SpawnConditions::Evaluate(cond, ctx));

        // unresolvable tag never passes
        auto ghost = SpawnConditions::Parse(SpawnConditions::EKind::TagNear, 30.f, 0.f, "never_tagged");
        SpawnConditions::ResolveTag(ghost, oIndex);
        // (tag resolves to a bit, but no entry carries it)
        EXPECT_FALSE(SpawnConditions::Evaluate(ghost, ctx));
}

// Step 9: the WorldFlag condition (global blackboard bool key)
TEST(SpawnerCoreTest, WorldFlagParseAndEvaluate) {
        // BlackboardCore change-event posts go through EventManager: Tier B
        // selective init (these plain TESTs have no fixture SetUp)
        se_test::EnsureEngine();

        auto cond = SpawnConditions::Parse(SpawnConditions::EKind::WorldFlag, 0.f, 0.f, {},
                                           "gate_open");
        EXPECT_EQ(cond.key, StrID("gate_open"));

        // empty key is an authoring error
        EXPECT_ANY_THROW(SpawnConditions::Parse(SpawnConditions::EKind::WorldFlag, 0.f, 0.f, {}, {}));

        // no blackboard in the context: never passes
        SpawnConditions::SEvalContext ctx;
        EXPECT_FALSE(SpawnConditions::Evaluate(cond, ctx));

        // live core: key false means gate closed; true means open
        BlackboardCore oCore;
        auto oDef = se_test::MakeBBDef("wf_bb");
        se_test::AddKey(*oDef, "gate_open", FlatBuffers::EBType::Bool);
        flatbuffers::FlatBufferBuilder oFbb;
        // direct construction: the Tier B composition lists no resources
        BlackboardAsset oAsset("wf_bb_asset", 0, se_test::PackBBDef(oFbb, *oDef));
        oCore.Init(&oAsset);

        ctx.pBlackboard = &oCore;
        EXPECT_FALSE(SpawnConditions::Evaluate(cond, ctx));
        EXPECT_TRUE(oCore.SetBool(StrID("gate_open"), true));
        EXPECT_TRUE(SpawnConditions::Evaluate(cond, ctx));

        // unresolvable key never passes (warned at Arm)
        auto ghost = SpawnConditions::Parse(SpawnConditions::EKind::WorldFlag, 0.f, 0.f, {},
                                            "no_such_flag");
        EXPECT_FALSE(SpawnConditions::Evaluate(ghost, ctx));
}

// Arm-time resolution warns and never throws (population/SpawnPoint Enable path)
TEST(SpawnerCoreTest, WorldFlagArmResolution) {
        se_test::EnsureEngine();

        auto cond = SpawnConditions::Parse(SpawnConditions::EKind::WorldFlag, 0.f, 0.f, {},
                                           "gate_open");
        EXPECT_NO_THROW(SpawnConditions::ResolveWorldFlag(cond, nullptr));

        BlackboardCore oCore;
        auto oDef = se_test::MakeBBDef("wf_arm_bb");
        se_test::AddKey(*oDef, "gate_open", FlatBuffers::EBType::Bool);
        flatbuffers::FlatBufferBuilder oFbb;
        BlackboardAsset oAsset("wf_arm_bb_asset", 0, se_test::PackBBDef(oFbb, *oDef));
        oCore.Init(&oAsset);
        EXPECT_NO_THROW(SpawnConditions::ResolveWorldFlag(cond, &oCore));

        auto bad = SpawnConditions::Parse(SpawnConditions::EKind::WorldFlag, 0.f, 0.f, {},
                                          "no_such_flag");
        EXPECT_NO_THROW(SpawnConditions::ResolveWorldFlag(bad, &oCore));
}

// The plan's shared-state loop: writer-style write, global key, condition flips
TEST(SpawnerCoreTest, WorldFlagCrossTalkWithCore) {
        se_test::EnsureEngine();

        BlackboardCore oGlobal;
        auto oDef = se_test::MakeBBDef("wf_loop_bb");
        se_test::AddKey(*oDef, "gate_open", FlatBuffers::EBType::Bool);
        flatbuffers::FlatBufferBuilder oFbb;
        BlackboardAsset oAsset("wf_loop_bb_asset", 0, se_test::PackBBDef(oFbb, *oDef));
        oGlobal.Init(&oAsset);

        auto cond = SpawnConditions::Parse(SpawnConditions::EKind::WorldFlag, 0.f, 0.f, {},
                                           "gate_open");
        SpawnConditions::SEvalContext ctx;
        ctx.pBlackboard = &oGlobal;
        EXPECT_FALSE(SpawnConditions::Evaluate(cond, ctx));

        // what a TriggerBinding write does
        oGlobal.SetBool(StrID("gate_open"), true);
        EXPECT_TRUE(SpawnConditions::Evaluate(cond, ctx));
}

// ---------------------------------------------------------------------------
// PopulationRuntime
// ---------------------------------------------------------------------------

TEST(SpawnerCoreTest, InitialDelayGatesFirstSpawn) {
        PopulationRuntime::SDesc desc;
        desc.members.push_back({ "wolf", "", 1.f, 1, /*initial_delay*/2.f });
        PopulationRuntime::Runtime rt;
        rt.Arm(desc);

        EXPECT_TRUE(TickRuntime(rt, 1.f, true).empty());
        auto vCmds = TickRuntime(rt, 2.f, true);
        ASSERT_EQ(vCmds.size(), 1u);
        EXPECT_EQ(vCmds[0].member, 0u);
}

TEST(SpawnerCoreTest, QuotaFillsByWeightOrder) {
        PopulationRuntime::SDesc desc;
        desc.members.push_back({ "light", "", 1.f, 2 });    // weight 1 → fills second
        desc.members.push_back({ "heavy", "", 5.f, 2 });    // weight 5 → fills first
        PopulationRuntime::Runtime rt;
        rt.Arm(desc);

        // Arm sorts by weight desc: [0]=heavy, [1]=light
        EXPECT_EQ(rt.Member(0).template_id, "heavy");
        EXPECT_EQ(rt.Member(1).template_id, "light");

        const auto vCmds = TickRuntime(rt, 0.f, true);
        ASSERT_EQ(vCmds.size(), 4u);
        EXPECT_EQ(vCmds[0].member, 0u);   // heavy x2 first
        EXPECT_EQ(vCmds[1].member, 0u);
        EXPECT_EQ(vCmds[2].member, 1u);   // then light x2
        EXPECT_EQ(vCmds[3].member, 1u);
}

TEST(SpawnerCoreTest, MaxActiveCapsCommands) {
        PopulationRuntime::SDesc desc;
        desc.members.push_back({ "wolf", "", 1.f, 5 });
        desc.max_active = 2;
        PopulationRuntime::Runtime rt;
        rt.Arm(desc);

        auto vCmds = TickRuntime(rt, 0.f, true);
        ASSERT_EQ(vCmds.size(), 2u);
        for (const auto & c : vCmds) { rt.NotifySpawned(c.member); }

        // quota full: nothing more even with headroom in members
        EXPECT_TRUE(TickRuntime(rt, 1.f, true).empty());
}

TEST(SpawnerCoreTest, ConditionsGateSpawning) {
        PopulationRuntime::SDesc desc;
        desc.members.push_back({ "wolf", "", 1.f, 1 });
        PopulationRuntime::Runtime rt;
        rt.Arm(desc);

        EXPECT_TRUE(TickRuntime(rt, 0.f, /*conditions_ok*/false).empty());
        EXPECT_EQ(TickRuntime(rt, 0.f, true).size(), 1u);
}

TEST(SpawnerCoreTest, RespawnAfterDelayWithinLimit) {
        PopulationRuntime::SDesc desc;
        desc.members.push_back({ "wolf", "", 1.f, 1 });
        desc.respawn_delay_s = 3.f;
        PopulationRuntime::Runtime rt;
        rt.Arm(desc);

        auto vCmds = TickRuntime(rt, 0.f, true);
        ASSERT_EQ(vCmds.size(), 1u);
        rt.NotifySpawned(0);

        rt.NotifyDied(0, 4.f);
        EXPECT_TRUE(TickRuntime(rt, 6.f, true).empty());          // delay not elapsed (4+3=7)
        vCmds = TickRuntime(rt, 7.f, true);
        ASSERT_EQ(vCmds.size(), 1u);                               // respawned
        rt.NotifySpawned(0);

        // respawn_limit default is unlimited; with limit 0 the death is final
        PopulationRuntime::SDesc noRespawn;
        noRespawn.members.push_back({ "wolf", "", 1.f, 1, 0.f, /*respawn_limit*/0 });
        noRespawn.respawn_delay_s = 1.f;
        PopulationRuntime::Runtime rt2;
        rt2.Arm(noRespawn);
        ASSERT_EQ(TickRuntime(rt2, 0.f, true).size(), 1u);
        rt2.NotifySpawned(0);
        rt2.NotifyDied(0, 1.f);
        EXPECT_TRUE(TickRuntime(rt2, 99.f, true).empty());
}

TEST(SpawnerCoreTest, RespawnDisabledByNegativeDelay) {
        PopulationRuntime::SDesc desc;
        desc.members.push_back({ "wolf", "", 1.f, 1 });
        desc.respawn_delay_s = -1.f;
        PopulationRuntime::Runtime rt;
        rt.Arm(desc);

        ASSERT_EQ(TickRuntime(rt, 0.f, true).size(), 1u);
        rt.NotifySpawned(0);
        rt.NotifyDied(0, 1.f);
        EXPECT_TRUE(TickRuntime(rt, 100.f, true).empty());
}

TEST(SpawnerCoreTest, TimedWaveFiresOnceConditionGated) {
        PopulationRuntime::SDesc desc;
        PopulationRuntime::SWaveDesc wave;
        wave.at_time_s = 5.f;
        wave.members.push_back({ "ambusher", "", 1.f, 3 });
        desc.waves.push_back(wave);
        PopulationRuntime::Runtime rt;
        rt.Arm(desc);

        // before the timer: nothing
        EXPECT_TRUE(TickRuntime(rt, 4.9f, true).empty());
        // conditions false AT the timer: the wave stays pending
        EXPECT_TRUE(TickRuntime(rt, 5.1f, false).empty());
        EXPECT_FALSE(rt.WaveFired(0));
        // conditions pass: fires once
        auto vCmds = TickRuntime(rt, 5.2f, true);
        ASSERT_EQ(vCmds.size(), 3u);
        EXPECT_TRUE(rt.WaveFired(0));
        for (const auto & c : vCmds) { rt.NotifySpawned(c.member); }   // simulate execution
        EXPECT_TRUE(TickRuntime(rt, 6.f, true).empty());
}

TEST(SpawnerCoreTest, EventWaveArmsViaFireWave) {
        PopulationRuntime::SDesc desc;
        PopulationRuntime::SWaveDesc wave;
        wave.on_event = "lever_pulled";
        wave.members.push_back({ "ambusher", "", 1.f, 2 });
        desc.waves.push_back(wave);
        PopulationRuntime::Runtime rt;
        rt.Arm(desc);

        // dormant until the event
        EXPECT_TRUE(TickRuntime(rt, 1.f, true).empty());
        EXPECT_TRUE(rt.FireWave(0, 2.f));
        EXPECT_FALSE(rt.FireWave(0, 2.f));            // second fire: rejected
        EXPECT_FALSE(rt.FireWave(7, 2.f));            // unknown index

        const auto vCmds = TickRuntime(rt, 2.f, true);
        ASSERT_EQ(vCmds.size(), 2u);
}

TEST(SpawnerCoreTest, HandleExpiryReconciliation) {
        // Cascade deaths (no NotifyDied) converge: counts drop when the component's
        // lazy sweep runs — here simulated by direct NotifySpawned/NotifyDied balance.
        PopulationRuntime::SDesc desc;
        desc.members.push_back({ "wolf", "", 1.f, 2 });
        PopulationRuntime::Runtime rt;
        rt.Arm(desc);

        auto vCmds = TickRuntime(rt, 0.f, true);
        for (const auto & c : vCmds) { rt.NotifySpawned(c.member); }
        EXPECT_EQ(rt.ActiveTotal(), 2u);

        rt.NotifyDied(0, 1.f);
        EXPECT_EQ(rt.ActiveTotal(), 1u);
}

TEST(SpawnerCoreTest, ReArmResetsEverything) {
        PopulationRuntime::SDesc desc;
        desc.members.push_back({ "wolf", "", 1.f, 1, /*delay*/1.f });
        PopulationRuntime::Runtime rt;
        rt.Arm(desc);
        EXPECT_TRUE(TickRuntime(rt, 0.5f, true).empty());
        EXPECT_EQ(TickRuntime(rt, 1.f, true).size(), 1u);

        // fresh arm replays the initial delay
        rt.Arm(desc);
        EXPECT_TRUE(TickRuntime(rt, 0.5f, true).empty());
        EXPECT_EQ(TickRuntime(rt, 1.f, true).size(), 1u);
}
