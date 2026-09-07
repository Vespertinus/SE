
#ifndef APP_HSM_RESOLVER_H
#define APP_HSM_RESOLVER_H 1

#include <vector>
#include <StrID.h>
#include <hsm/StateMachineAsset.h>

namespace SE {

// Computes the exit/enter path for an HSM transition.
// Finds the lowest common ancestor (LCA) of current and target states, then
// builds exit sequence (innermost-first) and enter sequence (outermost-first).
//
// Vectors are cached as mutable members to eliminate per-call heap allocations.
// Do NOT hold returned references across subsequent calls.
class HSMResolver {
public:
        struct TransitionPath {
                std::vector<StrID> vExitStates;   // innermost first
                std::vector<StrID> vEnterStates;  // outermost first
        };

        const TransitionPath& Resolve(
                        const std::vector<StateMachineAsset::StateEntry>& states,
                        StrID current_state,
                        StrID target_state) const;

        // Returns ancestor chain from state up to root, inclusive.
        // First element is the state itself; last is a top-level state.
        const std::vector<StrID>& AncestorChain(
                        const std::vector<StateMachineAsset::StateEntry>& states,
                        StrID state) const;

private:
        static const StateMachineAsset::StateEntry* FindState(
                        const std::vector<StateMachineAsset::StateEntry>& states,
                        StrID state_id);

        mutable std::vector<StrID>  mAncestorChain;
        mutable std::vector<StrID>  mTargetChain;
        mutable TransitionPath      mTransitionPath;
};

} // namespace SE

#endif
