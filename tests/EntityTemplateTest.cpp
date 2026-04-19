
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <loki/Singleton.h>

#define SE_IMPL
#include <Logging.h>
#include <Global.h>
#include <ErrCode.h>
#include <StrID.h>
#include <ResourceHolder.h>
#include <Engine.h>
#include <DebugRendererMock.h>
#include <EventManager.h>
#include <EventManager.tcc>
#include <CommonEvents.h>
#include <ResourceHandle.h>

namespace SE {

class EntityManager;
template <class TSystem> TSystem & GetSystem();

using DebugRenderer = DebugRendererMock;
using EngineBase = Engine<DebugRenderer, EventManager, EntityManager>;
using TEngine = typename Loki::SingletonHolder<
        EngineBase,
        Loki::CreateUsingNew,
        Loki::DeletableSingleton>;

template <class TSystem> TSystem & GetSystem() {
        return TEngine::Instance().Get<TSystem>();
}

template <class Resource, class ... TConcreateSettings>
H<Resource> CreateResource(const std::string & sPath, const TConcreateSettings & ...) {
        return H<Resource>::Null();
}

} // namespace SE

#include <SceneTree.h>

namespace SE {
class StaticModelMock;
using TSceneTree = SceneTree<StaticModelMock>;
} // namespace SE

#include <StaticModelMock.h>

#include <EntityManager.h>
#include <EntityManager.tcc>
#include <EntityTemplateSystem.h>
#include <EntityTemplateSystem.tcc>

#include <flatbuffers/flatbuffers.h>
#include <flatbuffers/flexbuffers.h>
#include <EntityTemplate_generated.h>

using ::testing::NiceMock;
using ::testing::Return;

// ---------------------------------------------------------------------------
// FlatBuffer building helpers
// ---------------------------------------------------------------------------

static std::vector<uint8_t> BuildMinimalTemplate(
                const std::string & sId,
                const std::string & sRootName,
                float tx = 0.f, float ty = 0.f, float tz = 0.f) {

        flatbuffers::FlatBufferBuilder fbb;

        SE::FlatBuffers::EntityTemplateT oTmpl;
        oTmpl.id = sId;
        oTmpl.root = std::make_unique<SE::FlatBuffers::NodeTemplateT>();
        oTmpl.root->name = sRootName;
        oTmpl.root->enabled = true;
        oTmpl.root->translation = std::make_unique<SE::FlatBuffers::Vec3>(tx, ty, tz);

        auto oOffset = SE::FlatBuffers::EntityTemplate::Pack(fbb, &oTmpl);
        SE::FlatBuffers::FinishEntityTemplateBuffer(fbb, oOffset);

        auto * pBuf = fbb.GetBufferPointer();
        return std::vector<uint8_t>(pBuf, pBuf + fbb.GetSize());
}

static std::vector<uint8_t> BuildTemplateWithChild(
                const std::string & sId,
                const std::string & sRootName,
                const std::string & sChildName,
                float child_tx = 5.f, float child_ty = 6.f, float child_tz = 7.f) {

        flatbuffers::FlatBufferBuilder fbb;

        SE::FlatBuffers::EntityTemplateT oTmpl;
        oTmpl.id   = sId;
        oTmpl.root = std::make_unique<SE::FlatBuffers::NodeTemplateT>();
        oTmpl.root->name    = sRootName;
        oTmpl.root->enabled = true;

        auto pChild = std::make_unique<SE::FlatBuffers::NodeTemplateT>();
        pChild->name    = sChildName;
        pChild->enabled = true;
        pChild->translation = std::make_unique<SE::FlatBuffers::Vec3>(child_tx, child_ty, child_tz);
        oTmpl.root->children.push_back(std::move(pChild));

        auto oOffset = SE::FlatBuffers::EntityTemplate::Pack(fbb, &oTmpl);
        SE::FlatBuffers::FinishEntityTemplateBuffer(fbb, oOffset);

        auto * pBuf = fbb.GetBufferPointer();
        return std::vector<uint8_t>(pBuf, pBuf + fbb.GetSize());
}

