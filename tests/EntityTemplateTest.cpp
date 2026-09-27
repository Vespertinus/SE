
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

// ===========================================================================
// Step 5 additions: test plugin for ApplyField dispatch coverage.
// MergeVariant runs at LOAD time — these statics capture the applied values.
// ===========================================================================

namespace SE {
template <>
struct EntityTemplatePlugin<StaticModelMock> {
        static float       sLastFloat;
        static std::string sLastPath;

        static void ApplyField(FlatBuffers::StaticModelT & /*obj*/,
                        std::string_view path,
                        const FlatBuffers::FieldOverride & fo) {
                sLastPath = std::string(path);
                if (fo.value_type() == FlatBuffers::FieldValueU::Float && fo.value_as_Float()) {
                        sLastFloat = fo.value_as_Float()->value();
                }
        }
};
float       EntityTemplatePlugin<StaticModelMock>::sLastFloat = 0.f;
std::string EntityTemplatePlugin<StaticModelMock>::sLastPath;
} // namespace SE

// Builder: template whose root carries a StaticModel component (for field overrides).
static std::vector<uint8_t> BuildTemplateWithStaticModel(
                const std::string & sId, const std::string & sRootName) {

        flatbuffers::FlatBufferBuilder fbb;

        SE::FlatBuffers::EntityTemplateT oTmpl;
        oTmpl.id   = sId;
        oTmpl.root = std::make_unique<SE::FlatBuffers::NodeTemplateT>();
        oTmpl.root->name    = sRootName;
        oTmpl.root->enabled = true;

        auto pComp = std::make_unique<SE::FlatBuffers::ComponentT>();
        auto pSM   = std::make_unique<SE::FlatBuffers::StaticModelT>();
        pSM->mesh  = std::make_unique<SE::FlatBuffers::MeshHolderT>();
        pSM->mesh->path = "test_mesh";
        pComp->component.Set(std::move(*pSM));
        oTmpl.root->components.push_back(std::move(pComp));

        auto oOffset = SE::FlatBuffers::EntityTemplate::Pack(fbb, &oTmpl);
        SE::FlatBuffers::FinishEntityTemplateBuffer(fbb, oOffset);

        auto * pBuf = fbb.GetBufferPointer();
        return std::vector<uint8_t>(pBuf, pBuf + fbb.GetSize());
}

// Builder: root + child carrying a RigidBody component — RigidBody is NOT in this
// binary's component pack, so instantiation fails mid-hierarchy (rollback case).
static std::vector<uint8_t> BuildTemplateWithUnknownComponent(
                const std::string & sId,
                const std::string & sRootName,
                const std::string & sChildName) {

        flatbuffers::FlatBufferBuilder fbb;

        SE::FlatBuffers::EntityTemplateT oTmpl;
        oTmpl.id   = sId;
        oTmpl.root = std::make_unique<SE::FlatBuffers::NodeTemplateT>();
        oTmpl.root->name    = sRootName;
        oTmpl.root->enabled = true;

        auto pChild = std::make_unique<SE::FlatBuffers::NodeTemplateT>();
        pChild->name    = sChildName;
        pChild->enabled = true;
        auto pComp = std::make_unique<SE::FlatBuffers::ComponentT>();
        auto pRB   = std::make_unique<SE::FlatBuffers::RigidBodyT>();
        auto pSphere = std::make_unique<SE::FlatBuffers::SphereColliderT>();
        pRB->collider.Set(std::move(*pSphere));
        pComp->component.Set(std::move(*pRB));
        pChild->components.push_back(std::move(pComp));
        oTmpl.root->children.push_back(std::move(pChild));

        auto oOffset = SE::FlatBuffers::EntityTemplate::Pack(fbb, &oTmpl);
        SE::FlatBuffers::FinishEntityTemplateBuffer(fbb, oOffset);

        auto * pBuf = fbb.GetBufferPointer();
        return std::vector<uint8_t>(pBuf, pBuf + fbb.GetSize());
}

// Builder: template with authoring tags.
static std::vector<uint8_t> BuildTaggedTemplate(
                const std::string & sId, const std::string & sRootName,
                const std::vector<std::string> & vTags) {

        flatbuffers::FlatBufferBuilder fbb;

        SE::FlatBuffers::EntityTemplateT oTmpl;
        oTmpl.id   = sId;
        oTmpl.root = std::make_unique<SE::FlatBuffers::NodeTemplateT>();
        oTmpl.root->name    = sRootName;
        oTmpl.root->enabled = true;
        oTmpl.tags.insert(oTmpl.tags.end(), vTags.begin(), vTags.end());

        auto oOffset = SE::FlatBuffers::EntityTemplate::Pack(fbb, &oTmpl);
        SE::FlatBuffers::FinishEntityTemplateBuffer(fbb, oOffset);

        auto * pBuf = fbb.GetBufferPointer();
        return std::vector<uint8_t>(pBuf, pBuf + fbb.GetSize());
}

