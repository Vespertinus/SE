
#include <gtest/gtest.h>
#include <gmock/gmock.h>

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

#include <loki/Singleton.h>


// Minimal engine used by this test binary
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

template <class Resource, class ... TConcreateSettings> H<Resource> CreateResource (const std::string & sPath, const TConcreateSettings & ... oSettings) {

        return H<Resource>::Null();
}

} // namespace SE

// SceneTree must be included before EntityManager so that
// SE::TSceneTree can be defined as SceneTree<> below.
#include <SceneTree.h>

namespace SE {

class StaticModelMock;
// Minimal TSceneTree with no custom components
using TSceneTree = SceneTree<StaticModelMock>;
} // namespace SE

#include <StaticModelMock.h>

// EntityManager relies on SE::TSceneTree being defined above
#include <EntityManager.h>
#include <EntityManager.tcc>

// -------------------------------------------------------------------------

class EntityManagerTest : public ::testing::Test {

protected:
        EntityManagerTest() {
                SE::TEngine::Instance().Init();
                pScene = std::make_unique<SE::TSceneTree>("test_scene", 0, /*empty=*/true);
                SE::GetSystem<SE::EntityManager>().SetSceneTree(pScene.get());
        }

        ~EntityManagerTest() noexcept {
                Loki::DeletableSingleton<SE::EngineBase>::GracefulDelete();
        }

        std::unique_ptr<SE::TSceneTree> pScene;
};

// -------------------------------------------------------------------------
// 1. Spawn returns a valid weak_ptr; lock() is non-null.
// -------------------------------------------------------------------------

TEST_F(EntityManagerTest, SpawnReturnsValidHandle) {

        SE::SpawnRequest oReq;
        oReq.name = "test_entity";

        auto pHandle = SE::GetSystem<SE::EntityManager>().Spawn(oReq);
        auto pNode   = pHandle.lock();

        ASSERT_NE(pNode, nullptr);
        EXPECT_EQ(pNode->GetName(), "test_entity");
}

// -------------------------------------------------------------------------
// 2. Destroy via queue: handle still lockable after Destroy(), expired after
//    FlushQueues() (triggered by EPostUpdate).
// -------------------------------------------------------------------------

TEST_F(EntityManagerTest, DestroyViaQueueDeferredUntilFlush) {

        SE::SpawnRequest oReq;
        oReq.name = "entity_to_destroy";

        auto pHandle = SE::GetSystem<SE::EntityManager>().Spawn(oReq);
        ASSERT_FALSE(pHandle.expired());

        SE::GetSystem<SE::EntityManager>().Destroy(pHandle);

        // Depth increases, node still alive
        EXPECT_EQ(SE::GetSystem<SE::EntityManager>().DestroyQueueDepth(), 1u);
        EXPECT_FALSE(pHandle.expired());

        // Flush via EPostUpdate
        SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EPostUpdate{0.016f});

        EXPECT_TRUE(pHandle.expired());
        EXPECT_EQ(SE::GetSystem<SE::EntityManager>().DestroyQueueDepth(), 0u);
}

// -------------------------------------------------------------------------
// 3. RequestSpawn enqueues an intent; node appears after FlushQueues().
// -------------------------------------------------------------------------

TEST_F(EntityManagerTest, RequestSpawnQueuedAndProcessedOnFlush) {

        SE::SpawnIntent oIntent;
        oIntent.name = "deferred_entity";

        SE::GetSystem<SE::EntityManager>().RequestSpawn(oIntent);
        EXPECT_EQ(SE::GetSystem<SE::EntityManager>().SpawnQueueDepth(), 1u);

        // Node should not exist yet
        auto pPreFlush = pScene->GetRoot()->FindChild(SE::StrID("deferred_entity"));
        EXPECT_EQ(pPreFlush, nullptr);

        SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EPostUpdate{0.016f});

        EXPECT_EQ(SE::GetSystem<SE::EntityManager>().SpawnQueueDepth(), 0u);
        auto pPostFlush = pScene->GetRoot()->FindChild(SE::StrID("deferred_entity"));
        EXPECT_NE(pPostFlush, nullptr);
}

// -------------------------------------------------------------------------
// 4. Filling the queue drops excess pushes without crashing.
// -------------------------------------------------------------------------

TEST_F(EntityManagerTest, SpawnQueueExhaustionNoCrash) {

        SE::SpawnIntent oIntent;
        oIntent.name = "overflow";

        // SPSC queue of capacity N can hold N-1 items
        for (uint32_t i = 0; i < SE::EntityManager::kQueueCapacity - 1; ++i) {
                SE::GetSystem<SE::EntityManager>().RequestSpawn(oIntent);
        }

        // The next push is silently dropped — must not crash
        EXPECT_NO_FATAL_FAILURE(SE::GetSystem<SE::EntityManager>().RequestSpawn(oIntent));
}

// -------------------------------------------------------------------------
// 5. After destroy + flush, lock() returns nullptr.
// -------------------------------------------------------------------------

TEST_F(EntityManagerTest, StaleHandleAfterDestroyFlush) {

        SE::SpawnRequest oReq;
        oReq.name = "stale";

        auto pHandle = SE::GetSystem<SE::EntityManager>().Spawn(oReq);
        ASSERT_NE(pHandle.lock(), nullptr);

        SE::GetSystem<SE::EntityManager>().Destroy(pHandle);
        SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EPostUpdate{0.016f});

        EXPECT_EQ(pHandle.lock(), nullptr);
}