static std::vector<uint8_t> BuildVariantWithNodeOverride(
                const std::string & sId,
                const std::string & sBaseId,
                const std::string & sTargetNode,
                float new_tx, float new_ty, float new_tz) {

        flatbuffers::FlatBufferBuilder fbb;

        SE::FlatBuffers::EntityTemplateT oTmpl;
        oTmpl.id      = sId;
        oTmpl.base_id = sBaseId;
        oTmpl.root    = std::make_unique<SE::FlatBuffers::NodeTemplateT>();
        oTmpl.root->name = sId + "_root"; // placeholder; overridden from base

        auto pOvr = std::make_unique<SE::FlatBuffers::NodeOverrideT>();
        pOvr->node_name       = sTargetNode;
        pOvr->has_translation = true;
        pOvr->translation     = std::make_unique<SE::FlatBuffers::Vec3>(new_tx, new_ty, new_tz);
        oTmpl.overrides.push_back(std::move(pOvr));

        auto oOffset = SE::FlatBuffers::EntityTemplate::Pack(fbb, &oTmpl);
        SE::FlatBuffers::FinishEntityTemplateBuffer(fbb, oOffset);

        auto * pBuf = fbb.GetBufferPointer();
        return std::vector<uint8_t>(pBuf, pBuf + fbb.GetSize());
}

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------

namespace SE {
        using TEntityTemplateSystem = EntityTemplateSystem<StaticModelMock>;
} // namespace SE

class EntityTemplateTest : public ::testing::Test {
        protected:
                EntityTemplateTest() {
                        SE::TEngine::Instance().Init();
                        pScene = std::make_unique<SE::TSceneTree>("test_scene", 0, /*empty=*/true);
                        oETS.SetSceneTree(pScene.get());
                }

                ~EntityTemplateTest() noexcept {
                        Loki::DeletableSingleton<SE::EngineBase>::GracefulDelete();
                }

                std::unique_ptr<SE::TSceneTree> pScene;
                SE::TEntityTemplateSystem        oETS;
};

// ===========================================================================
// 1. LoadFromMemory with invalid bytes → error
// ===========================================================================

TEST_F(EntityTemplateTest, LoadFromMemoryInvalidBytesReturnsError) {
        const uint8_t garbage[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x00, 0x00, 0x00 };
        EXPECT_NE(oETS.LoadFromMemory(garbage, sizeof(garbage), "garbage"), SE::uSUCCESS);
}

// ===========================================================================
// 2. HasTemplate — false before load, true after
// ===========================================================================

TEST_F(EntityTemplateTest, HasTemplateFalseBeforeLoad) {
        EXPECT_FALSE(oETS.HasTemplate("soldier"));
}

TEST_F(EntityTemplateTest, HasTemplateTrueAfterLoad) {
        auto vBuf = BuildMinimalTemplate("soldier", "root");
        ASSERT_EQ(oETS.LoadFromMemory(vBuf.data(), vBuf.size()), SE::uSUCCESS);
        EXPECT_TRUE(oETS.HasTemplate("soldier"));
}

// ===========================================================================
// 3. Instantiate unknown id → expired handle
// ===========================================================================

TEST_F(EntityTemplateTest, InstantiateUnknownIdReturnsExpired) {
        auto pHandle = oETS.Instantiate("nonexistent");
        EXPECT_TRUE(pHandle.expired());
}

// ===========================================================================
// 4. Instantiate creates SceneNode with the NodeTemplate root name
// ===========================================================================

TEST_F(EntityTemplateTest, InstantiateCreatesNodeWithTemplateName) {

        auto vBuf = BuildMinimalTemplate("archer", "ArcherRoot");
        ASSERT_EQ(oETS.LoadFromMemory(vBuf.data(), vBuf.size()), SE::uSUCCESS);

        auto pHandle = oETS.Instantiate("archer");
        ASSERT_FALSE(pHandle.expired());

        auto pNode = pHandle.lock();
        ASSERT_NE(pNode, nullptr);
        EXPECT_EQ(pNode->GetName(), std::string("ArcherRoot"));
}

