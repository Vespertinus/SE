
#ifdef SE_IMPL

#include <world/SpatialEntry.h>
#include <world/SpatialIndexSystem.h>

#include <GlobalTypes.h>
#include <Logging.h>

#include <stdexcept>

namespace SE {

// ---------------------------------------------------------------------------

SpatialEntry::SpatialEntry(TSceneTree::TSceneNodeExact * pNewNode, const SpatialEntryDesc & desc)
        : pNode(pNewNode)
        , oDesc(desc) {

        if (oDesc.tag.empty()) {
                throw std::runtime_error("SpatialEntry: tag is required");
        }
}

SpatialEntry::SpatialEntry(TSceneTree::TSceneNodeExact * pNewNode, const FlatBuffers::SpatialEntry * pFB)
        : pNode(pNewNode) {

        if (pFB->tag() == nullptr || pFB->tag()->size() == 0) {
                throw std::runtime_error("SpatialEntry: tag is required");
        }

        oDesc.tag        = pFB->tag()->str();
        oDesc.radius     = pFB->radius();
        oDesc.track_node = pFB->track_node();
}

SpatialEntry::~SpatialEntry() noexcept {
        Disable();
}

// ---------------------------------------------------------------------------
// lifecycle
// ---------------------------------------------------------------------------

void SpatialEntry::Enable() {

        if (enabled) { return; }
        enabled = true;

        auto & oSystem = GetSystem<SpatialIndexSystem>();

        SpatialIndex::SEntryDesc d;
        d.vPos      = pNode->GetTransform().GetWorldPos();
        d.radius    = oDesc.radius;
        d.tag       = StrID(oDesc.tag);
        d.sub_level = oSystem.CurrentSubLevel();

        hEntry = oSystem.Get().Add(d);

        if (oDesc.track_node && hEntry != 0) {
                pNode->AddListener(this);   // immediate fire if already dirty — harmless re-Move
        }
}

void SpatialEntry::Disable() {

        if (!enabled) { return; }
        enabled = false;

        if (oDesc.track_node) { pNode->RemoveListener(this); }

        if (hEntry != 0) {
                // stale handle (entry swept by a sub-level deactivation or a world
                // unload) is a tolerated no-op inside the index
                GetSystem<SpatialIndexSystem>().Get().Remove(hEntry);
                hEntry = 0;
        }
}

// ---------------------------------------------------------------------------
// node listener
// ---------------------------------------------------------------------------

void SpatialEntry::TargetTransformChanged(TSceneTree::TSceneNodeExact * pTargetNode) {

        if (!enabled || hEntry == 0) { return; }

        GetSystem<SpatialIndexSystem>().Get().Move(hEntry, pTargetNode->GetTransform().GetWorldPos());
}

// ---------------------------------------------------------------------------

std::string SpatialEntry::Str() const {
        return "SpatialEntry('" + oDesc.tag + "')";
}

} // namespace SE

#endif
