// ---------------------------------------------------------------------------
// WorldDemoAssetsTest — the committed world_demo asset chain loads and is
// internally consistent: world manifest, sub-level scenes, terrain bake,
// navmesh, AI assets, NPC templates. Bounds match the tile-aligned layout the
// nav bake claims (see scripts/village/build_village.py).
// ---------------------------------------------------------------------------

#define SE_IMPL
#include "WorldDemoHarness.h"

#include <world/WorldAsset.h>
#include <terrain/TerrainAsset.h>
#include <nav/NavMeshAsset.h>
#include <ai/BlackboardAsset.h>
#include <ai/BehaviorTreeAsset.h>

#include <boost/filesystem.hpp>

using namespace SE;

namespace {

constexpr const char * kSubLevels[] = {
        "resource/world/village_gen/wd_village.sesc",
        "resource/world/village_gen/wd_inn_interior.sesc",
        "resource/world/village_gen/wd_forest_w.sesc",
        "resource/world/village_gen/wd_forest_e.sesc",
        "resource/world/village_gen/wd_farm.sesc",
        "resource/world/village_gen/wd_tower.sesc",
        "resource/world/village_gen/wd_crypt.sesc",
};

} // namespace

TEST_F(WorldDemoTestBase, WorldFileLoads_AllSubLevelsDeclared) {
        // the real committed world loads headless (activation needs GL, not loading)
        ASSERT_TRUE(GetSystem<WorldSystem>().LoadWorld("resource/world/world_demo.seworld"));

        auto * pAsset = GetSystem<WorldSystem>().GetWorldAsset();
        ASSERT_NE(pAsset, nullptr);
        ASSERT_EQ(pAsset->GetSubLevels().size(), 7u);

        // Round 6: the inn dropped NoNav so its baked coexisting navmesh layer
        // page-flips with the Manual activation; the crypt joins with the same
        // Manual|NoSpawn (nav-enabled) profile.
        bool bFoundInn = false;
        bool bFoundCrypt = false;
        for (const auto & entry : pAsset->GetSubLevels()) {
                if (entry.path.find("wd_inn_interior") != std::string::npos) {
                        bFoundInn = true;
                        EXPECT_TRUE(entry.flags & 0x2u);   // Manual
                        EXPECT_FALSE(entry.flags & 0x4u);  // nav ENABLED now
                        EXPECT_TRUE(entry.flags & 0x8u);   // NoSpawn
                }
                if (entry.path.find("wd_crypt") != std::string::npos) {
                        bFoundCrypt = true;
                        EXPECT_TRUE(entry.flags & 0x2u);   // Manual
                        EXPECT_FALSE(entry.flags & 0x4u);  // nav ENABLED
                        EXPECT_TRUE(entry.flags & 0x8u);   // NoSpawn
                }
        }
        EXPECT_TRUE(bFoundInn);
        EXPECT_TRUE(bFoundCrypt);

        // world declares its nav + global blackboard
        EXPECT_STREQ(pAsset->GetDefaultNavClass().c_str(), "default");
}

TEST_F(WorldDemoTestBase, MirrorWorldLoads) {
        EXPECT_TRUE(LoadDemoWorld());
}

TEST_F(WorldDemoTestBase, AllReferencedAssetsExist) {
        const char * szFiles[] = {
                "resource/world/world_demo.seworld",
                "resource/world/world_demo_no_streaming.seworld",
                "resource/nav/world_demo_default.senm",
                "resource/terrain/world_demo/world_demo.seterrain",
                "resource/ai/world_demo/bb_world.sebb",
                "resource/ai/world_demo/bb_villager.sebb",
                "resource/ai/world_demo/bb_guard.sebb",
                "resource/ai/world_demo/bt_villager_patrol.sebt",
                "resource/ai/world_demo/bt_camp_guard.sebt",
                "resource/world/village_gen/villager_a.setm",
                "resource/world/village_gen/villager_b.setm",
                "resource/world/village_gen/farmer.setm",
                "resource/world/village_gen/guard.setm",
                "resource/sound_cue/village.secl",
                "resource/world/world_demo.json",
        };
        for (const char * sz : szFiles) {
                EXPECT_TRUE(boost::filesystem::exists(sz)) << "missing: " << sz;
        }
        for (const char * sz : kSubLevels) {
                EXPECT_TRUE(boost::filesystem::exists(sz)) << "missing: " << sz;
        }
}

