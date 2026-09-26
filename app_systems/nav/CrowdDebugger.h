
#ifndef APP_NAV_CROWD_DEBUGGER_H
#define APP_NAV_CROWD_DEBUGGER_H 1

#include <string>

namespace SE {

// ---------------------------------------------------------------------------
// CrowdDebugger — opt-in ImGui panel for the crowd pipeline
// (NavigationDebugger pattern). Compose after CrowdSystem in TCustomSystems;
// hidden by default, toggled with SetVisible()/Visible().
//
// Shows solver stats (ticks/frame, arrivals, repaths, solver ms), the
// backend's live enrollment count, and a per-follower tree with status, goal,
// distance-to-goal and desired vs. adjusted velocities.
// ---------------------------------------------------------------------------

class CrowdDebugger {
public:
        CrowdDebugger();
        ~CrowdDebugger() noexcept;

        void SetVisible(bool bVisible) { visible = bVisible; }
        bool Visible() const { return visible; }

        /** Tabbed mode: standalone window disabled; host window calls DrawTab(). */
        void SetTabbed(bool b) { bTabbed = b; }
        void DrawTab();

        void DrawContents();

private:
        void OnPostRenderUpdate(const Event & oEvent);

        bool visible = false;
        bool bTabbed = false;
};

} // namespace SE

#endif
