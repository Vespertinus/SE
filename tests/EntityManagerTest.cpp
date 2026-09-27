
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <thread>

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

// -------------------------------------------------------------------------
// 6. Deferred spawn intent carrying a parent lands under it (not at root).
// -------------------------------------------------------------------------

TEST_F(EntityManagerTest, DeferredSpawnLandsUnderParent) {

        auto & oEM = SE::GetSystem<SE::EntityManager>();
        auto pParent = pScene->Create("spawn_parent");
        ASSERT_NE(pParent, nullptr);

        SE::SpawnIntent oIntent;
        oIntent.name    = "child_entity";
        oIntent.pParent = pParent;

        oEM.RequestSpawn(oIntent);
        SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EPostUpdate{0.016f});

        EXPECT_NE(pParent->FindChild(SE::StrID("child_entity")), nullptr);
        EXPECT_EQ(pScene->GetRoot()->FindChild(SE::StrID("child_entity")), nullptr);
}

// -------------------------------------------------------------------------
// 7. SpawnTemplate routes through the instantiator and returns a usable handle.
// -------------------------------------------------------------------------

TEST_F(EntityManagerTest, SpawnTemplateRoutesAndReturnsHandle) {

        auto & oEM = SE::GetSystem<SE::EntityManager>();
        EXPECT_TRUE(oEM.SpawnTemplate({}).expired());  // no instantiator: expired, no crash

        bool called = false;
        oEM.SetTemplateInstantiator([&oEM, &called](const SE::SpawnIntent & oIntent)
                        -> SE::TSceneTree::TSceneNodeWeak {
                called = true;
                SE::SpawnRequest oReq;
                oReq.name    = oIntent.name;
                oReq.pParent = oIntent.pParent;
                return oEM.Spawn(oReq);
        });

        SE::SpawnIntent oIntent;
        oIntent.prefab_id = "some_template";
        oIntent.name      = "from_template";
        auto pHandle = oEM.SpawnTemplate(oIntent);

        EXPECT_TRUE(called);
        ASSERT_NE(pHandle.lock(), nullptr);
        EXPECT_EQ(pHandle.lock()->GetName(), "from_template");
        EXPECT_EQ(oEM.GetStats().spawned_template, 1u);
}

// -------------------------------------------------------------------------
// 8. Double destroy of the same node is collapsed to one queued request.
// -------------------------------------------------------------------------

TEST_F(EntityManagerTest, DoubleDestroyIsDeduplicated) {

        auto & oEM = SE::GetSystem<SE::EntityManager>();

        SE::SpawnRequest oReq;
        oReq.name = "dedup_target";
        auto pHandle = oEM.Spawn(oReq);
        ASSERT_NE(pHandle.lock(), nullptr);

        oEM.Destroy(pHandle);
        oEM.Destroy(pHandle);
        EXPECT_EQ(oEM.GetStats().destroy_dedups, 1u);
        EXPECT_EQ(oEM.DestroyQueueDepth(), 1u);

        SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EPostUpdate{0.016f});
        EXPECT_EQ(oEM.GetStats().destroyed, 1u);
        EXPECT_TRUE(pHandle.expired());
}

// -------------------------------------------------------------------------
// 9. Two producer threads: every intent is delivered exactly once (MPSC push).
// -------------------------------------------------------------------------

TEST_F(EntityManagerTest, TwoProducerThreadsAllIntentsDelivered) {

        auto & oEM = SE::GetSystem<SE::EntityManager>();
        constexpr uint32_t kPerThread = 50;

        auto worker = [&](uint32_t base) {
                for (uint32_t i = 0; i < kPerThread; ++i) {
                        SE::SpawnIntent oIntent;
                        oIntent.name = "t" + std::to_string(base) + "_" + std::to_string(i);
                        oEM.RequestSpawn(oIntent);
                }
        };
        std::thread oT1(worker, 0), oT2(worker, 1);
        oT1.join();
        oT2.join();

        EXPECT_EQ(oEM.SpawnQueueDepth(), kPerThread * 2);
        EXPECT_EQ(oEM.GetStats().spawn_drops, 0u);

        SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EPostUpdate{0.016f});

        uint32_t found = 0;
        for (uint32_t base = 0; base < 2; ++base) {
                for (uint32_t i = 0; i < kPerThread; ++i) {
                        const std::string sName = "t" + std::to_string(base) + "_" + std::to_string(i);
                        if (pScene->GetRoot()->FindChild(SE::StrID(sName)) != nullptr) {
                                ++found;
                        }
                }
        }
        EXPECT_EQ(found, kPerThread * 2);
}

// -------------------------------------------------------------------------
// 10. Queue-full drops are counted (no silent loss).
// -------------------------------------------------------------------------

TEST_F(EntityManagerTest, QueueFullDropsCounted) {

        auto & oEM = SE::GetSystem<SE::EntityManager>();

        SE::SpawnIntent oIntent;
        oIntent.name = "overflow";
        for (uint32_t i = 0; i < SE::EntityManager::kQueueCapacity - 1; ++i) {
                oEM.RequestSpawn(oIntent);
        }
        EXPECT_EQ(oEM.GetStats().spawn_drops, 0u);

        oEM.RequestSpawn(oIntent);  // over capacity
        EXPECT_EQ(oEM.GetStats().spawn_drops, 1u);
}

// -------------------------------------------------------------------------
// 11. Flush contract: an instantiator that queues a destroy mid-flush has it
//     processed in the same flush (drain-until-empty).
// -------------------------------------------------------------------------

TEST_F(EntityManagerTest, ReentrantDestroyProcessedSameFlush) {

        auto & oEM = SE::GetSystem<SE::EntityManager>();
        oEM.SetTemplateInstantiator([&oEM](const SE::SpawnIntent & oIntent)
                        -> SE::TSceneTree::TSceneNodeWeak {
                SE::SpawnRequest oReq;
                oReq.name = oIntent.name;
                auto pHandle = oEM.Spawn(oReq);
                oEM.Destroy(pHandle);  // re-entrant queue push during FlushQueues
                return pHandle;
        });

        SE::SpawnIntent oIntent;
        oIntent.prefab_id = "self_destructing";
        oIntent.name      = "ephemeral";
        oEM.RequestSpawn(oIntent);

        SE::GetSystem<SE::EventManager>().TriggerEvent(SE::EPostUpdate{0.016f});

        EXPECT_TRUE(oEM.SpawnQueueDepth() == 0u);
        EXPECT_EQ(oEM.DestroyQueueDepth(), 0u);
        EXPECT_EQ(pScene->GetRoot()->FindChild(SE::StrID("ephemeral")), nullptr);
}

// -------------------------------------------------------------------------
// 12. Scene switch drains requests referencing the previous scene.
// -------------------------------------------------------------------------

TEST_F(EntityManagerTest, SceneSwitchDrainsQueues) {

        auto & oEM = SE::GetSystem<SE::EntityManager>();

        SE::SpawnIntent oIntent;
        oIntent.name = "stale_intent";
        oEM.RequestSpawn(oIntent);
        EXPECT_EQ(oEM.SpawnQueueDepth(), 1u);

        auto pOther = std::make_unique<SE::TSceneTree>("other_scene", 0, /*empty=*/true);
        oEM.SetSceneTree(pOther.get());
        EXPECT_EQ(oEM.SpawnQueueDepth(), 0u);
        EXPECT_EQ(oEM.DestroyQueueDepth(), 0u);
}
