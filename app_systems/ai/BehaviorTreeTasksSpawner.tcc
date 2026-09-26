
#ifdef SE_IMPL

#ifndef APP_AI_BEHAVIOR_TREE_TASKS_SPAWNER_TCC_INCLUDED
#define APP_AI_BEHAVIOR_TREE_TASKS_SPAWNER_TCC_INCLUDED

// Opt-in spawner hook bodies: only compositions that concatenate
// TSpawnerBTTasks include this file (from the app's shadow App.h impl block) —
// the calls below instantiate GetSystem<SpawnerSystem>, a compile-time-checked
// system lookup.

#include <ai/BehaviorTreeTasks.h>
#include <Logging.h>

#include <spawner/SpawnerSystem.h>
#include <spawner/Population.h>

namespace SE {

namespace {
// Same param-check rule as the core tasks (BehaviorTreeTasks.tcc helpers are
// TU-local there): a string param 'population' must be authored.
bool HasPopulationParam(const FlatBuffers::BTTask* pTask) {
        if (!pTask || !pTask->params()) return false;
        const StrID key("population");
        for (const auto* p : *pTask->params()) {
                if (p && p->key() && StrID(p->key()->str()) == key &&
                    p->value_type() == FlatBuffers::BTParamValue::BTParamString) return true;
        }
        return false;
}
} // namespace

void BTSpawnFromPopulation::Validate(const FlatBuffers::BTTask* pTask) {
        if (!HasPopulationParam(pTask)) {
                throw std::runtime_error("BT task 'SpawnFromPopulation': requires string param 'population'");
        }
}

EBTStatus BTSpawnFromPopulation::Tick(STaskCtx& ctx) {
        const SParam* p = ctx.FindParam("population");
        if (!p) return EBTStatus::Failure;

        const StrID pop_id(p->s);

        auto& spawner = GetSystem<SpawnerSystem>();
        for (Population* pPop : spawner.vPopulations) {
                if (!pPop || pPop->Id() != pop_id) continue;

                // member: Int param (default 0) — which authored member to spawn.
                uint32_t member = 0;
                if (const SParam* pMember = ctx.FindParam("member");
                    pMember && pMember->type == EBTParamType::Int && pMember->i >= 0) {
                        member = static_cast<uint32_t>(pMember->i);
                }

                return spawner.ExecutePopulationSpawn(pPop, member)
                             ? EBTStatus::Success : EBTStatus::Failure;
        }

        SNodeState& st = ctx.states[ctx.node_index];
        if (!(st.flags & SNodeState::kWarnedOnce)) {
                st.flags |= SNodeState::kWarnedOnce;
                log_w("BT task 'SpawnFromPopulation': population '{}' not registered", p->s);
        }
        return EBTStatus::Failure;
}

} // namespace SE

#endif // APP_AI_BEHAVIOR_TREE_TASKS_SPAWNER_TCC_INCLUDED

#endif // SE_IMPL
