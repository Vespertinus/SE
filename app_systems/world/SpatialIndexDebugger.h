
#ifndef APP_WORLD_SPATIAL_INDEX_DEBUGGER_H
#define APP_WORLD_SPATIAL_INDEX_DEBUGGER_H 1

namespace SE {

// ---------------------------------------------------------------------------
// SpatialIndexDebugger — opt-in ImGui panel for the world spatial index
// (StateMachineDebugger pattern: NOT part of SpatialIndexSystem; add to
// TCustomSystems in debug builds only).
//
// Shows index stats (entries/cells/tags/pending/refused), the tag registry,
// a per-sub-level entry table, and a query probe: an editable sphere
// (position + radius + tag) with the result count and up to 8 entries.
// Hidden by default — the panel does query work per frame only while shown.
//
// Usage:
//   1. Add SpatialIndexDebugger to TCustomSystems.
//   2. SetVisible(true) (e.g. from a debug key handler).
// ---------------------------------------------------------------------------
class SpatialIndexDebugger {
public:
        SpatialIndexDebugger();
        ~SpatialIndexDebugger() noexcept;

        void SetVisible(bool new_visible) { visible = new_visible; }
        bool Visible() const { return visible; }

        /** Tabbed mode: standalone window disabled; host window calls DrawTab(). */
        void SetTabbed(bool b) { bTabbed = b; }
        void DrawTab();

        void DrawContents();

private:
        void OnPostRenderUpdate(const Event & e);

        bool visible = false;
        bool bTabbed = false;
};

} // namespace SE

#endif
