
// ---------------------------------------------------------------------------
// NavRuntimeTest — Stage 3 Step 7 (tests/functional/nav_app composition).
//
// Tier B: NavigationSystem driven directly over synthetic NavBake output —
// registration, queries, area costs, group lifecycle, terrain refcount paging,
// budget, corridors. No world involved (the system is world-agnostic).
// Tier C: world-driven lifecycle through WorldNavBridge — world-declared nav
// (path + inline holders), terrain-grid paging events, sub-level tile
// lifecycle, NavAgent component lifecycle, spawner snap=NavMesh, and the
// world-teardown drain (every terrain tile deactivation must unload its nav
// tiles).
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>
#include "EngineFixture.h"
#include "PhysicsHarness.h"
#include "NavAssets.h"
#include "TerrainAssets.h"
#include "WorldAssets.h"

#include <terrain/TerrainBake.tcc>
#include <nav/NavBake.h>
#include <nav/NavBake.tcc>
#include <nav/NavMeshAsset.h>
#include <nav/NavigationSystem.h>
#include <nav/WorldNavBridge.h>
#include <nav/NavAgent.h>
#include <world/WorldSystem.h>
#include <world/WorldAsset.h>
#include <terrain/TerrainSystem.h>
#include <terrain/TerrainAsset.h>
#include <EntityManager.h>
#include <spawner/SpawnerSystem.h>
#include <spawner/Population.h>

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>

#include <chrono>
#include <cmath>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

using namespace SE;

namespace {

// ---------------------------------------------------------------------------
// fixture
// ---------------------------------------------------------------------------

class NavRuntimeTest : public se_test::EnginePhysTestBase {

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
                        oEngine.Init<TEntityTemplateSystem>();
                        oEngine.Init<SpawnerSystem>();
                        oEngine.Init<WorldNavBridge>();   // after WorldSystem + NavigationSystem
                        return true;
                }();
                (void)systems_constructed;

                GetSystem<EntityManager>().SetSceneTree(scene.get());
                GetSystem<WorldSystem>().SetSceneTree(scene.get());
                GetSystem<TEntityTemplateSystem>().SetSceneTree(scene.get());

                // fast terrain paging (TerrainTest convention)
                TerrainSystem::SConfig oCfg;
                oCfg.load_radius   = 10.0f;
                oCfg.unload_radius = 16.0f;
                oCfg.unload_frame_delay = 3;
                oCfg.max_tile_activations_per_frame = 8;
                oCfg.max_active_tiles = 64;
                Terrain().SetConfig(oCfg);

                // deterministic random points + the app-side nav probe seam
                Nav().SetRandomSeed(0x5EEDC0DEull);
                auto * pNav = &Nav();
                Spawner().SetNavProbe([pNav](const glm::vec3 & vPos, glm::vec3 & vOut) {
                        // generous radius: spawn markers may hover far above the mesh
                        return pNav->ProjectToNavMesh(pNav->DefaultClassID(), vPos, 1024.f, vOut);
                });

                // plain-spawn delivery (SpawnerTerrainTest convention)
                auto * pEM = &GetSystem<EntityManager>();
                pEM->SetTemplateInstantiator(
                        [pEM](const SpawnIntent & oIntent) -> TSceneTree::TSceneNodeWeak {
                                SpawnRequest oReq;
                                oReq.name         = oIntent.name;
                                oReq.pParent      = oIntent.pParent;
                                oReq.vTranslation = oIntent.vTranslation;
                                oReq.vRotation    = oIntent.vRotation;
                                oReq.vScale       = oIntent.vScale;
                                oReq.enabled      = oIntent.enabled;
                                return pEM->Spawn(oReq);
                        });

                boost::filesystem::create_directories("resource/terrain/gen");
                boost::filesystem::create_directories("resource/world/gen");
                boost::filesystem::create_directories("resource/nav/gen");
        }

        void TearDown() override {
                for (const auto h : vFocusHandles) { Worlds().UnregisterFocus(h); }
                vFocusHandles.clear();

                Terrain().SetGpuSink(0, {}, {});
                Terrain().DetachAll();
                Spawner().SetTerrainProbe({});
                Spawner().SetPhysicsProbe({});
                Spawner().SetNavProbe({});
                GetSystem<EntityManager>().SetTemplateInstantiator({});

                Worlds().UnloadWorld();
                Drain();

                // nav state must not leak between tests in this TU
                Nav().UnregisterAll();
                Nav().SetDefaultClass("");
                EXPECT_EQ(Nav().GetStats().tiles_live, 0u);

                Worlds().SetSceneTree(nullptr);
                GetSystem<EntityManager>().SetSceneTree(nullptr);
                GetSystem<TEntityTemplateSystem>().SetSceneTree(nullptr);

                GetSystem<TResourceManager>().Clear<TerrainAsset>();
                GetSystem<TResourceManager>().Clear<WorldAsset>();
                GetSystem<TResourceManager>().Clear<StateMachineAsset>();
                GetSystem<TResourceManager>().Clear<NavMeshAsset>();

                EnginePhysTestBase::TearDown();
        }

        void Drain(const uint32_t n = 2) {
                for (uint32_t i = 0; i < n; ++i) { StepFrame(1.f / 60.f); }
        }

        template <class TPred>
        bool PumpUntil(TPred pred, const uint32_t max_frames = 3000) {
                for (uint32_t i = 0; i < max_frames; ++i) {
                        StepFrame(1.f / 60.f);
                        if (pred()) { return true; }
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                return pred();
        }

        NavigationSystem & Nav()     { return GetSystem<NavigationSystem>(); }
        WorldSystem  &     Worlds()  { return GetSystem<WorldSystem>(); }
        TerrainSystem &    Terrain() { return GetSystem<TerrainSystem>(); }
        SpawnerSystem &    Spawner() { return GetSystem<SpawnerSystem>(); }

        // ---- bake helpers -------------------------------------------------

        // Ground 16x16 + four 0.25 m grounded step boxes + 1 m plateau (the
        // NavBakeTest shape — floating slabs would become ledge-voids).
        NavBake::SOutput BakeStairs(const std::string & sGroup = "ground") {
                NavBake::SBakeInput in;
                NavBake::SGroup & g = in.vGroups.emplace_back();
                g.id = sGroup;
                g.bounds_min = glm::vec3(0.f, -1.f, 0.f);
                g.bounds_max = glm::vec3(16.f, 3.f, 16.f);
                se_nav_test::AddQuad(g.vMeshes.emplace_back(), 0.f, 0.f, 16.f, 16.f, 0.f);
                for (int k = 0; k < 4; ++k) {
                        se_nav_test::AddBox(g.vMeshes.back(), 4.f + k, 6.f, 5.f + k, 10.f,
                                            0.25f * float(k + 1));
                }
                se_nav_test::AddBox(g.vMeshes.back(), 8.f, 6.f, 16.f, 10.f, 1.0f);
                NavBake::SOutput out;
                EXPECT_TRUE(NavBake::Bake(in, se_nav_test::MakeConfig(8.f), out));
                return out;
        }

        // Flat .senm asset table from a bake output (also the inline-holder flavor).
        static FlatBuffers::NavMeshAssetT MakeNavAssetT(const NavBake::SOutput & out,
                                                        const std::string & sClass,
                                                        const std::vector<std::string> & vAreas) {
                FlatBuffers::NavMeshAssetT a;
                a.agent_class = sClass;
                a.version     = 1;
                a.origin      = std::make_unique<FlatBuffers::Vec3>(out.origin.x, out.origin.y, out.origin.z);
                a.tile_dim    = out.tile_dim;
                a.voxel_size   = 0.2f;
                a.voxel_height = 0.2f;
                a.areas        = vAreas;
                for (const NavBake::SNavTileBytes & t : out.vTiles) {
                        auto pTile  = std::make_unique<FlatBuffers::NavTileT>();
                        pTile->x    = t.x;
                        pTile->y    = t.y;
                        pTile->layer = t.layer;
                        pTile->group = t.group;
                        pTile->data = t.vBytes;
                        a.tiles.push_back(std::move(pTile));
                }
                for (const NavBake::SLinkRecord & l : out.vLinks) {
                        auto pLink  = std::make_unique<FlatBuffers::OffMeshLinkT>();
                        pLink->start = std::make_unique<FlatBuffers::Vec3>(l.start.x, l.start.y, l.start.z);
                        pLink->end   = std::make_unique<FlatBuffers::Vec3>(l.end.x, l.end.y, l.end.z);
                        pLink->radius = l.radius;
                        pLink->area   = static_cast<uint16_t>(l.registry_index);
                        pLink->flags  = l.flags;
                        pLink->bidirectional = l.bidirectional;
                        pLink->group  = l.group;
                        a.links.push_back(std::move(pLink));
                }
                return a;
        }

        // Flat .senm bytes from a bake output.
        static std::vector<uint8_t> BakeNavBytes(const NavBake::SOutput & out,
                                                 const std::string & sClass,
                                                 const std::vector<std::string> & vAreas) {
                FlatBuffers::NavMeshAssetT a = MakeNavAssetT(out, sClass, vAreas);
                flatbuffers::FlatBufferBuilder oBuilder;
                auto offset = FlatBuffers::NavMeshAsset::Pack(oBuilder, &a);
                FlatBuffers::FinishNavMeshAssetBuffer(oBuilder, offset);
                const uint8_t * p = oBuilder.GetBufferPointer();
                return std::vector<uint8_t>(p, p + oBuilder.GetSize());
        }

        // Bakes + registers one class over the output (unique resource name).
        H<NavMeshAsset> RegisterBake(const NavBake::SOutput & out, const std::string & sClass,
                                     const std::vector<std::string> & vAreas = {""}) {
                const std::vector<uint8_t> vBytes = BakeNavBytes(out, sClass, vAreas);
                H<NavMeshAsset> h = CreateResource<NavMeshAsset>(
                                "navrt_" + sClass + "_" + std::to_string(++resourceSerial),
                                vBytes.data(), vBytes.size());
                EXPECT_TRUE(h.IsValid());
                EXPECT_TRUE(Nav().RegisterClass(sClass, h));
                return h;
        }

        uint32_t resourceSerial = 0;
        std::vector<WorldSystem::FocusHandle> vFocusHandles;
};

