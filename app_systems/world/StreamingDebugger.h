
#ifndef APP_WORLD_STREAMING_DEBUGGER_H
#define APP_WORLD_STREAMING_DEBUGGER_H 1

namespace SE {

// ---------------------------------------------------------------------------
// StreamingDebugger — opt-in ImGui panel for the streaming director
// (StateMachineDebugger pattern: NOT part of WorldSystem; add to
// TCustomSystems in debug builds only).
//
// Shows the streaming snapshot (wanted/active/requested/loading/unloading,
// per-frame budget spend, in-flight reads), a per-level state table with focus
// distance, and a top-down map: sub-level AABBs colored by state plus focus
// circles. Hidden by default — the panel does per-level work per frame only
// while shown.
//
// Usage:
//   1. Add StreamingDebugger to TCustomSystems.
//   2. SetVisible(true) (e.g. from a debug key handler).
// ---------------------------------------------------------------------------
class StreamingDebugger {
public:
        StreamingDebugger();
        ~StreamingDebugger() noexcept;

        void SetVisible(bool new_visible) { visible = new_visible; }
        bool Visible() const { return visible; }

        /** Tabbed mode: standalone window disabled; host window calls DrawTab(). */
        void SetTabbed(bool b) { bTabbed = b; }
        void DrawTab();

        void DrawContents();

private:
        void OnPostRenderUpdate(const Event& e);

        bool visible = false;
        bool bTabbed = false;
};

} // namespace SE

#endif
