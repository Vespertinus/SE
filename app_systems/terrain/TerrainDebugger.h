
#ifndef APP_TERRAIN_DEBUGGER_H
#define APP_TERRAIN_DEBUGGER_H 1

#include <vector>

#include <terrain/TerrainSystem.h>

namespace SE {

// ---------------------------------------------------------------------------
// TerrainDebugger — opt-in ImGui panel (StateMachineDebugger / StreamingDebugger
// pattern): deliberately NOT part of TerrainSystem. Add to TCustomSystems in
// debug builds; polls TerrainSystem state, zero cost while hidden.
// ---------------------------------------------------------------------------

class TerrainDebugger {
public:
        TerrainDebugger();
        ~TerrainDebugger() noexcept;

        void SetVisible(const bool bVisible) { visible = bVisible; }
        bool IsVisible() const { return visible; }

        /** Tabbed mode: standalone window disabled; host window calls DrawTab(). */
        void SetTabbed(bool b) { bTabbed = b; }
        void DrawTab();

        void DrawContents();

private:
        void OnPostRenderUpdate(const Event & oEvent);

        bool visible = false;
        bool bTabbed = false;
        std::vector<TerrainSystem::STileDebug> vTiles;
        int      iOverrideTile = 0;   // tile picked in the LOD override section
};

} // namespace SE

#endif
