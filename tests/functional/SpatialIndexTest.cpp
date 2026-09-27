
// ---------------------------------------------------------------------------
// SpatialIndexTest — Tier B: the pure spatial query grid. No scene, no engine
// systems: SpatialIndex is constructed directly (StreamingDirectorTest
// pattern). Query results are compared against brute-force references;
// determinism tests run identical op scripts on fresh indices.
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>
#include "EngineFixture.h"

#include <world/SpatialIndex.h>
#ifdef SE_IMPL
#include <world/SpatialIndex.tcc>
#endif

#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include <random>
#include <set>
#include <stdexcept>
#include <vector>

using namespace SE;

namespace {

constexpr float kCell = 16.f;

// A reference entry for brute-force comparison.
struct SRef {
        glm::vec3 pos {};
        float     radius = 0.f;
        uint64_t  tags   = 0;
        SpatialIndex::EntryHandle handle = 0;
};

using THandleSet = std::set<uint64_t>;

THandleSet CollectSphere(SpatialIndex & oIndex, const glm::vec3 & vPos, const float radius,
                         const uint64_t mask) {
        THandleSet s;
        oIndex.ForEachInSphere(vPos, radius, mask,
                               [&](const SpatialIndex::SEntry & e) { s.insert(e.handle); });
        return s;
}

THandleSet CollectAABB(SpatialIndex & oIndex, const glm::vec3 & vMin, const glm::vec3 & vMax,
                       const uint64_t mask) {
        THandleSet s;
        oIndex.ForEachInAABB(vMin, vMax, mask,
                             [&](const SpatialIndex::SEntry & e) { s.insert(e.handle); });
        return s;
}

THandleSet BruteForceSphere(const std::vector<SRef> & vRef, const glm::vec3 & vPos,
                            const float radius, const uint64_t mask) {
        THandleSet s;
        for (const SRef & r : vRef) {
                if ((r.tags & mask) == 0) { continue; }
                const glm::vec3 d  = r.pos - vPos;
                const float     rr = radius + r.radius;
                if (d.x * d.x + d.y * d.y + d.z * d.z <= rr * rr) { s.insert(r.handle); }
        }
        return s;
}

THandleSet BruteForceAABB(const std::vector<SRef> & vRef, const glm::vec3 & vMin,
                          const glm::vec3 & vMax, const uint64_t mask) {
        THandleSet s;
        for (const SRef & r : vRef) {
                if ((r.tags & mask) == 0) { continue; }
                const bool overlap =
                        (r.pos.x - r.radius <= vMax.x && r.pos.x + r.radius >= vMin.x) &&
                        (r.pos.y - r.radius <= vMax.y && r.pos.y + r.radius >= vMin.y) &&
                        (r.pos.z - r.radius <= vMax.z && r.pos.z + r.radius >= vMin.z);
                if (overlap) { s.insert(r.handle); }
        }
        return s;
}

SpatialIndex::SEntryDesc MakeDesc(const glm::vec3 & vPos, const float radius, const StrID tag) {
        SpatialIndex::SEntryDesc d;
        d.vPos   = vPos;
        d.radius = radius;
        d.tag    = tag;
        return d;
}

} // namespace

// ---------------------------------------------------------------------------
// mask filtering
// ---------------------------------------------------------------------------

TEST(SpatialIndexTest, AddQueryByTagMask) {

        SpatialIndex idx;
        const StrID ta("a"), tb("b");

        const auto h1 = idx.Add(MakeDesc({ 5.f, 0.f, 5.f }, 0.f, ta));
        const auto h2 = idx.Add(MakeDesc({ 7.f, 0.f, 5.f }, 0.f, tb));
        ASSERT_NE(h1, 0u);
        ASSERT_NE(h2, 0u);

        const uint64_t mask_a    = idx.TagMask({ ta });
        const uint64_t mask_b    = idx.TagMask({ tb });
        const uint64_t mask_ab   = idx.TagMask({ ta, tb });
        const uint64_t mask_none = idx.TagMask({ StrID("never_registered_a1") });

        EXPECT_EQ(CollectSphere(idx, { 6.f, 0.f, 5.f }, 4.f, mask_a), THandleSet({ h1 }));
        EXPECT_EQ(CollectSphere(idx, { 6.f, 0.f, 5.f }, 4.f, mask_b), THandleSet({ h2 }));
        EXPECT_EQ(CollectSphere(idx, { 6.f, 0.f, 5.f }, 4.f, mask_ab), THandleSet({ h1, h2 }));
        EXPECT_TRUE(CollectSphere(idx, { 6.f, 0.f, 5.f }, 4.f, mask_none).empty());
}

