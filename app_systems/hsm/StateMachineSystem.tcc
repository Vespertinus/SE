
#include <EventManager.h>
#include <CommonEvents.h>
#include <hsm/StateMachineSystem.h>
#include <hsm/StateMachine.h>
#include <hsm/StateMachineAsset.h>
#include <hsm/StateCondition.h>
#include <Logging.h>

namespace SE {

StateMachineSystem::StateMachineSystem() {
        GetSystem<EventManager>().AddListener<EUpdate,   &StateMachineSystem::OnUpdate>  (this);
        GetSystem<EventManager>().AddListener<EHSMEvent, &StateMachineSystem::OnHSMEvent>(this);
}

StateMachineSystem::~StateMachineSystem() noexcept {
        GetSystem<EventManager>().RemoveListener<EUpdate,   &StateMachineSystem::OnUpdate>  (this);
        GetSystem<EventManager>().RemoveListener<EHSMEvent, &StateMachineSystem::OnHSMEvent>(this);
}

void StateMachineSystem::AddComponent(StateMachine* pComp) {
        if (!pComp || !pComp->pNode) return;
        mComponents.try_emplace(pComp->pNode->GetID(), pComp);
}

void StateMachineSystem::RemoveComponent(StateMachine* pComp) {
        if (!pComp || !pComp->pNode) return;
        mComponents.erase(pComp->pNode->GetID());
}

void StateMachineSystem::OnUpdate(const Event& e) {
        const float dt = e.Get<EUpdate>().last_frame_time;
        for (auto& [id, pComp] : mComponents) {
                if (pComp->tick_interval > 0.f) {
                        pComp->time_since_last_tick += dt;
                        if (pComp->time_since_last_tick < pComp->tick_interval) continue;
                        pComp->time_since_last_tick = 0.f;
                }
                ProcessComponent(*pComp, pComp->pNode, dt);
        }
}

void StateMachineSystem::OnHSMEvent(const Event& e) {

        const auto& ev = e.Get<EHSMEvent>();
        if (ev.pNode.expired()) return;
        auto pTarget = ev.pNode.lock();

        auto it = mComponents.find(pTarget->GetID());
        if (it == mComponents.end()) return;
        auto* pComp = it->second;

        for (uint8_t i = 0; i < ev.param_count && i < EHSMEvent::kMaxParams; ++i) {
                const auto& p = ev.params[i];
                if (std::holds_alternative<float>(p.value))
                        pComp->oParams.SetFloat(p.name, std::get<float>(p.value));
                else if (std::holds_alternative<bool>(p.value))
                        pComp->oParams.SetBool(p.name, std::get<bool>(p.value));
                else if (std::holds_alternative<int>(p.value))
                        pComp->oParams.SetInt(p.name, std::get<int>(p.value));
                else if (std::holds_alternative<SMTrigger>(p.value))
                        pComp->oParams.SetTrigger(p.name);
        }
}

void StateMachineSystem::ProcessComponent(StateMachine& comp,
                                          TSceneTree::TSceneNodeExact* pOwner,
                                          float dt) {
        const auto* pAsset = comp.GetAsset();
        if (!pAsset) return;

        comp.time_in_state += dt;

        const auto* pTransition = FindActiveTransition(*pAsset, comp);
        if (pTransition)
                ExecuteTransition(comp, pOwner, *pTransition, *pAsset);

        // Post on_update_event if set for the current state
        for (const auto& s : pAsset->GetStates()) {
                if (s.id != comp.current_state) continue;
                if (s.on_update != StrID{})
                        PostStateEvent(pOwner, s.on_update, comp.current_state, comp.previous_state);
                break;
        }
}

const StateMachineAsset::TransEntry* StateMachineSystem::FindActiveTransition(
                const StateMachineAsset& asset,
                StateMachine& comp) const {

        const auto& ancestor_chain = oResolver.AncestorChain(asset.GetStates(), comp.current_state);

        const StateMachineAsset::TransEntry* best      = nullptr;
        float                                best_prio = -std::numeric_limits<float>::infinity();

        for (const auto& t : asset.GetTransitions()) {
                // Check if 'from' matches current state or any ancestor
                bool from_match = false;
                for (const StrID& anc : ancestor_chain) {
                        if (anc == t.from) { from_match = true; break; }
                }
                if (!from_match) continue;

                if (t.exit_time > 0.f && comp.time_in_state < t.exit_time) continue;
                if (comp.in_transition && !t.can_interrupt) continue;

                // Evaluate all conditions (AND logic)
                bool conds_met = true;
                for (const auto& c : t.vConditions) {
                        StateCondition cond;
                        cond.parameter = c.parameter;
                        cond.op        = c.op;
                        cond.threshold = c.threshold;
                        if (!cond.Evaluate(comp.oParams)) { conds_met = false; break; }
                }
                if (!conds_met) continue;

                if (t.priority > best_prio) {
                        best_prio = t.priority;
                        best      = &t;
                }
        }

        return best;
}

void StateMachineSystem::ExecuteTransition(StateMachine& comp,
                                           TSceneTree::TSceneNodeExact* pOwner,
                                           const StateMachineAsset::TransEntry& transition,
                                           const StateMachineAsset& asset) {

        const auto& path = oResolver.Resolve(asset.GetStates(), comp.current_state, transition.to);

        // Fire exit events
        for (const StrID & exit_id : path.vExitStates) {
                for (const auto& s : asset.GetStates()) {
                        if (s.id != exit_id) continue;
                        if (s.on_exit != StrID{})
                                PostStateEvent(pOwner, s.on_exit, exit_id, comp.current_state);
                        break;
                }
        }

        comp.previous_state      = comp.current_state;
        comp.current_state       = transition.to;
        comp.time_in_state       = 0.f;
        comp.in_transition       = (transition.duration > 0.f);
        comp.transition_progress = 0.f;
        comp.transition_target   = transition.to;

        log_d("StateMachineSystem: '{}' → '{}'",
              static_cast<uint64_t>(comp.previous_state),
              static_cast<uint64_t>(comp.current_state));

        // Fire enter events
        for (const StrID & enter_id : path.vEnterStates) {
                for (const auto& s : asset.GetStates()) {
                        if (s.id != enter_id) continue;
                        if (s.on_enter != StrID{})
                                PostStateEvent(pOwner, s.on_enter, enter_id, comp.previous_state);
                        break;
                }
        }
}

void StateMachineSystem::PostStateEvent(TSceneTree::TSceneNodeExact* pOwner,
                                        StrID event_name,
                                        StrID state,
                                        StrID previous_state) {
        EStateMachineEvent ev;
        ev.event_name     = event_name;
        ev.state          = state;
        ev.previous_state = previous_state;
        if (pOwner)
                ev.pNode = pOwner->GetShared();
        GetSystem<EventManager>().QueueEvent(ev);
}

} // namespace SE