// ===========================================================================
// 5. Caller transform overrides root authored transform
// ===========================================================================

TEST_F(EntityTemplateTest, CallerTransformOverridesRootAuthored) {
        // Build template with authored translation (1,2,3)
        auto vBuf = BuildMinimalTemplate("box", "Box", 1.f, 2.f, 3.f);
        ASSERT_EQ(oETS.LoadFromMemory(vBuf.data(), vBuf.size()), SE::uSUCCESS);

        glm::vec3 vOverride(10.f, 20.f, 30.f);
        auto pHandle = oETS.Instantiate("box", {}, vOverride);
        ASSERT_FALSE(pHandle.expired());

        auto pNode = pHandle.lock();
        ASSERT_NE(pNode, nullptr);
        auto vPos = pNode->GetTransform().GetPos();
        EXPECT_FLOAT_EQ(vPos.x, 10.f);
        EXPECT_FLOAT_EQ(vPos.y, 20.f);
        EXPECT_FLOAT_EQ(vPos.z, 30.f);
}

// ===========================================================================
// 6. Child nodes use authored transform
// ===========================================================================

TEST_F(EntityTemplateTest, ChildNodesUseAuthoredTransform) {
        auto vBuf = BuildTemplateWithChild("parent_tmpl", "ParentRoot", "Child", 5.f, 6.f, 7.f);
        ASSERT_EQ(oETS.LoadFromMemory(vBuf.data(), vBuf.size()), SE::uSUCCESS);

        // Caller overrides root position to something different
        auto pHandle = oETS.Instantiate("parent_tmpl", {}, glm::vec3(100.f, 0.f, 0.f));
        ASSERT_FALSE(pHandle.expired());

        auto pRoot = pHandle.lock();
        ASSERT_NE(pRoot, nullptr);

        auto pChild = pRoot->FindChild(SE::StrID("Child"));
        ASSERT_NE(pChild, nullptr);

        auto vChildPos = pChild->GetTransform().GetPos();
        EXPECT_FLOAT_EQ(vChildPos.x, 5.f);
        EXPECT_FLOAT_EQ(vChildPos.y, 6.f);
        EXPECT_FLOAT_EQ(vChildPos.z, 7.f);
}

// ===========================================================================
// 7. fnInitializer called after hierarchy is built
// ===========================================================================

TEST_F(EntityTemplateTest, InitializerCalledAfterHierarchyBuilt) {
        auto vBuf = BuildTemplateWithChild("init_test", "Root", "Child");
        ASSERT_EQ(oETS.LoadFromMemory(vBuf.data(), vBuf.size()), SE::uSUCCESS);

        bool called = false;
        SE::TSceneTree::TSceneNodeExact * pCaptured = nullptr;

        auto pHandle = oETS.Instantiate("init_test", {}, {}, {}, {},
                        /*enabled=*/true,
                        [&called, &pCaptured](SE::TSceneTree::TSceneNodeExact & oNode) {
                        called   = true;
                        pCaptured = &oNode;
                        // Child must already exist when initializer fires
                        EXPECT_NE(oNode.FindChild(SE::StrID("Child")), nullptr);
                        });

        EXPECT_TRUE(called);
        ASSERT_FALSE(pHandle.expired());
        EXPECT_EQ(pCaptured, pHandle.lock().get());
}

// ===========================================================================
// 8. NodeOverride transform applied to named node in merged variant
// ===========================================================================

