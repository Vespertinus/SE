
#include <hsm/HSMResolver.h>

namespace SE {

const StateMachineAsset::StateEntry* HSMResolver::FindState(
                const std::vector<StateMachineAsset::StateEntry>& states,
                StrID state_id) {

        for (const auto& s : states) {
                if (s.id == state_id) return &s;
        }
        return nullptr;
}

const std::vector<StrID>& HSMResolver::AncestorChain(
                const std::vector<StateMachineAsset::StateEntry>& states,
                StrID state) const {

        mAncestorChain.clear();
        StrID cur = state;

        for (;;) {
                mAncestorChain.push_back(cur);
                const auto* s = FindState(states, cur);
                if (!s || s->parent == StrID{}) break;
                if (s->parent == cur) break; // guard against malformed data
                cur = s->parent;
        }
        return mAncestorChain;
}

const HSMResolver::TransitionPath& HSMResolver::Resolve(
                const std::vector<StateMachineAsset::StateEntry>& states,
                StrID current_state,
                StrID target_state) const {

        mTransitionPath.vExitStates.clear();
        mTransitionPath.vEnterStates.clear();

        if (current_state == target_state) return mTransitionPath;

        // Build ancestor chains (note: AncestorChain overwrites mAncestorChain)
        mTargetChain.clear();
        {
                StrID cur = target_state;
                for (;;) {
                        mTargetChain.push_back(cur);
                        const auto* s = FindState(states, cur);
                        if (!s || s->parent == StrID{}) break;
                        if (s->parent == cur) break;
                        cur = s->parent;
                }
        }

        // Current chain goes into mAncestorChain
        AncestorChain(states, current_state); // fills mAncestorChain

        // Find lowest common ancestor
        StrID lca{};
        bool  found_lca = false;
        for (const StrID& a : mAncestorChain) {
                for (const StrID& b : mTargetChain) {
                        if (a == b) { lca = a; found_lca = true; break; }
                }
                if (found_lca) break;
        }

        // Exit: from current up to (not including) LCA
        for (const StrID& s : mAncestorChain) {
                if (found_lca && s == lca) break;
                mTransitionPath.vExitStates.push_back(s);
        }

        // Enter: target chain states below LCA, reversed to outermost-first
        std::vector<StrID> enter_below_lca;
        for (const StrID& s : mTargetChain) {
                if (found_lca && s == lca) break;
                enter_below_lca.push_back(s);
        }
        mTransitionPath.vEnterStates.assign(enter_below_lca.rbegin(), enter_below_lca.rend());

        return mTransitionPath;
}

} // namespace SE
