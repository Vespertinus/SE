
#ifndef APP_WORLD_SPATIAL_INDEX_SYSTEM_H
#define APP_WORLD_SPATIAL_INDEX_SYSTEM_H 1

#include <vector>

#include <glm/glm.hpp>

#include <world/SpatialIndex.h>
#include <world/WorldSystem.h>

namespace SE {

class SpatialEntry;

// ---------------------------------------------------------------------------
// SpatialIndexSystem — thin owner of the SpatialIndex.
//
// Compose AFTER WorldSystem (ctor dependency — the TerrainSystem precedent).
// WorldSystem is untouched; this system only consumes its events and API:
//
//  - Focus mirror: every world focus point becomes an index entry under the
//    reserved 'focus' tag (radius 0 — a focus is a point; its streaming radius
//    is a different concept). One poll pass per frame over GetFocuses() —
//    the only polling in the design; handles register/move/unregister
//    uniformly with no new WorldSystem surface.
//  - Sub-level attribution: entries created while a sub-level activation is
//    in flight (components Enable() inside instantiation, between
//    ESubLevelActivating and ESubLevelActivated) are stamped with that
//    sub-level; entries created outside any window get 0 (dynamics /
//    world-less) and survive teardown.
//  - Teardown sweep: ESubLevelDeactivating drops the sub-level's entries
//    before consumers disarm (atomic-to-gameplay). Component dtors clean up
//    their own entries regardless; a sweep-first makes their later Remove a
//    tolerated stale-handle no-op.
//  - EWorldUnloaded clears the index.
// ---------------------------------------------------------------------------

class SpatialIndexSystem {
public:
        SpatialIndexSystem();
        ~SpatialIndexSystem() noexcept;

        SpatialIndex &       Get()       { return oIndex; }
        const SpatialIndex & Get() const { return oIndex; }

        /// Sub-level attribution for entries created right now: the level whose
        /// activation is in flight (stored as index + 1 — 0 means unowned), or
        /// 0 outside any activation window (dynamics / world-less).
        uint32_t CurrentSubLevel() const { return activating_sub_level; }

        /// Reserved tag bit: every world focus point mirrors into an entry
        /// under it ("focus"). Registers the tag on first call.
        uint32_t FocusTagBit();
        /// Mask over the reserved focus tag (0 while the registry is exhausted).
        uint64_t FocusTagMask();

        // component registry (SpatialEntry lifecycle; debug enumeration)
        void AddComponent(SpatialEntry * pComp);
        void RemoveComponent(SpatialEntry * pComp);
        const std::vector<SpatialEntry *> & Components() const { return vComponents; }

        SpatialIndex::SStats GetStats() const { return oIndex.GetStats(); }

private:
        void OnUpdate(const Event & oEvent);
        void OnSubLevelActivating(const Event & oEvent);
        void OnSubLevelActivated(const Event & oEvent);
        void OnSubLevelDeactivating(const Event & oEvent);
        void OnWorldUnloaded(const Event & oEvent);
        void SyncFocuses();

        WorldSystem * pWorld = nullptr;
        SpatialIndex  oIndex;

        uint32_t activating_sub_level = 0;

        std::vector<SpatialEntry *> vComponents;

        // focus registry mirror, keyed by the focus's node (the registry exposes
        // FocusPoint without handles; node identity is the stable key)
        struct SFocusMirror {
                TSceneTree::TSceneNodeWeak node;
                glm::vec3                  pos   {};
                SpatialIndex::EntryHandle  entry = 0;
        };
        std::vector<SFocusMirror> vFocusMirror;
};

} // namespace SE

#endif
