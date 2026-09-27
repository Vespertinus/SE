
// ---------------------------------------------------------------------------
// SpatialTest — Tier C: the spatial index wired through the world_app
// composition (EntityManager, StateMachineSystem, WorldSystem,
// SpatialIndexSystem — in that order; the index system composes after the
// world system). Covers authored SpatialEntry activation/sweep, focus
// mirroring, node-listener tracking (no polling), and the spawner-condition
// query shape. Fixture scenes are baked inline (WorldAssets.h) into
// resource/world/gen/ — the StreamingTest convention.
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>
#include "EngineFixture.h"
#include "PhysicsHarness.h"
#include "WorldAssets.h"

#include <world/WorldSystem.h>
#include <world/WorldAsset.h>
#include <world/SpatialIndexSystem.h>
#include <world/SpatialEntry.h>
#include <EntityManager.h>
#include <hsm/StateMachine.h>

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>

#include <memory>
#include <set>
#include <string>
#include <vector>

using namespace SE;

namespace {

constexpr const char * sAlphaScenePath = "resource/world/gen/spatial_alpha.sesc";
constexpr const char * sBetaScenePath  = "resource/world/gen/spatial_beta.sesc";

std::vector<uint8_t> MakeAlphaScene() {

        auto oAlpha = se_test::MakeNode("Alpha");
        oAlpha.children.push_back(std::make_unique<FlatBuffers::NodeT>(
                se_test::MakeNodeWithSpatialEntry("poi_a", "poi", glm::vec3(2.f, 0.f, 2.f), 1.f, false)));
        oAlpha.children.push_back(std::make_unique<FlatBuffers::NodeT>(
                se_test::MakeNodeWithSpatialEntry("tracker", "mover", glm::vec3(5.f, 0.f, 5.f), 0.f, true)));
        return se_test::BakeScene(std::move(oAlpha));
}

std::vector<uint8_t> MakeBetaScene() {

        auto oBeta = se_test::MakeNode("Beta");
        oBeta.children.push_back(std::make_unique<FlatBuffers::NodeT>(
                se_test::MakeNodeWithSpatialEntry("poi_b", "poi", glm::vec3(1.f, 0.f, 1.f), 0.f, false)));
        return se_test::BakeScene(std::move(oBeta));
}

using THandleSet = std::set<uint64_t>;

} // namespace

// ---------------------------------------------------------------------------

class SpatialTest : public se_test::EnginePhysTestBase {

protected:
        void SetUp() override {
                EnginePhysTestBase::SetUp();

                // order matters: SpatialIndexSystem grabs WorldSystem in its ctor
                static bool systems_constructed = []() {
                        auto & oEngine = TEngine::Instance();
                        oEngine.Init<EntityManager>();
                        oEngine.Init<StateMachineSystem>();
                        oEngine.Init<WorldSystem>();
                        oEngine.Init<SpatialIndexSystem>();
                        return true;
                }();
                (void)systems_constructed;

                GetSystem<EntityManager>().SetSceneTree(scene.get());
                GetSystem<WorldSystem>().SetSceneTree(scene.get());

                boost::filesystem::create_directories("resource/world/gen");
                EXPECT_TRUE(se_test::WriteBytes(sAlphaScenePath, MakeAlphaScene()));
                EXPECT_TRUE(se_test::WriteBytes(sBetaScenePath, MakeBetaScene()));

                auto pWorldT = se_test::MakeWorld("spatial_world_test");
                se_test::AddSubLevel(*pWorldT, sAlphaScenePath,
                                     FlatBuffers::Vec3(0.f, 0.f, 0.f), FlatBuffers::Vec3(16.f, 8.f, 16.f));
                se_test::AddSubLevel(*pWorldT, sBetaScenePath,
                                     FlatBuffers::Vec3(32.f, 0.f, 0.f), FlatBuffers::Vec3(48.f, 8.f, 16.f));
                se_test::SetStreaming(*pWorldT, false, 40.f, 64.f);
                const auto vBytes = se_test::BakeWorld(*pWorldT);
                auto hWorld = CreateResource<WorldAsset>("spatial_world_test",
                                                         FlatBuffers::GetWorld(vBytes.data()));
                ASSERT_TRUE(Worlds().LoadWorld(hWorld));

                Drain();
        }

