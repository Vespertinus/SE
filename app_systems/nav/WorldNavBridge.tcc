
#ifdef SE_IMPL

#include <nav/WorldNavBridge.h>

#include <nav/NavigationSystem.h>
#include <nav/NavMeshAsset.h>
#include <world/WorldSystem.h>
#include <world/WorldAsset.h>
#include <terrain/TerrainSystem.h>

#include <Logging.h>
#include <EventManager.h>
#include <GlobalTypes.h>

namespace SE {

// ---------------------------------------------------------------------------

WorldNavBridge::WorldNavBridge()
        : pNav(&GetSystem<NavigationSystem>()) {

        auto & oEvents = GetSystem<EventManager>();
        oEvents.AddListener<EWorldLoaded,            &WorldNavBridge::OnWorldLoaded>(this);
        oEvents.AddListener<EWorldUnloaded,          &WorldNavBridge::OnWorldUnloaded>(this);
        oEvents.AddListener<ESubLevelActivated,      &WorldNavBridge::OnSubLevelActivated>(this);
        oEvents.AddListener<ESubLevelDeactivating,   &WorldNavBridge::OnSubLevelDeactivating>(this);
        oEvents.AddListener<ETerrainTileActivated,   &WorldNavBridge::OnTerrainTileActivated>(this);
        oEvents.AddListener<ETerrainTileDeactivated, &WorldNavBridge::OnTerrainTileDeactivated>(this);
}

WorldNavBridge::~WorldNavBridge() noexcept {

        auto & oEvents = GetSystem<EventManager>();
        oEvents.RemoveListener<EWorldLoaded,            &WorldNavBridge::OnWorldLoaded>(this);
        oEvents.RemoveListener<EWorldUnloaded,          &WorldNavBridge::OnWorldUnloaded>(this);
        oEvents.RemoveListener<ESubLevelActivated,      &WorldNavBridge::OnSubLevelActivated>(this);
        oEvents.RemoveListener<ESubLevelDeactivating,   &WorldNavBridge::OnSubLevelDeactivating>(this);
        oEvents.RemoveListener<ETerrainTileActivated,   &WorldNavBridge::OnTerrainTileActivated>(this);
        oEvents.RemoveListener<ETerrainTileDeactivated, &WorldNavBridge::OnTerrainTileDeactivated>(this);
}

// ---------------------------------------------------------------------------

void WorldNavBridge::OnWorldLoaded(const Event &) {

        WorldSystem * pWorld = &GetSystem<WorldSystem>();
        H<WorldAsset> hWorld = pWorld->GetWorld();
        if (!hWorld.IsValid()) { return; }
        const WorldAsset * pAsset = GetResource(hWorld);

        for (const WorldAsset::NavMeshEntry & e : pAsset->GetNavMeshes()) {

                H<NavMeshAsset> hMesh;
                try {
                        if (!e.path.empty()) {
                                hMesh = CreateResource<NavMeshAsset>(e.path);
                        } else if (e.pInlineData) {
                                hMesh = CreateResource<NavMeshAsset>(e.cache_name,
                                                                     e.pInlineData->data(),
                                                                     e.pInlineData->size());
                        }
                } catch (const std::exception & oErr) {
                        log_e("WorldNavBridge: failed to load nav mesh for class '{}' — {}",
                              e.agent_class, oErr.what());
                        continue;
                }

                if (!hMesh.IsValid()) { continue; }

                RetainResource(hMesh);
                vPinned.push_back(hMesh);

                if (!pNav->RegisterClass(e.agent_class, hMesh)) {
                        log_e("WorldNavBridge: class '{}' registration failed", e.agent_class);
                }
        }

        if (!pAsset->GetDefaultNavClass().empty()) {
                pNav->SetDefaultClass(pAsset->GetDefaultNavClass());
        }

        log_i("WorldNavBridge: world '{}' nav registered ({} classes, default '{}')",
              pAsset->Name(), vPinned.size(), pNav->DefaultClass());
}

void WorldNavBridge::OnWorldUnloaded(const Event &) {

        for (const H<NavMeshAsset> hMesh : vPinned) { UnretainResource(hMesh); }
        vPinned.clear();
        mWarnedLevels.clear();

        pNav->UnregisterAll();   // idempotent; terrain events already drained live tiles
}

void WorldNavBridge::OnSubLevelActivated(const Event & oEvent) {

        const ESubLevelActivated & ev = oEvent.Get<ESubLevelActivated>();

        H<WorldAsset> hWorld = GetSystem<WorldSystem>().GetWorld();
        if (!hWorld.IsValid()) { return; }
        const WorldAsset * pAsset = GetResource(hWorld);
        if (ev.sub_level >= pAsset->GetSubLevels().size()) { return; }

        pNav->ActivateGroup(StrID(pAsset->GetSubLevels()[ev.sub_level].path));
}

void WorldNavBridge::OnSubLevelDeactivating(const Event & oEvent) {

        const ESubLevelDeactivating & ev = oEvent.Get<ESubLevelDeactivating>();

        H<WorldAsset> hWorld = GetSystem<WorldSystem>().GetWorld();
        if (!hWorld.IsValid()) { return; }
        const WorldAsset * pAsset = GetResource(hWorld);
        if (ev.sub_level >= pAsset->GetSubLevels().size()) { return; }

        pNav->DeactivateGroup(StrID(pAsset->GetSubLevels()[ev.sub_level].path));
}

void WorldNavBridge::OnTerrainTileActivated(const Event & oEvent) {
        const ETerrainTileActivated & ev = oEvent.Get<ETerrainTileActivated>();
        pNav->ActivateTerrainTile(ev.manifest, ev.bounds_min, ev.bounds_max);
}

void WorldNavBridge::OnTerrainTileDeactivated(const Event & oEvent) {
        const ETerrainTileDeactivated & ev = oEvent.Get<ETerrainTileDeactivated>();
        pNav->DeactivateTerrainTile(ev.manifest, ev.bounds_min, ev.bounds_max);
}

// ---------------------------------------------------------------------------

void WorldNavBridge::WarnIfUnnavigable(const glm::vec3 & vPos) {

        WorldSystem * pWorld = &GetSystem<WorldSystem>();
        H<WorldAsset> hWorld = pWorld->GetWorld();
        if (!hWorld.IsValid()) { return; }
        const WorldAsset * pAsset = GetResource(hWorld);

        const std::vector<WorldAsset::SubLevelEntry> & vLevels = pAsset->GetSubLevels();
        for (uint32_t i = 0; i < uint32_t(vLevels.size()); ++i) {
                const WorldAsset::SubLevelEntry & e = vLevels[i];

                if (pWorld->GetLevelState(i) != WorldSystem::ELevelState::Active) { continue; }
                if (e.IsNoNav()) { continue; }
                if (glm::any(glm::lessThan(vPos, e.bounds_min)) || glm::any(glm::greaterThan(vPos, e.bounds_max))) {
                        continue;
                }
                if (pNav->HasGroup(StrID(e.path))) { continue; }

                const uint64_t key = uint64_t(StrID(e.path));
                if (mWarnedLevels.insert(key).second) {
                        log_w("WorldNavBridge: point ({:.1f}, {:.1f}, {:.1f}) lies in active sub-level "
                              "'{}' but no registered navmesh covers it (bake/sync bug or missing nav entry)",
                              vPos.x, vPos.y, vPos.z, e.path);
                }
        }
}

} // namespace SE

#endif // SE_IMPL
