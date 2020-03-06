
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <loki/Singleton.h>

#define SE_IMPL
#include <Logging.h>
#include <Global.h>
#include <ErrCode.h>
#include <StrID.h>
#include <Engine.h>
#include <ResourceHolder.h>

#include <DebugRendererMock.h>

#include <Transform.tcc>

using ::testing::NiceMock;

namespace SE {

template <class TSystem> TSystem & GetSystem();

using DebugRenderer = DebugRendererMock;
using EngineBase = Engine<DebugRenderer>;

using TEngine = typename Loki::SingletonHolder< EngineBase, Loki::CreateUsingNew, Loki::DeletableSingleton >;

template <class TSystem> TSystem & GetSystem() {

        return TEngine::Instance().Get<TSystem>();
}

}

#include <SceneTree.h>

class MockComponent1;
class MockComponent2;

using NiceMockComponent1 = NiceMock<MockComponent1>;

namespace SE {

class StaticModelMock;

using TSceneTree = SE::SceneTree<NiceMockComponent1, MockComponent2, StaticModelMock >;

}

#include <StaticModelMock.h>

class MockComponent1 {

        public:

        SE::TSceneTree::TSceneNodeExact   * pNode;

        MockComponent1(SE::TSceneTree::TSceneNodeExact * pNewNode);
        MOCK_METHOD0(DrawDebug, void());
        MOCK_METHOD0(Enable, void());
        MOCK_METHOD0(Disable, void());
};

MockComponent1::MockComponent1(SE::TSceneTree::TSceneNodeExact * pNewNode) : pNode(pNewNode) { ;; }

class MockComponent2 {

        public:

        SE::TSceneTree::TSceneNodeExact   * pNode;

        MockComponent2(SE::TSceneTree::TSceneNodeExact * pNewNode);
        MOCK_METHOD0(DrawDebug, void());
        MOCK_METHOD0(Enable, void());
        MOCK_METHOD0(Disable, void());
};

MockComponent2::MockComponent2(SE::TSceneTree::TSceneNodeExact * pNewNode) : pNode(pNewNode) { ;; }

/**
  TODO:
  - check node listeners
  - load SceneTree from file
  -- custom command for asset import from fbx
  - check node callbacks:
  -- *Walk
  -- ForEachComponent
  - check node flags
 */

class SceneTreeTest : public ::testing::Test {

        SE::TSceneTree * pScene;

        protected:

        SceneTreeTest() {

                SE::TEngine::Instance().Init();
                //gLogger->set_level(spdlog::level::debug);
                pScene = new SE::TSceneTree("TestScene", 0, true);
        }

        ~SceneTreeTest() noexcept {

                delete pScene;
                Loki::DeletableSingleton<SE::EngineBase>::GracefulDelete();
        };

        public:
        SE::TSceneTree * Scene() const {
                return pScene;
        }

};

TEST_F(SceneTreeTest, BasicNodeManagement) {

        auto * pScene = Scene();

        ASSERT_TRUE(pScene->GetRoot());

        auto pNode = pScene->Create("test node 1", true);
        ASSERT_TRUE(pNode);

        auto pNode2 = pScene->Create(pNode, "test node 2", true);
        ASSERT_TRUE(pNode2);

        EXPECT_STREQ(pNode->GetName().c_str(), "test node 1");
        EXPECT_STREQ(pNode->GetFullName().c_str(), "root|test node 1");

        auto pSearchNode = pScene->FindFullName("root|test node 1");
        EXPECT_EQ(pNode, pSearchNode);

        auto * pNodes = pScene->FindLocalName("test node 1");
        ASSERT_TRUE(pNodes);

        //pScene->Print();

        EXPECT_EQ(pNodes->size(), 1);
        EXPECT_EQ(pNode, (*pNodes)[0]);

        EXPECT_TRUE(pNode->Enabled());
        pScene->DisableAll();
        EXPECT_FALSE(pNode->Enabled());
        pScene->EnableAll();
        EXPECT_TRUE(pNode->Enabled());

        EXPECT_TRUE(pNode->SetName("first"));
        EXPECT_TRUE(pScene->FindLocalName("first"));

        pScene->Create(pNode, "some node x");
        pScene->Create(pNode, "some node y");

        auto pResNode = pNode->FindChild("test node 2");
        EXPECT_EQ(pNode2, pResNode);

        pNode->RemoveChild(pNode2);

        EXPECT_FALSE(pNode->FindChild("test node 2"));

        EXPECT_TRUE(pNode->GetCustomInfo().empty());

        pNode->SetCustomInfo("some key:some data|k2:v2");
        EXPECT_FALSE(pNode->GetCustomInfo().empty());

        EXPECT_EQ(pScene, pNode->GetScene());
}


TEST_F(SceneTreeTest, NodeTransforms) {

        auto * pScene = Scene();
        auto pNode = pScene->Create("test node 1", true);
        ASSERT_TRUE(pNode);
        auto pNode2 = pScene->Create(pNode, "test node 2", true);
        ASSERT_TRUE(pNode2);

        EXPECT_EQ(pNode->GetTransform().GetPos(), glm::vec3(0.0f));
        EXPECT_EQ(pNode2->GetTransform().GetPos(), glm::vec3(0.0f));

        glm::vec3 vPos1(4, 2, 0);

        pNode->Translate(vPos1);
        EXPECT_EQ(pNode->GetTransform().GetWorldPos(), vPos1);
        EXPECT_EQ(pNode2->GetTransform().GetWorldPos(), vPos1);

        /**
         TODO check all transform methods
         glm::epsilonEqual
         */

}

TEST_F(SceneTreeTest, NodeComponents) {

        auto * pScene = Scene();
        auto pNode = pScene->Create("test node 1", true);
        ASSERT_TRUE(pNode);

        EXPECT_EQ(pNode->GetComponentsCnt(), 0);

        EXPECT_EQ(pNode->CreateComponent<NiceMockComponent1>(), 0);
        EXPECT_EQ(pNode->GetComponentsCnt(), 1);

        EXPECT_TRUE(pNode->HasComponent<NiceMockComponent1>());

        pNode->DestroyComponent<NiceMockComponent1>();
        EXPECT_EQ(pNode->GetComponentsCnt(), 0);
}


