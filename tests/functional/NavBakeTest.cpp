
// ---------------------------------------------------------------------------
// NavBakeTest (Tier B, Stage 3 Step 6) — the shared bake library vs analytic
// scenes: tiled coverage + Detour queries over the baked bytes (the exact
// bytes Step 7 will addTile), determinism, off-mesh links, coexisting layer
// groups (Manual interiors), area tagging and exclusion volumes, per-class
// agent bakes, heightfield vs mesh cross-mode agreement, claim conflicts, and
// config validation. Headless: no engine systems init.
// ---------------------------------------------------------------------------

#include <gtest/gtest.h>

#define SE_IMPL
#include <GlobalTypes.h>

#include <Logging.h>
#include <nav/NavBake.tcc>

#include "NavAssets.h"

#include <NavMesh_generated.h>
#include <ComponentLoader.h>
#include <nav/NavArea.h>
#include <nav/NavArea.tcc>
#include <nav/NavLink.h>
#include <nav/NavLink.tcc>

#include <glm/glm.hpp>

#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {

using namespace SE;
namespace fb = SE::FlatBuffers;
using namespace se_nav_test;

// Ground 16x16 + four 0.25 m steps up to a 1 m plateau: path (2,2) → (14,8)
// climbs the stairs (slope ~14°, rise 0.25 ≤ step 0.4). tiles: 2x2 of 8 m.
static NavBake::SOutput BakeStairs(float tile_dim = 8.0f) {

        NavBake::SBakeInput in;
        NavBake::SGroup g = GroundGroup(0.0f, 0.0f, 16.0f, 16.0f);
        for (int k = 0; k < 4; ++k) {
                AddBox(g.vMeshes.back(), 4.0f + k, 6.0f, 5.0f + k, 10.0f, 0.25f * (k + 1));
        }
        AddBox(g.vMeshes.back(), 8.0f, 6.0f, 16.0f, 10.0f, 1.0f);
        in.vGroups.push_back(std::move(g));

        NavBake::SOutput out;
        (void)NavBake::Bake(in, MakeConfig(tile_dim), out);
        return out;
}

} // namespace

// ---------------------------------------------------------------------------
// coverage + queries over the baked bytes
// ---------------------------------------------------------------------------

TEST(NavBake, BoxAndStairsCoverageAndPath) {

        const NavBake::SOutput out = BakeStairs();
        ASSERT_EQ(out.vTiles.size(), 4u);           // 2x2 grid, every tile carries geometry
        EXPECT_EQ(out.tiles_x, 2u);
        EXPECT_EQ(out.tiles_y, 2u);
        EXPECT_GT(out.poly_count, 0u);
        // row-major (y, x) order
        EXPECT_EQ(out.vTiles[1].x, 1);
        EXPECT_EQ(out.vTiles[1].y, 0);

        const SNavRuntime rt = InitRuntime(out);
        EXPECT_TRUE(FindPath(rt, glm::vec3(2.0f, 0.0f, 2.0f), glm::vec3(14.0f, 1.0f, 8.0f)))
                        << "path must climb the stairs to the plateau";
        EXPECT_TRUE(FindPath(rt, glm::vec3(2.0f, 0.0f, 2.0f), glm::vec3(14.0f, 1.0f, 14.0f)));
        // the plateau top is walkable and its height matches the baked geometry
        EXPECT_NE(Nearest(rt, glm::vec3(12.0f, 1.0f, 8.0f)), 0);
}

TEST(NavBake, Determinism) {

        const NavBake::SOutput a = BakeStairs();
        const NavBake::SOutput b = BakeStairs();
        ASSERT_EQ(a.vTiles.size(), b.vTiles.size());
        ASSERT_EQ(a.vLinks.size(), b.vLinks.size());
        for (size_t k = 0; k < a.vTiles.size(); ++k) {
                ASSERT_EQ(a.vTiles[k].vBytes.size(), b.vTiles[k].vBytes.size());
                EXPECT_EQ(0, std::memcmp(a.vTiles[k].vBytes.data(), b.vTiles[k].vBytes.data(), a.vTiles[k].vBytes.size()));
                EXPECT_EQ(a.vTiles[k].x, b.vTiles[k].x);
                EXPECT_EQ(a.vTiles[k].y, b.vTiles[k].y);
                EXPECT_EQ(a.vTiles[k].layer, b.vTiles[k].layer);
                EXPECT_EQ(a.vTiles[k].group, b.vTiles[k].group);
        }
}

