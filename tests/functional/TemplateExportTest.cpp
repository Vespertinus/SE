// ---------------------------------------------------------------------------
// TemplateExportTest — Tier C (Stage 3 Step 5): the .setm authoring path.
// Bakes a fixture .sesc (WorldAssets builders), extracts a subtree through the
// template_export extraction lib (linked directly — no engine SE_IMPL inside),
// then loads and instantiates the template through EntityTemplateSystem over
// the world_app composition. Proves: hierarchy/transform fidelity, name
// uniquification across instances (double instantiate under one parent), and
// the offline --flatten overlay.
// ---------------------------------------------------------------------------

#define SE_IMPL
#include <GlobalTypes.h>
#include "EngineFixture.h"
#include "WorldAssets.h"

// EntityTemplateSystem.tcc arrives via world_app/App.h impl sections (SE_IMPL).

#include <template_export/TemplateExporter.h>

#include <gtest/gtest.h>

#include <boost/filesystem.hpp>

#include <memory>
#include <string>
#include <vector>

using namespace SE;

namespace {

class TemplateExportTest : public se_test::FuncTestBase {

protected:
        void SetUp() override {
                FuncTestBase::SetUp();
                pScene = std::make_unique<TSceneTree>("template_export_scene", 0, true);
                oETS.SetSceneTree(pScene.get());
                boost::filesystem::create_directories("resource/world/gen");
        }

        // House subtree: root at (5,0,7) with Door (1,2,3) and Roof (0,3,0).
        FlatBuffers::NodeT MakeHouse(const std::string & sName) {
                auto oHouse = se_test::MakeNode(sName, glm::vec3(5.f, 0.f, 7.f));
                oHouse.children.push_back(std::make_unique<FlatBuffers::NodeT>(
                        se_test::MakeNode("Door", glm::vec3(1.f, 2.f, 3.f))));
                oHouse.children.push_back(std::make_unique<FlatBuffers::NodeT>(
                        se_test::MakeNode("Roof", glm::vec3(0.f, 3.f, 0.f))));
                return oHouse;
        }

        bool Export(const std::string & sScenePath,
                    const std::string & sNode,
                    const std::string & sId,
                    const std::string & sOut,
                    const std::string & sFlattenBase = {}) {
                se_template_export::SOptions o;
                o.sScenePath   = sScenePath;
                o.sNodeName    = sNode;
                o.sTemplateId  = sId;
                o.sOutPath     = sOut;
                o.sFlattenBase = sFlattenBase;
                return se_template_export::ExportSubtree(o);
        }

        std::unique_ptr<TSceneTree>     pScene;
        TEntityTemplateSystem           oETS;
};

} // namespace

// ---------------------------------------------------------------------------
// Round trip: .sesc subtree → .setm → Instantiate matches the authored subtree.
// ---------------------------------------------------------------------------

TEST_F(TemplateExportTest, ExportRoundTripMatchesAuthoredSubtree) {
        const std::string sScene = "resource/world/gen/template_export_scene.sesc";

        auto oRoot = se_test::MakeNode("WorldRoot");
        oRoot.children.push_back(std::make_unique<FlatBuffers::NodeT>(MakeHouse("House")));
        oRoot.children.push_back(std::make_unique<FlatBuffers::NodeT>(
                se_test::MakeNode("Tree")));
        ASSERT_TRUE(se_test::WriteBytes(sScene, se_test::BakeScene(std::move(oRoot))));

        EXPECT_TRUE(Export(sScene, "House", "house_tmpl",
                           "resource/world/gen/house_tmpl.setm"));

        // load through the production file path
        ASSERT_EQ(oETS.Load("resource/world/gen/house_tmpl.setm"), uSUCCESS);
        EXPECT_TRUE(oETS.HasTemplate("house_tmpl"));

        // the caller transform overrides the ROOT (Instantiate contract); children
        // keep their authored values — the authored root lands via the override here.
        auto pHouse = oETS.Instantiate("house_tmpl", pScene->GetRoot(),
                                       glm::vec3(5.f, 0.f, 7.f));
        ASSERT_FALSE(pHouse.expired());
        EXPECT_EQ(pHouse.lock()->GetName(), "House");
        EXPECT_NEAR(pHouse.lock()->GetTransform().GetPos().x, 5.f, 0.001f);
        EXPECT_NEAR(pHouse.lock()->GetTransform().GetPos().z, 7.f, 0.001f);

        auto pDoor = pHouse.lock()->FindChild(StrID("Door"));
        ASSERT_NE(pDoor, nullptr);
        EXPECT_NEAR(pDoor->GetTransform().GetPos().x, 1.f, 0.001f);
        EXPECT_NEAR(pDoor->GetTransform().GetPos().y, 2.f, 0.001f);

        // the sibling ("Tree") must NOT be part of the template
        EXPECT_EQ(pHouse.lock()->FindChild(StrID("Tree")), nullptr);
}