        void TearDown() override {
                auto & oWorldSystem = GetSystem<WorldSystem>();

                oWorldSystem.UnloadWorld();
                Drain();

                oWorldSystem.SetSceneTree(nullptr);
                GetSystem<EntityManager>().SetSceneTree(nullptr);

                GetSystem<TResourceManager>().Clear<WorldAsset>();
                GetSystem<TResourceManager>().Clear<StateMachineAsset>();

                EnginePhysTestBase::TearDown();
        }

        void Drain(const uint32_t n = 2) {
                for (uint32_t i = 0; i < n; ++i) { StepFrame(1.f / 60.f); }
        }

        WorldSystem &        Worlds() { return GetSystem<WorldSystem>(); }
        SpatialIndexSystem & Spatial() { return GetSystem<SpatialIndexSystem>(); }

        uint64_t TagMask(const char * sTag) {
                return 1ull << Spatial().Get().TagBit(StrID(sTag));
        }

        // every live entry handle attributed to the world sub-level slot
        // (entries store index + 1; 0 means unowned) — enumerated via the
        // debug dump (a "find everything" query would trip the cell guard)
        THandleSet EntriesOfLevel(const uint32_t level_index) {
                THandleSet s;
                std::vector<SpatialIndex::SDebugEntry> vAll;
                Spatial().Get().DebugDump(vAll);
                for (const auto & e : vAll) {
                        if (e.sub_level == level_index + 1) { s.insert(e.handle); }
                }
                return s;
        }

        TSceneTree::TSceneNode Find(const std::string & sFullName) {
                return scene->FindFullName(StrID(sFullName.c_str()));
        }
};

// ---------------------------------------------------------------------------
// authored entries
// ---------------------------------------------------------------------------

TEST_F(SpatialTest, AuthoredEntryActivatesWithSubLevel) {

        // streaming disabled: both levels activated synchronously in LoadWorld
        const uint64_t poi = TagMask("poi");

        const THandleSet near_a = [&] {
                THandleSet s;
                Spatial().Get().ForEachInSphere({ 2.f, 0.f, 2.f }, 0.5f, poi,
                                                [&](const SpatialIndex::SEntry & e) { s.insert(e.handle); });
                return s;
        }();
        ASSERT_EQ(near_a.size(), 1u);   // poi_a (radius 1, at 2,0,2); poi_b is 1.4 m away

        // the beta entry answers its own probe
        const THandleSet near_b = [&] {
                THandleSet s;
                Spatial().Get().ForEachInSphere({ 1.f, 0.f, 1.f }, 0.1f, poi,
                                                [&](const SpatialIndex::SEntry & e) { s.insert(e.handle); });
                return s;
        }();
        EXPECT_EQ(near_b.size(), 1u);

        // sub-level attribution: alpha's entries carry level 0 (stored 1), beta's level 1 (stored 2)
        EXPECT_FALSE(EntriesOfLevel(0).empty());
        EXPECT_FALSE(EntriesOfLevel(1).empty());
        EXPECT_EQ(EntriesOfLevel(0).size(), 2u);   // poi_a + tracker
        EXPECT_EQ(EntriesOfLevel(1).size(), 1u);   // poi_b
}