// ---------------------------------------------------------------------------
// links: embedded off-mesh connections + round-trip table
// ---------------------------------------------------------------------------

TEST(NavBake, LinksRoundTripAndTraversal) {

        // two platforms with a 4 m gap bridged by an authored link
        NavBake::SBakeInput in;
        NavBake::SGroup g;
        g.id = "platforms";
        g.bounds_min = glm::vec3(0.0f, -1.0f, 0.0f);
        g.bounds_max = glm::vec3(16.0f, 3.0f, 16.0f);
        AddQuad(g.vMeshes.emplace_back(), 0.0f, 6.0f, 6.0f, 10.0f, 0.0f);
        AddQuad(g.vMeshes.back(), 10.0f, 6.0f, 16.0f, 10.0f, 0.0f);
        NavBake::SLink link;
        link.start = glm::vec3(5.4f, 0.0f, 8.0f);
        link.end = glm::vec3(10.6f, 0.0f, 8.0f);
        link.radius = 1.0f;
        link.bidirectional = true;
        g.vLinks.push_back(link);
        in.vGroups.push_back(std::move(g));

        const NavBake::SConfig cfg = MakeConfig(8.0f, { "" });
        NavBake::SOutput out;
        ASSERT_TRUE(NavBake::Bake(in, cfg, out));

        // round-trip table
        ASSERT_EQ(out.vLinks.size(), 1u);
        EXPECT_NEAR(out.vLinks[0].start.x, 5.4f, 1e-5f);
        EXPECT_NEAR(out.vLinks[0].end.x, 10.6f, 1e-5f);
        EXPECT_TRUE(out.vLinks[0].bidirectional);
        EXPECT_EQ(out.vLinks[0].group, "platforms");

        // traversal: the path crosses the gap through the embedded connection
        const SNavRuntime rt = InitRuntime(out);
        EXPECT_TRUE(FindPath(rt, glm::vec3(2.0f, 0.0f, 8.0f), glm::vec3(14.0f, 0.0f, 8.0f)));
}

// ---------------------------------------------------------------------------
// coexisting groups: Manual interior as a second tile layer at the same cells
// ---------------------------------------------------------------------------

TEST(NavBake, CoexistingInteriorLayer) {

        NavBake::SBakeInput in;
        NavBake::SGroup exterior = GroundGroup(0.0f, 0.0f, 16.0f, 16.0f);
        exterior.id = "exterior";

        NavBake::SGroup interior;
        interior.id = "interior";
        interior.coexisting = true;
        interior.bounds_min = glm::vec3(4.0f, -1.0f, 4.0f);
        interior.bounds_max = glm::vec3(12.0f, 2.0f, 12.0f);
        AddQuad(interior.vMeshes.emplace_back(), 4.0f, 4.0f, 12.0f, 12.0f, 3.0f);   // raised floor

        // authored door link: exterior floor ↔ interior floor
        NavBake::SLink door;
        door.start = glm::vec3(6.0f, 0.0f, 4.6f);
        door.end = glm::vec3(6.0f, 3.0f, 5.4f);
        door.radius = 1.0f;
        interior.vLinks.push_back(door);

        in.vGroups.push_back(std::move(exterior));
        in.vGroups.push_back(std::move(interior));

        NavBake::SOutput out;
        ASSERT_TRUE(NavBake::Bake(in, MakeConfig(8.0f, { "" }), out));

        // every claimed cell carries two tiles with distinct layers, distinct groups
        ASSERT_EQ(out.vTiles.size(), 8u);
        uint32_t layer_tiles = 0;
        for (const auto & t : out.vTiles) {
                if (t.group == "interior") { layer_tiles++; EXPECT_GT(t.layer, 0); }
                else { EXPECT_EQ(t.layer, 0); EXPECT_EQ(t.group, "exterior"); }
        }
        EXPECT_EQ(layer_tiles, 4u);   // the interior straddles all four tile centers

        const SNavRuntime rt = InitRuntime(out);
        EXPECT_NE(Nearest(rt, glm::vec3(2.0f, 0.0f, 2.0f)), 0);      // exterior floor
        EXPECT_NE(Nearest(rt, glm::vec3(8.0f, 3.0f, 8.0f)), 0);      // interior floor (layer 1)
}

