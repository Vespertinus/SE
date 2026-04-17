
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
        TSceneTree::TSceneNodeWeak oDestroyEntry;
        while (oDestroyQueue.TryPop(oDestroyEntry)) {

                auto pNode = oDestroyEntry.lock();
                if (!pNode) {
                        continue;  // already gone
                }

                GetSystem<EventManager>().TriggerEvent(EEntityDestroyed{ oDestroyEntry });
                pNode->Unlink();
        }

        // Process pending spawn intents
        SpawnIntent oIntent;
        while (oSpawnQueue.TryPop(oIntent)) {

                if (!oIntent.prefab_id.empty()) {
                        log_w("EntityManager: prefab '{}' not yet supported, intent dropped",
                              oIntent.prefab_id);
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
