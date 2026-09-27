// ---------------------------------------------------------------------------
// Portal component (Step 12): door↔Manual-sub-level streaming with hysteresis,
// over the demo-mirror world. The inn interior is sub-level 1 there
// (Manual | NoNav | NoSpawn) — the same role the crypt/inn interiors play in
// the authored world.
// ---------------------------------------------------------------------------

#include "WorldDemoHarness.h"

#include <world/Portal.h>
#include <world/WorldSystem.h>

namespace {

using se_test::WorldDemoTestBase;

// the Portal accepts a distinctive suffix of the sub-level path
constexpr const char * kInnSuffix = "wd_inn_interior";

TEST_F(WorldDemoTestBase, Portal_StagesAndReleases_OnFocusBand) {
        ASSERT_TRUE(LoadDemoWorld());
        Drain(2);

        auto & oWorlds = GetSystem<WorldSystem>();
        const uint32_t uInn = 1;
        ASSERT_TRUE(oWorlds.GetSubLevelCount() > uInn);
        ASSERT_EQ(oWorlds.GetLevelState(uInn), WorldSystem::ELevelState::Unloaded);

        // portal node + a movable streaming focus parked right on it
        auto pDoor  = CreateNode("PortalDoor");
        pDoor->SetPos({0.f, 2.f, 0.f});
        pDoor->CreateComponent<Portal>(PortalDesc{kInnSuffix, 10.f, 20.f});
        pDoor->Enable();

        auto pFocusNode = CreateNode("PortalFocus");
        pFocusNode->SetPos({0.f, 2.f, 0.f});
        auto hFocus = oWorlds.RegisterFocus(pFocusNode, 0.f, 0.f);

        // inside the activate band: the Manual interior is staged (budgeted
        // drain finishes within a couple of frames for the tiny scene)
        Drain(8);
        EXPECT_EQ(oWorlds.GetLevelState(uInn), WorldSystem::ELevelState::Active);

        // focus leaves the release band → the interior is released again
        pFocusNode->SetPos({0.f, 2.f, 60.f});
        Drain(10);
        EXPECT_EQ(oWorlds.GetLevelState(uInn), WorldSystem::ELevelState::Unloaded);

        // and re-staged on return (the request was rearmed by the release)
        pFocusNode->SetPos({0.f, 2.f, 0.f});
        Drain(8);
        EXPECT_EQ(oWorlds.GetLevelState(uInn), WorldSystem::ELevelState::Active);

        oWorlds.UnregisterFocus(hFocus);
}

TEST_F(WorldDemoTestBase, Portal_DirectorManagedLevel_IsIgnored) {
        ASSERT_TRUE(LoadDemoWorld());
        Drain(2);

        // village (sub-level 0) has NO Manual flag — the streaming director
        // owns it; a portal pointed at it must refuse to manage anything
        auto & oWorlds = GetSystem<WorldSystem>();
        const uint32_t uVillage = 0;
        // the director streams the village in from the initial focus — give it
        // time, then confirm the baseline
        for (int i = 0; i < 120 && oWorlds.GetLevelState(uVillage) != WorldSystem::ELevelState::Active; ++i) {
                Drain(1);
        }
        ASSERT_EQ(oWorlds.GetLevelState(uVillage), WorldSystem::ELevelState::Active);

        auto pDoor = CreateNode("PortalDoorVillage");
        pDoor->SetPos({0.f, 2.f, 0.f});
        pDoor->CreateComponent<Portal>(PortalDesc{"wd_village", 10.f, 20.f});
        pDoor->Enable();

        // the director keeps it wanted regardless of what the portal would do;
        // a portal can only DEactivate — and it never even tries here
        Drain(8);
        EXPECT_EQ(oWorlds.GetLevelState(uVillage), WorldSystem::ELevelState::Active);
}

} // namespace