// ---------------------------------------------------------------------------
// brute-force oracle
// ---------------------------------------------------------------------------

TEST(SpatialIndexTest, SphereMatchesBruteForce_RandomChurn) {

        SpatialIndex idx;
        const StrID tags[3] = { StrID("churn_a"), StrID("churn_b"), StrID("churn_c") };
        const uint64_t tag_masks[3] = {
                1ull << idx.TagBit(tags[0]),
                1ull << idx.TagBit(tags[1]),
                1ull << idx.TagBit(tags[2]),
        };
        const uint64_t all_mask = idx.TagMask({ tags[0], tags[1], tags[2] });

        std::mt19937 rng(20260912u);
        std::uniform_real_distribution<float> dist_pos(-100.f, 100.f);
        std::uniform_real_distribution<float> dist_radius(0.f, 6.f);
        std::uniform_int_distribution<int>    dist_tag(0, 2);
        std::uniform_int_distribution<int>    dist_op(0, 9);

        std::vector<SRef> vRef;

        for (int step = 0; step < 500; ++step) {

                const int op = dist_op(rng);
                if (op < 4 || vRef.empty()) {          // add
                        const int   t      = dist_tag(rng);
                        const float radius = dist_radius(rng);
                        const glm::vec3 vPos(dist_pos(rng), dist_pos(rng) * 0.25f, dist_pos(rng));
                        const auto h = idx.Add(MakeDesc(vPos, radius, tags[t]));
                        ASSERT_NE(h, 0u);
                        vRef.push_back({ vPos, radius, tag_masks[t], h });
                } else if (op < 7) {                   // move
                        std::uniform_int_distribution<size_t> dist_i(0, vRef.size() - 1);
                        const size_t i = dist_i(rng);
                        const glm::vec3 vPos(dist_pos(rng), dist_pos(rng) * 0.25f, dist_pos(rng));
                        EXPECT_TRUE(idx.Move(vRef[i].handle, vPos));
                        vRef[i].pos = vPos;
                } else {                               // remove
                        std::uniform_int_distribution<size_t> dist_i(0, vRef.size() - 1);
                        const size_t i = dist_i(rng);
                        EXPECT_TRUE(idx.Remove(vRef[i].handle));
                        vRef[i] = vRef.back();
                        vRef.pop_back();
                }

                // probes: near origin, off-center, above the cloud
                const glm::vec3 probes[3] = {
                        { 0.f, 0.f, 0.f },
                        { dist_pos(rng), 0.f, dist_pos(rng) },
                        { dist_pos(rng), 10.f, dist_pos(rng) },
                };
                for (const glm::vec3 & p : probes) {
                        const float qr = 10.f + dist_radius(rng) * 8.f;
                        for (int t = 0; t < 3; ++t) {
                                EXPECT_EQ(CollectSphere(idx, p, qr, tag_masks[t]),
                                          BruteForceSphere(vRef, p, qr, tag_masks[t])) << "step " << step;
                        }
                        EXPECT_EQ(CollectSphere(idx, p, qr, all_mask),
                                  BruteForceSphere(vRef, p, qr, all_mask)) << "step " << step;
                        const glm::vec3 vMin(p.x - qr, p.y - 4.f, p.z - qr);
                        const glm::vec3 vMax(p.x + qr, p.y + 4.f, p.z + qr);
                        EXPECT_EQ(CollectAABB(idx, vMin, vMax, all_mask),
                                  BruteForceAABB(vRef, vMin, vMax, all_mask)) << "step " << step;
                }
        }

        EXPECT_EQ(idx.GetStats().entries, uint32_t(vRef.size()));
}

