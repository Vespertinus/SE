
#ifdef SE_IMPL

#include <hsm/StateMachineAsset.h>
#include <hsm/StateCondition.h>
#include <Logging.h>

#include <StateMachine_generated.h>
#include <flatbuffers/flatbuffers.h>

#include <fstream>

namespace SE {

StateMachineAsset::StateMachineAsset(const std::string& sName, rid_t rid)
        : ResourceHolder(rid, sName) {

        std::ifstream f(sName, std::ios::binary | std::ios::ate);
        if (!f.is_open()) {
                log_e("StateMachineAsset: failed to open '{}'", sName);
                return;
        }

        const size_t sz = static_cast<size_t>(f.tellg());
        f.seekg(0);
        std::vector<uint8_t> raw(sz);
        f.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(sz));

        flatbuffers::Verifier verifier(raw.data(), raw.size());
        if (!SE::FlatBuffers::VerifyStateMachineDefinitionBuffer(verifier)) {
                log_e("StateMachineAsset: FlatBuffer verify failed for '{}'", sName);
                return;
        }

        Build(SE::FlatBuffers::GetStateMachineDefinition(raw.data()));
        size = static_cast<uint32_t>(sz);

        log_d("StateMachineAsset: loaded '{}' ({} bytes, {} states, {} transitions)",
                        sName,
                        sz,
                        vStates.size(),
                        vTransitions.size());
}

StateMachineAsset::StateMachineAsset(const std::string& sName, rid_t rid,
                                     const FlatBuffers::StateMachineDefinition* pDef)
        : ResourceHolder(rid, sName) {

        Build(pDef);
        log_d("StateMachineAsset: built inline '{}' ({} states, {} transitions)",
                        sName,
                        vStates.size(),
                        vTransitions.size());
}

void StateMachineAsset::Build(const FlatBuffers::StateMachineDefinition* pDef) {

        if (!pDef) return;

        if (pDef->initial_state())
                initial_state = StrID(pDef->initial_state()->str());

        if (pDef->states()) {
                for (const auto* s : *pDef->states()) {
                        if (!s || !s->id()) continue;
                        StateEntry e;
                        e.id       = StrID(s->id()->str());
                        e.parent   = (s->parent()          && s->parent()->size() > 0)
                                ? StrID(s->parent()->str()) : StrID{};
                        e.on_enter = (s->on_enter_event()  && s->on_enter_event()->size() > 0)
                                ? StrID(s->on_enter_event()->str()) : StrID{};
                        e.on_exit  = (s->on_exit_event()   && s->on_exit_event()->size() > 0)
                                ? StrID(s->on_exit_event()->str()) : StrID{};
                        e.on_update= (s->on_update_event() && s->on_update_event()->size() > 0)
                                ? StrID(s->on_update_event()->str()) : StrID{};
                        vStates.push_back(std::move(e));
                }
        }

        if (pDef->transitions()) {
                for (const auto* t : *pDef->transitions()) {
                        if (!t || !t->from() || !t->to()) continue;
                        TransEntry e;
                        e.from         = StrID(t->from()->str());
                        e.to           = StrID(t->to()->str());
                        e.priority     = t->priority();
                        e.duration     = t->duration();
                        e.exit_time    = t->exit_time();
                        e.can_interrupt= t->can_interrupt();
                        if (t->conditions()) {
                                for (const auto* c : *t->conditions()) {
                                        if (!c) continue;
                                        CondEntry ce;
                                        if (c->parameter()) ce.parameter = StrID(c->parameter()->str());
                                        ce.op        = c->op();
                                        ce.threshold = c->threshold();
                                        e.vConditions.push_back(ce);
                                }
                        }
                        vTransitions.push_back(std::move(e));
                }
        }

        if (pDef->parameters()) {
                for (const auto* p : *pDef->parameters()) {
                        if (!p || !p->name()) continue;
                        ParamDefault pd;
                        pd.sName = p->name()->str();
                        pd.name  = StrID(pd.sName);

                        using PV = FlatBuffers::SMParamValue;
                        switch (p->value_type()) {
                                case PV::SMParamFloat: {
                                                               const auto* fp = p->value_as_SMParamFloat();
                                                               pd.value = fp ? fp->value() : 0.f;
                                                               break;
                                                       }
                                case PV::SMParamBool: {
                                                              const auto* bp = p->value_as_SMParamBool();
                                                              pd.value = bp ? (bp->value() != 0) : false;
                                                              break;
                                                      }
                                case PV::SMParamInt: {
                                                             const auto* ip = p->value_as_SMParamInt();
                                                             pd.value = ip ? ip->value() : 0;
                                                             break;
                                                     }
                                case PV::SMParamTrigger:
                                                     pd.value = SMTrigger{false}; // triggers default to unset
                                                     break;
                                default: break;
                        }
                        vDefaults.push_back(std::move(pd));
                }
        }
}

std::string StateMachineAsset::Str() const {
        return fmt::format("StateMachineAsset['{}' initial='{}' states={} transitions={}]",
                        sName,
                        static_cast<uint64_t>(initial_state),
                        vStates.size(),
                        vTransitions.size());
}

} // namespace SE

#endif // SE_IMPL
