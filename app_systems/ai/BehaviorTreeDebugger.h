
#ifndef APP_AI_BEHAVIOR_TREE_DEBUGGER_H
#define APP_AI_BEHAVIOR_TREE_DEBUGGER_H 1

#include <string>
#include <vector>

#include <ai/BehaviorTreeCommon.h>

namespace SE {

template <class TTasks> class TBehaviorTree;

// ---------------------------------------------------------------------------
// TBehaviorTreeDebugger<TTasks> — ImGui debug panel for behavior trees.
//
// NOT part of TBehaviorTreeSystem. Include in TCustomSystems in debug/UI builds
// (StateMachineDebugger pattern); never in test compositions. Templated on the
// same task typelist as the rest of the BT trio — derive the debugger alias
// from the app's task-list alias too:
//
//   using BehaviorTreeDebugger = TBehaviorTreeDebugger<TMyBTTasks>;
//
// Usage:
//   1. Add the debugger to TCustomSystems (after BehaviorTreeSystem).
//   2. After scene load: SetSceneTree(pTree) — walks the tree and registers
//      every node carrying a BehaviorTree component.
//   3. Show/hide with SetVisible() (hidden by default — the panel costs
//      per-frame work while shown).
//
// Per entity: recursive node view from the baked pool (status-colored, active
// path highlighted), per-node timer/counter/memory, resolved blackboard
// bindings, tick stats.
// ---------------------------------------------------------------------------
template <class TTasks>
class TBehaviorTreeDebugger {
public:
        using TComp  = TBehaviorTree<TTasks>;

        TBehaviorTreeDebugger();
        ~TBehaviorTreeDebugger() noexcept;

        // Walk the scene tree and (re-)register all nodes with a BehaviorTree.
        void SetSceneTree(TSceneTree* pTree);

        void Register(TSceneTree::TSceneNodeWeak pNode);
        void Unregister(TSceneTree::TSceneNodeWeak pNode);

        void SetVisible(bool bNewVisible) { bVisible = bNewVisible; }
        bool Visible() const { return bVisible; }
        /** Tabbed mode: standalone window disabled; host calls DrawTab(). */
        void SetTabbed(bool b) { bTabbed = b; }
        void DrawTab();

private:
        void OnPostRenderUpdate(const Event& e);
        void DrawContents();
        void DrawNode(const class TBehaviorTreeAsset<TTasks>* pAsset, const TComp* pComp,
                      uint16_t idx);

        std::vector<TSceneTree::TSceneNodeWeak> vNodes;
        bool bVisible = false;
        bool bTabbed  = false;
};

} // namespace SE

#endif
