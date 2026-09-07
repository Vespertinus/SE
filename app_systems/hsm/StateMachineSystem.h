
#ifndef APP_STATE_MACHINE_SYSTEM_H
#define APP_STATE_MACHINE_SYSTEM_H 1

#include <unordered_map>
#include <StrID.h>
#include <hsm/HSMResolver.h>
#include <hsm/StateMachineAsset.h>

namespace SE {

class StateMachine;

// Drives all registered StateMachines each EUpdate.
// Components self-register via Enable() / Disable().
// Also handles EHSMEvent to route targeted parameter updates.
class StateMachineSystem {
public:
        StateMachineSystem();
        ~StateMachineSystem() noexcept;

        void AddComponent(StateMachine* pComp);
        void RemoveComponent(StateMachine* pComp);

        // Advance one component directly — useful for scripted or test scenarios.
        void ProcessComponent(StateMachine& comp,
                              TSceneTree::TSceneNodeExact* pOwner,
                              float dt);

private:
        void OnUpdate(const Event& e);
        void OnHSMEvent(const Event& e);

        const StateMachineAsset::TransEntry* FindActiveTransition(
                        const StateMachineAsset& asset,
                        StateMachine& comp) const;

        void ExecuteTransition(StateMachine& comp,
                               TSceneTree::TSceneNodeExact* pOwner,
                               const StateMachineAsset::TransEntry& transition,
                               const StateMachineAsset& asset);

        void PostStateEvent(TSceneTree::TSceneNodeExact* pOwner,
                            StrID event_name,
                            StrID state,
                            StrID previous_state);

        HSMResolver                                          oResolver;
        std::unordered_map<uint32_t, StateMachine*> mComponents;
};

} // namespace SE

#endif
