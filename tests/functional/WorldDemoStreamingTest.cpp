// ---------------------------------------------------------------------------
// WorldDemoStreamingTest — the streaming lifecycle over the real demo world:
// startup policy, focus-driven activation of the village/forest/farm/tower
// cells, hysteresis on retreat, the Manual inn staying dormant until explicit
// activation, and teardown returning the scene to its baseline.
// ---------------------------------------------------------------------------

#define SE_IMPL
#include "WorldDemoHarness.h"

using namespace SE;

namespace {

constexpr uint32_t kInnIndex = 1;   // world JSON order: village, inn, forest_w, forest_e, farm, tower

} // namespace

TEST_F(WorldDemoTestBase, LoadWorld_StreamingEnabled_InitialRingOnly) {
        ASSERT_TRUE(LoadDemoWorld());

        auto & oWorlds = GetSystem<WorldSystem>();
        ASSERT_EQ(oWorlds.GetSubLevelCount(), 6u);

        // the inn is Manual: dormant no matter where the focus sits
        Drain(4);
        EXPECT_NE(oWorlds.GetLevelState(kInnIndex), WorldSystem::ELevelState::Active);

        // with a focus at the village, the village cell content activates;
        // the far forest cell does not
        auto pFocus = scene->Create("Focus");
        pFocus->SetPos({0.f, 3.f, 0.f});
        oWorlds.RegisterFocus(pFocus, 55.f, 0.f);

        RunSeconds(2.0f);
        EXPECT_EQ(oWorlds.GetLevelState(0), WorldSystem::ELevelState::Active);   // village
        EXPECT_NE(oWorlds.GetLevelState(2), WorldSystem::ELevelState::Active);   // forest west @ ~76 m
}

TEST_F(WorldDemoTestBase, FocusWalk_ForestActivatesOnApproach) {
        ASSERT_TRUE(LoadDemoWorld());

        auto & oWorlds = GetSystem<WorldSystem>();
        auto pFocus = scene->Create("Focus");
        pFocus->SetPos({0.f, 3.f, 0.f});
        oWorlds.RegisterFocus(pFocus, 55.f, 0.f);
        RunSeconds(2.0f);
        EXPECT_EQ(oWorlds.GetLevelState(0), WorldSystem::ELevelState::Active);

        // walk west toward the forest camp (~76 m out)
        pFocus->SetPos({-70.f, 3.f, 0.f});
        RunSeconds(2.5f);
        EXPECT_EQ(oWorlds.GetLevelState(2), WorldSystem::ELevelState::Active);   // forest west
        // hysteresis: the village is 70 m away — inside the unload radius (85)
        EXPECT_EQ(oWorlds.GetLevelState(0), WorldSystem::ELevelState::Active);
}

TEST_F(WorldDemoTestBase, ManualInn_OnlyExplicitActivation) {
        ASSERT_TRUE(LoadDemoWorld());
        auto & oWorlds = GetSystem<WorldSystem>();

        Drain(4);
        EXPECT_NE(oWorlds.GetLevelState(kInnIndex), WorldSystem::ELevelState::Active);

        // the demo's door trigger does exactly this
        EXPECT_TRUE(oWorlds.ActivateLevel(kInnIndex));
        Drain(4);
        EXPECT_EQ(oWorlds.GetLevelState(kInnIndex), WorldSystem::ELevelState::Active);
        EXPECT_TRUE(oWorlds.GetLevelRoot(kInnIndex).lock() != nullptr);

        // exit trigger
        EXPECT_TRUE(oWorlds.DeactivateLevel(kInnIndex));
        Drain(6);
        EXPECT_NE(oWorlds.GetLevelState(kInnIndex), WorldSystem::ELevelState::Active);
}

TEST_F(WorldDemoTestBase, UnloadWorld_TeardownReturnsToBaseline) {
        size_t baseline = 0;
        scene->GetRoot()->DepthFirstWalk([&baseline](auto &) { ++baseline; return true; });

        ASSERT_TRUE(LoadDemoWorld());
        auto & oWorlds = GetSystem<WorldSystem>();
        auto pFocus = scene->Create("Focus");
        pFocus->SetPos({0.f, 3.f, 0.f});
        oWorlds.RegisterFocus(pFocus, 200.f, 0.f);   // pull everything in
        RunSeconds(3.0f);
        EXPECT_GT(ActiveLevelCount(), 0u);

        oWorlds.UnloadWorld();
        Drain(8);

        size_t after = 0;
        scene->GetRoot()->DepthFirstWalk([&after](auto &) { ++after; return true; });
        EXPECT_EQ(after, baseline);
}