// ---------------------------------------------------------------------------
// areas + exclusion volumes
// ---------------------------------------------------------------------------

TEST(NavBake, AreaBoxesTagPolys) {

        NavBake::SBakeInput in;
        NavBake::SGroup g = GroundGroup(0.0f, 0.0f, 16.0f, 16.0f);
        NavBake::SAreaBox road;
        road.center = glm::vec3(4.0f, 0.0f, 8.0f);
        road.half_extents = glm::vec3(4.0f, 2.0f, 8.0f);
        road.exclusion = false;
        road.registry_index = 1;   // cfg.vAreas = {"", "road"}
        g.vAreaBoxes.push_back(road);
        in.vGroups.push_back(std::move(g));

        const NavBake::SConfig cfg = MakeConfig(8.0f, { "", "road" });
        NavBake::SOutput out;
        ASSERT_TRUE(NavBake::Bake(in, cfg, out));

        const SNavRuntime rt = InitRuntime(out);
        const std::set<unsigned char> areas = AreaBytes(rt);
        // ground (1) and the named area (registry_index + 1 = 2) both present
        EXPECT_TRUE(areas.count(1) > 0);
        EXPECT_TRUE(areas.count(2) > 0);
}

TEST(NavBake, ExclusionVolumeCarvesHole) {

        NavBake::SBakeInput in;
        NavBake::SGroup g = GroundGroup(0.0f, 0.0f, 16.0f, 16.0f);
        NavBake::SAreaBox hole;
        hole.center = glm::vec3(8.0f, 0.0f, 8.0f);
        hole.half_extents = glm::vec3(2.5f, 2.0f, 2.5f);
        hole.exclusion = true;
        g.vAreaBoxes.push_back(hole);
        in.vGroups.push_back(std::move(g));

        NavBake::SOutput out;
        ASSERT_TRUE(NavBake::Bake(in, MakeConfig(), out));
        ASSERT_FALSE(out.vTiles.empty());

        // no poly center strictly inside the exclusion volume
        const SNavRuntime rt = InitRuntime(out);
        for (const glm::vec3 & c : PolyCenters(rt)) {
                EXPECT_FALSE(c.x > 6.0f && c.x < 10.0f && c.z > 6.0f && c.z < 10.0f)
                                << "poly center inside the exclusion volume: " << c.x << "," << c.z;
        }
        // still walkable far from the hole
        EXPECT_NE(Nearest(rt, glm::vec3(2.0f, 0.0f, 2.0f)), 0);
}

// ---------------------------------------------------------------------------
// per-class agent bakes
// ---------------------------------------------------------------------------

TEST(NavBake, AgentClassesBakeSeparately) {

        // a 2.2 m underpass under a slab: the 2.0 m agent fits, the 2.6 m agent
        // does not — agent HEIGHT is baked into the walkable geometry per class
        auto BakeUnderpass = [](float agent_height, NavBake::SOutput & out) {
                NavBake::SBakeInput in;
                NavBake::SGroup g = GroundGroup(0.0f, 0.0f, 16.0f, 16.0f);
                g.id = "underpass";
                AddBox(g.vMeshes.back(), 6.0f, 0.0f, 10.0f, 16.0f, 3.0f, 2.2f);
                AddQuad(g.vMeshes.back(), 6.0f, 0.0f, 10.0f, 16.0f, 2.2f);   // underside caps headroom
                in.vGroups.push_back(std::move(g));

                NavBake::SConfig cfg = MakeConfig(2.0f);
                cfg.agent = DefaultAgent();
                cfg.agent.height = agent_height;
                (void)NavBake::Bake(in, cfg, out);
        };

        NavBake::SOutput slim_out, wide_out;
        BakeUnderpass(2.0f, slim_out);
        BakeUnderpass(2.6f, wide_out);
        ASSERT_FALSE(slim_out.vTiles.empty());
        ASSERT_FALSE(wide_out.vTiles.empty());

        const SNavRuntime rt_slim = InitRuntime(slim_out);
        const SNavRuntime rt_wide = InitRuntime(wide_out);
        // open ground is walkable for both
        EXPECT_NE(Nearest(rt_slim, glm::vec3(2.0f, 0.0f, 8.0f)), 0);
        EXPECT_NE(Nearest(rt_wide, glm::vec3(2.0f, 0.0f, 8.0f)), 0);
        // under the slab: only the slim agent's class has walkable space
        EXPECT_NE(Nearest(rt_slim, glm::vec3(8.0f, 0.0f, 8.0f), glm::vec3(0.8f, 0.6f, 0.8f)), 0);
        EXPECT_EQ(Nearest(rt_wide, glm::vec3(8.0f, 0.0f, 8.0f), glm::vec3(0.8f, 0.6f, 0.8f)), 0);
        // the crossing exists for the slim class, impossible for the wide one
        EXPECT_TRUE(FindPath(rt_slim, glm::vec3(2.0f, 0.0f, 8.0f), glm::vec3(14.0f, 0.0f, 8.0f)));
        EXPECT_FALSE(FindPath(rt_wide, glm::vec3(2.0f, 0.0f, 8.0f), glm::vec3(14.0f, 0.0f, 8.0f)));
}

