
#ifndef APP_CHARACTER_DEBUGGER_H
#define APP_CHARACTER_DEBUGGER_H 1

#include <unordered_map>

namespace SE {

// ---------------------------------------------------------------------------
// CharacterDebugger — ImGui debug panel for CharacterController components.
//
// NOT part of CharacterMovementSystem. Include in TCustomSystems in debug
// builds (or via #ifdef SE_DEBUG_TOOLS in the application's App.h).
//
// Usage:
//   1. Add CharacterDebugger to TCustomSystems.
//   2. After scene load (or on entity spawn): call SetSceneTree(pTree).
//      This walks the tree and registers all CharacterController nodes.
//   3. Show/hide the panel with SetVisible() (hidden by default — the panel
//      costs per-entry work per frame while shown).
//   4. The debugger subscribes to EPostRenderUpdate and renders an ImGui panel
//      showing live movement mode, velocity, timers, and ground state per entity.
// ---------------------------------------------------------------------------
class CharacterDebugger {
public:
        CharacterDebugger();
        ~CharacterDebugger() noexcept;

        // Walk the scene tree and (re-)register all nodes that have a CharacterController.
        void SetSceneTree(TSceneTree* pTree);

        void Register(TSceneTree::TSceneNodeWeak pNode);
        void Unregister(TSceneTree::TSceneNodeWeak pNode);

        void SetVisible(bool new_visible) { visible = new_visible; }
        bool Visible() const { return visible; }

private:
        void OnPostRenderUpdate(const Event& e);

        std::unordered_map<uint32_t, TSceneTree::TSceneNodeWeak> mEntries;
        bool visible = false;
};

} // namespace SE

#endif