// ---------------------------------------------------------------------------
// Tier B — the world-agnostic runtime
// ---------------------------------------------------------------------------

TEST_F(NavRuntimeTest, AssetRoundTrip) {
        const NavBake::SOutput out = BakeStairs();
        ASSERT_FALSE(out.vTiles.empty());

        const std::vector<uint8_t> vBytes = BakeNavBytes(out, "default", {""});
        H<NavMeshAsset> h = CreateResource<NavMeshAsset>("navrt_roundtrip", vBytes.data(), vBytes.size());
        ASSERT_TRUE(h.IsValid());

        const NavMeshAsset * pAsset = GetResource(h);
        ASSERT_NE(pAsset, nullptr);
        EXPECT_EQ(pAsset->AgentClass(), "default");
        EXPECT_EQ(pAsset->GetTiles().size(), out.vTiles.size());
        ASSERT_EQ(pAsset->GetGroups().size(), 1u);
        EXPECT_EQ(pAsset->GetGroups()[0], "ground");
        EXPECT_EQ(pAsset->GroupIndex("ground"), 0);
        EXPECT_EQ(pAsset->GroupIndex("nowhere"), -1);
        EXPECT_FLOAT_EQ(pAsset->TileDim(), 8.f);
        EXPECT_GT(pAsset->MaxTilePolys(), 0u);
        EXPECT_GT(pAsset->GetTiles()[0].size, 0u);
        EXPECT_NE(pAsset->GetTiles()[0].pData, nullptr);

        // corrupt bytes are rejected with a throw, not silently loaded
        std::vector<uint8_t> vJunk(256, 0xAB);
        EXPECT_ANY_THROW(CreateResource<NavMeshAsset>("navrt_garbage", vJunk.data(), vJunk.size()));
}

TEST_F(NavRuntimeTest, RegistrationLifecycle) {
        EXPECT_FALSE(Nav().RegisterClass("default", H<NavMeshAsset>{}));

        RegisterBake(BakeStairs(), "default");
        EXPECT_NE(Nav().ClassIndex(StrID("default")), -1);
        EXPECT_TRUE(Nav().HasGroup(StrID("ground")));
        EXPECT_FALSE(Nav().HasGroup(StrID("nowhere")));

        // nothing is queryable before its group activates
        SNavPoint pt;
        EXPECT_FALSE(Nav().FindNearest(StrID("default"), glm::vec3(2.f, 0.f, 2.f), glm::vec3(1.f, 2.f, 1.f), pt));
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));
        EXPECT_GT(Nav().GetStats().tiles_live, 0u);
        EXPECT_TRUE(Nav().FindNearest(StrID("default"), glm::vec3(2.f, 0.f, 2.f), glm::vec3(1.f, 2.f, 1.f), pt));

        Nav().UnregisterAll();
        EXPECT_EQ(Nav().ClassIndex(StrID("default")), -1);
        EXPECT_FALSE(Nav().FindNearest(StrID("default"), glm::vec3(2.f, 0.f, 2.f), glm::vec3(1.f, 2.f, 1.f), pt));
        EXPECT_EQ(Nav().GetStats().tiles_live, 0u);
}

