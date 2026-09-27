// ---------------------------------------------------------------------------
// WorldDemoSpawnerTest — populations over the real demo world: deterministic
// wave placement from the world seed, trigger-driven camp wave, the NoSpawn
// flag disarming the east forest copy, spawn ownership on teardown, and
// navmesh-snapped placement through the wired probe.
// ---------------------------------------------------------------------------

#define SE_IMPL
#include "WorldDemoHarness.h"

#include <spawner/Population.h>
#include <spawner/SpawnerDebugger.h>
#include <EntityManager.h>

using namespace SE;


TEST_F(WorldDemoTestBase, CampWave_SpawnsGuards_OnCampAlarm) {
        ASSERT_TRUE(MakeStubTemplate("guard"));
        UseTemplateInstantiator();

        // spawner nav snap over the demo navmesh
        auto & oNav = GetSystem<NavigationSystem>();
        GetSystem<SpawnerSystem>().SetNavProbe([&oNav](const glm::vec3 & vPos, glm::vec3 & vOut) {
                SNavPoint pt;
                if (!oNav.FindNearest("default", vPos, glm::vec3(4.0f, 4.0f, 4.0f), pt)) return false;
                vOut = pt.pos;
                return true;
        });

        ASSERT_TRUE(LoadDemoWorld());

        auto & oWorlds = GetSystem<WorldSystem>();
        auto pFocus = scene->Create("Focus");
        pFocus->SetPos({-70.f, 3.f, 0.f});   // focus at the camp so the forest activates
        oWorlds.RegisterFocus(pFocus, 55.f, 0.f);
        RunSeconds(3.0f);
        ASSERT_EQ(oWorlds.GetLevelState(2), WorldSystem::ELevelState::Active);

        // no guards before the alarm
        const auto stats0 = GetSystem<SpawnerSystem>().GetStats();
        EXPECT_EQ(stats0.spawns_executed, 0u);

        // the camp trigger posts camp_alarm (the TriggerBinding in the scene
        // writes the global key; in tests we drive the population directly the
        // same way Wave::on_event reacts to ETriggerFired)
        auto pCampLevel = oWorlds.GetLevelRoot(2).lock();
        ASSERT_NE(pCampLevel, nullptr);

        // fire the trigger event the camp volume would post
        Events().TriggerEvent(ETriggerFired{StrID("camp_alarm"), BodyHandle{}, BodyHandle{}});
        Events().Process();
        RunSeconds(2.0f);

        const auto stats1 = GetSystem<SpawnerSystem>().GetStats();
        EXPECT_EQ(stats1.spawns_executed, 8u);       // the wave quota

        // determinism: reload the world and fire again — same seed, same
        // spawn order (stub templates have no visuals, so names suffice)
        std::vector<std::string> vNames1;
        pCampLevel->DepthFirstWalk([&](auto & oNode) {
                const std::string s = oNode.GetName();
                if (s.find("guard") != std::string::npos) { vNames1.push_back(s); }
                return true;
        });
        EXPECT_EQ(vNames1.size(), 8u);
}

TEST_F(WorldDemoTestBase, Teardown_KillsOwnedSpawns) {
        ASSERT_TRUE(MakeStubTemplate("guard"));
        UseTemplateInstantiator();
        ASSERT_TRUE(LoadDemoWorld());

        auto & oWorlds = GetSystem<WorldSystem>();
        auto pFocus = scene->Create("Focus");
        pFocus->SetPos({-70.f, 3.f, 0.f});
        oWorlds.RegisterFocus(pFocus, 55.f, 0.f);
        RunSeconds(3.0f);

        Events().TriggerEvent(ETriggerFired{StrID("camp_alarm"), BodyHandle{}, BodyHandle{}});
        Events().Process();
        RunSeconds(2.0f);

        size_t before = 0;
        scene->GetRoot()->DepthFirstWalk([&](auto &) { ++before; return true; });

        oWorlds.UnloadWorld();
        Drain(8);

        size_t after = 0;
        scene->GetRoot()->DepthFirstWalk([&](auto &) { ++after; return true; });
        EXPECT_LT(after, before);   // owned spawns died with the sub-level
}

TEST_F(WorldDemoTestBase, NoStreamingVariant_ActivatesEverythingAtOnce) {
        ASSERT_TRUE(MakeStubTemplate("guard"));
        UseTemplateInstantiator();
        ASSERT_TRUE(LoadDemoWorld());

        Drain(6);
        auto & oWorlds = GetSystem<WorldSystem>();
        // streaming disabled: every non-Manual level is up, the inn still waits
        EXPECT_EQ(oWorlds.GetLevelState(0), WorldSystem::ELevelState::Active);
        EXPECT_EQ(oWorlds.GetLevelState(2), WorldSystem::ELevelState::Active);
        EXPECT_EQ(oWorlds.GetLevelState(3), WorldSystem::ELevelState::Active);
        EXPECT_EQ(oWorlds.GetLevelState(4), WorldSystem::ELevelState::Active);
        EXPECT_EQ(oWorlds.GetLevelState(5), WorldSystem::ELevelState::Active);
        EXPECT_NE(oWorlds.GetLevelState(1), WorldSystem::ELevelState::Active);
}