TEST(SpatialIndexTest, BoundaryCells) {

        SpatialIndex idx;
        const StrID t("edge");
        const uint64_t mask = 1ull << idx.TagBit(t);

        // entries exactly on cell borders (multiples of kCell)
        std::vector<SRef> vRef;
        for (int i = -2; i <= 2; ++i) {
                for (int j = -2; j <= 2; ++j) {
                        const glm::vec3 vPos(float(i) * kCell, 0.f, float(j) * kCell);
                        const auto h = idx.Add(MakeDesc(vPos, 0.f, t));
                        ASSERT_NE(h, 0u);
                        vRef.push_back({ vPos, 0.f, mask, h });
                }
        }

        // queries centered on borders with radii spanning many cells
        for (const glm::vec3 & p : { glm::vec3(0.f, 0.f, 0.f), glm::vec3(kCell, 0.f, -kCell),
                                     glm::vec3(kCell * 0.5f, 0.f, -kCell * 0.5f) }) {
                for (const float r : { 0.f, kCell * 0.5f, kCell, kCell * 2.5f, kCell * 5.f }) {
                        EXPECT_EQ(CollectSphere(idx, p, r, mask),
                                  BruteForceSphere(vRef, p, r, mask))
                                        << "p=(" << p.x << "," << p.z << ") r=" << r;
                }
        }
}

TEST(SpatialIndexTest, AABB_YFiltering) {

        SpatialIndex idx;
        const StrID t("y");
        const uint64_t mask = 1ull << idx.TagBit(t);

        std::vector<SRef> vRef;
        for (int i = 0; i < 10; ++i) {
                const float     fi   = float(i);
                const glm::vec3 vPos(fi, fi * 3.f - 15.f, fi * 2.f);
                const auto h = idx.Add(MakeDesc(vPos, 0.5f, t));
                ASSERT_NE(h, 0u);
                vRef.push_back({ vPos, 0.5f, mask, h });
        }

        // a thin slab: only entries whose y extent intersects it
        const glm::vec3 vMin(-1.f, -2.f, -1.f);
        const glm::vec3 vMax(12.f, 2.f, 22.f);
        EXPECT_EQ(CollectAABB(idx, vMin, vMax, mask), BruteForceAABB(vRef, vMin, vMax, mask));

        // inclusive at the exact overlap boundary: entry at y=3, radius 0.5, slab top 2.5
        SpatialIndex idx2;
        const uint64_t m2 = 1ull << idx2.TagBit(t);
        const auto h = idx2.Add(MakeDesc({ 0.f, 3.f, 0.f }, 0.5f, t));
        EXPECT_EQ(CollectAABB(idx2, { -1.f, -1.f, -1.f }, { 1.f, 2.5f, 1.f }, m2), THandleSet({ h }));
}

TEST(SpatialIndexTest, SphereExpandsByEntryRadius) {

        SpatialIndex idx;
        const StrID t("fat");
        const uint64_t mask = 1ull << idx.TagBit(t);

        // a fat entry, center 12 m from the probe
        const auto h = idx.Add(MakeDesc({ 12.f, 0.f, 0.f }, 3.f, t));
        ASSERT_NE(h, 0u);

        EXPECT_TRUE(CollectSphere(idx, { 0.f, 0.f, 0.f }, 8.f, mask).empty());   // 8+3 < 12
        EXPECT_EQ(CollectSphere(idx, { 0.f, 0.f, 0.f }, 9.f, mask), THandleSet({ h }));  // 9+3 == 12
        EXPECT_EQ(CollectSphere(idx, { 0.f, 0.f, 0.f }, 20.f, mask), THandleSet({ h }));
}

// ---------------------------------------------------------------------------
// Nearest
// ---------------------------------------------------------------------------

