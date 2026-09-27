
// ---------------------------------------------------------------------------
// WorldAssetTest — Tier B: the .seworld asset (parse, defaults, validation,
// cell membership) without a scene tree. Runs against the committed baked
// fixture resource/world/test_world.seworld and inline object-api tables.
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>
#include "EngineFixture.h"
#include "WorldAssets.h"

#include <world/WorldAsset.h>

#include <gtest/gtest.h>

#include <memory>

using namespace SE;

namespace {

std::unique_ptr<WorldAsset> BuildInline(FlatBuffers::WorldT & oWorldT) {
        auto vBytes = se_test::BakeWorld(oWorldT);
        const auto * pWorld = FlatBuffers::GetWorld(vBytes.data());
        return std::make_unique<WorldAsset>("inline_world", 0, pWorld);
}

} // namespace

// ---------------------------------------------------------------------------
// Committed fixture round-trip
// ---------------------------------------------------------------------------

TEST(WorldAssetTest, LoadsCommittedFixture) {

        // the resource manager is the first Engine system and no longer
        // self-constructs — initialize it for this fixture-free test
        TEngine::Instance().Init<TResourceManager>();
        auto hWorld = CreateResource<WorldAsset>("resource/world/test_world.seworld");
        const auto * pWorld = GetResource(hWorld);
        ASSERT_NE(pWorld, nullptr);

        EXPECT_EQ(pWorld->GetId(), StrID("test_world"));
        EXPECT_EQ(pWorld->GetSeed(), 12345ull);
        EXPECT_TRUE(pWorld->GetStreaming().enabled);
        EXPECT_FLOAT_EQ(pWorld->GetStreaming().cell_size, 32.f);

        const auto & vLevels = pWorld->GetSubLevels();
        ASSERT_EQ(vLevels.size(), 4u);

        EXPECT_EQ(vLevels[0].path, "resource/world/sublevels/sub_room.sesc");
        EXPECT_TRUE(vLevels[0].IsAlwaysLoaded());
        EXPECT_FALSE(vLevels[1].IsAlwaysLoaded());
        EXPECT_FALSE(vLevels[1].IsManual());

        // slot 2 instances the same .sesc as slot 1, placed + rotated
        EXPECT_EQ(vLevels[2].path, vLevels[1].path);
        EXPECT_FLOAT_EQ(vLevels[2].translation.x, 40.f);
        EXPECT_FLOAT_EQ(vLevels[2].rotation.y, 90.f);

        EXPECT_TRUE(vLevels[3].IsManual());
        EXPECT_EQ(vLevels[3].layer, StrID("interior"));

        EXPECT_FLOAT_EQ(pWorld->GetInitialFocus().y, 2.f);
}

TEST(WorldAssetTest, LoadsNoStreamingFixture) {

        TEngine::Instance().Init<TResourceManager>();
        auto hWorld = CreateResource<WorldAsset>("resource/world/test_world_no_streaming.seworld");
        const auto * pWorld = GetResource(hWorld);
        ASSERT_NE(pWorld, nullptr);

        EXPECT_FALSE(pWorld->GetStreaming().enabled);
        ASSERT_EQ(pWorld->GetSubLevels().size(), 2u);
        EXPECT_FALSE(pWorld->GetSubLevels()[0].IsManual());
        EXPECT_TRUE(pWorld->GetSubLevels()[1].IsManual());
}

// ---------------------------------------------------------------------------
// Defaults and validation
// ---------------------------------------------------------------------------

TEST(WorldAssetTest, StreamingDefaults) {

        auto pWorldT = se_test::MakeWorld("w");
        se_test::AddSubLevel(*pWorldT, "a.sesc",
                        FlatBuffers::Vec3(0, 0, 0), FlatBuffers::Vec3(1, 1, 1));

        auto pAsset = BuildInline(*pWorldT);

        const auto & s = pAsset->GetStreaming();
        EXPECT_TRUE(s.enabled);
        EXPECT_FLOAT_EQ(s.cell_size, 64.f);
        EXPECT_FLOAT_EQ(s.load_radius, 96.f);
        EXPECT_FLOAT_EQ(s.unload_radius, 128.f);
        EXPECT_EQ(s.max_activations_per_frame, 1u);
        EXPECT_FLOAT_EQ(s.main_budget_ms, 2.f);
        EXPECT_EQ(s.unload_frame_delay, 60u);
}

TEST(WorldAssetTest, TransformDefaultsToZero) {

        auto pWorldT = se_test::MakeWorld("w");
        auto * pRef = se_test::AddSubLevel(*pWorldT, "a.sesc",
                        FlatBuffers::Vec3(0, 0, 0), FlatBuffers::Vec3(1, 1, 1));
        (void)pRef;

        auto pAsset = BuildInline(*pWorldT);
        const auto & e = pAsset->GetSubLevels()[0];

        EXPECT_FLOAT_EQ(e.translation.x, 0.f);
        EXPECT_FLOAT_EQ(e.translation.y, 0.f);
        EXPECT_FLOAT_EQ(e.rotation.z, 0.f);
        EXPECT_EQ(e.name, StrID("a.sesc#0"));
        EXPECT_FLOAT_EQ(e.weight, 1.f);
}

