
#ifndef APP_ENTITY_MANAGER_H
#define APP_ENTITY_MANAGER_H 1

#include <functional>
#include <string>

#include <SPSCQueue.h>
#include <glm/glm.hpp>

namespace SE {

// ---------------------------------------------------------------------------
// Events posted by EntityManager on the EventManager
// ---------------------------------------------------------------------------

struct EEntitySpawned {
        TSceneTree::TSceneNodeWeak pNode;
};

struct EEntityDestroyed {
        TSceneTree::TSceneNodeWeak pNode;
};

// ---------------------------------------------------------------------------
// SpawnRequest — main-thread only; may carry resource handles and initializer
// ---------------------------------------------------------------------------

struct SpawnRequest {
        TSceneTree::TSceneNodeWeak                        pParent;        ///< expired → attach to scene root
        glm::vec3                                         vTranslation    {0.f, 0.f, 0.f};
        glm::vec3                                         vRotation       {0.f, 0.f, 0.f}; ///< Euler degrees
        glm::vec3                                         vScale          {1.f, 1.f, 1.f};
        bool                                              enabled         = true;
        std::string                                       name;
        std::function<void(TSceneTree::TSceneNodeExact&)> fnInitializer;  ///< optional post-create callback
};

// ---------------------------------------------------------------------------
// SpawnIntent — safe to post from any thread; carries only POD + strings
// ---------------------------------------------------------------------------

struct SpawnIntent {
        std::string prefab_id;                          ///< placeholder — no prefab system yet
        glm::vec3   vTranslation    {0.f, 0.f, 0.f};
        glm::vec3   vRotation       {0.f, 0.f, 0.f};
        glm::vec3   vScale          {1.f, 1.f, 1.f};
        bool        enabled         = true;
        std::string name;
};

// ---------------------------------------------------------------------------
// EntityManager
// ---------------------------------------------------------------------------

class EntityManager {
public:
        static constexpr uint32_t kQueueCapacity = 256;  ///< must be power-of-two

        EntityManager();
        ~EntityManager() noexcept;

        /// Call once after the active SceneTree is available (or on scene switch).
        void SetSceneTree(TSceneTree * pTree);

        /// Main-thread only: immediate spawn.  Returns a weak_ptr to the new node.
        TSceneTree::TSceneNodeWeak Spawn(const SpawnRequest & req);

        /// Any thread: enqueues a spawn intent for processing at the next EPostUpdate.
        void RequestSpawn(const SpawnIntent & intent);

        /// Any thread: enqueues a destroy request for processing at the next EPostUpdate.
        void Destroy(TSceneTree::TSceneNodeWeak pNode);

        uint32_t SpawnQueueDepth()   const { return oSpawnQueue.Size();   }
        uint32_t DestroyQueueDepth() const { return oDestroyQueue.Size(); }

private:
        void OnPostUpdate(const Event & oEvent);
        void FlushQueues();

        TSceneTree *                                              pSceneTree   { nullptr };
        SPSCQueue<SpawnIntent,               kQueueCapacity>      oSpawnQueue;
        SPSCQueue<TSceneTree::TSceneNodeWeak,kQueueCapacity>      oDestroyQueue;
};

} // namespace SE

#endif
