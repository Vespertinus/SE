
#ifdef SE_IMPL

// Xlib (via graphics headers in mixed TUs) does `#define Bool int` —
// keep it out of this file; generated union members use Bool as a
// scope-qualified identifier (same dance as the entity plugins).
#pragma push_macro("Bool")
#undef Bool

#ifndef APP_AI_BEHAVIORTREE_TCC
#define APP_AI_BEHAVIORTREE_TCC 1


#include <ai/BehaviorTree.h>
#include <ai/BehaviorTreeAsset.h>
#include <ai/BehaviorTreeAsset.tcc>
#include <ai/BehaviorTreeSystem.h>
#include <ai/Blackboard.h>
#include <ai/BlackboardSystem.h>
#include <Logging.h>
#include <GlobalTypes.h>

namespace SE {

template <class TTasks>
TBehaviorTree<TTasks>::TBehaviorTree(TSceneTree::TSceneNodeExact* pNewNode, const Desc& desc)
        : pNode(pNewNode) {

        if (!desc.tree_path || desc.tree_path[0] == '\0') {
                throw std::runtime_error("BehaviorTree: empty tree_path");
        }
        Init(desc.tree_path, nullptr, desc.tick_interval);
}

template <class TTasks>
TBehaviorTree<TTasks>::TBehaviorTree(TSceneTree::TSceneNodeExact* pNewNode,
                                     const FlatBuffers::BehaviorTree* pFB)
        : pNode(pNewNode) {

        if (!pFB || !pFB->tree()) {
                throw std::runtime_error("BehaviorTree: null FlatBuffer");
        }

        const auto* holder = pFB->tree();

        if (holder->behavior_tree() && holder->name()) {
                // Inline embedded definition — name is the resource cache key.
                Init(holder->name()->c_str(), holder->behavior_tree(), pFB->tick_interval());
        } else if (holder->path() && holder->path()->size() > 0) {
                Init(holder->path()->c_str(), nullptr, pFB->tick_interval());
        } else {
                throw std::runtime_error(
                                "BehaviorTree: BehaviorTreeHolder has neither path nor inline definition + name");
        }
}

template <class TTasks>
TBehaviorTree<TTasks>::~TBehaviorTree() noexcept {
        Disable();
}

template <class TTasks>
void TBehaviorTree<TTasks>::Init(const char* szTreePath,
                                 const FlatBuffers::BehaviorTreeDefinition* pInlineDef,
                                 float tick_interval_val) {

        if (pInlineDef) {
                hTree = CreateResource<TAsset>(szTreePath, pInlineDef);
        } else {
                hTree = CreateResource<TAsset>(szTreePath);
        }

        const TAsset* pAsset = GetResource(hTree);
        if (!pAsset) {
                throw std::runtime_error(
                                fmt::format("BehaviorTree: failed to load '{}'",
                                            szTreePath ? szTreePath : "<empty>"));
        }

        tick_interval = tick_interval_val;
        vStates.resize(pAsset->NodeCount());
        vBindings.resize(pAsset->NodeCount());
}

template <class TTasks>
BlackboardCore* TBehaviorTree<TTasks>::EntityCore() {
        return pSiblingBB ? &pSiblingBB->GetCore() : nullptr;
}

template <class TTasks>
BlackboardCore* TBehaviorTree<TTasks>::GlobalCore() {
        auto& sys = GetSystem<BlackboardSystem>();
        return sys.HasGlobal() ? &sys.GetGlobal() : nullptr;
}

template <class TTasks>
void TBehaviorTree<TTasks>::ResolveBindings() {

        bBindingsResolved = true;

        const TAsset* pAsset = GetResource(hTree);
        if (!pAsset) return;

        // Both may legitimately be absent: trees without a Blackboard sibling or
        // a world-declared global still tick — conditions read as false.
        if (pNode && pNode->GetComponent<Blackboard>()) pSiblingBB = pNode->GetComponent<Blackboard>();
        BlackboardCore* pGlobal = GlobalCore();

        const auto& nodes = pAsset->GetNodes();
        for (size_t i = 0; i < nodes.size(); ++i) {
                const SNode& n = nodes[i];
                if (n.type != FlatBuffers::BTNodeU::BTDecorator) continue;
                if (n.decorator_kind != FlatBuffers::EBTDecoratorKind::BlackboardCondition) continue;

                SBBBinding& b = vBindings[i];

                // Float comparison ops read the Float array; IsTrue/IsFalse the Bool array.
                const EBBType expected =
                                (n.op == FlatBuffers::SMConditionOp::IsTrue ||
                                 n.op == FlatBuffers::SMConditionOp::IsFalse)
                                ? EBBType::Bool : EBBType::Float;

                SSlotRef slot;
                if (pSiblingBB && pSiblingBB->IsInitialized() &&
                    pSiblingBB->GetCore().TryResolve(n.cond_key, expected, slot)) {
                        b = { &pSiblingBB->GetCore(), slot, true };
                } else if (pGlobal && pGlobal->TryResolve(n.cond_key, expected, slot)) {
                        b = { pGlobal, slot, true };
                } else if (!(vStates[i].flags & SNodeState::kWarnedOnce)) {
                        vStates[i].flags |= SNodeState::kWarnedOnce;
                        log_w("BehaviorTree '{}': condition key resolves on neither entity nor "
                              "global blackboard (node {}, condition reads false)",
                              pAsset->Str(), i);
                }
        }
}

template <class TTasks>
void TBehaviorTree<TTasks>::Enable() {
        GetSystem<TSystem>().AddComponent(this);
}

template <class TTasks>
void TBehaviorTree<TTasks>::Disable() {
        GetSystem<TSystem>().RemoveComponent(this);
}

} // namespace SE

#endif // SE_IMPL

#endif // APP_AI_BEHAVIORTREE_TCC

#pragma pop_macro("Bool")
