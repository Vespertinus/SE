
#ifndef APP_STATE_MACHINE_ASSET_H
#define APP_STATE_MACHINE_ASSET_H 1

#include <string>
#include <vector>

#include <ResourceHolder.h>
#include <StrID.h>
#include <hsm/ParameterStore.h>
#include <StateMachine_generated.h>

namespace SE {

// Shared, immutable state machine asset built from a .sesm binary.
// All data is baked from FlatBuffers at construction; the raw buffer is discarded.
// Use H<StateMachineAsset> to share across components.
class StateMachineAsset : public ResourceHolder {

public:
        struct StateEntry {
                StrID id;
                StrID parent;
                StrID on_enter;
                StrID on_exit;
                StrID on_update;
        };

        struct CondEntry {
                StrID                      parameter;
                FlatBuffers::SMConditionOp op = FlatBuffers::SMConditionOp::IsTrue;
                float                      threshold = 0.f;
        };

        struct TransEntry {
                StrID                    from;
                StrID                    to;
                float                    priority     = 0.f;
                float                    duration     = 0.f;
                float                    exit_time    = 0.f;
                bool                     can_interrupt = false;
                std::vector<CondEntry>   vConditions;
        };

        struct ParamDefault {
                StrID        name;
                std::string  sName;   // original string, for debug display
                TParamValue  value;
        };

        // Production: loads .sesm file, builds tables, discards raw bytes.
        StateMachineAsset(const std::string& sName, rid_t rid);

        // Test / inline embedding: builds tables from an in-memory definition.
        StateMachineAsset(const std::string& sName, rid_t rid,
                          const FlatBuffers::StateMachineDefinition* pDef);

        StrID                            GetInitialState()  const { return initial_state; }
        const std::vector<StateEntry>&   GetStates()        const { return vStates; }
        const std::vector<TransEntry>&   GetTransitions()   const { return vTransitions; }
        const std::vector<ParamDefault>& GetDefaults()      const { return vDefaults; }

        std::string Str() const;

private:
        StrID                     initial_state;
        std::vector<StateEntry>   vStates;
        std::vector<TransEntry>   vTransitions;
        std::vector<ParamDefault> vDefaults;

        void Build(const FlatBuffers::StateMachineDefinition* pDef);
};

} // namespace SE

#endif