// ===========================================================================
// 13. Instantiating the same template twice under one parent uniquifies names
//     (pre-Step-5 this FAILED: Create rejects duplicate full names).
// ===========================================================================

TEST_F(EntityTemplateTest, DoubleInstantiateSameParentUniquifies) {
        auto vBuf = BuildMinimalTemplate("dup_tmpl", "Warrior");
        ASSERT_EQ(oETS.LoadFromMemory(vBuf.data(), vBuf.size()), SE::uSUCCESS);

        auto h1 = oETS.Instantiate("dup_tmpl", pScene->GetRoot());
        auto h2 = oETS.Instantiate("dup_tmpl", pScene->GetRoot());
        ASSERT_FALSE(h1.expired());
        ASSERT_FALSE(h2.expired());

        const std::string s1 = h1.lock()->GetName();
        const std::string s2 = h2.lock()->GetName();
        EXPECT_NE(s1, s2);
        EXPECT_NE(pScene->GetRoot()->FindChild(SE::StrID(s2)), nullptr);
        EXPECT_NE(pScene->GetRoot()->FindChild(SE::StrID(s1)), nullptr);
}

// ===========================================================================
// 14. A failure mid-hierarchy rolls back — no orphan subtrees, no stale names.
// ===========================================================================

TEST_F(EntityTemplateTest, FailedInstantiateRollsBackPartialHierarchy) {
        auto vBuf = BuildTemplateWithUnknownComponent("bad_tmpl", "Base", "Gear");
        ASSERT_EQ(oETS.LoadFromMemory(vBuf.data(), vBuf.size()), SE::uSUCCESS);

        auto pHandle = oETS.Instantiate("bad_tmpl", pScene->GetRoot());
        EXPECT_TRUE(pHandle.expired());

        // Neither the root nor the (failed) child may remain attached or findable.
        EXPECT_EQ(pScene->GetRoot()->FindChild(SE::StrID("Base")), nullptr);
        EXPECT_EQ(pScene->GetRoot()->FindChild(SE::StrID("Gear")), nullptr);
        EXPECT_EQ(pScene->FindFullName(SE::StrID("root|Base")), nullptr);

        // And the parent is otherwise unaffected: a good template still instantiates.
        auto vGood = BuildMinimalTemplate("good_tmpl", "Ok");
        ASSERT_EQ(oETS.LoadFromMemory(vGood.data(), vGood.size()), SE::uSUCCESS);
        EXPECT_FALSE(oETS.Instantiate("good_tmpl", pScene->GetRoot()).expired());
}

// ===========================================================================
// 15. Re-loading an id REPLACES the template.
// ===========================================================================

TEST_F(EntityTemplateTest, DuplicateIdReplaces) {
        auto v1 = BuildMinimalTemplate("replace_me", "Version1", 1.f, 0.f, 0.f);
        auto v2 = BuildMinimalTemplate("replace_me", "Version2", 2.f, 0.f, 0.f);
        ASSERT_EQ(oETS.LoadFromMemory(v1.data(), v1.size()), SE::uSUCCESS);
        ASSERT_EQ(oETS.LoadFromMemory(v2.data(), v2.size()), SE::uSUCCESS);

        EXPECT_EQ(oETS.TemplateCount(), 1u);

        auto pHandle = oETS.Instantiate("replace_me", pScene->GetRoot());
        ASSERT_FALSE(pHandle.expired());
        EXPECT_EQ(pHandle.lock()->GetName(), "Version2");
}

// ===========================================================================
// 16. Per-template unload; already-merged variants keep working.
// ===========================================================================

