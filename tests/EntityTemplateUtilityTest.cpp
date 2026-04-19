
#include <gtest/gtest.h>

#define SE_IMPL
#include <Logging.h>
#include <ErrCode.h>

#include <EntityTemplateUtility.h>
#include <EntityTemplateUtility.tcc>

// ===========================================================================
// ParsePath tests
// ===========================================================================

TEST(EntityTemplateUtilityTest, ParsePathSimpleField) {
        auto segs = SE::EntityTemplateUtility::ParsePath("StaticModel.mesh");
        ASSERT_EQ(segs.size(), 2u);
        EXPECT_EQ(segs[0].name, "StaticModel");
        EXPECT_EQ(segs[0].index, -1);
        EXPECT_TRUE(segs[0].key.empty());
        EXPECT_EQ(segs[1].name, "mesh");
}

TEST(EntityTemplateUtilityTest, ParsePathNumericIndex) {
        auto segs = SE::EntityTemplateUtility::ParsePath("StaticModel.materials[2].path");
        ASSERT_EQ(segs.size(), 3u);
        EXPECT_EQ(segs[1].name, "materials");
        EXPECT_EQ(segs[1].index, 2);
        EXPECT_TRUE(segs[1].key.empty());
        EXPECT_EQ(segs[2].name, "path");
}

TEST(EntityTemplateUtilityTest, ParsePathNameKey) {
        auto segs = SE::EntityTemplateUtility::ParsePath(
                        "StaticModel.materials[0].material.variables[Color].float_val");
        ASSERT_EQ(segs.size(), 5u);
        EXPECT_EQ(segs[3].name, "variables");
        EXPECT_EQ(segs[3].index, -1);
        EXPECT_EQ(segs[3].key, "Color");
        EXPECT_EQ(segs[4].name, "float_val");
}

TEST(EntityTemplateUtilityTest, ParsePathSingleSegment) {
        auto segs = SE::EntityTemplateUtility::ParsePath("StaticModel");
        ASSERT_EQ(segs.size(), 1u);
        EXPECT_EQ(segs[0].name, "StaticModel");
}

TEST(EntityTemplateUtilityTest, ParsePathEmptyString) {
        auto segs = SE::EntityTemplateUtility::ParsePath("");
        EXPECT_TRUE(segs.empty());
}

// ===========================================================================
// ComponentNameToEnum tests
// ===========================================================================

TEST(EntityTemplateUtilityTest, ComponentNameToEnumKnown) {
        using CU = SE::FlatBuffers::ComponentU;
        EXPECT_EQ(SE::EntityTemplateUtility::ComponentNameToEnum("StaticModel"),   CU::StaticModel);
        EXPECT_EQ(SE::EntityTemplateUtility::ComponentNameToEnum("RigidBody"),     CU::RigidBody);
        EXPECT_EQ(SE::EntityTemplateUtility::ComponentNameToEnum("AnimatedModel"), CU::AnimatedModel);
        EXPECT_EQ(SE::EntityTemplateUtility::ComponentNameToEnum("Animator"),      CU::Animator);
}

TEST(EntityTemplateUtilityTest, ComponentNameToEnumUnknownReturnsNone) {
        EXPECT_EQ(SE::EntityTemplateUtility::ComponentNameToEnum("Camera"),
                        SE::FlatBuffers::ComponentU::NONE);
        EXPECT_EQ(SE::EntityTemplateUtility::ComponentNameToEnum(""),
                        SE::FlatBuffers::ComponentU::NONE);
}
