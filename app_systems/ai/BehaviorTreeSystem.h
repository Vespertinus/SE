
#ifndef APP_AI_BEHAVIOR_TREE_SYSTEM_H
#define APP_AI_BEHAVIOR_TREE_SYSTEM_H 1

#include <cstdint>
#include <unordered_map>

#include <StrID.h>

#include <ai/BehaviorTreeCommon.h>
#include <ai/BehaviorTree.h>

namespace SE {

class Event;

// ---------------------------------------------------------------------------
// TBehaviorTreeSystem<TTasks> — staggered scheduler + tree ticking.
//
// Scheduler: per-component accumulator (StateMachineSystem skeleton) seeded
// from hash64(entity_name) at registration so equal intervals still spread
// across frames (plan D5). The accumulated dt is passed to ProcessComponent,
// so Wait/TimeLimit/Cooldown measure real time regardless of tick rate —
// AppClock-driven, pause-proof.
//
// Tick model: recursive tick from the root; composites keep
// running-child memory; priority = child index (earlier = higher). Same-tick
// reactivity: a composite resuming child rc re-checks earlier siblings'
// LowerPriority condition decorators (pass ⇒ preempt rc this tick); a Self
// abort decorator on the active path re-checks every tick (fail ⇒ its subtree
// is deactivated and Failure propagates this tick). Deactivation is a
// post-order Stop walk; Cooldown timers survive it.
//
// Compose after WorldSystem, BlackboardSystem and EntityManager (typelist
// order); NavigationSystem/CrowdSystem when MoveTo should move bodies.
// ---------------------------------------------------------------------------
template <class TTasks>
class TBehaviorTreeSystem {

public:
        using TComp  = TBehaviorTree<TTasks>;
        using TTable = TTaskTable<TTasks>;

        TBehaviorTreeSystem();
        ~TBehaviorTreeSystem() noexcept;

        TBehaviorTreeSystem(const TBehaviorTreeSystem&) = delete;
        TBehaviorTreeSystem& operator=(const TBehaviorTreeSystem&) = delete;

        void AddComponent(TComp* pComp);
        void RemoveComponent(TComp* pComp);

        // Public tick seam (StateMachine precedent): one tree tick with an
        // explicit dt. Resolves blackboard bindings on the first call.
        void ProcessComponent(TComp& comp, TSceneTree::TSceneNodeExact* pOwner, float dt);

private:
        void OnUpdate(const Event& e);

        EBTStatus TickNode(STaskCtx& ctx, uint16_t i);
        EBTStatus TickComposite(STaskCtx& ctx, uint16_t i);
        EBTStatus TickDecorator(STaskCtx& ctx, uint16_t i);
        EBTStatus TickTask(STaskCtx& ctx, uint16_t i);
        EBTStatus TickParallel(STaskCtx& ctx, uint16_t i);

        // Continue composite i from child-list position `start` with
        // Sequence/Selector semantics; a result carried in from a just-completed
        // resumed child folds in before the next child is tried.
        EBTStatus TickChildrenFrom(STaskCtx& ctx, uint16_t i, uint16_t start,
                                   bool bHaveCarried, EBTStatus carried);
        EBTStatus FinishComposite(STaskCtx& ctx, uint16_t i, EBTStatus r);

        bool CondPasses(STaskCtx& ctx, uint16_t decorator_idx) const;
        void DeactivateSubtree(STaskCtx& ctx, uint16_t i);

        // Node identity events (authored names only — plan D9).
        void PostNodeEvent(STaskCtx& ctx, uint16_t i, EBTEventKind kind) const;

        std::unordered_map<uint32_t, TComp*> mComponents;
};

} // namespace SE

#endif