TEST_F(EntityTemplateTest, UnloadSingleTemplate) {
        auto vBase = BuildTemplateWithChild("unload_base", "Base", "Wing");
        auto vVar  = BuildVariantWithNodeOverride("unload_var", "unload_base", "Wing", 9.f, 9.f, 9.f);
        ASSERT_EQ(oETS.LoadFromMemory(vBase.data(), vBase.size()), SE::uSUCCESS);
        ASSERT_EQ(oETS.LoadFromMemory(vVar.data(), vVar.size()), SE::uSUCCESS);
        ASSERT_TRUE(oETS.HasTemplate("unload_var"));

        EXPECT_TRUE(oETS.Unload("unload_base"));
        EXPECT_FALSE(oETS.Unload("unload_base"));   // second unload: nothing to do
        EXPECT_FALSE(oETS.HasTemplate("unload_base"));
        EXPECT_TRUE(oETS.HasTemplate("unload_var"));

        // The merged variant instantiates fine without its base; the override
        // targets the child "Wing" (a caller transform override only stomps the root).
        auto pHandle = oETS.Instantiate("unload_var", pScene->GetRoot());
        ASSERT_FALSE(pHandle.expired());
        auto pWing = pHandle.lock()->FindChild(SE::StrID("Wing"));
        ASSERT_NE(pWing, nullptr);
        EXPECT_FLOAT_EQ(pWing->GetTransform().GetPos().x, 9.f);

        // Reloading the unloaded id starts fresh.
        ASSERT_EQ(oETS.LoadFromMemory(vBase.data(), vBase.size()), SE::uSUCCESS);
        EXPECT_TRUE(oETS.HasTemplate("unload_base"));
}

// ===========================================================================
// 17. Tags query + deterministic enumeration.
// ===========================================================================

TEST_F(EntityTemplateTest, HasTagAndSortedEnumeration) {
        auto vA = BuildTaggedTemplate("tag_c", "RootC", {"enemy", "ground"});
        auto vB = BuildTaggedTemplate("tag_a", "RootA", {});
        auto vD = BuildTaggedTemplate("tag_b", "RootB", {"enemy"});
        ASSERT_EQ(oETS.LoadFromMemory(vA.data(), vA.size()), SE::uSUCCESS);
        ASSERT_EQ(oETS.LoadFromMemory(vB.data(), vB.size()), SE::uSUCCESS);
        ASSERT_EQ(oETS.LoadFromMemory(vD.data(), vD.size()), SE::uSUCCESS);

        EXPECT_TRUE(oETS.HasTag("tag_c", "enemy"));
        EXPECT_TRUE(oETS.HasTag("tag_c", "ground"));
        EXPECT_FALSE(oETS.HasTag("tag_c", "water"));
        EXPECT_FALSE(oETS.HasTag("tag_a", "enemy"));
        EXPECT_FALSE(oETS.HasTag("missing", "enemy"));

        std::vector<std::string> vIds;
        oETS.ForEachTemplateId([&vIds](const std::string & sId) { vIds.push_back(sId); });
        ASSERT_EQ(vIds.size(), 3u);
        EXPECT_EQ(vIds[0], "tag_a");
        EXPECT_EQ(vIds[1], "tag_b");
        EXPECT_EQ(vIds[2], "tag_c");
}

// ===========================================================================
// 18. Field overrides dispatch to a component plugin (ApplyField).
// ===========================================================================

TEST_F(EntityTemplateTest, FieldOverrideDispatchesToPlugin) {
        // Base: root with a StaticModel component.
        auto vBase = BuildTemplateWithStaticModel("field_base", "FieldRoot");
        ASSERT_EQ(oETS.LoadFromMemory(vBase.data(), vBase.size()), SE::uSUCCESS);

        // Variant overriding a field via the plugin path.
        flatbuffers::FlatBufferBuilder fbb;
        SE::FlatBuffers::EntityTemplateT oTmpl;
        oTmpl.id      = "field_var";
        oTmpl.base_id = "field_base";
        oTmpl.root    = std::make_unique<SE::FlatBuffers::NodeTemplateT>();
        oTmpl.root->name = "field_var_root";

        auto pOvr = std::make_unique<SE::FlatBuffers::NodeOverrideT>();
        pOvr->node_name = "FieldRoot";
        auto pFO = std::make_unique<SE::FlatBuffers::FieldOverrideT>();
        pFO->path = "StaticModel.any_field";
        pFO->value.Set(SE::FlatBuffers::Float(42.5f));
        pOvr->field_overrides.push_back(std::move(pFO));
        oTmpl.overrides.push_back(std::move(pOvr));

        auto oOffset = SE::FlatBuffers::EntityTemplate::Pack(fbb, &oTmpl);
        SE::FlatBuffers::FinishEntityTemplateBuffer(fbb, oOffset);
        auto * pBuf = fbb.GetBufferPointer();
        ASSERT_EQ(oETS.LoadFromMemory(pBuf, fbb.GetSize()), SE::uSUCCESS);

        // Merge happens at load: the plugin saw the path and value.
        EXPECT_EQ(SE::EntityTemplatePlugin<SE::StaticModelMock>::sLastPath, "any_field");
        EXPECT_FLOAT_EQ(SE::EntityTemplatePlugin<SE::StaticModelMock>::sLastFloat, 42.5f);

        EXPECT_FALSE(oETS.Instantiate("field_var", pScene->GetRoot()).expired());
}
