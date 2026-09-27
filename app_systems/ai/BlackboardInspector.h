
#ifndef APP_AI_BLACKBOARD_INSPECTOR_H
#define APP_AI_BLACKBOARD_INSPECTOR_H 1

namespace SE {

// ---------------------------------------------------------------------------
// BlackboardInspector — opt-in ImGui panel for the blackboards
// (NOT part of BlackboardSystem; add to TCustomSystems in debug builds only).
//
// Shows the world-level (global) blackboard and every registered per-entity
// instance as typed key/value tables (schema order, live values). Read-only:
// the panel is a diagnosis aid, writes stay with gameplay systems. Hidden by
// default — the per-key work runs only while shown.
//
// Usage:
//   1. Add BlackboardInspector to TCustomSystems.
//   2. SetVisible(true) (e.g. from a debug key handler).
// ---------------------------------------------------------------------------
class BlackboardInspector {
public:
        BlackboardInspector();
        ~BlackboardInspector() noexcept;

        void SetVisible(bool new_visible) { visible = new_visible; }
        bool Visible() const { return visible; }

        /** Tabbed mode: the standalone window is disabled and the host window
         *  (world_demo's Debug tab bar) calls DrawTab() for the active tab. */
        void SetTabbed(bool b) { bTabbed = b; }
        void DrawTab();

private:
        void OnPostRenderUpdate(const Event& e);
        void DrawContents();

        bool visible = false;
        bool bTabbed = false;
};

} // namespace SE

#endif // APP_AI_BLACKBOARD_INSPECTOR_H
