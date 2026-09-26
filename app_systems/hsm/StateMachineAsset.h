
#ifndef APP_STATE_MACHINE_ASSET_H
#define APP_STATE_MACHINE_ASSET_H 1

#include <string>
#include <vector>

#include <ResourceHolder.h>
#include <StrID.h>
#include <hsm/ParameterStore.h>
#include <StateMachine_generated.h>
#include <world/ResourcePinCore.h>

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

// Declared-dependency pin (lives here, next to the resource: an explicit
// specialization compiles in every TU that includes this header).
template <> struct TResourcePin<StateMachineAsset> {
        using THolder    = FlatBuffers::StateMachineHolder;
        using TKeyHolder = FlatBuffers::StateMachineHolderT;

        static H<StateMachineAsset> Create(const THolder * pH) {
                if (pH->path() && pH->path()->size() > 0) {
                        return CreateResource<StateMachineAsset>(pH->path()->str());
                }
                if (pH->name() && pH->state_machine()) {
                        return CreateResource<StateMachineAsset>(pH->name()->str(), pH->state_machine());
                }

                log_w("ResourcePin<StateMachineAsset>: anonymous inline state machine cannot be pinned");
                return H<StateMachineAsset>::Null();
        }
};

} // namespace SE

#endif