TEST_F(EntityTemplateTest, NodeOverrideTransformAppliedToChild) {
        // Base template: root "Base" with child "Wing"
        auto vBase = BuildTemplateWithChild("base_ship", "Base", "Wing", 1.f, 0.f, 0.f);
        ASSERT_EQ(oETS.LoadFromMemory(vBase.data(), vBase.size()), SE::uSUCCESS);

        // Variant overrides "Wing" translation to (99, 0, 0)
        auto vVariant = BuildVariantWithNodeOverride("heavy_ship", "base_ship", "Wing", 99.f, 0.f, 0.f);
        ASSERT_EQ(oETS.LoadFromMemory(vVariant.data(), vVariant.size()), SE::uSUCCESS);

        auto pHandle = oETS.Instantiate("heavy_ship");
        ASSERT_FALSE(pHandle.expired());

        auto pRoot = pHandle.lock();
        ASSERT_NE(pRoot, nullptr);
        auto pWing = pRoot->FindChild(SE::StrID("Wing"));
        ASSERT_NE(pWing, nullptr);

        EXPECT_FLOAT_EQ(pWing->GetTransform().GetPos().x, 99.f);
}

// ===========================================================================
// 9. base_id not found → error, expired handle (+ HasTemplate false)
// ===========================================================================

TEST_F(EntityTemplateTest, BaseIdNotFoundReturnsError) {
        // Variant that references a base that has NOT been loaded
        auto vVariant = BuildVariantWithNodeOverride("orphan_variant", "missing_base",
                        "AnyNode", 0.f, 0.f, 0.f);
        auto res = oETS.LoadFromMemory(vVariant.data(), vVariant.size());
        EXPECT_NE(res, SE::uSUCCESS);
        EXPECT_FALSE(oETS.HasTemplate("orphan_variant"));
}

// ===========================================================================
// 10. EntityManager::FlushQueues routes prefab_id to the functor
// ===========================================================================

TEST_F(EntityTemplateTest, EntityManagerFlushQueueRoutesToFunctor) {
        SE::TEngine::Instance().Init();
        SE::GetSystem<SE::EntityManager>().SetSceneTree(pScene.get());

        bool functor_called = false;
        SE::GetSystem<SE::EntityManager>().SetTemplateInstantiator(
                        [&functor_called](const SE::SpawnIntent & oIntent) ->
                        SE::TSceneTree::TSceneNodeWeak {
                                EXPECT_EQ(oIntent.prefab_id, "test_prefab");
                                functor_called = true;
                                return {};
                        });

        SE::SpawnIntent oIntent;
        oIntent.prefab_id = "test_prefab";
        SE::GetSystem<SE::EntityManager>().RequestSpawn(oIntent);

        SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EPostUpdate{0.016f});
        EXPECT_TRUE(functor_called);
}

// ===========================================================================
// 11. No functor set → warning, no crash
// ===========================================================================

TEST_F(EntityTemplateTest, NoFunctorSetNoCrash) {
        SE::GetSystem<SE::EntityManager>().SetSceneTree(pScene.get());
        SE::GetSystem<SE::EntityManager>().SetTemplateInstantiator({});

        SE::SpawnIntent oIntent;
        oIntent.prefab_id = "some_template";
        SE::GetSystem<SE::EntityManager>().RequestSpawn(oIntent);

        EXPECT_NO_FATAL_FAILURE(
                        SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EPostUpdate{0.016f}));
}

// ===========================================================================
// 12. UnloadAll clears all registered templates
// ===========================================================================

TEST_F(EntityTemplateTest, UnloadAllClearsTemplates) {
        auto vBuf = BuildMinimalTemplate("temp_id", "TempRoot");
        ASSERT_EQ(oETS.LoadFromMemory(vBuf.data(), vBuf.size()), SE::uSUCCESS);
        ASSERT_TRUE(oETS.HasTemplate("temp_id"));

        oETS.UnloadAll();
        EXPECT_FALSE(oETS.HasTemplate("temp_id"));

        // Instantiate after unload must return expired
        auto pHandle = oETS.Instantiate("temp_id");
        EXPECT_TRUE(pHandle.expired());
}