TEST_F(WorldDemoTestBase, TerrainBake_CoversWorld_TileAligned) {
        auto hTerrain = CreateResource<TerrainAsset>("resource/terrain/world_demo/world_demo.seterrain");
        const TerrainAsset * pTerrain = GetResource(hTerrain);
        ASSERT_NE(pTerrain, nullptr);

        // 640 m span at 40 m tiles (16x16), origin -320
        EXPECT_EQ(pTerrain->GetTilesX(), 16u);
        EXPECT_EQ(pTerrain->GetTilesY(), 16u);
        EXPECT_FLOAT_EQ(pTerrain->GetTileSize(), 40.0f);
        EXPECT_FLOAT_EQ(pTerrain->GetOrigin().x, -320.0f);
        EXPECT_FLOAT_EQ(pTerrain->GetOrigin().z, -320.0f);

        // the village plateau is walkable-flat near the origin
        // height queries need an active tile; the bake manifest itself only
        // carries ranges — existence of the tile grid is the contract here
        EXPECT_NE(pTerrain->FindTile(8, 8), nullptr);
}

TEST_F(WorldDemoTestBase, NavmeshAndBlackboards_Load) {        auto hNav = CreateResource<NavMeshAsset>("resource/nav/world_demo_default.senm");
        const NavMeshAsset * pNav = GetResource(hNav);
        ASSERT_NE(pNav, nullptr);
        EXPECT_STREQ(pNav->AgentClass().c_str(), "default");
        EXPECT_GT(pNav->GetTiles().size(), 512u);   // full terrain coverage incl. merged groups

        auto hBB = CreateResource<BlackboardAsset>("resource/ai/world_demo/bb_world.sebb");
        const BlackboardAsset * pBB = GetResource(hBB);
        ASSERT_NE(pBB, nullptr);
        // global schema: the four keys the demo + brains rely on
        EXPECT_EQ(pBB->GetKeyCount(), 4u);

        auto hBt = CreateResource<BlackboardAsset>("resource/ai/world_demo/bb_villager.sebb");
        EXPECT_NE(GetResource(hBt), nullptr);
}

TEST_F(WorldDemoTestBase, BehaviorTrees_BuildWithDemoTaskset) {
        // The full TBehaviorTreeAsset<TMyBTTasks>::Build path over the exact
        // .sebt bytes the NPC templates instantiate at runtime (existence alone
        // does not exercise node emission).
        for (const char * szTree : { "resource/ai/world_demo/bt_villager_patrol.sebt",
                                     "resource/ai/world_demo/bt_camp_guard.sebt" }) {
                auto hTree = CreateResource<BTAsset>(szTree);
                ASSERT_NE(GetResource(hTree), nullptr) << szTree;
        }
}

TEST_F(WorldDemoTestBase, NpcTemplate_Instantiate) {
        // The exact runtime flow of the demo: load a committed NPC template and
        // instantiate it (NavAgent/PathFollower/Blackboard/BehaviorTree load +
        // the Armature scene subtree). With a GL context the full hierarchy
        // comes up; headless, the visual component's shader creation throws and
        // ETS rolls the hierarchy back. The regressions guarded here are the
        // mid-dispatch EUpdate listener-vector-realloc SEGV and the
        // renderer-less RemoveRenderable deref — both crashed hard before.
        auto & oETS = GetSystem<TEntityTemplateSystem>();
        oETS.SetSceneTree(scene.get());
        ASSERT_EQ(oETS.Load("resource/world/village_gen/villager_b.setm"), uSUCCESS);

        auto pNode = oETS.Instantiate("villager_b", TSceneTree::TSceneNode(nullptr),
                                      glm::vec3(0.f, 2.f, 10.f)).lock();
        EXPECT_TRUE(pNode == nullptr ||
                    pNode->FindChild(StrID("Armature"), /*recursive=*/true) != nullptr);
        Drain(2);
}