TEST_F(NavRuntimeTest, NearestAndPathOverStairs) {
        RegisterBake(BakeStairs(), "default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        SNavPoint pt;
        EXPECT_TRUE(Nav().FindNearest(StrID("default"), glm::vec3(2.f, 0.f, 2.f), glm::vec3(1.f, 2.f, 1.f), pt));
        EXPECT_NEAR(pt.pos.y, 0.f, 0.25f);   // voxel-quantized surface

        SNavPath path;
        EXPECT_TRUE(Nav().FindPath(StrID("default"), glm::vec3(2.f, 0.f, 2.f), glm::vec3(14.f, 1.f, 8.f), path));
        EXPECT_FALSE(path.partial);
        EXPECT_GE(path.vPoints.size(), 2u);
        EXPECT_GT(path.vCorridor.size(), 1u);   // corridor spans more than the start poly
        EXPECT_LE(path.vCornerPolys.size(), path.vCorridor.size());
        EXPECT_NEAR(path.vPoints.back().y, 1.0f, 0.35f);   // climbed the steps to the plateau

        // a point in the air still projects (maxDist is the search radius)
        EXPECT_TRUE(Nav().ProjectToNavMesh(StrID("default"), glm::vec3(12.f, 3.f, 8.f), 2.5f, pt.pos));
        EXPECT_NEAR(pt.pos.y, 1.0f, 0.3f);
}

TEST_F(NavRuntimeTest, AreaCostReroute) {
        // a ring: two parallel strips joined at both ends; the top strip's middle
        // is a named "slow" area. A high cost must push the corridor onto the
        // cheap strip.
        NavBake::SBakeInput in;
        NavBake::SGroup & g = in.vGroups.emplace_back();
        g.id = "ground";
        g.bounds_min = glm::vec3(-1.f, -1.f, -1.f);
        g.bounds_max = glm::vec3(25.f, 3.f, 17.f);
        se_nav_test::AddQuad(g.vMeshes.emplace_back(), 0.f, 0.f, 24.f, 8.f, 0.f);    // top strip
        se_nav_test::AddQuad(g.vMeshes.emplace_back(), 0.f, 8.f, 24.f, 16.f, 0.f);   // bottom strip
        se_nav_test::AddQuad(g.vMeshes.emplace_back(), 0.f, 0.f, 2.f, 16.f, 0.f);    // left joint
        se_nav_test::AddQuad(g.vMeshes.emplace_back(), 22.f, 0.f, 24.f, 16.f, 0.f);  // right joint
        NavBake::SAreaBox & area = g.vAreaBoxes.emplace_back();
        area.center      = glm::vec3(12.f, 0.f, 4.f);
        area.half_extents = glm::vec3(13.f, 2.f, 4.f);   // the entire top strip
        area.registry_index = 1;   // "slow"

        NavBake::SOutput out;
        ASSERT_TRUE(NavBake::Bake(in, se_nav_test::MakeConfig(8.f, {"", "slow"}), out));

        RegisterBake(out, "default", {"", "slow"});
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        const glm::vec3 vFrom(1.f, 0.f, 4.f);
        const glm::vec3 vTo(23.f, 0.f, 4.f);

        SNavPath oDirect;
        ASSERT_TRUE(Nav().FindPath(StrID("default"), vFrom, vTo, oDirect));

        EXPECT_TRUE(Nav().SetAreaCost("default", "slow", 80.f));
        SNavPath oDetour;
        ASSERT_TRUE(Nav().FindPath(StrID("default"), vFrom, vTo, oDetour));

        auto PathLength = [](const SNavPath & p) {
                float f = 0.f;
                for (size_t i = 1; i < p.vPoints.size(); ++i) {
                        f += glm::distance(p.vPoints[i - 1], p.vPoints[i]);
                }
                return f;
        };
        const float fDirect = PathLength(oDirect);
        const float fDetour = PathLength(oDetour);
        // the expensive strip pushes the corridor onto the longer route (the
        // funnel rides the strip boundary, so the margin is small but fixed)
        EXPECT_GT(fDetour, fDirect + 1.5f);

        // reverting the cost restores the short route
        EXPECT_TRUE(Nav().SetAreaCost("default", "slow", 1.f));
        SNavPath oRestored;
        ASSERT_TRUE(Nav().FindPath(StrID("default"), vFrom, vTo, oRestored));
        EXPECT_NEAR(PathLength(oRestored), fDirect, 0.5f);

        EXPECT_FALSE(Nav().SetAreaCost("default", "unknown_area", 1.f));
}

TEST_F(NavRuntimeTest, LinkTraversal) {
        // two 6x4 platforms with a 4 m gap; one authored link crosses it
        // (endpoints inside the eroded walkable space — NavBakeTest geometry)
        NavBake::SBakeInput in;
        NavBake::SGroup & g = in.vGroups.emplace_back();
        g.id = "ground";
        g.bounds_min = glm::vec3(-1.f, -1.f, -1.f);
        g.bounds_max = glm::vec3(17.f, 3.f, 17.f);
        se_nav_test::AddQuad(g.vMeshes.emplace_back(), 0.f, 6.f, 6.f, 10.f, 0.f);
        se_nav_test::AddQuad(g.vMeshes.emplace_back(), 10.f, 6.f, 16.f, 10.f, 0.f);
        NavBake::SLink & link = g.vLinks.emplace_back();
        link.start = glm::vec3(5.4f, 0.f, 8.f);
        link.end   = glm::vec3(10.6f, 0.f, 8.f);
        link.radius = 1.0f;

        NavBake::SOutput out;
        ASSERT_TRUE(NavBake::Bake(in, se_nav_test::MakeConfig(8.f), out));
        ASSERT_FALSE(out.vLinks.empty());

        RegisterBake(out, "default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        SNavPath path;
        EXPECT_TRUE(Nav().FindPath(StrID("default"), glm::vec3(2.f, 0.f, 8.f), glm::vec3(14.f, 0.f, 8.f), path));
        // the only route crosses the gap through the off-mesh connection
        EXPECT_GE(path.vCorridor.size(), 3u);
        EXPECT_GT(path.vPoints.size(), 2u);
}

TEST_F(NavRuntimeTest, RandomPointAndProject) {
        RegisterBake(BakeStairs(), "default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        Nav().SetRandomSeed(42);
        for (int i = 0; i < 16; ++i) {
                glm::vec3 vOut;
                EXPECT_TRUE(Nav().RandomPointAround(StrID("default"), glm::vec3(4.f, 0.f, 4.f), 6.f, vOut));
                // the returned point must be on the mesh
                SNavPoint pt;
                EXPECT_TRUE(Nav().FindNearest(StrID("default"), vOut, glm::vec3(0.5f, 2.f, 0.5f), pt));
                EXPECT_LT(glm::distance(pt.pos, vOut), 0.5f);
        }
        // deterministic: same seed => same first point
        glm::vec3 vA, vB;
        Nav().SetRandomSeed(7);
        ASSERT_TRUE(Nav().RandomPointAround(StrID("default"), glm::vec3(4.f, 0.f, 4.f), 6.f, vA));
        Nav().SetRandomSeed(7);
        ASSERT_TRUE(Nav().RandomPointAround(StrID("default"), glm::vec3(4.f, 0.f, 4.f), 6.f, vB));
        EXPECT_EQ(vA, vB);
}

TEST_F(NavRuntimeTest, GroupActivationLifecycle) {
        // two adjacent ground groups (shared tile edge at x = 16) — the
        // cross-group path appears only when both sides are active. The meshes
        // overlap the boundary by 4 m so each tile build sees walkable data in
        // its border band and Detour can match the external portal edges.
        NavBake::SBakeInput in;
        NavBake::SGroup & a = in.vGroups.emplace_back();
        a.id = "group_a";
        a.bounds_min = glm::vec3(0.f, -1.f, 0.f);
        a.bounds_max = glm::vec3(16.f, 3.f, 16.f);
        se_nav_test::AddQuad(a.vMeshes.emplace_back(), 0.f, 0.f, 20.f, 16.f, 0.f);
        NavBake::SGroup & b = in.vGroups.emplace_back();
        b.id = "group_b";
        b.bounds_min = glm::vec3(16.f, -1.f, 0.f);
        b.bounds_max = glm::vec3(32.f, 3.f, 16.f);
        se_nav_test::AddQuad(b.vMeshes.emplace_back(), 12.f, 0.f, 32.f, 16.f, 0.f);

        NavBake::SOutput out;
        ASSERT_TRUE(NavBake::Bake(in, se_nav_test::MakeConfig(8.f), out));
        RegisterBake(out, "default");

        const glm::vec3 vFrom(2.f, 0.f, 8.f);
        const glm::vec3 vTo(30.f, 0.f, 8.f);
        SNavPath path;

        // only A: the far side does not exist
        ASSERT_TRUE(Nav().ActivateGroup(StrID("group_a")));
        EXPECT_FALSE(Nav().FindPath(StrID("default"), vFrom, vTo, path));

        // both: the corridor crosses the tile border
        ASSERT_TRUE(Nav().ActivateGroup(StrID("group_b")));
        EXPECT_TRUE(Nav().FindPath(StrID("default"), vFrom, vTo, path));
        const uint32_t tiles_both = Nav().GetStats().tiles_live;
        EXPECT_GT(tiles_both, 0u);

        // idempotent activation (nesting) — no double tiles
        EXPECT_TRUE(Nav().ActivateGroup(StrID("group_b")));
        EXPECT_EQ(Nav().GetStats().tiles_live, tiles_both);

        // one release keeps the group alive, the second removes it
        EXPECT_TRUE(Nav().DeactivateGroup(StrID("group_b")));
        EXPECT_TRUE(Nav().FindPath(StrID("default"), vFrom, vTo, path));
        EXPECT_TRUE(Nav().DeactivateGroup(StrID("group_b")));
        EXPECT_FALSE(Nav().FindPath(StrID("default"), vFrom, vTo, path));

        // deactivating an inactive group is a no-op
        EXPECT_FALSE(Nav().DeactivateGroup(StrID("group_b")));

        // re-activation restores
        ASSERT_TRUE(Nav().ActivateGroup(StrID("group_b")));
        EXPECT_TRUE(Nav().FindPath(StrID("default"), vFrom, vTo, path));
}

TEST_F(NavRuntimeTest, TerrainPagingRefcount) {
        // 16x16 flat heightfield under group id == a terrain manifest path,
        // nav tile_dim 4 => 4x4 nav tiles
        NavBake::SBakeInput in;
        NavBake::SGroup & g = in.vGroups.emplace_back();
        g.id = "resource/terrain/gen/navrt_refcount.seterrain";
        g.bounds_min = glm::vec3(0.f, -1.f, 0.f);
        g.bounds_max = glm::vec3(16.f, 3.f, 16.f);
        NavBake::SGeomHeightfield & hf = g.vHeightfields.emplace_back();
        hf.origin = glm::vec3(0.f, 0.f, 0.f);
        hf.tile_size = 16.f;
        hf.resolution = 9;
        hf.vHeights.assign(81, 0.f);

        NavBake::SOutput out;
        ASSERT_TRUE(NavBake::Bake(in, se_nav_test::MakeConfig(4.f), out));
        ASSERT_EQ(out.vTiles.size(), 16u);
        RegisterBake(out, "default");

        const StrID manifest("resource/terrain/gen/navrt_refcount.seterrain");

        // terrain tile T1 covers nav columns 0-1, rows 0-1
        Nav().ActivateTerrainTile(manifest, glm::vec3(0.f, 0.f, 0.f), glm::vec3(8.f, 1.f, 8.f));
        EXPECT_EQ(Nav().GetStats().tiles_live, 4u);

        // T2 (4..12) overlaps column 1: one nav tile gains a second supporter
        Nav().ActivateTerrainTile(manifest, glm::vec3(4.f, 0.f, 0.f), glm::vec3(12.f, 1.f, 8.f));
        EXPECT_EQ(Nav().GetStats().tiles_live, 6u);

        // releasing T1 unloads only its exclusive tiles; the straddler stays
        Nav().DeactivateTerrainTile(manifest, glm::vec3(0.f, 0.f, 0.f), glm::vec3(8.f, 1.f, 8.f));
        EXPECT_EQ(Nav().GetStats().tiles_live, 4u);

        // releasing T2 drains the rest
        Nav().DeactivateTerrainTile(manifest, glm::vec3(4.f, 0.f, 0.f), glm::vec3(12.f, 1.f, 8.f));
        EXPECT_EQ(Nav().GetStats().tiles_live, 0u);

        // unknown manifest (component-merged terrain) is inert
        EXPECT_FALSE(Nav().ActivateTerrainTile(StrID("some/other.seterrain"),
                                               glm::vec3(0.f, 0.f, 0.f), glm::vec3(8.f, 1.f, 8.f)));
        EXPECT_EQ(Nav().GetStats().tiles_live, 0u);
}

TEST_F(NavRuntimeTest, BudgetEnforcement) {
        RegisterBake(BakeStairs(), "default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        NavigationSystem::SConfig cfg;
        cfg.max_queries_per_frame = 2;
        Nav().SetConfig(cfg);

        SNavPoint pt;
        EXPECT_TRUE(Nav().FindNearest(StrID("default"), glm::vec3(2.f, 0.f, 4.f), glm::vec3(1.f, 2.f, 1.f), pt));
        EXPECT_TRUE(Nav().FindNearest(StrID("default"), glm::vec3(2.f, 0.f, 4.f), glm::vec3(1.f, 2.f, 1.f), pt));
        EXPECT_FALSE(Nav().FindNearest(StrID("default"), glm::vec3(2.f, 0.f, 4.f), glm::vec3(1.f, 2.f, 1.f), pt));
        EXPECT_EQ(Nav().GetStats().queries, 2u);
        EXPECT_EQ(Nav().GetStats().budget_rejected, 1u);

        // frame boundary resets the counters
        GetSystem<EventManager>().TriggerEvent(EPostUpdate{1.f / 60.f});
        EXPECT_TRUE(Nav().FindNearest(StrID("default"), glm::vec3(2.f, 0.f, 4.f), glm::vec3(1.f, 2.f, 1.f), pt));

        Nav().SetConfig(NavigationSystem::SConfig{});
}

TEST_F(NavRuntimeTest, CorridorPrimitives) {
        RegisterBake(BakeStairs(), "default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        NavAgent oAgent(nullptr, NavAgentDesc{"default", 0.5f, 2.f});
        Nav().AddComponent(&oAgent);
        EXPECT_EQ(Nav().GetStats().agents, 1u);

        EXPECT_TRUE(Nav().SetAgentPath(&oAgent, glm::vec3(2.f, 0.f, 4.f), glm::vec3(15.f, 1.f, 4.f)));
        dtPathCorridor * pCorridor = Nav().GetAgentCorridor(&oAgent);
        ASSERT_NE(pCorridor, nullptr);
        EXPECT_GT(pCorridor->getPathCount(), 0);

        std::vector<NavigationSystem::SAgentDebug> vAgents;
        Nav().GetDebugAgents(vAgents);
        ASSERT_EQ(vAgents.size(), 1u);
        EXPECT_EQ(vAgents[0].sClass, "default");
        EXPECT_TRUE(vAgents[0].has_path);
        EXPECT_GT(vAgents[0].path_points, 1u);

        EXPECT_TRUE(Nav().MoveAgentAlong(&oAgent, glm::vec3(8.f, 0.5f, 4.f)));

        Nav().RemoveComponent(&oAgent);
        EXPECT_EQ(Nav().GetAgentCorridor(&oAgent), nullptr);
        EXPECT_EQ(Nav().GetStats().agents, 0u);
}

TEST_F(NavRuntimeTest, UnknownClassCleanFailure) {
        RegisterBake(BakeStairs(), "default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        // no default declared: unknown class fails cleanly (warned once)
        SNavPoint pt;
        EXPECT_FALSE(Nav().FindNearest(StrID("crawler"), glm::vec3(2.f, 0.f, 4.f), glm::vec3(1.f, 2.f, 1.f), pt));

        // with a default class, unknown names resolve to it
        Nav().SetDefaultClass("default");
        EXPECT_TRUE(Nav().FindNearest(StrID("crawler"), glm::vec3(2.f, 0.f, 4.f), glm::vec3(1.f, 2.f, 1.f), pt));
}

} // namespace

// ---------------------------------------------------------------------------
// Tier C — world-driven lifecycle through WorldNavBridge
// ---------------------------------------------------------------------------

namespace {

// trivially-copyable event counters (method-pointer listeners)
struct TileEventCounter {
        uint32_t nActivated   = 0;
        uint32_t nDeactivated = 0;
        void OnActivated(const Event &)   { ++nActivated; }
        void OnDeactivated(const Event &) { ++nDeactivated; }
};

// Attaches a small 2x2-tile analytic terrain grid + a matching heightfield nav
// bake whose group id equals the manifest path (the D3 matching invariant).
// Returns the nav bake output.
NavBake::SOutput SetupTerrainAndNavBake(const std::string & sId) {

        const std::string sManifest = "resource/terrain/gen/" + sId + ".seterrain";

        TerrainBake::SConfig oCfg = se_test::MakeTerrainConfig(2);
        oCfg.id = sId;
        TerrainBake::SOutput oTerrain = se_test::BakeTerrain(oCfg, se_test::TestHeight);
        EXPECT_FALSE(oTerrain.vManifest.empty());
        for (const auto & oTile : oTerrain.vTiles) {
                const std::string sTileName = sId + "_tile_"
                                + std::to_string(oTile.x) + "_" + std::to_string(oTile.y) + ".setile";
                EXPECT_TRUE(se_test::WriteBytes("resource/terrain/gen/" + sTileName, oTile.vBytes));
        }
        EXPECT_TRUE(se_test::WriteBytes(sManifest, oTerrain.vManifest));

        // nav heightfield over the same extent, sampled with the same height fn
        NavBake::SBakeInput in;
        NavBake::SGroup & g = in.vGroups.emplace_back();
        g.id = sManifest;
        g.bounds_min = glm::vec3(0.f, 1.f, 0.f);
        g.bounds_max = glm::vec3(16.f, 8.f, 16.f);
        NavBake::SGeomHeightfield & hf = g.vHeightfields.emplace_back();
        hf.origin = glm::vec3(0.f, 0.f, 0.f);
        hf.tile_size = 16.f;
        hf.resolution = 9;
        hf.vHeights.reserve(81);
        for (uint32_t i = 0; i < 9; ++i) {
                for (uint32_t j = 0; j < 9; ++j) {
                        hf.vHeights.push_back(se_test::TestHeight(float(j) * 2.f, float(i) * 2.f));
                }
        }

        NavBake::SOutput out;
        EXPECT_TRUE(NavBake::Bake(in, se_nav_test::MakeConfig(4.f), out));
        return out;
}

TEST_F(NavRuntimeTest, WorldTeardownDrainsTerrainNav) {
        const std::string sId = "navrt_teardown";
        const std::string sManifest = "resource/terrain/gen/" + sId + ".seterrain";

        const NavBake::SOutput oNavOut = SetupTerrainAndNavBake(sId);
        ASSERT_EQ(oNavOut.vTiles.size(), 16u);

        // world: streaming on, terrain grid, nav declared inline (holder mesh)
        FlatBuffers::WorldT oWorld = *se_test::MakeWorld("navrt_teardown_world");
        se_test::SetStreaming(oWorld, true, 10.f, 16.f);
        oWorld.terrain_grid = std::make_unique<FlatBuffers::TerrainGridDefT>();
        oWorld.terrain_grid->manifest = sManifest;

        auto pNavCfg = std::make_unique<FlatBuffers::NavConfigT>();
        pNavCfg->default_class = "default";
        auto pEntry = std::make_unique<FlatBuffers::NavMeshEntryT>();
        pEntry->agent_class = "default";
        auto pHolder = std::make_unique<FlatBuffers::NavMeshHolderT>();
        pHolder->mesh = std::make_unique<FlatBuffers::NavMeshAssetT>(
                        MakeNavAssetT(oNavOut, "default", {""}));
        pHolder->name = sId + "_default";
        pEntry->mesh = std::move(pHolder);
        pNavCfg->meshes.push_back(std::move(pEntry));
        oWorld.nav = std::move(pNavCfg);

        const std::string sWorldPath = "resource/world/gen/" + sId + ".seworld";
        ASSERT_TRUE(se_test::WriteBytes(sWorldPath, se_test::BakeWorld(oWorld)));

        // count terrain tile events from outside (the user-requested check)
        TileEventCounter oCounter;
        auto & oEvents = GetSystem<EventManager>();
        ASSERT_TRUE((oEvents.AddListener<ETerrainTileActivated,
                        &TileEventCounter::OnActivated>(&oCounter)));
        ASSERT_TRUE((oEvents.AddListener<ETerrainTileDeactivated,
                        &TileEventCounter::OnDeactivated>(&oCounter)));

        ASSERT_TRUE(Worlds().LoadWorld(sWorldPath));

        // manifest/group identity: the world's manifest IS the nav group
        ASSERT_TRUE(Nav().HasGroup(StrID(sManifest)));
        EXPECT_EQ(GetResource(Worlds().GetWorld())->GetTerrainGridManifest(), sManifest);

        // page terrain in: activation events carry nav tiles with them
        auto pFocus = scene->Create("navrt_focus");
        pFocus->SetPos(glm::vec3(8.f, 6.f, 8.f));
        vFocusHandles.push_back(Worlds().RegisterFocus(pFocus, 0.f, 0.f));

        float h = 0.f;
        ASSERT_TRUE(PumpUntil([&] { return Terrain().GetActiveTileCount() == 4; }));
        ASSERT_TRUE(Terrain().TryGetHeight(glm::vec3(8.f, 0.f, 8.f), h));
        EXPECT_EQ(oCounter.nActivated, 4u);
        EXPECT_EQ(Nav().GetStats().tiles_live, 16u);   // 4x4 nav tiles over the full grid

        SNavPoint pt;
        EXPECT_TRUE(Nav().FindNearest(StrID("default"),
                                      glm::vec3(8.f, se_test::TestHeight(8.f, 8.f) + 0.5f, 8.f),
                                      glm::vec3(2.f, 3.f, 2.f), pt));
        EXPECT_NEAR(pt.pos.y, se_test::TestHeight(8.f, 8.f), 0.4f);

        // world teardown: every active terrain tile must post its deactivation,
        // and the nav tiles must be gone with them
        Worlds().UnloadWorld();
        ASSERT_TRUE(PumpUntil([&] { return Nav().GetStats().tiles_live == 0; }));
        EXPECT_EQ(oCounter.nDeactivated, 4u);
        EXPECT_EQ(oCounter.nActivated, 4u);
        EXPECT_FALSE(Nav().FindNearest(StrID("default"),
                                       glm::vec3(8.f, 4.f, 8.f), glm::vec3(2.f, 3.f, 2.f), pt));

        // bridge UnregisterAll already ran through EWorldUnloaded; idempotent
        Nav().UnregisterAll();
        EXPECT_EQ(Nav().ClassIndex(StrID("default")), -1);

        oEvents.RemoveListener<ETerrainTileActivated,   &TileEventCounter::OnActivated>(&oCounter);
        oEvents.RemoveListener<ETerrainTileDeactivated, &TileEventCounter::OnDeactivated>(&oCounter);

        GetSystem<TResourceManager>().Clear<NavMeshAsset>();
}

TEST_F(NavRuntimeTest, SubLevelTilesFollowLifecycle) {
        // two sub-levels over adjacent ground groups; streaming disabled so both
        // auto-activate at world load (the platformer path)
        const std::string sA = "resource/world/gen/navrt_a.sesc";
        const std::string sB = "resource/world/gen/navrt_b.sesc";

        NavBake::SBakeInput in;
        NavBake::SGroup & a = in.vGroups.emplace_back();
        a.id = sA;
        a.bounds_min = glm::vec3(0.f, -1.f, 0.f);
        a.bounds_max = glm::vec3(16.f, 3.f, 16.f);
        se_nav_test::AddQuad(a.vMeshes.emplace_back(), 0.f, 0.f, 20.f, 16.f, 0.f);
        NavBake::SGroup & b = in.vGroups.emplace_back();
        b.id = sB;
        b.bounds_min = glm::vec3(16.f, -1.f, 0.f);
        b.bounds_max = glm::vec3(32.f, 3.f, 16.f);
        se_nav_test::AddQuad(b.vMeshes.emplace_back(), 12.f, 0.f, 32.f, 16.f, 0.f);
        NavBake::SOutput oOut;
        ASSERT_TRUE(NavBake::Bake(in, se_nav_test::MakeConfig(8.f), oOut));

        ASSERT_TRUE(se_test::WriteBytes(sA, se_test::BakeScene(se_test::MakeNode("root_a"))));
        ASSERT_TRUE(se_test::WriteBytes(sB, se_test::BakeScene(se_test::MakeNode("root_b"))));

        FlatBuffers::WorldT oWorld = *se_test::MakeWorld("navrt_sublevels");
        se_test::SetStreaming(oWorld, false, 10.f, 16.f);
        se_test::AddSubLevel(oWorld, sA, FlatBuffers::Vec3(0.f, 0.f, 0.f), FlatBuffers::Vec3(16.f, 4.f, 16.f));
        se_test::AddSubLevel(oWorld, sB, FlatBuffers::Vec3(16.f, 0.f, 0.f), FlatBuffers::Vec3(32.f, 4.f, 16.f));

        // inline holder variant
        auto pNavCfg = std::make_unique<FlatBuffers::NavConfigT>();
        pNavCfg->default_class = "default";
        auto pEntry = std::make_unique<FlatBuffers::NavMeshEntryT>();
        pEntry->agent_class = "default";
        auto pHolder = std::make_unique<FlatBuffers::NavMeshHolderT>();
        pHolder->mesh = std::make_unique<FlatBuffers::NavMeshAssetT>(
                        MakeNavAssetT(oOut, "default", {""}));
        pHolder->name = "navrt_sublevels_default";
        pEntry->mesh = std::move(pHolder);
        pNavCfg->meshes.push_back(std::move(pEntry));
        oWorld.nav = std::move(pNavCfg);

        const std::string sWorldPath = "resource/world/gen/navrt_sublevels.seworld";
        ASSERT_TRUE(se_test::WriteBytes(sWorldPath, se_test::BakeWorld(oWorld)));

        ASSERT_TRUE(Worlds().LoadWorld(sWorldPath));
        Drain();

        // both groups auto-activated with their sub-levels (4 tiles each)
        EXPECT_EQ(Nav().GetStats().tiles_live, 8u);
        EXPECT_TRUE(Nav().HasGroup(StrID(sA)));
        EXPECT_TRUE(Nav().HasGroup(StrID(sB)));

        SNavPoint pt;
        EXPECT_TRUE(Nav().FindNearest(StrID("default"), glm::vec3(8.f, 0.f, 8.f), glm::vec3(1.f, 2.f, 1.f), pt));
        EXPECT_TRUE(Nav().FindNearest(StrID("default"), glm::vec3(24.f, 0.f, 8.f), glm::vec3(1.f, 2.f, 1.f), pt));

        // deactivating level 0 removes exactly its group's tiles
        ASSERT_TRUE(Worlds().DeactivateLevel(0));
        ASSERT_TRUE(PumpUntil([&] { return Worlds().GetLevelState(0) == WorldSystem::ELevelState::Unloaded; }));
        EXPECT_EQ(Nav().GetStats().tiles_live, 4u);
        EXPECT_FALSE(Nav().FindNearest(StrID("default"), glm::vec3(8.f, 0.f, 8.f), glm::vec3(1.f, 2.f, 1.f), pt));
        EXPECT_TRUE(Nav().FindNearest(StrID("default"), glm::vec3(24.f, 0.f, 8.f), glm::vec3(1.f, 2.f, 1.f), pt));

        // re-activation restores
        ASSERT_TRUE(Worlds().ActivateLevel(0));
        EXPECT_EQ(Nav().GetStats().tiles_live, 8u);
        EXPECT_TRUE(Nav().FindNearest(StrID("default"), glm::vec3(8.f, 0.f, 8.f), glm::vec3(1.f, 2.f, 1.f), pt));
}

TEST_F(NavRuntimeTest, NavAgentComponentLifecycle) {
        RegisterBake(BakeStairs(), "default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        auto pNode = scene->Create("agent_node");
        pNode->CreateComponent<NavAgent>(NavAgentDesc{"default", 0.5f, 2.f});
        auto * pAgent = pNode->GetComponent<NavAgent>();
        ASSERT_NE(pAgent, nullptr);

        // CreateComponent -> Enable -> registered
        std::vector<NavigationSystem::SAgentDebug> vAgents;
        Nav().GetDebugAgents(vAgents);
        ASSERT_EQ(vAgents.size(), 1u);
        EXPECT_EQ(vAgents[0].sName, "agent_node");

        // explicit Disable/Enable cycle keeps the registry consistent
        pAgent->Disable();
        Nav().GetDebugAgents(vAgents);
        EXPECT_EQ(vAgents.size(), 0u);
        pAgent->Enable();
        Nav().GetDebugAgents(vAgents);
        EXPECT_EQ(vAgents.size(), 1u);

        // deferred destroy deregisters (no stale agent, no dangling corridor).
        // The local strong reference must go first, or Unlink leaves the node
        // alive and the component dtor never runs.
        TSceneTree::TSceneNodeWeak hNode = pNode;
        pNode.reset();
        GetSystem<EntityManager>().Destroy(hNode);
        Drain();
        EXPECT_TRUE(hNode.expired());
        Nav().GetDebugAgents(vAgents);
        EXPECT_EQ(vAgents.size(), 0u);
}

TEST_F(NavRuntimeTest, SpawnerNavSnap) {
        NavBake::SBakeInput in;
        NavBake::SGroup & g = in.vGroups.emplace_back();
        g.id = "ground";
        g.bounds_min = glm::vec3(-1.f, -1.f, -1.f);
        g.bounds_max = glm::vec3(17.f, 3.f, 17.f);
        se_nav_test::AddQuad(g.vMeshes.emplace_back(), 0.f, 0.f, 16.f, 16.f, 0.f);
        NavBake::SOutput oOut;
        ASSERT_TRUE(NavBake::Bake(in, se_nav_test::MakeConfig(8.f), oOut));
        RegisterBake(oOut, "default");
        Nav().SetDefaultClass("default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("ground")));

        // camp hovers 40 m above the ground; snap=NavMesh must place the wolf on it
        auto pCamp = scene->Create("Camp");
        pCamp->SetPos(glm::vec3(8.f, 40.f, 8.f));
        PopulationDesc oDesc;
        oDesc.id = "wolves";
        oDesc.tick_interval_s = 0.05f;
        PopulationMemberDesc m;
        m.template_id = "npc_wolf";
        m.count       = 1;
        oDesc.members.push_back(m);
        oDesc.snap = FlatBuffers::SpawnSnap::NavMesh;
        pCamp->CreateComponent<Population>(oDesc);

        ASSERT_TRUE(PumpUntil([&] { return pCamp->FindChild(StrID("npc_wolf")) != nullptr; }));
        const glm::vec3 vWolfPos = pCamp->FindChild(StrID("npc_wolf"))->GetTransform().GetWorldPos();
        EXPECT_NEAR(vWolfPos.y, 0.f, 0.4f);   // snapped onto the mesh
        EXPECT_LT(vWolfPos.y, 39.f);

        // without a probe the spawn falls back to the sampled position
        Spawner().SetNavProbe({});
        auto pCamp2 = scene->Create("Camp2");
        pCamp2->SetPos(glm::vec3(8.f, 3.f, 8.f));
        PopulationDesc oDesc2 = oDesc;
        oDesc2.id = "wolves2";
        pCamp2->CreateComponent<Population>(oDesc2);
        ASSERT_TRUE(PumpUntil([&] { return pCamp2->FindChild(StrID("wolves2_wolf")) != nullptr
                                        || pCamp2->FindChild(StrID("npc_wolf")) != nullptr; }));
        const auto pWolf2 = pCamp2->FindChild(StrID("npc_wolf"));
        ASSERT_NE(pWolf2, nullptr);
        EXPECT_NEAR(pWolf2->GetTransform().GetWorldPos().y, 3.f, 0.01f);
}

TEST_F(NavRuntimeTest, WorldNavHolderRoundTrip) {
        // path variant + inline variant + default_class, in one world
        const NavBake::SOutput oOut = BakeStairs();

        const std::string sSenm = "resource/nav/gen/navrt_class.senm";
        const std::vector<uint8_t> vBytes = BakeNavBytes(oOut, "default", {""});
        ASSERT_TRUE(se_test::WriteBytes(sSenm, vBytes));

        FlatBuffers::WorldT oWorld = *se_test::MakeWorld("navrt_holders");
        se_test::SetStreaming(oWorld, false, 10.f, 16.f);

        auto pNavCfg = std::make_unique<FlatBuffers::NavConfigT>();
        pNavCfg->default_class = "big";

        auto pPathEntry = std::make_unique<FlatBuffers::NavMeshEntryT>();
        pPathEntry->agent_class = "default";
        auto pPathHolder = std::make_unique<FlatBuffers::NavMeshHolderT>();
        pPathHolder->path = sSenm;
        pPathEntry->mesh = std::move(pPathHolder);
        pNavCfg->meshes.push_back(std::move(pPathEntry));

        auto pInlineEntry = std::make_unique<FlatBuffers::NavMeshEntryT>();
        pInlineEntry->agent_class = "big";
        auto pInlineHolder = std::make_unique<FlatBuffers::NavMeshHolderT>();
        pInlineHolder->mesh = std::make_unique<FlatBuffers::NavMeshAssetT>();
        {
                // same geometry under a different class name (inline flavor)
                const std::vector<uint8_t> vBig = BakeNavBytes(oOut, "big", {""});
                flatbuffers::Verifier oVerifier(vBig.data(), vBig.size());
                ASSERT_TRUE(FlatBuffers::VerifyNavMeshAssetBuffer(oVerifier));
                FlatBuffers::GetNavMeshAsset(vBig.data())->UnPackTo(pInlineHolder->mesh.get());
        }
        pInlineHolder->name = "navrt_big_inline";
        pInlineEntry->mesh = std::move(pInlineHolder);
        pNavCfg->meshes.push_back(std::move(pInlineEntry));
        oWorld.nav = std::move(pNavCfg);

        const std::string sWorldPath = "resource/world/gen/navrt_holders.seworld";
        ASSERT_TRUE(se_test::WriteBytes(sWorldPath, se_test::BakeWorld(oWorld)));

        ASSERT_TRUE(Worlds().LoadWorld(sWorldPath));
        Drain();

        EXPECT_NE(Nav().ClassIndex(StrID("default")), -1);   // from the .senm file
        EXPECT_NE(Nav().ClassIndex(StrID("big")), -1);       // from the inline holder
        EXPECT_EQ(Nav().DefaultClass(), "big");

        // the default class covers unknown names; queries work on both classes
        // (tiles arrive with the group activation)
        EXPECT_TRUE(Nav().ActivateGroup(StrID("ground")));
        SNavPoint pt;
        EXPECT_TRUE(Nav().FindNearest(StrID("whatever"), glm::vec3(2.f, 0.f, 2.f), glm::vec3(1.f, 2.f, 1.f), pt));
        EXPECT_TRUE(Nav().FindNearest(StrID("default"), glm::vec3(2.f, 0.f, 2.f), glm::vec3(1.f, 2.f, 1.f), pt));
        SNavPath path;
        EXPECT_TRUE(Nav().FindPath(StrID("big"), glm::vec3(2.f, 0.f, 2.f), glm::vec3(14.f, 1.f, 8.f), path));
}

// ---------------------------------------------------------------------------
// Group-seam connectivity — the world_demo failure: terrain partitioned
// between bake groups never linked because each tile build saw only its own
// group's geometry, so the border band at every group seam was empty and
// erosion pulled the portal edges back from the tile boundary.
// ---------------------------------------------------------------------------

TEST_F(NavRuntimeTest, PartitionedTerrain_GroupSeamLinks) {
        // mirrors the world-mode bake shape: ground heightfields partitioned
        // between two normal groups by tile-center, no overlap across the seam
        // at x = 8. Portal edges need walkable data past the tile boundary on
        // both sides, so each tile build must sample the partition neighbor's
        // ground inside its border band — otherwise the groups never link and
        // agents stay confined to the group whose tiles they spawned on.
        NavBake::SBakeInput in;
        NavBake::SGroup & west = in.vGroups.emplace_back();
        west.id = "wd_west";
        west.bounds_min = glm::vec3(0.f, -1.f, 0.f);
        west.bounds_max = glm::vec3(8.f, 3.f, 8.f);
        NavBake::SGeomHeightfield & hw = west.vHeightfields.emplace_back();
        hw.origin = glm::vec3(0.f, 0.f, 0.f);
        hw.tile_size = 8.f;
        hw.resolution = 5;
        hw.vHeights.assign(25, 0.f);
        NavBake::SGroup & east = in.vGroups.emplace_back();
        east.id = "wd_east";
        east.bounds_min = glm::vec3(8.f, -1.f, 0.f);
        east.bounds_max = glm::vec3(16.f, 3.f, 8.f);
        NavBake::SGeomHeightfield & he = east.vHeightfields.emplace_back();
        he.origin = glm::vec3(8.f, 0.f, 0.f);
        he.tile_size = 8.f;
        he.resolution = 5;
        he.vHeights.assign(25, 0.f);

        NavBake::SOutput out;
        ASSERT_TRUE(NavBake::Bake(in, se_nav_test::MakeConfig(8.f), out));
        ASSERT_EQ(out.vTiles.size(), 2u);   // 2x1 grid, one column per group

        RegisterBake(out, "default");
        ASSERT_TRUE(Nav().ActivateGroup(StrID("wd_west")));
        ASSERT_TRUE(Nav().ActivateGroup(StrID("wd_east")));

        SNavPath path;
        EXPECT_TRUE(Nav().FindPath(StrID("default"),
                                   glm::vec3(2.f, 0.f, 4.f), glm::vec3(14.f, 0.f, 4.f), path));
        EXPECT_FALSE(path.partial);
}

TEST_F(NavRuntimeTest, SenmSeamLinks_DemoAsset) {
        // audits the shipped demo navmesh: every grid seam (grid-adjacent tile
        // pair, same layer) where both sides have a polygon vertex on the shared
        // boundary plane must carry at least one Detour link between the tiles.
        // A missing link there means the bake fragmented walkable space into
        // per-group components — agents cannot path across it.
        const std::string sPath = "resource/nav/world_demo_default.senm";
        std::ifstream f(sPath, std::ios::binary);
        if (!f) { GTEST_SKIP() << sPath << " not baked"; }
        std::vector<uint8_t> vBytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        flatbuffers::Verifier v(vBytes.data(), vBytes.size());
        ASSERT_TRUE(FlatBuffers::VerifyNavMeshAssetBuffer(v));
        const FlatBuffers::NavMeshAsset * pAsset = FlatBuffers::GetNavMeshAsset(vBytes.data());
        ASSERT_NE(pAsset->tiles(), nullptr);
        ASSERT_GT(pAsset->tiles()->size(), 0u);
        ASSERT_NE(pAsset->origin(), nullptr);
        ASSERT_GT(pAsset->tile_dim(), 0.f);

        uint32_t max_tile_polys = 0;
        for (const FlatBuffers::NavTile * pTile : *pAsset->tiles()) {
                if (pTile == nullptr || pTile->data() == nullptr ||
                    pTile->data()->size() < sizeof(dtMeshHeader)) { continue; }
                const dtMeshHeader * h = reinterpret_cast<const dtMeshHeader *>(pTile->data()->data());
                max_tile_polys = glm::max<uint32_t>(max_tile_polys, static_cast<uint32_t>(h->polyCount));
        }
        ASSERT_GT(max_tile_polys, 0u);

        auto Pow2Ceil = [](uint32_t x) {
                uint32_t p = 1;
                while (p < x) { p <<= 1; }
                return p;
        };
        dtNavMeshParams params;
        memset(&params, 0, sizeof(params));
        params.orig[0] = pAsset->origin()->x();
        params.orig[1] = pAsset->origin()->y();
        params.orig[2] = pAsset->origin()->z();
        params.tileWidth = pAsset->tile_dim();
        params.tileHeight = pAsset->tile_dim();
        params.maxTiles = Pow2Ceil(static_cast<uint32_t>(pAsset->tiles()->size()));
        params.maxPolys = Pow2Ceil(max_tile_polys);
        std::unique_ptr<dtNavMesh> pMesh(dtAllocNavMesh());
        ASSERT_EQ(pMesh->init(&params), DT_SUCCESS);

        // (x, y, layer) -> live tile + authoring group (for failure messages)
        struct SCell { const dtMeshTile * pTile; std::string sGroup; };
        std::map<std::tuple<int, int, int>, SCell> mCells;
        for (const FlatBuffers::NavTile * pTile : *pAsset->tiles()) {
                if (pTile == nullptr || pTile->data() == nullptr) { continue; }
                unsigned char * pCopy = reinterpret_cast<unsigned char *>(
                                dtAlloc(pTile->data()->size(), DT_ALLOC_PERM));
                ASSERT_NE(pCopy, nullptr);
                memcpy(pCopy, pTile->data()->data(), pTile->data()->size());
                dtTileRef ref = 0;
                ASSERT_EQ(pMesh->addTile(pCopy, static_cast<int>(pTile->data()->size()),
                                         DT_TILE_FREE_DATA, 0, &ref), DT_SUCCESS)
                                << "tile (" << pTile->x() << ", " << pTile->y() << ") layer "
                                << pTile->layer() << " rejected";
                const dtMeshHeader * h = reinterpret_cast<const dtMeshHeader *>(pCopy);
                mCells[{h->x, h->y, h->layer}] =
                                SCell{pMesh->getTileByRef(ref),
                                      pTile->group() ? pTile->group()->str() : "?"};
        }

        // closest approach of a tile's walkable polys to the boundary plane, and
        // whether anything lies exactly on it (a portal-edge candidate)
        auto PlaneApproach = [](const dtMeshTile * pT, const int axis, const float plane, float & out_dist) {
                out_dist = FLT_MAX;
                for (int p = 0; p < pT->header->polyCount; ++p) {
                        const dtPoly & poly = pT->polys[p];
                        if (poly.getType() == DT_POLYTYPE_OFFMESH_CONNECTION) { continue; }
                        for (int j = 0; j < poly.vertCount; ++j) {
                                out_dist = glm::min(out_dist,
                                                std::fabs(pT->verts[poly.verts[j] * 3 + axis] - plane));
                        }
                }
        };
        auto HasCrossLink = [&pMesh](const dtMeshTile * pA, const dtMeshTile * pB) {
                auto Linked = [&pMesh](const dtMeshTile * pFrom, const dtMeshTile * pTo) {
                        for (int p = 0; p < pFrom->header->polyCount; ++p) {
                                for (unsigned int li = pFrom->polys[p].firstLink;
                                     li != DT_NULL_LINK; li = pFrom->links[li].next) {
                                        const dtMeshTile * pLTile = nullptr;
                                        const dtPoly * pLPoly = nullptr;
                                        if (dtStatusSucceed(pMesh->getTileAndPolyByRef(pFrom->links[li].ref,
                                                                                       &pLTile, &pLPoly)) &&
                                            pLTile == pTo) { return true; }
                                }
                        }
                        return false;
                };
                return Linked(pA, pB) || Linked(pB, pA);
        };

        // Erosion pulls walkable polys walkableRadius (2 voxels = 0.4 m) back
        // from the edge of each tile build's input data. When that data stops at
        // a group seam, both sides of the seam end short of the boundary plane
        // and no portal edge ever forms — walkable space fragments per bake
        // group. Polys within ~2 walkableRadii of the plane on both sides is the
        // gap signature; nothing within 1.5 m is a legitimate hard edge (cliff /
        // hole / world rim).
        constexpr float kOnPlane = 0.05f;
        constexpr float kGapMax  = 0.9f;
        constexpr float kHardMax = 1.5f;
        uint32_t seams_linked = 0, seams_gap = 0, seams_hard = 0;
        for (const auto & entry : mCells) {
                const int tx = std::get<0>(entry.first);
                const int ty = std::get<1>(entry.first);
                for (const auto & dir : {glm::ivec2(1, 0), glm::ivec2(0, 1)}) {
                        const auto itN = mCells.find({tx + dir.x, ty + dir.y, std::get<2>(entry.first)});
                        if (itN == mCells.end()) { continue; }
                        const dtMeshTile * pA = entry.second.pTile;
                        const dtMeshTile * pB = itN->second.pTile;
                        const int axis = (dir.x != 0) ? 0 : 2;
                        const float plane = (axis == 0)
                                        ? params.orig[0] + static_cast<float>(tx + 1) * params.tileWidth
                                        : params.orig[2] + static_cast<float>(ty + 1) * params.tileHeight;
                        float dA = 0.f, dB = 0.f;
                        PlaneApproach(pA, axis, plane, dA);
                        PlaneApproach(pB, axis, plane, dB);

                        if (dA <= kOnPlane && dB <= kOnPlane) {
                                ++seams_linked;
                                EXPECT_TRUE(HasCrossLink(pA, pB))
                                                << "unlinked grid seam at " << (axis == 0 ? "x" : "z") << " = " << plane
                                                << ": tile (" << tx << ", " << ty << ") [" << entry.second.sGroup
                                                << "] vs tile (" << tx + dir.x << ", " << ty + dir.y << ") ["
                                                << itN->second.sGroup << "]";
                        } else if (glm::max(dA, dB) < kGapMax) {
                                ++seams_gap;
                                ADD_FAILURE() << "erosion-gap grid seam at "
                                              << (axis == 0 ? "x" : "z") << " = " << plane
                                              << ": tiles [" << entry.second.sGroup << "] and ["
                                              << itN->second.sGroup << "] approach to "
                                              << dA << "m / " << dB << "m but neither reaches the"
                                              << " boundary — border band was empty at bake time,"
                                              << " no portal edge, pathing fragments per group";
                        } else if (glm::max(dA, dB) < kHardMax) {
                                ++seams_hard;
                                log_w("SenmSeamLinks_DemoAsset: hard seam at {} = {:.1f} ({} | {} m) between [{}] and [{}]",
                                      (axis == 0 ? "x" : "z"), plane, dA, dB,
                                      entry.second.sGroup, itN->second.sGroup);
                        }
                        // beyond kHardMax: world rim / fully unwalkable boundary — fine
                }
        }
        log_i("SenmSeamLinks_DemoAsset: {} tiles, seams linked={} gap={} hard={}",
              mCells.size(), seams_linked, seams_gap, seams_hard);
        EXPECT_GT(seams_linked, 0u);
}

} // namespace