TEST(SpatialIndexTest, Nearest_TieBreaksBySerial) {

        SpatialIndex idx;
        const StrID t("n");
        const uint64_t mask = 1ull << idx.TagBit(t);

        const auto h_first  = idx.Add(MakeDesc({ 10.f, 0.f, 0.f }, 0.f, t));   // serial 1
        const auto h_second = idx.Add(MakeDesc({ -10.f, 0.f, 0.f }, 0.f, t));  // serial 2
        idx.Add(MakeDesc({ 500.f, 0.f, 0.f }, 0.f, t));                        // serial 3, far

        SpatialIndex::SEntry out;
        ASSERT_TRUE(idx.Nearest({ 0.f, 0.f, 0.f }, 50.f, mask, out));
        EXPECT_EQ(out.handle, h_first);

        // tie: four entries all exactly 10 m away — the oldest wins, deterministically
        idx.Add(MakeDesc({ 0.f, 0.f, 10.f }, 0.f, t));
        idx.Add(MakeDesc({ 0.f, 10.f, 0.f }, 0.f, t));
        ASSERT_TRUE(idx.Nearest({ 0.f, 0.f, 0.f }, 50.f, mask, out));
        EXPECT_EQ(out.handle, h_first);

        // range bounds the entry CENTER: everything is 10 m away
        ASSERT_TRUE(idx.Nearest({ 0.f, 0.f, 0.f }, 10.f, mask, out));
        EXPECT_EQ(out.handle, h_first);
        EXPECT_FALSE(idx.Nearest({ 0.f, 0.f, 0.f }, 5.f, mask, out));

        (void)h_second;
}

// ---------------------------------------------------------------------------
// handles
// ---------------------------------------------------------------------------

TEST(SpatialIndexTest, StaleHandlesRejected) {

        SpatialIndex idx;
        const StrID t("h");
        const uint64_t mask = 1ull << idx.TagBit(t);

        const auto h1 = idx.Add(MakeDesc({ 1.f, 0.f, 1.f }, 0.f, t));
        ASSERT_TRUE(idx.Remove(h1));
        EXPECT_FALSE(idx.Remove(h1));    // already removed
        EXPECT_FALSE(idx.Move(h1, { 9.f, 0.f, 9.f }));

        // slot reuse must not alias: h1 predates the generation bump
        const auto h2 = idx.Add(MakeDesc({ 2.f, 0.f, 2.f }, 0.f, t));
        EXPECT_NE(h1, h2);
        EXPECT_FALSE(idx.Remove(h1));
        EXPECT_EQ(CollectSphere(idx, { 2.f, 0.f, 2.f }, 0.1f, mask), THandleSet({ h2 }));

        SpatialIndex::SEntry out;
        EXPECT_TRUE(idx.Nearest({ 2.f, 0.f, 2.f }, 1.f, mask, out));
        EXPECT_EQ(out.handle, h2);
}

// ---------------------------------------------------------------------------
// reentrancy
// ---------------------------------------------------------------------------

