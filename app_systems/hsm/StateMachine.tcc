
#include <hsm/StateMachine.h>
#include <hsm/StateMachineSystem.h>
#include <hsm/StateMachineAsset.h>
#include <StateMachine_generated.h>
#include <Component_generated.h>
#include <Logging.h>

namespace SE {

StateMachine::StateMachine(TSceneTree::TSceneNodeExact* pNewNode,
                                             const Desc& desc)
        : pNode(pNewNode) {

        if (!desc.definition_path || desc.definition_path[0] == '\0')
                throw std::runtime_error("StateMachine: empty definition_path");

        Init(desc.definition_path, nullptr, desc.tick_interval);
}

StateMachine::StateMachine(TSceneTree::TSceneNodeExact* pNewNode,
                                             const FlatBuffers::StateMachine* pFB)
        : pNode(pNewNode) {

        if (!pFB || !pFB->hms())
                throw std::runtime_error("StateMachine: null FlatBuffer");

        const auto* hms = pFB->hms();

        if (hms->state_machine() && hms->name()) {
                // Inline embedded definition — create owned asset directly
                //Init(nullptr, hms->state_machine(), pFB->tick_interval());
                Init(hms->name()->c_str(), hms->state_machine(), pFB->tick_interval());
        } else if (hms->path() && hms->path()->size() > 0) {
                Init(hms->path()->c_str(), nullptr, pFB->tick_interval());
        } else {
                throw std::runtime_error(fmt::format(
                        "StateMachine: StateMachineHolder has neither path ({:p}) nor state_machine ({:p}) and name ({:p})",
                        (void*)hms->path(),
                        (void*)hms->state_machine(),
                        (void*)hms->name()
                        ));
        }
}

void StateMachine::Init(const char* definition_path,
                                 const FlatBuffers::StateMachineDefinition* pInlineDef,
                                 float tick_interval_val) {

        tick_interval = tick_interval_val;

        const StateMachineAsset* pAsset = nullptr;

        if (pInlineDef) {
                hDefinition = CreateResource<StateMachineAsset>(definition_path, pInlineDef);
                pAsset = GetResource(hDefinition);
        } else {
                hDefinition = CreateResource<StateMachineAsset>(definition_path);
                pAsset = GetResource(hDefinition);
        }

        if (!pAsset) {
                throw std::runtime_error(
                        fmt::format("StateMachine: failed to load '{}'",
                                    definition_path ? definition_path : "<empty>"));
        }

        current_state = pAsset->GetInitialState();

        for (const auto& d : pAsset->GetDefaults()) {
                if (std::holds_alternative<float>(d.value))
                        oParams.SetFloat(d.name, std::get<float>(d.value));
                else if (std::holds_alternative<bool>(d.value))
                        oParams.SetBool(d.name, std::get<bool>(d.value));
                else if (std::holds_alternative<int>(d.value))
                        oParams.SetInt(d.name, std::get<int>(d.value));
                // SMTrigger defaults to unset — nothing to do
        }
}

StateMachine::~StateMachine() noexcept {
        Disable();
}

void StateMachine::Enable() {
        GetSystem<StateMachineSystem>().AddComponent(this);
}

void StateMachine::Disable() {
        GetSystem<StateMachineSystem>().RemoveComponent(this);
}

const StateMachineAsset* StateMachine::GetAsset() const {
        return GetResource(hDefinition);
}

std::string StateMachine::Str() const {
        return fmt::format("StateMachine[state='{}' time_in_state={:.2f}]",
                           static_cast<uint64_t>(current_state),
                           time_in_state);
}

} // namespace SE
