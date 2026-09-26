
#ifndef APP_WORLD_SPATIAL_ENTRY_H
#define APP_WORLD_SPATIAL_ENTRY_H 1

#include <string>

#include <StrID.h>
#include <Component_generated.h>

#include <world/SpatialIndex.h>

namespace SE {

// ---------------------------------------------------------------------------
// SpatialEntryDesc — programmatic construction (tests, spawner code).
// ---------------------------------------------------------------------------

struct SpatialEntryDesc {
        std::string tag;
        float       radius     = 0.f;
        bool        track_node = false;
};

// ---------------------------------------------------------------------------
// SpatialEntry — an authored marker in the world spatial
// query grid (spawner conditions, perception, streaming-context queries).
//
// Standard component shape (Terrain/TriggerVolume precedent): authored via
// the FlatBuffers table in .sesc files, or constructed from SpatialEntryDesc.
//
// Enable() registers an index entry stamped with the sub-level whose
// activation is in flight (components enable inside instantiation —
// SpatialIndexSystem::CurrentSubLevel); entries created outside any
// activation window get 0 and survive sub-level teardown.
//
// With track_node the entry position follows the node transform through the
// node listener (TargetTransformChanged — the TriggerVolume pattern): parent
// motion cascades, identical writes early out, idle entries cost nothing.
// The listener is added in Enable() and removed in Disable()/dtor (the
// component lifecycle rule — never in the ctor).
// ---------------------------------------------------------------------------

class SpatialEntry {
public:
        using TSerialized = FlatBuffers::SpatialEntry;

        SpatialEntry(TSceneTree::TSceneNodeExact * pNode, const SpatialEntryDesc & desc);
        SpatialEntry(TSceneTree::TSceneNodeExact * pNode, const FlatBuffers::SpatialEntry * pFB);
        ~SpatialEntry() noexcept;

        void Enable();
        void Disable();

        /// Node-listener callback (SceneNode::AddListener duck type).
        void TargetTransformChanged(TSceneTree::TSceneNodeExact * pTargetNode);

        SpatialIndex::EntryHandle Handle() const { return hEntry; }
        const std::string &       Tag()    const { return oDesc.tag; }
        float                     Radius() const { return oDesc.radius; }

        std::string Str() const;

private:
        TSceneTree::TSceneNodeExact * pNode = nullptr;
        SpatialEntryDesc              oDesc;
        SpatialIndex::EntryHandle     hEntry = 0;
        bool                          enabled = false;
};

} // namespace SE

#endif
