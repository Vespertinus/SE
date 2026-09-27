// ---------------------------------------------------------------------------
// WorldDemoNavTest — the baked demo navmesh through the runtime: tiles ride
// sub-level groups (terrain merged into them at bake time), the village↔camp
// corridor routes once both ends are active, paths fail cleanly into unloaded
// or NoNav areas, and the inn interior contributes no tiles.
// ---------------------------------------------------------------------------

#define SE_IMPL
#include "WorldDemoHarness.h"

#include <nav/NavMeshAsset.h>

using namespace SE;

TEST_F(WorldDemoTestBase, NavMeshRegisters_WorldNavTable) {
        ASSERT_TRUE(LoadDemoWorld());
        Drain(4);

        auto & oNav = GetSystem<NavigationSystem>();
        // the world nav table declared "default" — the bridge registered it
        EXPECT_TRUE(oNav.HasGroup("resource/world/village_gen/wd_village.sesc") ||
                    oNav.GetStats().tiles_live > 0u);
}

TEST_F(WorldDemoTestBase, VillageToCampPath_OnceBothEndsActive) {
        ASSERT_TRUE(LoadDemoWorld());
        auto & oWorlds = GetSystem<WorldSystem>();
        auto & oNav = GetSystem<NavigationSystem>();

        // everything in: streaming disabled world loads all non-Manual levels
        ASSERT_TRUE(oWorlds.LoadWorld("resource/world/world_demo_no_streaming.seworld"));
        Drain(8);

        SNavPath path;
        // village center street -> camp clearing in the west forest
        const bool bOk = oNav.FindPath("default",
                                       glm::vec3(0.0f, 3.0f, 0.0f),
                                       glm::vec3(-76.0f, 3.0f, 4.0f),
                                       path, /*bPartial=*/false);
        EXPECT_TRUE(bOk);
        EXPECT_GT(path.vPoints.size(), 2u);

        // end point lands near the requested goal (surface snapped)
        if (!path.vPoints.empty()) {
                const glm::vec3 vEnd = path.vPoints.back();
                EXPECT_LT(glm::length(vEnd - glm::vec3(-76.0f, 3.0f, 4.0f)), 8.0f);
        }
}

TEST_F(WorldDemoTestBase, PathFailsCleanly_IntoNoNavInterior) {
        ASSERT_TRUE(LoadDemoWorld());

        auto & oNav = GetSystem<NavigationSystem>();
        SNavPath path;
        // the inn interior is NoNav: no tiles, no route — a clean failure, no crash
        const bool bOk = oNav.FindPath("default",
                                       glm::vec3(10.0f, 3.0f, 0.0f),
                                       glm::vec3(10.0f, 3.0f, -10.0f),
                                       path, /*bPartial=*/false);
        EXPECT_FALSE(bOk);
}

TEST_F(WorldDemoTestBase, DeactivatedForest_RemovesItsTiles) {
        ASSERT_TRUE(LoadDemoWorld());
        auto & oWorlds = GetSystem<WorldSystem>();
        auto & oNav = GetSystem<NavigationSystem>();

        auto pFocus = scene->Create("Focus");
        pFocus->SetPos({-70.f, 3.f, 0.f});
        oWorlds.RegisterFocus(pFocus, 55.f, 0.f);
        RunSeconds(3.0f);
        ASSERT_EQ(oWorlds.GetLevelState(2), WorldSystem::ELevelState::Active);

        SNavPath path;
        EXPECT_TRUE(oNav.FindPath("default",
                                  glm::vec3(-70.0f, 3.0f, 0.0f),
                                  glm::vec3(-76.0f, 3.0f, 4.0f),
                                  path, /*bPartial=*/false));

        oWorlds.DeactivateLevel(2);
        Drain(6);

        // same query now fails cleanly — its tiles left with the sub-level
        SNavPath path2;
        EXPECT_FALSE(oNav.FindPath("default",
                                   glm::vec3(-70.0f, 3.0f, 0.0f),
                                   glm::vec3(-76.0f, 3.0f, 4.0f),
                                   path2, /*bPartial=*/false));
}
