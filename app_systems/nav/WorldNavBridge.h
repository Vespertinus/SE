
#ifndef APP_NAV_WORLD_NAV_BRIDGE_H
#define APP_NAV_WORLD_NAV_BRIDGE_H 1

#include <cstdint>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

#include <ResourceHandle.h>
#include <StrID.h>

namespace SE {

class Event;
class NavigationSystem;
class NavMeshAsset;

// ---------------------------------------------------------------------------
// WorldNavBridge — the ONE world-aware piece of the nav stack.
//
// The user-locked inversion: nav objects are CREATED when the world loads and
// REGISTERED with NavigationSystem — the system itself never reads WorldAsset
// and subscribes no world events. This bridge:
//   EWorldLoaded             -> create + pin NavMeshAsset(s) from the baked
//                               world nav config, RegisterClass (+default)
//   ESubLevelActivated       -> ActivateGroup(sub-level .sesc path)
//   ESubLevelDeactivating    -> DeactivateGroup(sub-level .sesc path)
//   ETerrainTileActivated    -> ActivateTerrainTile(manifest, bounds)
//   ETerrainTileDeactivated  -> DeactivateTerrainTile(manifest, bounds)
//   EWorldUnloaded           -> UnregisterAll + unpin (idempotent; terrain
//                               teardown has usually drained the tile sets
//                               through its own events already)
//
// Composition: after WorldSystem + NavigationSystem. Event dispatch is
// synchronous and in listener order, so terrain tile releases drain the nav
// tile sets before this bridge unregisters the classes.
// ---------------------------------------------------------------------------

class WorldNavBridge {
public:
        WorldNavBridge();
        ~WorldNavBridge() noexcept;

        /// Dev diagnostic warn once per
        /// sub-level when vPos lies in a loaded, non-NoNav sub-level whose
        /// group is missing from every registered navmesh — a bake/sync bug
        /// detector for "unreachable where I should walk" reports.
        void WarnIfUnnavigable(const glm::vec3 & vPos);

private:
        WorldNavBridge(const WorldNavBridge &) = delete;
        WorldNavBridge & operator = (const WorldNavBridge &) = delete;

        void OnWorldLoaded(const Event & oEvent);
        void OnWorldUnloaded(const Event & oEvent);
        void OnSubLevelActivated(const Event & oEvent);
        void OnSubLevelDeactivating(const Event & oEvent);
        void OnTerrainTileActivated(const Event & oEvent);
        void OnTerrainTileDeactivated(const Event & oEvent);

        NavigationSystem *           pNav = nullptr;   // composition requirement (compile-time)
        std::vector<H<NavMeshAsset>> vPinned;          // nav resources pinned for the world lifetime
        std::unordered_set<uint64_t> mWarnedLevels;
};

} // namespace SE

#endif