TEST(SpatialIndexTest, Reentrancy_InsertsAndCrossCellMovesDefer) {

        SpatialIndex idx;
        const StrID ta("re_a"), tb("re_b");
        const uint64_t mask = idx.TagMask({ ta, tb });

        // three cells along +x
        const auto e0 = idx.Add(MakeDesc({ 5.f, 0.f, 5.f }, 0.f, ta));
        const auto e1 = idx.Add(MakeDesc({ 21.f, 0.f, 5.f }, 0.f, ta));
        const auto e2 = idx.Add(MakeDesc({ 37.f, 0.f, 5.f }, 0.f, tb));

        int visited = 0;
        SpatialIndex::EntryHandle added_during = 0;
        int nested_calls = 0;

        idx.ForEachInSphere({ 21.f, 0.f, 5.f }, 30.f, mask, [&](const SpatialIndex::SEntry & e) {
                ++visited;
                if (e.handle == e0) {
                        // insert (deferred), cross-cell move of an unvisited entry
                        // (deferred — it stays in its old bucket and the precision
                        // test rejects it at the new position), removal (deferred
                        // dead record — the bucket is not mutated mid-iteration)
                        SpatialIndex::SEntryDesc d = MakeDesc({ 100.f, 0.f, 5.f }, 0.f, ta);
                        added_during = idx.Add(d);
                        EXPECT_TRUE(idx.Move(e2, { 4000.f, 0.f, 4000.f }));   // far out of every probe below
                        EXPECT_TRUE(idx.Remove(e1));
                }
                // nested query inside the callback: must not flush nor crash
                idx.ForEachInSphere({ 5.f, 0.f, 5.f }, 1.f, mask,
                                    [&](const SpatialIndex::SEntry &) { ++nested_calls; });
        });

        // e0 visits and mutates; e1 is dead by the time its record is reached;
        // e2 is precision-rejected at its relocated position
        EXPECT_EQ(visited, 1);
        EXPECT_EQ(nested_calls, 1);
        EXPECT_NE(added_during, 0u);

        // all deferrals flushed at the outer query end
        EXPECT_EQ(idx.GetStats().pending, 0u);

        // the next query sees the post-mutation layout exactly:
        // e1 removed, e2 relocated far out, e0 + the deferred insert live
        // (r=150 keeps the scan inside the default max_query_cells guard)
        const THandleSet live = CollectSphere(idx, { 0.f, 0.f, 0.f }, 150.f, mask);
        EXPECT_EQ(live, THandleSet({ e0, added_during }));

        // and a fresh query agrees with the brute force over that layout
        std::vector<SRef> vRef;
        idx.ForEachInSphere({ 0.f, 0.f, 0.f }, 150.f, mask, [&](const SpatialIndex::SEntry & e) {
                vRef.push_back({ e.pos, e.radius, e.tags, e.handle });
        });
        ASSERT_EQ(vRef.size(), 2u);
        EXPECT_EQ(CollectSphere(idx, { 100.f, 0.f, 5.f }, 1.f, mask),
                  BruteForceSphere(vRef, { 100.f, 0.f, 5.f }, 1.f, mask));
}

TEST(SpatialIndexTest, DeferredFlushVisibility) {

        SpatialIndex idx;
        const StrID t("df");
        const uint64_t mask = 1ull << idx.TagBit(t);

        const auto outer = idx.Add(MakeDesc({ 5.f, 0.f, 5.f }, 0.f, t));
        ASSERT_NE(outer, 0u);

        bool inserted_in_callback = false;
        idx.ForEachInSphere({ 5.f, 0.f, 5.f }, 2.f, mask, [&](const SpatialIndex::SEntry &) {
                if (!inserted_in_callback) {
                        inserted_in_callback = true;
                        idx.Add(MakeDesc({ 6.f, 0.f, 5.f }, 0.f, t));   // deferred: lands after
                }
                // the in-flight query must not observe the pending insert
                // (probe radius 0.5 stays clear of the outer entry, 1 m away)
                THandleSet in_flight = CollectSphere(idx, { 6.f, 0.f, 5.f }, 0.5f, mask);
                EXPECT_TRUE(in_flight.empty());
        });
        EXPECT_TRUE(inserted_in_callback);

        const THandleSet after = CollectSphere(idx, { 6.f, 0.f, 5.f }, 1.f, mask);
        EXPECT_EQ(after.size(), 2u);   // both entries findable after the flush
}

// ---------------------------------------------------------------------------
// determinism
// ---------------------------------------------------------------------------

