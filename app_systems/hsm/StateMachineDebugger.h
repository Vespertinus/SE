
#ifndef APP_STATE_MACHINE_DEBUGGER_H
#define APP_STATE_MACHINE_DEBUGGER_H 1

#include <string>
#include <vector>

namespace SE {

// ---------------------------------------------------------------------------
// StateMachineDebugger — ImGui debug panel for state machines.
//
// NOT part of StateMachineSystem. Include in TCustomSystems in debug builds
// (or via #ifdef SE_DEBUG_TOOLS in the application's App.h).
//
// Usage:
//   1. Add StateMachineDebugger to TCustomSystems.
//   2. After scene load (or on entity spawn): call SetSceneTree(pTree).
//      This walks the tree and registers all StateMachine nodes.
//   3. Show/hide the panel with SetVisible() (hidden by default — the panel
//      costs per-entry work per frame while shown).
//   4. The debugger subscribes to EPostRenderUpdate and renders an ImGui panel
//      showing live state, parameters, and transition history per entity.
// ---------------------------------------------------------------------------
class StateMachineDebugger {
public:
        StateMachineDebugger();
        ~StateMachineDebugger() noexcept;

        // Walk the scene tree and (re-)register all nodes that have a StateMachine.
        void SetSceneTree(TSceneTree* pTree);

        void Register(TSceneTree::TSceneNodeWeak pNode);
        void Unregister(TSceneTree::TSceneNodeWeak pNode);

        void SetVisible(bool new_visible) { visible = new_visible; }
        bool Visible() const { return visible; }

private:
        void OnPostRenderUpdate(const Event& e);

        struct DebugEntry {
                TSceneTree::TSceneNodeWeak pNode;

                // ring buffer of recent transitions
                static constexpr size_t kHistoryCapacity = 16;
                struct HistoryEntry { StrID from; StrID to; float timestamp; };
                HistoryEntry history[kHistoryCapacity]{};
                size_t history_count = 0;
                size_t history_head  = 0;

                void PushHistory(StrID from_state, StrID to_state, float ts) {
                        history[history_head] = { from_state, to_state, ts };
                        history_head  = (history_head + 1) % kHistoryCapacity;
                        if (history_count < kHistoryCapacity) ++history_count;
                }
        };

        std::vector<DebugEntry> vEntries;
        float elapsed = 0.f;
        bool visible = false;
};

} // namespace SE

#endif
