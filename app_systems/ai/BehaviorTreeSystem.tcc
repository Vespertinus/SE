
#ifdef SE_IMPL

#ifndef APP_AI_BEHAVIOR_TREE_SYSTEM_TCC_INCLUDED
#define APP_AI_BEHAVIOR_TREE_SYSTEM_TCC_INCLUDED

#include <ai/BehaviorTreeTasks.tcc>
#include <ai/BehaviorTreeAsset.tcc>
#include <ai/BehaviorTree.tcc>

#include <EventManager.h>
#include <CommonEvents.h>
#include <Logging.h>

namespace SE {

template <class TTasks>
TBehaviorTreeSystem<TTasks>::TBehaviorTreeSystem() {
        GetSystem<EventManager>().AddListener<EUpdate, &TBehaviorTreeSystem::OnUpdate>(this);
}

template <class TTasks>
TBehaviorTreeSystem<TTasks>::~TBehaviorTreeSystem() noexcept {
        GetSystem<EventManager>().RemoveListener<EUpdate, &TBehaviorTreeSystem::OnUpdate>(this);
}

template <class TTasks>
void TBehaviorTreeSystem<TTasks>::AddComponent(TComp* pComp) {
        if (!pComp || !pComp->pNode) return;

        mComponents.try_emplace(pComp->pNode->GetID(), pComp);

        // Deterministic stagger phase: equal intervals spread across
        // frames because the accumulator starts pre-loaded from hash64(name).
        if (pComp->tick_interval > 0.f) {
                const uint64_t h = static_cast<uint64_t>(StrID(pComp->pNode->GetName()));
                pComp->acc = pComp->tick_interval * static_cast<float>(h % 16) / 16.f;
        } else {
                pComp->acc = 0.f;
        }
}

template <class TTasks>
void TBehaviorTreeSystem<TTasks>::RemoveComponent(TComp* pComp) {
        if (!pComp || !pComp->pNode) return;
        mComponents.erase(pComp->pNode->GetID());
}

template <class TTasks>
void TBehaviorTreeSystem<TTasks>::OnUpdate(const Event& e) {
        const float dt = e.Get<EUpdate>().last_frame_time;
        for (auto& [id, pComp] : mComponents) {
                if (pComp->tick_interval > 0.f) {
                        pComp->acc += dt;
                        if (pComp->acc < pComp->tick_interval) continue;
                        const float acc = pComp->acc;
                        pComp->acc = 0.f;
                        ProcessComponent(*pComp, pComp->pNode, acc);
                } else {
                        ProcessComponent(*pComp, pComp->pNode, dt);
                }
        }
}

template <class TTasks>
void TBehaviorTreeSystem<TTasks>::ProcessComponent(TComp& comp,
                                                   TSceneTree::TSceneNodeExact* pOwner,
                                                   float dt) {
        const auto* pAsset = comp.GetAsset();
        if (!pAsset || pAsset->NodeCount() == 0) return;

        if (!comp.bBindingsResolved) comp.ResolveBindings();

        STaskCtx ctx;
        ctx.nodes     = pAsset->GetNodes().data();
        ctx.params    = pAsset->GetParams().data();
        ctx.child_indices = pAsset->GetChildIndices().data();
        ctx.states    = comp.vStates.data();
        ctx.bindings  = comp.vBindings.data();
        ctx.pNode     = pOwner;
        ctx.dt        = dt;
        ctx.pEntityBB = comp.EntityCore();
        ctx.pGlobalBB = comp.GlobalCore();

        TickNode(ctx, 0);

        ++comp.tick_count;
        comp.last_tick_dt = dt;
}

// --- node dispatch -----------------------------------------------------------

template <class TTasks>
EBTStatus TBehaviorTreeSystem<TTasks>::TickNode(STaskCtx& ctx, uint16_t i) {
        ctx.node_index = i;
        switch (ctx.nodes[i].type) {
                case FlatBuffers::BTNodeU::BTComposite: return TickComposite(ctx, i);
                case FlatBuffers::BTNodeU::BTDecorator: return TickDecorator(ctx, i);
                default:                                return TickTask(ctx, i);
        }
}

// --- tasks -------------------------------------------------------------------

template <class TTasks>
EBTStatus TBehaviorTreeSystem<TTasks>::TickTask(STaskCtx& ctx, uint16_t i) {
        const SNode&  n  = ctx.nodes[i];
        SNodeState&   st = ctx.states[i];
        const auto&   ex = TTable::At(n.task_index);

        if (st.status != EBTNodeStatus::Running) {
                st.EnterTask();
                ex.fnInit(ctx);
                PostNodeEvent(ctx, i, EBTEventKind::Enter);
        }

        EBTStatus r = ex.fnTick(ctx);

        if (r != EBTStatus::Running) {
                st.status = (r == EBTStatus::Success) ? EBTNodeStatus::Succeeded
                                                      : EBTNodeStatus::Failed;
                ex.fnStop(ctx);   // symmetric on abort and completion
                PostNodeEvent(ctx, i, EBTEventKind::Exit);
        }
        return r;
}

// --- decorators ---------------------------------------------------------------

template <class TTasks>
EBTStatus TBehaviorTreeSystem<TTasks>::TickDecorator(STaskCtx& ctx, uint16_t i) {
        const SNode& n  = ctx.nodes[i];
        SNodeState&  st = ctx.states[i];

        const bool bCooldown = (n.decorator_kind == FlatBuffers::EBTDecoratorKind::Cooldown);
        if (bCooldown) st.timer += ctx.dt;   // cross-entry: accumulates while idle too (D7)

        if (st.status != EBTNodeStatus::Running) {
                // Entry gates. A failed gate is a failed branch — no events, the
                // parent composite moves on in this tick. A Cooldown gates only
                // RE-entry: until its first completion (counter == 0) it passes.
                if (bCooldown && st.counter > 0 && st.timer < n.duration) {
                        st.status = EBTNodeStatus::Failed;
                        return EBTStatus::Failure;
                }
                if (n.decorator_kind == FlatBuffers::EBTDecoratorKind::BlackboardCondition &&
                    !CondPasses(ctx, i)) {
                        st.status = EBTNodeStatus::Failed;
                        return EBTStatus::Failure;
                }
                if (n.decorator_kind == FlatBuffers::EBTDecoratorKind::TimeLimit) st.timer = 0.f;
                if (n.decorator_kind == FlatBuffers::EBTDecoratorKind::Loop)      st.counter = 0;

                st.status = EBTNodeStatus::Running;
                PostNodeEvent(ctx, i, EBTEventKind::Enter);
        } else if (n.abort == FlatBuffers::EBTAbort::Self &&
                   n.decorator_kind == FlatBuffers::EBTDecoratorKind::BlackboardCondition &&
                   !CondPasses(ctx, i)) {
                // Self abort on the active path: subtree dies now, this tick; 
                // Failure propagates and the parent advances immediately.
                DeactivateSubtree(ctx, n.decorated_child);
                st.status = EBTNodeStatus::Failed;
                PostNodeEvent(ctx, i, EBTEventKind::Exit);
                return EBTStatus::Failure;
        }

        EBTStatus r = TickNode(ctx, static_cast<uint16_t>(n.decorated_child));

        switch (n.decorator_kind) {
                case FlatBuffers::EBTDecoratorKind::Cooldown:
                        if (r != EBTStatus::Running) {
                                st.timer = 0.f;    // restart the gate
                                ++st.counter;      // armed: gates re-entry (see above)
                        }
                        break;

                case FlatBuffers::EBTDecoratorKind::TimeLimit:
                        if (r == EBTStatus::Running) {
                                st.timer += ctx.dt;
                                if (st.timer >= n.duration) {
                                        DeactivateSubtree(ctx, static_cast<uint16_t>(n.decorated_child));
                                        r = EBTStatus::Failure;
                                }
                        }
                        break;

                case FlatBuffers::EBTDecoratorKind::Loop:
                        if (r == EBTStatus::Success) {
                                ++st.counter;
                                if (n.count == 0 || st.counter < static_cast<uint32_t>(n.count)) {
                                        // next iteration next tree tick — no same-tick spin
                                        DeactivateSubtree(ctx, static_cast<uint16_t>(n.decorated_child));
                                        r = EBTStatus::Running;
                                }
                        }
                        // Failure propagates as-is; Running passes through.
                        break;

                case FlatBuffers::EBTDecoratorKind::ForceStatus:
                        if (r != EBTStatus::Running) {
                                r = (n.force_to == FlatBuffers::EBTForceTo::Success)
                                    ? EBTStatus::Success : EBTStatus::Failure;
                        }
                        break;

                default:
                        break;
        }

        if (r != EBTStatus::Running) {
                st.status = (r == EBTStatus::Success) ? EBTNodeStatus::Succeeded
                                                      : EBTNodeStatus::Failed;
                PostNodeEvent(ctx, i, EBTEventKind::Exit);
        }
        return r;
}

// --- composites ----------------------------------------------------------------

template <class TTasks>
EBTStatus TBehaviorTreeSystem<TTasks>::TickComposite(STaskCtx& ctx, uint16_t i) {
        const SNode& n  = ctx.nodes[i];
        SNodeState&  st = ctx.states[i];

        // Parallel ticks every child itself — no memory-resume semantics.
        if (n.composite_kind == FlatBuffers::EBTCompositeKind::Parallel) {
                return TickParallel(ctx, i);
        }

        if (st.status != EBTNodeStatus::Running) {
                st.status        = EBTNodeStatus::Running;
                st.running_child = -1;
                // A re-entered composite starts every branch fresh (Parallel keeps
                // terminal child statuses only while IT runs).
                for (uint16_t k = 0; k < n.child_count; ++k) {
                        DeactivateSubtree(ctx, static_cast<uint16_t>(ctx.child_indices[n.child_first + k]));
                }
                PostNodeEvent(ctx, i, EBTEventKind::Enter);
        }

        if (st.running_child >= 0) {
                const int32_t rc = st.running_child;

                // LowerPriority preemption: only direct children before
                // the running one are observed.
                for (uint16_t k = 0; k < n.child_count; ++k) {
                        const int32_t s = ctx.child_indices[n.child_first + k];
                        if (s >= rc) break;
                        const SNode& sib = ctx.nodes[s];
                        if (sib.type == FlatBuffers::BTNodeU::BTDecorator &&
                            sib.abort == FlatBuffers::EBTAbort::LowerPriority &&
                            sib.decorator_kind == FlatBuffers::EBTDecoratorKind::BlackboardCondition &&
                            CondPasses(ctx, static_cast<uint16_t>(s))) {
                                DeactivateSubtree(ctx, static_cast<uint16_t>(rc));
                                st.running_child = -1;
                                return TickChildrenFrom(ctx, i, k, false, EBTStatus::Failure);
                        }
                }

                const EBTStatus r = TickNode(ctx, static_cast<uint16_t>(rc));
                if (r == EBTStatus::Running) return EBTStatus::Running;

                // Resume completed: the composite advances with the child's result
                // folded in — same tick, no latency.
                st.running_child = -1;
                uint16_t k_rc = 0;
                for (uint16_t k = 0; k < n.child_count; ++k) {
                        if (ctx.child_indices[n.child_first + k] == rc) { k_rc = k; break; }
                }
                return TickChildrenFrom(ctx, i, k_rc + 1, true, r);
        }

        return TickChildrenFrom(ctx, i, 0, false, EBTStatus::Failure);
}

template <class TTasks>
EBTStatus TBehaviorTreeSystem<TTasks>::TickChildrenFrom(STaskCtx& ctx, uint16_t i, uint16_t start,
                                                        bool bHaveCarried, EBTStatus carried) {
        const SNode& n  = ctx.nodes[i];
        SNodeState&  st = ctx.states[i];

        const bool bSequence = (n.composite_kind == FlatBuffers::EBTCompositeKind::Sequence);

        // A carried result from a just-completed resumed child folds into the
        // composite semantics before the next child is tried.
        if (bHaveCarried) {
                if (bSequence && carried == EBTStatus::Failure) return FinishComposite(ctx, i, carried);
                if (!bSequence && carried == EBTStatus::Success) return FinishComposite(ctx, i, carried);
        }

        for (uint16_t k = start; k < n.child_count; ++k) {
                const uint16_t s = static_cast<uint16_t>(ctx.child_indices[n.child_first + k]);
                const EBTStatus r = TickNode(ctx, s);
                if (r == EBTStatus::Running) {
                        st.running_child = s;
                        return EBTStatus::Running;
                }
                if (bSequence && r == EBTStatus::Failure) return FinishComposite(ctx, i, r);
                if (!bSequence && r == EBTStatus::Success) return FinishComposite(ctx, i, r);
        }

        return FinishComposite(ctx, i, bSequence ? EBTStatus::Success : EBTStatus::Failure);
}

template <class TTasks>
EBTStatus TBehaviorTreeSystem<TTasks>::TickParallel(STaskCtx& ctx, uint16_t i) {
        const SNode& n  = ctx.nodes[i];
        SNodeState&  st = ctx.states[i];

        if (st.status != EBTNodeStatus::Running) {
                st.status        = EBTNodeStatus::Running;
                st.running_child = -1;
                for (uint16_t k = 0; k < n.child_count; ++k) {
                        DeactivateSubtree(ctx, static_cast<uint16_t>(ctx.child_indices[n.child_first + k]));
                }
                PostNodeEvent(ctx, i, EBTEventKind::Enter);
        }

        const uint16_t total     = n.child_count;
        const uint16_t required  = (n.n_required == 0) ? total : n.n_required;

        uint16_t succeeded = 0;
        uint16_t failed    = 0;
        for (uint16_t k = 0; k < total; ++k) {
                const uint16_t s = static_cast<uint16_t>(ctx.child_indices[n.child_first + k]);
                const SNodeState& cs = ctx.states[s];
                if (cs.status == EBTNodeStatus::Succeeded) { ++succeeded; continue; }
                if (cs.status == EBTNodeStatus::Failed)    { ++failed;    continue; }

                const EBTStatus r = TickNode(ctx, s);
                if (r == EBTStatus::Running) continue;
                if (r == EBTStatus::Success) ++succeeded; else ++failed;
        }

        // Completion deactivates still-running siblings (Stop hooks fire).
        const auto Finish = [&](EBTStatus result) {
                for (uint16_t k = 0; k < total; ++k) {
                        const uint16_t s = static_cast<uint16_t>(ctx.child_indices[n.child_first + k]);
                        if (ctx.states[s].status == EBTNodeStatus::Running) {
                                DeactivateSubtree(ctx, s);
                        }
                }
                return FinishComposite(ctx, i, result);
        };

        if (succeeded >= required) return Finish(EBTStatus::Success);
        if ((int)failed > (int)total - (int)required) return Finish(EBTStatus::Failure);
        return EBTStatus::Running;
}

template <class TTasks>
EBTStatus TBehaviorTreeSystem<TTasks>::FinishComposite(STaskCtx& ctx, uint16_t i, EBTStatus r) {
        SNodeState& st = ctx.states[i];
        st.status        = (r == EBTStatus::Success) ? EBTNodeStatus::Succeeded
                                                     : EBTNodeStatus::Failed;
        st.running_child = -1;
        PostNodeEvent(ctx, i, EBTEventKind::Exit);
        return r;
}

// --- conditions / deactivation / events -----------------------------------------

template <class TTasks>
bool TBehaviorTreeSystem<TTasks>::CondPasses(STaskCtx& ctx, uint16_t decorator_idx) const {
        const SBBBinding& b = ctx.bindings[decorator_idx];
        if (!b.valid || !b.pCore->IsInitialized()) return false;

        const SNode& n = ctx.nodes[decorator_idx];
        const BlackboardCore& core = *b.pCore;

        switch (n.op) {
                case FlatBuffers::SMConditionOp::Greater:      return core.GetFloat(b.slot) >  n.threshold;
                case FlatBuffers::SMConditionOp::GreaterEqual: return core.GetFloat(b.slot) >= n.threshold;
                case FlatBuffers::SMConditionOp::Less:         return core.GetFloat(b.slot) <  n.threshold;
                case FlatBuffers::SMConditionOp::LessEqual:    return core.GetFloat(b.slot) <= n.threshold;
                case FlatBuffers::SMConditionOp::Equal:        return core.GetFloat(b.slot) == n.threshold;
                case FlatBuffers::SMConditionOp::NotEqual:     return core.GetFloat(b.slot) != n.threshold;
                case FlatBuffers::SMConditionOp::IsTrue:       return core.GetBool(b.slot);
                case FlatBuffers::SMConditionOp::IsFalse:      return !core.GetBool(b.slot);
                default:                                       return false;
        }
}

template <class TTasks>
void TBehaviorTreeSystem<TTasks>::DeactivateSubtree(STaskCtx& ctx, uint16_t i) {
        const SNode& n  = ctx.nodes[i];
        SNodeState&  st = ctx.states[i];

        const bool bWasRunning = (st.status == EBTNodeStatus::Running);

        switch (n.type) {
                case FlatBuffers::BTNodeU::BTTask: {
                        if (bWasRunning) {
                                ctx.node_index = i;
                                TTable::At(n.task_index).fnStop(ctx);
                        }
                        st.status  = EBTNodeStatus::Idle;
                        st.timer   = 0.f;
                        st.counter = 0;
                        st.mem.fill(std::byte{0});
                        break;
                }
                case FlatBuffers::BTNodeU::BTComposite: {
                        for (uint16_t k = 0; k < n.child_count; ++k) {
                                DeactivateSubtree(ctx, static_cast<uint16_t>(ctx.child_indices[n.child_first + k]));
                        }
                        st.running_child = -1;
                        st.status        = EBTNodeStatus::Idle;
                        break;
                }
                default: {
                        // Decorator: recurse; `timer` is NOT touched — a Cooldown
                        // gate survives aborts; TimeLimit/Loop re-init
                        // at entry.
                        DeactivateSubtree(ctx, static_cast<uint16_t>(n.decorated_child));
                        st.status = EBTNodeStatus::Idle;
                        break;
                }
        }

        if (bWasRunning) PostNodeEvent(ctx, i, EBTEventKind::Exit);
}

template <class TTasks>
void TBehaviorTreeSystem<TTasks>::PostNodeEvent(STaskCtx& ctx, uint16_t i, EBTEventKind kind) const {
        if (ctx.nodes[i].name == StrID{}) return;   // unnamed nodes stay silent (D9)

        EBTEvent ev;
        ev.name   = ctx.nodes[i].name;
        ev.kind   = kind;
        ev.entity = ctx.pNode ? ctx.pNode->GetShared() : TSceneTree::TSceneNodeWeak{};
        GetSystem<EventManager>().QueueEvent(ev);
}

} // namespace SE

#endif // APP_AI_BEHAVIOR_TREE_SYSTEM_TCC_INCLUDED

#endif // SE_IMPL
