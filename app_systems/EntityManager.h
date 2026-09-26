
#ifndef APP_ENTITY_MANAGER_H
#define APP_ENTITY_MANAGER_H 1

#include <functional>
#include <mutex>
#include <string>
#include <unordered_set>

#include <SPSCQueue.h>
#include <glm/glm.hpp>

namespace SE {

// ---------------------------------------------------------------------------
// Events posted by EntityManager on the EventManager
//
// EEntitySpawned is fired by the template instantiator (EntityTemplateSystem::
// Instantiate) when a template-spawn succeeds — not by EntityManager itself.
// EEntityDestroyed is fired by FlushQueues right before the node is unlinked,
// so listeners can still lock the handle during the event.
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
// SpawnIntent — safe to post from any thread; carries only POD + strings + a
// weak parent handle (control-block refcount is atomic).
//
// Delivery:
//   - RequestSpawn()  → queued, processed at the next EPostUpdate (any thread).
//   - SpawnTemplate() → routed to the template instantiator immediately and the
//     resulting handle returned to the caller (main thread only — it builds
//     scene nodes).
//
// Both deliveries funnel through the same instantiator set by
// SetTemplateInstantiator; there is exactly one instantiation path.
// ---------------------------------------------------------------------------

struct SpawnIntent {
        std::string prefab_id;                          ///< entity template id; routed to fnTemplateInstantiator
        TSceneTree::TSceneNodeWeak pParent;             ///< expired → instantiator's default (scene root)
        glm::vec3   vTranslation    {0.f, 0.f, 0.f};
        glm::vec3   vRotation       {0.f, 0.f, 0.f};
        glm::vec3   vScale          {1.f, 1.f, 1.f};
        bool        enabled         = true;
        std::string name;
};

// ---------------------------------------------------------------------------
// EntityManager
//
// Flush contract (FlushQueues, called once per frame at EPostUpdate):
//   1. Destroy queue first (frees slots before new spawns).
//   2. Spawn queue second, drained until empty.
//   3. Re-entrant RequestSpawn/Destroy calls from event listeners,
//      instantiators or initializers are processed in the same flush —
//      a listener may see cascading work; keep listeners cheap.
//
// The instantiator fires EEntitySpawned on success. Recursive instantiation
// (a template initializer that spawns) is depth-limited: past
// kMaxInstantiationDepth the request fails with log_e.
// ---------------------------------------------------------------------------

class EntityManager {
public:
        static constexpr uint32_t kQueueCapacity = 256;              ///< must be power-of-two
        static constexpr uint32_t kMaxInstantiationDepth = 32;       ///< recursive SpawnTemplate guard

        struct SStats {
                uint32_t spawned_sync      = 0;   ///< Spawn() immediate spawns
                uint32_t spawned_template  = 0;   ///< instantiator spawns (both deliveries)
                uint32_t destroyed         = 0;   ///< queued destroys executed
                uint32_t spawn_drops       = 0;   ///< queue-full drops
                uint32_t destroy_drops     = 0;   ///< queue-full drops
                uint32_t destroy_dedups    = 0;   ///< double-destroy requests collapsed
                uint32_t instantiation_refused = 0; ///< depth-guard refusals
        };

        EntityManager();
        ~EntityManager() noexcept;

        /// Call once after the active SceneTree is available (or on scene switch).
        /// Queues holding requests for a previous scene are drained with a warning.
        void SetSceneTree(TSceneTree * pTree);

        /// Main-thread only: immediate spawn of a bare node hierarchy.
        /// Returns a weak_ptr to the new node.
        TSceneTree::TSceneNodeWeak Spawn(const SpawnRequest & req);

        /// Main-thread only: immediate template spawn. Routes to the template
        /// instantiator (SetTemplateInstantiator) and returns the resulting handle,
        /// so callers can track what they spawned. Expired on failure.
        TSceneTree::TSceneNodeWeak SpawnTemplate(const SpawnIntent & intent);

        /// Any thread: enqueues a spawn intent for processing at the next EPostUpdate.
        void RequestSpawn(const SpawnIntent & intent);

        /// Any thread: enqueues a destroy request for processing at the next EPostUpdate.
        /// Expired handles are ignored; duplicate requests for the same node are collapsed.
        void Destroy(TSceneTree::TSceneNodeWeak pNode);

        /// Set the functor called for SpawnIntents with a non-empty prefab_id —
        /// both the deferred (RequestSpawn) and the sync (SpawnTemplate) delivery.
        /// Pass {} to clear (reverts to a log-warning-and-drop).
        /// The functor fires EEntitySpawned on success.
        void SetTemplateInstantiator(
                std::function<TSceneTree::TSceneNodeWeak(const SpawnIntent &)> fn);

        uint32_t SpawnQueueDepth()   const { return oSpawnQueue.Size();   }
        uint32_t DestroyQueueDepth() const { return oDestroyQueue.Size(); }
        const SStats & GetStats()   const { return oStats; }

private:
        void OnPostUpdate(const Event & oEvent);
        void FlushQueues();
        TSceneTree::TSceneNodeWeak InstantiateThroughFunctor(const SpawnIntent & intent);

        TSceneTree *                                              pSceneTree   { nullptr };
        SPSCQueue<SpawnIntent,               kQueueCapacity>      oSpawnQueue;
        SPSCQueue<TSceneTree::TSceneNodeWeak,kQueueCapacity>      oDestroyQueue;
        std::mutex                                                oSpawnPushMutex;    ///< serializes producers (SPSC invariant)
        std::mutex                                                oDestroyPushMutex;
        std::unordered_set<TSceneTree::TSceneNodeExact *>         sQueuedDestroys;    ///< destroy dedup, guarded by oDestroyPushMutex
        std::function<TSceneTree::TSceneNodeWeak(const SpawnIntent &)> fnTemplateInstantiator;
        uint32_t                                                  instantiation_depth { 0 };
        SStats                                                    oStats;
};

} // namespace SE

#endif