TEST(SpatialIndexTest, Determinism_SameOpsSameResults) {

        const StrID tags[2] = { StrID("det_a"), StrID("det_b") };

        auto RunScript = [&tags]() {
                SpatialIndex idx;
                std::vector<uint64_t> results;

                std::mt19937 rng(777u);
                std::uniform_real_distribution<float> dist_pos(-50.f, 50.f);
                std::uniform_int_distribution<int>    dist_tag(0, 1);
                std::uniform_int_distribution<int>    dist_op(0, 2);

                std::vector<SpatialIndex::EntryHandle> vHandles;
                for (int step = 0; step < 200; ++step) {
                        const int op = dist_op(rng);
                        if (op == 0 || vHandles.empty()) {
                                vHandles.push_back(idx.Add(MakeDesc(
                                        { dist_pos(rng), 0.f, dist_pos(rng) }, 0.f, tags[dist_tag(rng)])));
                        } else if (op == 1) {
                                std::uniform_int_distribution<size_t> di(0, vHandles.size() - 1);
                                idx.Move(vHandles[di(rng)], { dist_pos(rng), 0.f, dist_pos(rng) });
                        } else if (vHandles.size() > 4) {
                                std::uniform_int_distribution<size_t> di(0, vHandles.size() - 1);
                                idx.Remove(vHandles[di(rng)]);
                                vHandles[di(rng)] = vHandles.back();
                                vHandles.pop_back();
                        }

                        idx.ForEachInSphere({ 0.f, 0.f, 0.f }, 60.f, 0xFFFFFFFFFFFFFFFFull,
                                            [&](const SpatialIndex::SEntry & e) {
                                                    results.push_back(e.handle);
                                                    results.push_back(uint64_t(e.pos.x * 100.f));
                                            });
                }
                return results;
        };

        const std::vector<uint64_t> run_a = RunScript();
        const std::vector<uint64_t> run_b = RunScript();
        EXPECT_EQ(run_a, run_b);
        EXPECT_FALSE(run_a.empty());
}

// ---------------------------------------------------------------------------
// guards & config
// ---------------------------------------------------------------------------

TEST(SpatialIndexTest, OversizedQueryRefused) {

        SpatialIndex idx;
        const StrID t("big");
        const uint64_t mask = 1ull << idx.TagBit(t);

        const auto h = idx.Add(MakeDesc({ 5.f, 0.f, 5.f }, 0.f, t));
        ASSERT_NE(h, 0u);

        // a 2 km-radius query over 16 m cells dwarfs the 4096-cell guard
        THandleSet got;
        idx.ForEachInSphere({ 0.f, 0.f, 0.f }, 2000.f, mask,
                            [&](const SpatialIndex::SEntry & e) { got.insert(e.handle); });
        EXPECT_TRUE(got.empty());
        EXPECT_EQ(idx.GetStats().refused_queries, 1u);

        // raising the limit makes it work (~63k cells scanned — still fast)
        SpatialIndex::SConfig cfg;
        cfg.max_query_cells = 1000000u;
        ASSERT_TRUE(idx.SetConfig(cfg));
        got.clear();
        idx.ForEachInSphere({ 0.f, 0.f, 0.f }, 2000.f, mask,
                            [&](const SpatialIndex::SEntry & e) { got.insert(e.handle); });
        EXPECT_EQ(got, THandleSet({ h }));
        EXPECT_EQ(idx.GetStats().refused_queries, 1u);
}

TEST(SpatialIndexTest, CellSizeVariants) {

        const StrID t("cell");
        std::vector<SRef> vRef;

        // golden-angle-ish scatter; identical layout in both grids
        auto Populate = [&vRef, t](SpatialIndex & idx) {
                const uint64_t mask = 1ull << idx.TagBit(t);
                vRef.clear();
                for (int i = 0; i < 40; ++i) {
                        const float     a    = float(i) * 2.399963f;
                        const glm::vec3 vPos(std::cos(a) * float(i) * 3.f, 0.f, std::sin(a) * float(i) * 3.f);
                        const float     radius = float(i % 3);
                        const auto      h = idx.Add(MakeDesc(vPos, radius, t));
                        EXPECT_NE(h, 0u);
                        vRef.push_back({ vPos, radius, mask, h });
                }
                return mask;
        };

        SpatialIndex fine, coarse;
        SpatialIndex::SConfig cfg_fine;   cfg_fine.cell_size = 4.f;
        SpatialIndex::SConfig cfg_coarse; cfg_coarse.cell_size = 64.f;
        ASSERT_TRUE(fine.SetConfig(cfg_fine));
        ASSERT_TRUE(coarse.SetConfig(cfg_coarse));

        const uint64_t mask_fine   = Populate(fine);
        const uint64_t mask_coarse = Populate(coarse);

        for (const glm::vec3 & p : { glm::vec3(0.f, 0.f, 0.f), glm::vec3(30.f, 0.f, -40.f),
                                     glm::vec3(-100.f, 0.f, 10.f) }) {
                const float r = 45.f;
                EXPECT_EQ(CollectSphere(fine, p, r, mask_fine),
                          CollectSphere(coarse, p, r, mask_coarse));
                EXPECT_EQ(CollectSphere(fine, p, r, mask_fine),
                          BruteForceSphere(vRef, p, r, mask_fine));
        }

        // cell_size change with live entries is refused
        EXPECT_FALSE(fine.SetConfig(cfg_coarse));
}

