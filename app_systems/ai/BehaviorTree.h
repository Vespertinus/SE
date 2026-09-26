
#ifndef APP_AI_BEHAVIOR_TREE_H
#define APP_AI_BEHAVIOR_TREE_H 1

#include <vector>

#include <ResourceHandle.h>
#include <StrID.h>

#include <ai/BehaviorTreeCommon.h>

namespace SE {

class Blackboard;
class BlackboardCore;
class BlackboardSystem;

namespace FlatBuffers { struct BehaviorTree; }

template <class TTasks> class TBehaviorTreeAsset;
template <class TTasks> class TBehaviorTreeSystem;

// ---------------------------------------------------------------------------
// TBehaviorTree<TTasks> — the per-entity behavior-tree brain.
//
// The tree structure is the referenced .sebt asset (path or inline holder);
// runtime state (node statuses, timers, task memory) lives here, one slot per
// baked node. Blackboard keys referenced by BlackboardCondition decorators
// resolve once against the entity's Blackboard core, falling back to the world
// global; resolution happens lazily at the first tick, when both sibling
// components and the world schema exist.
//
// Authorable in .sesc/.setm (TSerialized detection). Enable()/Disable() register
// with TBehaviorTreeSystem; the dtor disables (component lifecycle rule).
//
// Add to TCustomComponents in the application's App.h, along with the derived
// asset/system aliases:
//
//   using TMyBTTasks        = TCoreBTTasks;   // or concatenated with game tasks
//   using BehaviorTreeAsset = TBehaviorTreeAsset<TMyBTTasks>;
//   using BehaviorTree      = TBehaviorTree<TMyBTTasks>;
//   using BehaviorTreeSystem= TBehaviorTreeSystem<TMyBTTasks>;
// ---------------------------------------------------------------------------
template <class TTasks>
class TBehaviorTree {

        friend class TBehaviorTreeSystem<TTasks>;

public:
        using TSerialized = FlatBuffers::BehaviorTree;
        using TAsset      = TBehaviorTreeAsset<TTasks>;
        using TSystem     = TBehaviorTreeSystem<TTasks>;

        struct Desc {
                const char* tree_path;
                float       tick_interval = 0.f;   // seconds between tree ticks; 0 = every frame
        };

        // SBBBinding (resolved condition key location) comes from BehaviorTreeCommon.h.

        TBehaviorTree(TSceneTree::TSceneNodeExact* pNode, const Desc& desc);
        TBehaviorTree(TSceneTree::TSceneNodeExact* pNode, const FlatBuffers::BehaviorTree* pFB);
        ~TBehaviorTree() noexcept;

        void Enable();
        void Disable();

        const TAsset* GetAsset() const { return GetResource(hTree); }
        float         GetTickInterval() const { return tick_interval; }

        // --- introspection surface (system + debugger + tests) --------------

        size_t                        NodeCount() const { return vStates.size(); }
        const SNodeState&             State(size_t i) const { return vStates[i]; }
        const SBBBinding&             Binding(size_t i) const { return vBindings[i]; }
        EBTNodeStatus                 StatusOf(size_t i) const { return vStates[i].status; }
        uint32_t                      TickCount() const { return tick_count; }
        float                         LastTickDt() const { return last_tick_dt; }
        TSceneTree::TSceneNodeWeak    GetHandle() const { return pNode ? pNode->GetShared()
                                                                        : TSceneTree::TSceneNodeWeak{}; }
        BlackboardCore*               EntityCore();
        BlackboardCore*               GlobalCore();

private:
        void Init(const char* szTreePath, const FlatBuffers::BehaviorTreeDefinition* pInlineDef,
                  float tick_interval_val);
        void ResolveBindings();

        TSceneTree::TSceneNodeExact* pNode = nullptr;
        H<TAsset>                    hTree;
        Blackboard*                  pSiblingBB = nullptr;   // same-node Blackboard, cached at first tick

        float    tick_interval = 0.f;
        float    acc           = 0.f;    // stagger accumulator (seeded by the system)
        uint32_t tick_count    = 0;
        float    last_tick_dt  = 0.f;

        bool bBindingsResolved = false;

        std::vector<SNodeState>  vStates;
        std::vector<SBBBinding>  vBindings;
};

} // namespace SE

#endif