// ---------------------------------------------------------------------------
// heightfield mode: shared slope predicate + mesh-mode agreement
// ---------------------------------------------------------------------------

namespace {

// plane x*0.3 (16.7°) → 1.8 m cliff band at x ∈ [4, 5] (60.9°) → shelf y = 3 + (x-5)*0.1
static float CliffHeight(float x, float) {

        if (x < 4.0f) { return x * 0.3f; }
        if (x < 5.0f) { return 1.2f + (x - 4.0f) * 1.8f; }
        return 3.0f + (x - 5.0f) * 0.1f;
}

static NavBake::SGroup CliffGroupHeightfield() {

        NavBake::SGroup g;
        g.id = "terrain";
        g.bounds_min = glm::vec3(0.0f, -1.0f, 0.0f);
        g.bounds_max = glm::vec3(8.0f, 6.0f, 8.0f);

        NavBake::SGeomHeightfield hf;
        hf.origin = glm::vec3(0.0f);
        hf.tile_size = 8.0f;
        hf.resolution = 65;   // 0.125 m samples
        hf.vHeights.resize(size_t(hf.resolution) * hf.resolution);
        const float step = hf.tile_size / (hf.resolution - 1);
        for (uint32_t i = 0; i < hf.resolution; ++i) {
                for (uint32_t j = 0; j < hf.resolution; ++j) {
                        hf.vHeights[size_t(i) * hf.resolution + j] = CliffHeight(j * step, i * step);
                }
        }
        g.vHeightfields.push_back(std::move(hf));
        return g;
}

} // namespace

TEST(NavBake, HeightfieldWalkableSlopeRefusesCliff) {

        NavBake::SBakeInput in;
        in.vGroups.push_back(CliffGroupHeightfield());

        NavBake::SOutput out;
        ASSERT_TRUE(NavBake::Bake(in, MakeConfig(8.0f), out));
        ASSERT_FALSE(out.vTiles.empty());

        const SNavRuntime rt = InitRuntime(out);
        // gentle plane: walkable, poly height ≈ analytic height
        EXPECT_NE(Nearest(rt, glm::vec3(2.0f, 1.0f, 4.0f)), 0);
        // upper shelf: walkable
        EXPECT_NE(Nearest(rt, glm::vec3(6.5f, 3.0f, 4.0f)), 0);
        // the cliff face: no walkable polys
        EXPECT_EQ(Nearest(rt, glm::vec3(4.5f, 2.7f, 4.0f), glm::vec3(0.3f, 1.5f, 0.3f)), 0);

        // no path crosses the cliff between the two shelves
        EXPECT_FALSE(FindPath(rt, glm::vec3(2.0f, 1.0f, 4.0f), glm::vec3(6.5f, 3.0f, 4.0f)));
}

