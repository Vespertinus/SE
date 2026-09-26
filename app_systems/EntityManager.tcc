
#include <CommonEvents.h>

namespace SE {

EntityManager::EntityManager() {
        GetSystem<EventManager>().AddListener<EPostUpdate, &EntityManager::OnPostUpdate>(this);
}

EntityManager::~EntityManager() noexcept {
        GetSystem<EventManager>().RemoveListener<EPostUpdate, &EntityManager::OnPostUpdate>(this);
}

void EntityManager::SetSceneTree(TSceneTree * pTree) {

        if (pTree != pSceneTree) {
                // Requests referencing a previous scene can no longer be honored —
                // drop them loudly instead of spawning into (or destroying across) the wrong tree.
                SpawnIntent oStaleIntent;
                uint32_t dropped = 0;
                while (oSpawnQueue.TryPop(oStaleIntent)) { ++dropped; }
                TSceneTree::TSceneNodeWeak hStale;
                while (oDestroyQueue.TryPop(hStale)) { ++dropped; }
                {
                        std::lock_guard<std::mutex> oLock(oDestroyPushMutex);
                        sQueuedDestroys.clear();
                }
                if (dropped > 0) {
                        log_w("EntityManager: scene switch dropped {} queued request(s)", dropped);
                }
        }
        pSceneTree = pTree;
}

void EntityManager::SetTemplateInstantiator(
                std::function<TSceneTree::TSceneNodeWeak(const SpawnIntent &)> fn) {
        fnTemplateInstantiator = std::move(fn);
}

TSceneTree::TSceneNodeWeak EntityManager::Spawn(const SpawnRequest & req) {

        if (!pSceneTree) {
                log_e("EntityManager::Spawn called without a SceneTree");
                return {};
        }

        // Resolve parent: use locked parent or fall back to scene root
        TSceneTree::TSceneNode pParent = req.pParent.lock();
        if (!pParent) {
                pParent = pSceneTree->GetRoot();
        }

        // Resolve a unique full name up-front so repeated spawns of the same name
        // never collide (Create would otherwise fail and log a duplicate-name warning).
        std::string node_name = pSceneTree->MakeUniqueName(pParent, req.name);

        auto pNode = pSceneTree->Create(pParent, node_name, req.enabled);
        if (!pNode) {
                log_e("EntityManager::Spawn failed to create node '{}'", req.name);
                return {};
        }

        pNode->SetPos(req.vTranslation);
        pNode->SetRotation(req.vRotation);
        pNode->SetScale(req.vScale);

        if (req.fnInitializer) {
                req.fnInitializer(*pNode);
        }

        ++oStats.spawned_sync;

        TSceneTree::TSceneNodeWeak handle { pNode };
        GetSystem<EventManager>().TriggerEvent(EEntitySpawned{ handle });

        return handle;
}

TSceneTree::TSceneNodeWeak EntityManager::SpawnTemplate(const SpawnIntent & intent) {

        auto handle = InstantiateThroughFunctor(intent);
        if (handle.expired()) {
                log_e("EntityManager::SpawnTemplate failed for id='{}' name='{}'",
                                intent.prefab_id, intent.name);
        }
        return handle;
}

TSceneTree::TSceneNodeWeak EntityManager::InstantiateThroughFunctor(const SpawnIntent & intent) {

        if (!fnTemplateInstantiator) {
                log_w("EntityManager: no template instantiator set, dropping id='{}' name='{}'",
                                intent.prefab_id, intent.name);
                return {};
        }

        // Recursive instantiation guard: an initializer that spawns a template that
        // spawns... must terminate. 32 nesting levels is generous for legitimate graphs.
        if (instantiation_depth >= kMaxInstantiationDepth) {
                log_e("EntityManager: instantiation depth limit ({}) exceeded at id='{}'",
                                kMaxInstantiationDepth, intent.prefab_id);
                ++oStats.instantiation_refused;
                return {};
        }

        ++instantiation_depth;
        auto handle = fnTemplateInstantiator(intent);
        --instantiation_depth;

        if (!handle.expired()) {
                ++oStats.spawned_template;
        }
        return handle;
}

void EntityManager::RequestSpawn(const SpawnIntent & intent) {

        std::lock_guard<std::mutex> oLock(oSpawnPushMutex);
        if (!oSpawnQueue.TryPush(intent)) {
                ++oStats.spawn_drops;
                log_e("EntityManager: spawn queue full, intent '{}' dropped", intent.name);
        }
}

void EntityManager::Destroy(TSceneTree::TSceneNodeWeak pNode) {

        auto pLocked = pNode.lock();
        if (!pLocked) {
                return;  // already gone — nothing to destroy
        }

        std::lock_guard<std::mutex> oLock(oDestroyPushMutex);

        // Collapse duplicate destroy requests: the second Unlink would otherwise hit
        // the "Unlink without parent" error path at flush.
        if (!sQueuedDestroys.insert(pLocked.get()).second) {
                ++oStats.destroy_dedups;
                return;
        }

        if (!oDestroyQueue.TryPush(pNode)) {
                sQueuedDestroys.erase(pLocked.get());
                ++oStats.destroy_drops;
                log_e("EntityManager: destroy queue full, request dropped");
        }
}

void EntityManager::OnPostUpdate(const Event & /*oEvent*/) {
        FlushQueues();
}

void EntityManager::FlushQueues() {

        if (!pSceneTree) {
                return;
        }

        // Contract: destroys first, then spawns, drain-until-empty. Work queued by
        // callbacks (EEntityDestroyed listeners, instantiators, initializers) during
        // the flush is processed in the same flush — bounded by kMaxFlushCycles to
        // keep a pathological spawn/destroy loop from spinning forever (leftovers
        // flush next frame, warning logged).
        constexpr uint32_t kMaxFlushCycles = 8;

        for (uint32_t cycle = 0; cycle < kMaxFlushCycles; ++cycle) {

                // Destroy first to free slots before spawning new ones.
                TSceneTree::TSceneNodeWeak pDestroyEntry;
                while (oDestroyQueue.TryPop(pDestroyEntry)) {

                        {
                                std::lock_guard<std::mutex> oLock(oDestroyPushMutex);
                                if (auto pLocked = pDestroyEntry.lock()) {
                                        sQueuedDestroys.erase(pLocked.get());
                                }
                        }

                        auto pNode = pDestroyEntry.lock();
                        if (!pNode) {
                                continue;  // already gone
                        }

                        ++oStats.destroyed;
                        GetSystem<EventManager>().TriggerEvent(EEntityDestroyed{ pDestroyEntry });
                        pNode->Unlink();
                }

                // Process pending spawn intents
                SpawnIntent oIntent;
                while (oSpawnQueue.TryPop(oIntent)) {


                        if (!oIntent.prefab_id.empty()) {
                                if (InstantiateThroughFunctor(oIntent).expired()) {
                                        log_e("EntityManager: template instantiation failed "
                                              "for id='{}' name='{}'",
                                              oIntent.prefab_id, oIntent.name);
                                }
                                continue;
                        }

                        SpawnRequest oReq;
                        oReq.pParent      = oIntent.pParent;
                        oReq.name         = oIntent.name;
                        oReq.vTranslation = oIntent.vTranslation;
                        oReq.vRotation    = oIntent.vRotation;
                        oReq.vScale       = oIntent.vScale;
                        oReq.enabled      = oIntent.enabled;

                        Spawn(oReq);
                }

                if (oDestroyQueue.Empty() && oSpawnQueue.Empty()) {
                        break;
                }
                if (cycle == kMaxFlushCycles - 1) {
                        log_w("EntityManager: flush cycle limit reached, {} spawn(s) / {} destroy(s) "
                                        "deferred to the next frame",
                                        oSpawnQueue.Size(), oDestroyQueue.Size());
                }
        }
}

} // namespace SE
