
#ifdef SE_IMPL

#include <world/SpatialIndexSystem.h>

#include <GlobalTypes.h>
#include <EventManager.h>
#include <CommonEvents.h>
#include <Logging.h>

#include <glm/glm.hpp>

namespace SE {

namespace {

constexpr float kFocusMoveEpsilon = 1e-4f;   // matches WorldSystem's focus-move detection

} // namespace

// ---------------------------------------------------------------------------

SpatialIndexSystem::SpatialIndexSystem()
        : pWorld(&GetSystem<WorldSystem>()) {

        auto & em = GetSystem<EventManager>();
        em.AddListener<EUpdate,               &SpatialIndexSystem::OnUpdate>(this);
        em.AddListener<ESubLevelActivating,   &SpatialIndexSystem::OnSubLevelActivating>(this);
        em.AddListener<ESubLevelActivated,    &SpatialIndexSystem::OnSubLevelActivated>(this);
        em.AddListener<ESubLevelDeactivating, &SpatialIndexSystem::OnSubLevelDeactivating>(this);
        em.AddListener<EWorldUnloaded,        &SpatialIndexSystem::OnWorldUnloaded>(this);
}

SpatialIndexSystem::~SpatialIndexSystem() noexcept {

        auto & em = GetSystem<EventManager>();
        em.RemoveListener<EUpdate,               &SpatialIndexSystem::OnUpdate>(this);
        em.RemoveListener<ESubLevelActivating,   &SpatialIndexSystem::OnSubLevelActivating>(this);
        em.RemoveListener<ESubLevelActivated,    &SpatialIndexSystem::OnSubLevelActivated>(this);
        em.RemoveListener<ESubLevelDeactivating, &SpatialIndexSystem::OnSubLevelDeactivating>(this);
        em.RemoveListener<EWorldUnloaded,        &SpatialIndexSystem::OnWorldUnloaded>(this);
}

// ---------------------------------------------------------------------------
// component registry
// ---------------------------------------------------------------------------

void SpatialIndexSystem::AddComponent(SpatialEntry * pComp) {

        for (SpatialEntry * pExisting : vComponents) {
                if (pExisting == pComp) {
                        log_w("SpatialIndexSystem: component added twice");
                        return;
                }
        }
        vComponents.push_back(pComp);
}

void SpatialIndexSystem::RemoveComponent(SpatialEntry * pComp) {

        for (size_t i = 0; i < vComponents.size(); ++i) {
                if (vComponents[i] == pComp) {
                        vComponents[i] = vComponents.back();
                        vComponents.pop_back();
                        return;
                }
        }
}

// ---------------------------------------------------------------------------
// tags
// ---------------------------------------------------------------------------

uint32_t SpatialIndexSystem::FocusTagBit() {
        static const StrID kFocusTag("focus");
        return oIndex.TagBit(kFocusTag);
}

uint64_t SpatialIndexSystem::FocusTagMask() {
        const uint32_t bit = FocusTagBit();
        return bit < SpatialIndex::kMaxTags ? (1ull << bit) : 0;
}

// ---------------------------------------------------------------------------
// focus mirror (the one polling pass in the design)
// ---------------------------------------------------------------------------

namespace {

bool SameNode(const TSceneTree::TSceneNodeWeak & a, const TSceneTree::TSceneNodeWeak & b) {
        return !a.owner_before(b) && !b.owner_before(a);
}

} // namespace

void SpatialIndexSystem::SyncFocuses() {

        const std::vector<WorldSystem::FocusPoint> & focuses = pWorld->GetFocuses();

        // drop mirrors whose focus is gone (unregistered / world unloaded)
        for (size_t i = vFocusMirror.size(); i-- > 0;) {
                const SFocusMirror & m = vFocusMirror[i];

                bool found = false;
                for (const WorldSystem::FocusPoint & f : focuses) {
                        if (SameNode(f.node, m.node)) { found = true; break; }
                }

                if (!found) {
                        if (m.entry != 0) { oIndex.Remove(m.entry); }
                        vFocusMirror[i] = vFocusMirror.back();
                        vFocusMirror.pop_back();
                }
        }

        // add new focuses, move changed ones
        for (const WorldSystem::FocusPoint & f : focuses) {

                SFocusMirror * pMirror = nullptr;
                for (SFocusMirror & m : vFocusMirror) {
                        if (SameNode(f.node, m.node)) { pMirror = &m; break; }
                }

                if (pMirror == nullptr) {
                        SpatialIndex::SEntryDesc d;
                        d.vPos = f.pos;
                        d.tag  = StrID("focus");   // Add registers the reserved tag
                        SpatialIndex::EntryHandle h = oIndex.Add(d);
                        vFocusMirror.push_back(SFocusMirror { f.node, f.pos, h });
                        continue;
                }

                const glm::vec3 delta = glm::abs(f.pos - pMirror->pos);
                if (delta.x > kFocusMoveEpsilon || delta.y > kFocusMoveEpsilon || delta.z > kFocusMoveEpsilon) {
                        oIndex.Move(pMirror->entry, f.pos);
                        pMirror->pos = f.pos;
                }
        }
}

// ---------------------------------------------------------------------------
// events
// ---------------------------------------------------------------------------

void SpatialIndexSystem::OnUpdate(const Event & /*oEvent*/) {
        SyncFocuses();
}

void SpatialIndexSystem::OnSubLevelActivating(const Event & oEvent) {
        const auto & ev = oEvent.Get<ESubLevelActivating>();
        // stored offset by one: 0 means "unowned" (dynamics / world-less), and
        // WorldSystem level slots are 0-based — an unshifted index would sweep
        // unowned entries on level-0 deactivation
        activating_sub_level = ev.sub_level + 1;
}

void SpatialIndexSystem::OnSubLevelActivated(const Event & /*oEvent*/) {
        activating_sub_level = 0;
}

void SpatialIndexSystem::OnSubLevelDeactivating(const Event & oEvent) {
        const auto & ev = oEvent.Get<ESubLevelDeactivating>();
        oIndex.RemoveBySubLevel(ev.sub_level + 1);   // +1: see OnSubLevelActivating
}

void SpatialIndexSystem::OnWorldUnloaded(const Event & /*oEvent*/) {
        oIndex.Clear();
        vFocusMirror.clear();
}

} // namespace SE

#endif