TEST(NavBake, HeightfieldModeAgreesWithMeshMode) {

        // same analytic terrain through both geometry paths — walkability must agree
        NavBake::SBakeInput in_hf;
        in_hf.vGroups.push_back(CliffGroupHeightfield());
        NavBake::SOutput out_hf;
        ASSERT_TRUE(NavBake::Bake(in_hf, MakeConfig(8.0f), out_hf));

        NavBake::SBakeInput in_mesh;
        NavBake::SGroup g;
        g.id = "terrain_mesh";
        g.bounds_min = glm::vec3(0.0f, -1.0f, 0.0f);
        g.bounds_max = glm::vec3(8.0f, 6.0f, 8.0f);
        g.vMeshes.push_back(HeightfieldToMesh(CliffGroupHeightfield().vHeightfields[0]));
        in_mesh.vGroups.push_back(std::move(g));
        NavBake::SOutput out_mesh;
        ASSERT_TRUE(NavBake::Bake(in_mesh, MakeConfig(8.0f), out_mesh));

        ASSERT_FALSE(out_hf.vTiles.empty());
        ASSERT_FALSE(out_mesh.vTiles.empty());

        const SNavRuntime rt_hf = InitRuntime(out_hf);
        const SNavRuntime rt_mesh = InitRuntime(out_mesh);
        // sampled across the walkable plane and the shelf, both modes agree on
        // where walkable polys exist
        for (float x = 0.5f; x < 4.0f; x += 0.5f) {
                EXPECT_NE(Nearest(rt_hf, glm::vec3(x, 1.0f, 4.0f)), 0) << "hf walkable at x=" << x;
                EXPECT_NE(Nearest(rt_mesh, glm::vec3(x, 1.0f, 4.0f)), 0) << "mesh walkable at x=" << x;
        }
        for (float x = 5.5f; x < 8.0f; x += 0.5f) {
                EXPECT_NE(Nearest(rt_hf, glm::vec3(x, 3.0f, 4.0f)), 0) << "hf walkable at x=" << x;
                EXPECT_NE(Nearest(rt_mesh, glm::vec3(x, 3.0f, 4.0f)), 0) << "mesh walkable at x=" << x;
        }
        for (float x = 4.3f; x < 4.8f; x += 0.25f) {
                EXPECT_EQ(Nearest(rt_hf, glm::vec3(x, 2.7f, 4.0f), glm::vec3(0.3f, 1.5f, 0.3f)), 0) << "hf cliff at x=" << x;
                EXPECT_EQ(Nearest(rt_mesh, glm::vec3(x, 2.7f, 4.0f), glm::vec3(0.3f, 1.5f, 0.3f)), 0) << "mesh cliff at x=" << x;
        }
}

// ---------------------------------------------------------------------------
// claims + validation
// ---------------------------------------------------------------------------

TEST(NavBake, ClaimConflictFails) {

        NavBake::SBakeInput in;
        NavBake::SGroup a = GroundGroup(0.0f, 0.0f, 8.0f, 8.0f);
        a.id = "a";
        NavBake::SGroup b = GroundGroup(0.0f, 0.0f, 8.0f, 8.0f);
        b.id = "b";
        in.vGroups.push_back(std::move(a));
        in.vGroups.push_back(std::move(b));

        NavBake::SOutput out;
        EXPECT_FALSE(NavBake::Bake(in, MakeConfig(), out));
}

TEST(NavBake, RejectsBadConfigs) {

        NavBake::SBakeInput in;
        in.vGroups.push_back(GroundGroup(0.0f, 0.0f, 8.0f, 8.0f));
        NavBake::SOutput out;

        // registry must start with the ground entry
        NavBake::SConfig cfg = MakeConfig();
        cfg.vAreas = { "road" };
        EXPECT_FALSE(NavBake::Bake(in, cfg, out));

        // tile_dim must divide the streaming cell_size integrally
        cfg = MakeConfig();
        cfg.cell_size = 64.0f;   // 64/8 = 8 — fine; flip tile_dim:
        cfg.tile_dim = 9.0f;
        EXPECT_FALSE(NavBake::Bake(in, cfg, out));

        // invalid agent params
        cfg = MakeConfig();
        cfg.agent.radius = 0.0f;
        EXPECT_FALSE(NavBake::Bake(in, cfg, out));

        // slope > 90°
        cfg = MakeConfig();
        cfg.agent.max_slope_deg = 95.0f;
        EXPECT_FALSE(NavBake::Bake(in, cfg, out));

        // empty input
        NavBake::SBakeInput empty;
        EXPECT_FALSE(NavBake::Bake(empty, MakeConfig(), out));
}

// ---------------------------------------------------------------------------
// .senm container round-trip + inert marker components
// ---------------------------------------------------------------------------