// ---------------------------------------------------------------------------
// sweeps
// ---------------------------------------------------------------------------

TEST(SpatialIndexTest, RemoveBySubLevel_And_Clear) {

        SpatialIndex idx;
        const StrID t("sweep");
        const uint64_t mask = 1ull << idx.TagBit(t);

        const auto a1 = idx.Add(MakeDesc({ 1.f, 0.f, 1.f }, 0.f, t));   // sub_level default 0
        SpatialIndex::SEntryDesc d = MakeDesc({ 2.f, 0.f, 2.f }, 0.f, t);
        d.sub_level = 7;
        const auto b1 = idx.Add(d);
        d.vPos = { 3.f, 0.f, 3.f };
        const auto b2 = idx.Add(d);
        d.vPos = { 4.f, 0.f, 4.f };
        d.sub_level = 9;
        const auto c1 = idx.Add(d);
        ASSERT_NE(a1, 0u);
        ASSERT_NE(b1, 0u);
        ASSERT_NE(b2, 0u);
        ASSERT_NE(c1, 0u);

        EXPECT_EQ(idx.GetStats().entries, 4u);

        idx.RemoveBySubLevel(7);
        EXPECT_EQ(idx.GetStats().entries, 2u);
        THandleSet got;
        idx.ForEachInSphere({ 0.f, 0.f, 0.f }, 100.f, mask,
                            [&](const SpatialIndex::SEntry & e) { got.insert(e.handle); });
        EXPECT_EQ(got, THandleSet({ a1, c1 }));

        // swept handles are stale
        EXPECT_FALSE(idx.Remove(b1));

        idx.Clear();
        EXPECT_EQ(idx.GetStats().entries, 0u);
        EXPECT_EQ(idx.GetStats().cells, 0u);
        got.clear();
        idx.ForEachInSphere({ 0.f, 0.f, 0.f }, 100.f, mask,
                            [&](const SpatialIndex::SEntry & e) { got.insert(e.handle); });
        EXPECT_TRUE(got.empty());

        // handles held across a Clear() never alias new entries
        const auto fresh = idx.Add(MakeDesc({ 1.f, 0.f, 1.f }, 0.f, t));
        EXPECT_NE(fresh, a1);
        EXPECT_NE(fresh, c1);
        EXPECT_FALSE(idx.Remove(a1));
        EXPECT_FALSE(idx.Remove(c1));
}

TEST(SpatialIndexTest, TagRegistryLimit) {

        SpatialIndex idx;

        for (size_t i = 0; i < SpatialIndex::kMaxTags; ++i) {
                const StrID tag(("lim_" + std::to_string(i)).c_str());
                EXPECT_EQ(idx.TagBit(tag), i) << "tag " << i;
        }
        EXPECT_EQ(idx.TagCount(), SpatialIndex::kMaxTags);

        // the 65th tag is refused (kMaxTags sentinel) without disturbing existing bits
        const StrID extra("lim_extra");
        EXPECT_EQ(idx.TagBit(extra), SpatialIndex::kMaxTags);
        EXPECT_EQ(idx.TagCount(), SpatialIndex::kMaxTags);
        EXPECT_EQ(idx.TagBit(StrID("lim_0")), 0u);
        EXPECT_EQ(idx.TagBit(StrID("lim_63")), 63u);

        // a refused tag yields an empty mask contribution and a 0 handle
        EXPECT_EQ(idx.TagMask({ extra }), 0u);
        EXPECT_EQ(idx.Add(MakeDesc({ 0.f, 0.f, 0.f }, 0.f, extra)), 0u);

        // Add without any tag throws
        EXPECT_THROW(idx.Add(MakeDesc({ 0.f, 0.f, 0.f }, 0.f, StrID {})), std::runtime_error);
}
