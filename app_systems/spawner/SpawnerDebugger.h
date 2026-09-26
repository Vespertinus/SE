
#ifndef APP_SPAWNER_SPAWNER_DEBUGGER_H
#define APP_SPAWNER_SPAWNER_DEBUGGER_H 1

namespace SE {

// ---------------------------------------------------------------------------
// SpawnerDebugger — opt-in ImGui panel for the spawner system
// (StreamingDebugger pattern: NOT part of SpawnerSystem; add to
// TCustomSystems in debug builds only. Syntax-checked only until world_demo
// composes it — no target includes it before then).
//
// Population table (id, active/max, next tick, waves, world slot), per-system
// stats (spawns/despawns/budget leftovers), and manual FireWave / despawn-sweep
// triggers. Hidden by default — per-frame work only while shown.
//
// Usage:
//   1. Add SpawnerDebugger to TCustomSystems.
//   2. SetVisible(true) (e.g. from a debug key handler).
// ---------------------------------------------------------------------------
class SpawnerDebugger {
public:
        SpawnerDebugger();
        ~SpawnerDebugger() noexcept;

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