TEST_F(SpatialTest, AuthoredEntryDiesAtDeactivating_AndSweepsBeforeConsumers) {

        // listener registered AFTER SpatialIndexSystem (construction order), so the
        // sweep has already run when this sink fires — the atomic-to-gameplay rule
        struct Sink {
                SpatialIndexSystem * pSys;
                int                  seen     = 0;
                size_t               level0_entries_at_deactivating = 99;
                void OnDeactivating(const Event & e) {
                        const auto & ev = e.Get<ESubLevelDeactivating>();
                        if (ev.sub_level != 0) { return; }
                        ++seen;
                        size_t count = 0;
                        pSys->Get().ForEachInSphere({ 0.f, 0.f, 0.f }, 100000.f, 0xFFFFFFFFFFFFFFFFull,
                                                     [&](const SpatialIndex::SEntry & en) {
                                                             if (en.sub_level == ev.sub_level + 1) { ++count; }
                                                     });
                        level0_entries_at_deactivating = count;
                }
        } sink { &Spatial() };
        Events().AddListener<ESubLevelDeactivating, &Sink::OnDeactivating>(&sink);

        ASSERT_TRUE(Worlds().DeactivateLevel(0));

        // swept synchronously with the event, before any deferred node destruction
        EXPECT_EQ(sink.seen, 1);
        EXPECT_EQ(sink.level0_entries_at_deactivating, 0u);
        EXPECT_TRUE(EntriesOfLevel(0).empty());
        EXPECT_FALSE(EntriesOfLevel(1).empty());   // beta untouched

        Events().RemoveListener<ESubLevelDeactivating, &Sink::OnDeactivating>(&sink);

        Drain();

        // still gone after the nodes actually die (component dtors made no duplicates)
        EXPECT_TRUE(EntriesOfLevel(0).empty());

        // and reactivation re-registers them through the activation window
        ASSERT_TRUE(Worlds().ActivateLevel(0));
        EXPECT_EQ(EntriesOfLevel(0).size(), 2u);
}

TEST_F(SpatialTest, TwoLevelsSweepIndependently) {

        const THandleSet alpha_before = EntriesOfLevel(0);
        const THandleSet beta_before  = EntriesOfLevel(1);
        ASSERT_EQ(alpha_before.size(), 2u);
        ASSERT_EQ(beta_before.size(), 1u);

        ASSERT_TRUE(Worlds().DeactivateLevel(0));
        Drain();

        EXPECT_TRUE(EntriesOfLevel(0).empty());
        EXPECT_EQ(EntriesOfLevel(1), beta_before);   // beta's entries unaffected
}

// ---------------------------------------------------------------------------
// programmatic entries & teardown
// ---------------------------------------------------------------------------

TEST_F(SpatialTest, DynamicsEntrySurvivesTeardown) {

        SpatialIndex::SEntryDesc d;
        d.vPos = { 100.f, 0.f, 100.f };
        d.tag  = StrID("dyn");
        const auto h = Spatial().Get().Add(d);
        ASSERT_NE(h, 0u);

        ASSERT_TRUE(Worlds().DeactivateLevel(0));
        Drain();

        // sub_level 0-defaulted here would be swept — an explicitly unowned entry
        // must be added with sub_level 0 OUTSIDE any activation window, which is
        // exactly what a test-body Add is; deactivation of level 0 must not touch it
        SpatialIndex::SEntry out;
        EXPECT_TRUE(Spatial().Get().Nearest({ 100.f, 0.f, 100.f }, 1.f, 1ull << Spatial().Get().TagBit(StrID("dyn")), out));
        EXPECT_EQ(out.handle, h);
}

TEST_F(SpatialTest, ComponentDtorUnregisters) {

        auto pNode = scene->Create("dyn_node_spatial_test");
        ASSERT_NE(pNode, nullptr);
        pNode->SetPos({ 60.f, 0.f, 60.f });

        SpatialEntryDesc desc;
        desc.tag = "dyn_comp";
        ASSERT_EQ(pNode->CreateComponent<SpatialEntry>(desc), uSUCCESS);

        const uint64_t mask = TagMask("dyn_comp");
        SpatialIndex::SEntry out;
        ASSERT_TRUE(Spatial().Get().Nearest({ 60.f, 0.f, 60.f }, 1.f, mask, out));

        GetSystem<EntityManager>().Destroy(pNode);
        Drain();

        // the flush unlinked the node, but the component object is still alive —
        // this test holds the last strong reference — so the entry (rightly)
        // persists until the component itself dies
        EXPECT_TRUE(Spatial().Get().Nearest({ 60.f, 0.f, 60.f }, 1.f, mask, out));

        // dropping the reference destroys the component: Disable removes the
        // listener registration and the index entry
        pNode.reset();
        EXPECT_FALSE(Spatial().Get().Nearest({ 60.f, 0.f, 60.f }, 1.f, mask, out));
        EXPECT_EQ(Spatial().GetStats().pending, 0u);
}

