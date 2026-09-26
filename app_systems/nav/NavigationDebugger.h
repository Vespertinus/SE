
#ifndef APP_NAV_NAVIGATION_DEBUGGER_H
#define APP_NAV_NAVIGATION_DEBUGGER_H 1

namespace SE {

// ---------------------------------------------------------------------------
// NavigationDebugger — ImGui debug panel for the navigation runtime.
//
// NOT part of NavigationSystem. Include in TCustomSystems in debug builds,
// composed after NavigationSystem (StateMachineDebugger pattern). Hidden by
// default; shows per-class stats, per-frame budget counters, live tile count,
// draw toggles (wired straight into the system's SDebug flags) and the
// registered agent list.
// ---------------------------------------------------------------------------
class NavigationDebugger {
public:
        NavigationDebugger();
        ~NavigationDebugger() noexcept;

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
