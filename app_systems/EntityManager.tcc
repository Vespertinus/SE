
#include <CommonEvents.h>

namespace SE {

EntityManager::EntityManager() {
        GetSystem<EventManager>().AddListener<EPostUpdate, &EntityManager::OnPostUpdate>(this);
}

EntityManager::~EntityManager() noexcept {
        GetSystem<EventManager>().RemoveListener<EPostUpdate, &EntityManager::OnPostUpdate>(this);
}

void EntityManager::SetSceneTree(TSceneTree * pTree) {
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

        auto pNode = pSceneTree->Create(pParent, req.name, req.enabled);
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

        TSceneTree::TSceneNodeWeak handle { pNode };
        GetSystem<EventManager>().TriggerEvent(EEntitySpawned{ handle });

        return handle;
}

void EntityManager::RequestSpawn(const SpawnIntent & intent) {

        if (!oSpawnQueue.TryPush(intent)) {
                log_w("EntityManager: spawn queue full, intent '{}' dropped", intent.name);
        }
}

void EntityManager::Destroy(TSceneTree::TSceneNodeWeak pNode) {

        if (!oDestroyQueue.TryPush(pNode)) {
                log_w("EntityManager: destroy queue full, request dropped");
        }
}

void EntityManager::OnPostUpdate(const Event & /*oEvent*/) {
        FlushQueues();
}

void EntityManager::FlushQueues() {

        if (!pSceneTree) {
                return;
        }

        // Destroy first to free slots before spawning new ones
        TSceneTree::TSceneNodeWeak pDestroyEntry;
        while (oDestroyQueue.TryPop(pDestroyEntry)) {

                auto pNode = pDestroyEntry.lock();
                if (!pNode) {
                        continue;  // already gone
                }

                GetSystem<EventManager>().TriggerEvent(EEntityDestroyed{ pDestroyEntry });
                pNode->Unlink();
        }

        // Process pending spawn intents
        SpawnIntent oIntent;
        while (oSpawnQueue.TryPop(oIntent)) {

                if (!oIntent.prefab_id.empty()) {
                        if (fnTemplateInstantiator) {
                                auto pNode = fnTemplateInstantiator(oIntent);
                                if (pNode.expired()) {
                                        log_e("EntityManager: template instantiation failed "
                                              "for id='{}' name='{}'",
                                              oIntent.prefab_id, oIntent.name);
                                }
                        } else {
                                log_w("EntityManager: no template instantiator set, "
                                      "dropping id='{}' name='{}'",
                                      oIntent.prefab_id, oIntent.name);
                        }
                        continue;
                }

                SpawnRequest oReq;
                oReq.name        = oIntent.name;
                oReq.vTranslation = oIntent.vTranslation;
                oReq.vRotation   = oIntent.vRotation;
                oReq.vScale      = oIntent.vScale;
                oReq.enabled     = oIntent.enabled;

                Spawn(oReq);
        }
}

} // namespace SE