// ---------------------------------------------------------------------------
// focus mirror
// ---------------------------------------------------------------------------

TEST_F(SpatialTest, FocusEntriesRegisteredAndMoved) {

        auto pFocus = scene->Create("sp_focus_test");
        ASSERT_NE(pFocus, nullptr);
        pFocus->SetPos({ 8.f, 0.f, 8.f });
        const auto hFocus = Worlds().RegisterFocus(pFocus, 0.f, 0.f);

        Drain();

        const uint64_t focus_mask = Spatial().FocusTagMask();
        ASSERT_NE(focus_mask, 0u);

        // registered at the node position, radius 0 (a focus is a point)
        SpatialIndex::SEntry out;
        ASSERT_TRUE(Spatial().Get().Nearest({ 8.f, 0.f, 8.f }, 0.5f, focus_mask, out));
        EXPECT_FLOAT_EQ(out.pos.x, 8.f);
        EXPECT_FLOAT_EQ(out.pos.z, 8.f);
        EXPECT_FLOAT_EQ(out.radius, 0.f);

        // the mirror follows node movement (WorldSystem focus pass -> poll pass)
        pFocus->SetPos({ 30.f, 0.f, 30.f });
        Drain();
        ASSERT_TRUE(Spatial().Get().Nearest({ 30.f, 0.f, 30.f }, 0.5f, focus_mask, out));
        EXPECT_FALSE(Spatial().Get().Nearest({ 8.f, 0.f, 8.f }, 0.5f, focus_mask, out));

        Worlds().UnregisterFocus(hFocus);
        Drain();
        EXPECT_FALSE(Spatial().Get().Nearest({ 30.f, 0.f, 30.f }, 0.5f, focus_mask, out));
}

TEST_F(SpatialTest, FocusEntriesClearedOnWorldUnload) {

        auto pFocus = scene->Create("sp_focus_clear_test");
        pFocus->SetPos({ 12.f, 0.f, 12.f });
        Worlds().RegisterFocus(pFocus, 0.f, 0.f);
        Drain();
        EXPECT_GT(Spatial().GetStats().entries, 0u);

        Worlds().UnloadWorld();
        Drain();

        // the index is empty: focus mirrors and sub-level entries all dropped
        EXPECT_EQ(Spatial().GetStats().entries, 0u);
        EXPECT_EQ(Spatial().GetStats().cells, 0u);

        // leave a clean world for TearDown's UnloadWorld
        auto pWorldT = se_test::MakeWorld("spatial_world_empty_test");
        const auto vBytes = se_test::BakeWorld(*pWorldT);
        auto hWorld = CreateResource<WorldAsset>("spatial_world_empty_test",
                                                 FlatBuffers::GetWorld(vBytes.data()));
        ASSERT_TRUE(Worlds().LoadWorld(hWorld));
}

// ---------------------------------------------------------------------------
// node-listener tracking (no polling)
// ---------------------------------------------------------------------------

TEST_F(SpatialTest, TrackedEntryFollowsNode_WithoutFrameStep) {

        const uint64_t mover = TagMask("mover");
        auto pTracker = Find("root|world|spatial_alpha.sesc#0|Alpha|tracker");
        ASSERT_NE(pTracker, nullptr);

        // entry sits at the authored position
        SpatialIndex::SEntry out;
        ASSERT_TRUE(Spatial().Get().Nearest({ 5.f, 0.f, 5.f }, 0.1f, mover, out));

        // SetPos updates the index SYNCHRONOUSLY through the node listener —
        // no frame step in between
        pTracker->SetPos({ 9.f, 0.f, 9.f });
        EXPECT_TRUE(Spatial().Get().Nearest({ 9.f, 0.f, 9.f }, 0.1f, mover, out));
        EXPECT_FALSE(Spatial().Get().Nearest({ 5.f, 0.f, 5.f }, 0.1f, mover, out));
}