TEST(WorldAssetTest, MissingPathThrows) {

        auto pWorldT = se_test::MakeWorld("w");
        pWorldT->sub_levels.push_back(std::make_unique<FlatBuffers::SubLevelRefT>());
        pWorldT->sub_levels.back()->bounds_min =
                        std::make_unique<FlatBuffers::Vec3>(0, 0, 0);
        pWorldT->sub_levels.back()->bounds_max =
                        std::make_unique<FlatBuffers::Vec3>(1, 1, 1);

        EXPECT_THROW(BuildInline(*pWorldT), std::runtime_error);
}

TEST(WorldAssetTest, InvertedBoundsThrows) {

        auto pWorldT = se_test::MakeWorld("w");
        se_test::AddSubLevel(*pWorldT, "a.sesc",
                        FlatBuffers::Vec3(10, 0, 0), FlatBuffers::Vec3(0, 0, 0));

        EXPECT_THROW(BuildInline(*pWorldT), std::runtime_error);
}

TEST(WorldAssetTest, NonPositiveCellSizeThrows) {

        auto pWorldT = se_test::MakeWorld("w");
        pWorldT->streaming = std::make_unique<FlatBuffers::StreamingConfigT>();
        pWorldT->streaming->cell_size = 0.f;
        se_test::AddSubLevel(*pWorldT, "a.sesc",
                        FlatBuffers::Vec3(0, 0, 0), FlatBuffers::Vec3(1, 1, 1));

        EXPECT_THROW(BuildInline(*pWorldT), std::runtime_error);
}

// ---------------------------------------------------------------------------
// Cell membership (baked bounds -> cell grid, computed at load)
// ---------------------------------------------------------------------------

TEST(WorldAssetTest, CellForPos) {

        auto pWorldT = se_test::MakeWorld("w");
        pWorldT->streaming = std::make_unique<FlatBuffers::StreamingConfigT>();
        pWorldT->streaming->cell_size = 32.f;
        se_test::AddSubLevel(*pWorldT, "a.sesc",
                        FlatBuffers::Vec3(0, 0, 0), FlatBuffers::Vec3(1, 1, 1));

        auto pAsset = BuildInline(*pWorldT);

        int32_t cx = 0, cy = 0;
        pAsset->CellForPos(glm::vec3(0.f, 0.f, 0.f), cx, cy);
        EXPECT_EQ(cx, 0); EXPECT_EQ(cy, 0);

        pAsset->CellForPos(glm::vec3(33.f, 0.f, -1.f), cx, cy);
        EXPECT_EQ(cx, 1); EXPECT_EQ(cy, -1);
}

TEST(WorldAssetTest, LevelsInCell_MembershipFromBounds) {

        // cell_size 16: level A covers x/z [0..15]  -> cell (0,0)
        //               level B covers x[32..33] z[0..1] -> cell (2,0)
        // (bounds are closed: a max exactly on a boundary touches the next cell)
        auto pWorldT = se_test::MakeWorld("w");
        pWorldT->streaming = std::make_unique<FlatBuffers::StreamingConfigT>();
        pWorldT->streaming->cell_size = 16.f;
        se_test::AddSubLevel(*pWorldT, "a.sesc",
                        FlatBuffers::Vec3(0, 0, 0), FlatBuffers::Vec3(15, 1, 15));
        se_test::AddSubLevel(*pWorldT, "b.sesc",
                        FlatBuffers::Vec3(32, 0, 0), FlatBuffers::Vec3(33, 1, 1));

        auto pAsset = BuildInline(*pWorldT);

        std::vector<uint32_t> vOut;

        EXPECT_TRUE(pAsset->LevelsInCell(0, 0, vOut));
        ASSERT_EQ(vOut.size(), 1u);
        EXPECT_EQ(vOut[0], 0u);

        EXPECT_TRUE(pAsset->LevelsInCell(2, 0, vOut));
        ASSERT_EQ(vOut.size(), 1u);
        EXPECT_EQ(vOut[0], 1u);

        EXPECT_FALSE(pAsset->LevelsInCell(1, 1, vOut));
        EXPECT_TRUE(vOut.empty());
}

TEST(WorldAssetTest, LevelsInCell_LargeLevelSpansMultipleCells) {

        // cell_size 8; a level covering x/z [0..24] overlaps cells (0..3) on both axes
        auto pWorldT = se_test::MakeWorld("w");
        pWorldT->streaming = std::make_unique<FlatBuffers::StreamingConfigT>();
        pWorldT->streaming->cell_size = 8.f;
        se_test::AddSubLevel(*pWorldT, "big.sesc",
                        FlatBuffers::Vec3(0, 0, 0), FlatBuffers::Vec3(24, 1, 24));

        auto pAsset = BuildInline(*pWorldT);

        std::vector<uint32_t> vOut;
        for (int32_t cz = 0; cz <= 2; ++cz) {
                for (int32_t cx = 0; cx <= 2; ++cx) {
                        ASSERT_TRUE(pAsset->LevelsInCell(cx, cz, vOut));
                        ASSERT_EQ(vOut.size(), 1u);
                        EXPECT_EQ(vOut[0], 0u);
                }
        }
}