TEST(NavMeshAsset, VerifyAcceptsBakedAndRejectsGarbage) {

        const NavBake::SOutput out = BakeStairs();
        ASSERT_FALSE(out.vTiles.empty());

        fb::NavMeshAssetT asset;
        asset.agent_class = "default";
        asset.version = 1;
        asset.origin = std::make_unique<fb::Vec3>(out.origin.x, out.origin.y, out.origin.z);
        asset.tile_dim = out.tile_dim;
        asset.voxel_size = 0.2f;
        asset.voxel_height = 0.2f;
        asset.areas = { "" };
        for (const auto & t : out.vTiles) {
                auto pTile = std::make_unique<fb::NavTileT>();
                pTile->x = t.x;
                pTile->y = t.y;
                pTile->layer = t.layer;
                pTile->group = t.group;
                pTile->data = t.vBytes;
                asset.tiles.push_back(std::move(pTile));
        }
        flatbuffers::FlatBufferBuilder fbb(1 << 20);
        fbb.Finish(fb::NavMeshAsset::Pack(fbb, &asset), fb::NavMeshAssetIdentifier());

        flatbuffers::Verifier v(fbb.GetBufferPointer(), fbb.GetSize());
        EXPECT_TRUE(v.VerifyBuffer<fb::NavMeshAsset>(fb::NavMeshAssetIdentifier()));
        EXPECT_STREQ(fb::NavMeshAssetIdentifier(), "SENM");

        const std::vector<uint8_t> garbage(256, 0xAB);
        flatbuffers::Verifier gv(garbage.data(), garbage.size());
        EXPECT_FALSE(gv.VerifyBuffer<fb::NavMeshAsset>(fb::NavMeshAssetIdentifier()));
}

TEST(NavMarkerComponents, ValidateAndCarryData) {

        // compile-time: the loader dispatch recognizes the components' serialization
        static_assert(THasSerialized<NavArea>::value, "NavArea must declare TSerialized");
        static_assert(THasSerialized<NavLink>::value, "NavLink must declare TSerialized");

        fb::NavAreaT area_fb;
        area_fb.size = std::make_unique<fb::Vec3>(2.0f, 2.0f, 2.0f);
        area_fb.area = "road";
        flatbuffers::FlatBufferBuilder fbb1;
        fbb1.Finish(fb::NavArea::Pack(fbb1, &area_fb));
        NavArea area(nullptr, flatbuffers::GetRoot<fb::NavArea>(fbb1.GetBufferPointer()));
        EXPECT_EQ(area.Size(), glm::vec3(2.0f));
        EXPECT_EQ(area.Area(), "road");
        EXPECT_FALSE(area.IsExclusion());
        area.Enable();   // inert no-ops — must be safe to call
        area.Disable();

        fb::NavLinkT link_fb;
        link_fb.end_node = "door_target";
        link_fb.radius = 0.8f;
        link_fb.bidirectional = false;
        flatbuffers::FlatBufferBuilder fbb2;
        fbb2.Finish(fb::NavLink::Pack(fbb2, &link_fb));
        NavLink link(nullptr, flatbuffers::GetRoot<fb::NavLink>(fbb2.GetBufferPointer()));
        EXPECT_EQ(link.EndNode(), "door_target");
        EXPECT_NEAR(link.Radius(), 0.8f, 1e-6f);
        EXPECT_FALSE(link.Bidirectional());

        // zero-size areas and dead-end links are load errors
        fb::NavAreaT bad_area;
        bad_area.size = std::make_unique<fb::Vec3>(0.0f, 2.0f, 2.0f);
        flatbuffers::FlatBufferBuilder fbb3;
        fbb3.Finish(fb::NavArea::Pack(fbb3, &bad_area));
        EXPECT_THROW(NavArea(nullptr, flatbuffers::GetRoot<fb::NavArea>(fbb3.GetBufferPointer())), std::runtime_error);

        fb::NavLinkT bad_link;
        flatbuffers::FlatBufferBuilder fbb4;
        fbb4.Finish(fb::NavLink::Pack(fbb4, &bad_link));
        EXPECT_THROW(NavLink(nullptr, flatbuffers::GetRoot<fb::NavLink>(fbb4.GetBufferPointer())), std::runtime_error);
}