TEST_F(SpatialTest, TrackedEntryFollowsParentCascade) {

        const uint64_t mover = TagMask("mover");
        auto pAlpha = Find("root|world|spatial_alpha.sesc#0|Alpha");
        ASSERT_NE(pAlpha, nullptr);

        // the tracker entry's node never moves; its PARENT does — InvalidateChildren
        // cascades to descendant listeners
        pAlpha->SetPos({ 10.f, 0.f, 0.f });

        SpatialIndex::SEntry out;
        EXPECT_TRUE(Spatial().Get().Nearest({ 15.f, 0.f, 5.f }, 0.1f, mover, out));   // 5+10, 0, 5
        EXPECT_FALSE(Spatial().Get().Nearest({ 5.f, 0.f, 5.f }, 0.1f, mover, out));
}

TEST_F(SpatialTest, ComponentLifecycleCycles) {

        auto pTracker = Find("root|world|spatial_alpha.sesc#0|Alpha|tracker");
        ASSERT_NE(pTracker, nullptr);
        auto * pEntry = pTracker->GetComponent<SpatialEntry>();
        ASSERT_NE(pEntry, nullptr);

        const uint64_t mover = TagMask("mover");
        SpatialIndex::SEntry out;

        // disable: gone
        const auto h = pEntry->Handle();
        pEntry->Disable();
        EXPECT_EQ(pEntry->Handle(), 0u);
        EXPECT_FALSE(Spatial().Get().Nearest({ 5.f, 0.f, 5.f }, 0.1f, mover, out));

        // re-enable: back, at the node's current position (a fresh fixture per
        // test, so the tracker restarts at its authored 5,0,5)
        pEntry->Enable();
        ASSERT_TRUE(Spatial().Get().Nearest({ 5.f, 0.f, 5.f }, 0.1f, mover, out));

        // double enable is a guarded no-op (no duplicate entry)
        pEntry->Enable();
        std::vector<SpatialIndex::SDebugEntry> vAll;
        Spatial().Get().DebugDump(vAll);
        THandleSet movers;
        for (const auto & e : vAll) {
                if (e.tags == mover) { movers.insert(e.handle); }
        }
        EXPECT_EQ(movers.size(), 1u);
        EXPECT_NE(*movers.begin(), h);   // generation moved across the cycle

        // stale handle of the first registration no longer validates
        EXPECT_FALSE(Spatial().Get().Remove(h));
}

// ---------------------------------------------------------------------------
// consumer shape (what Step 5 spawner conditions will look like)
// ---------------------------------------------------------------------------

TEST_F(SpatialTest, SpawnerCondition_FocusWithinRadius) {

        auto pFocus = scene->Create("sp_focus_spawn_test");
        pFocus->SetPos({ 8.f, 0.f, 8.f });
        Worlds().RegisterFocus(pFocus, 0.f, 0.f);
        Drain();

        // a spawn point 30 m away: "is a focus within 40 m of me?"
        SpatialIndex::SEntryDesc spawn;
        spawn.vPos = { 38.f, 0.f, 8.f };
        spawn.tag  = StrID("spawn_point");
        const auto hSpawn = Spatial().Get().Add(spawn);
        ASSERT_NE(hSpawn, 0u);

        const uint64_t focus_mask = Spatial().FocusTagMask();
        SpatialIndex::SEntry out;
        ASSERT_TRUE(Spatial().Get().Nearest({ 38.f, 0.f, 8.f }, 40.f, focus_mask, out));
        // tag hygiene: the match carries exactly the focus tag, never the spawn tag
        EXPECT_EQ(out.tags, focus_mask);

        // the focus moves away: the condition turns false after the focus pass
        pFocus->SetPos({ 200.f, 0.f, 200.f });
        Drain();
        EXPECT_FALSE(Spatial().Get().Nearest({ 38.f, 0.f, 8.f }, 40.f, focus_mask, out));
}