// ---------------------------------------------------------------------------
// Double instantiation under one parent: uniquified names, both hierarchies
// intact (the Part-A guarantee the old prototype failed).
// ---------------------------------------------------------------------------

TEST_F(TemplateExportTest, DoubleInstantiateUniquifiesNames) {
        const std::string sScene = "resource/world/gen/template_export_scene2.sesc";

        auto oRoot = se_test::MakeNode("WorldRoot");
        oRoot.children.push_back(std::make_unique<FlatBuffers::NodeT>(MakeHouse("House")));
        ASSERT_TRUE(se_test::WriteBytes(sScene, se_test::BakeScene(std::move(oRoot))));
        ASSERT_TRUE(Export(sScene, "House", "dup_house", "resource/world/gen/dup_house.setm"));
        ASSERT_EQ(oETS.Load("resource/world/gen/dup_house.setm"), uSUCCESS);
        ASSERT_TRUE(oETS.HasTemplate("dup_house"));

        auto pH1 = oETS.Instantiate("dup_house", pScene->GetRoot());
        auto pH2 = oETS.Instantiate("dup_house", pScene->GetRoot());
        ASSERT_FALSE(pH1.expired());
        ASSERT_FALSE(pH2.expired());
        EXPECT_NE(pH1.lock()->GetName(), pH2.lock()->GetName());
        EXPECT_NE(pH2.lock()->FindChild(StrID("Door")), nullptr);
        EXPECT_NE(pH1.lock()->FindChild(StrID("Door")), nullptr);

        // exactly one House and one suffixed instance under the root
        EXPECT_EQ(pScene->GetRoot()->FindChild(StrID("House")), pH1.lock());
        EXPECT_EQ(pScene->GetRoot()->FindChild(StrID("House_1")), pH2.lock());
}

// ---------------------------------------------------------------------------
// --flatten: the export overlays same-named base nodes offline; the result is
// a standalone template (no base_id, no runtime merge).
// ---------------------------------------------------------------------------

TEST_F(TemplateExportTest, FlattenMergesOverBaseOffline) {
        // base template: House with Door at origin
        {
                flatbuffers::FlatBufferBuilder fbb;
                FlatBuffers::EntityTemplateT oTmpl;
                oTmpl.id   = "house_base";
                oTmpl.root = std::make_unique<FlatBuffers::NodeTemplateT>();
                oTmpl.root->name = "House";
                oTmpl.root->enabled = true;
                auto pDoor = std::make_unique<FlatBuffers::NodeTemplateT>();
                pDoor->name = "Door";
                oTmpl.root->children.push_back(std::move(pDoor));
                auto offset = FlatBuffers::EntityTemplate::Pack(fbb, &oTmpl);
                FlatBuffers::FinishEntityTemplateBuffer(fbb, offset);
                ASSERT_TRUE(se_test::WriteBytes("resource/world/gen/house_base.setm",
                                std::vector<uint8_t>(fbb.GetBufferPointer(),
                                                     fbb.GetBufferPointer() + fbb.GetSize())));
        }

        // exported subtree: House2 with Door at (2,2,2) — overlays the base Door
        const std::string sScene = "resource/world/gen/template_export_scene3.sesc";
        {
                auto oHouse = MakeHouse("House2");
                oHouse.children[0]->translation =
                                std::make_unique<FlatBuffers::Vec3>(2.f, 2.f, 2.f);
                auto oRoot = se_test::MakeNode("WorldRoot");
                oRoot.children.push_back(std::make_unique<FlatBuffers::NodeT>(
                                std::move(oHouse)));
                ASSERT_TRUE(se_test::WriteBytes(sScene, se_test::BakeScene(std::move(oRoot))));
        }

        EXPECT_TRUE(Export(sScene, "House2", "house_flattened",
                           "resource/world/gen/house_flattened.setm",
                           /*flatten*/"resource/world/gen/house_base.setm"));

        ASSERT_EQ(oETS.Load("resource/world/gen/house_flattened.setm"), uSUCCESS);
        ASSERT_TRUE(oETS.HasTemplate("house_flattened"));
        auto pHouse = oETS.Instantiate("house_flattened", pScene->GetRoot());
        ASSERT_FALSE(pHouse.expired());
        EXPECT_EQ(pHouse.lock()->GetName(), "House2");          // overlay root wins

        auto pDoor = pHouse.lock()->FindChild(StrID("Door"));
        ASSERT_NE(pDoor, nullptr);
        EXPECT_NEAR(pDoor->GetTransform().GetPos().x, 2.f, 0.001f);  // overlay transform
}
