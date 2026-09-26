
#ifdef SE_IMPL

#ifndef APP_AI_BEHAVIOR_TREE_DEBUGGER_TCC_INCLUDED
#define APP_AI_BEHAVIOR_TREE_DEBUGGER_TCC_INCLUDED

#include <GlobalTypes.h>

#include <ai/BehaviorTreeDebugger.h>
#include <ai/BehaviorTree.h>
#include <ai/BehaviorTreeAsset.h>
#include <ai/BehaviorTree.tcc>
#include <ai/BehaviorTreeAsset.tcc>
#include <Logging.h>

#include <imgui.h>

namespace SE {

template <class TTasks>
TBehaviorTreeDebugger<TTasks>::TBehaviorTreeDebugger() {
        GetSystem<EventManager>().AddListener<EPostRenderUpdate,
                                             &TBehaviorTreeDebugger::OnPostRenderUpdate>(this);
}

template <class TTasks>
TBehaviorTreeDebugger<TTasks>::~TBehaviorTreeDebugger() noexcept {
        GetSystem<EventManager>().RemoveListener<EPostRenderUpdate,
                                                 &TBehaviorTreeDebugger::OnPostRenderUpdate>(this);
}

template <class TTasks>
void TBehaviorTreeDebugger<TTasks>::SetSceneTree(TSceneTree* pTree) {
        vNodes.clear();
        if (!pTree) return;
        TSceneTree::TSceneNode pRoot = pTree->GetRoot();
        if (!pRoot) return;

        pRoot->DepthFirstWalk([this](auto& oNode) {
                if (oNode.template GetComponent<TComp>()) {
                        Register(oNode.GetShared());
                        return false;   // stop the walk
                }
                return true;
        });
}

template <class TTasks>
void TBehaviorTreeDebugger<TTasks>::Register(TSceneTree::TSceneNodeWeak pNode) {
        for (const auto& n : vNodes) {
                if (!n.owner_before(pNode) && !pNode.owner_before(n)) return;   // already registered
        }
        vNodes.push_back(pNode);
}

template <class TTasks>
void TBehaviorTreeDebugger<TTasks>::Unregister(TSceneTree::TSceneNodeWeak pNode) {
        for (size_t i = 0; i < vNodes.size(); ++i) {
                if (!vNodes[i].owner_before(pNode) && !pNode.owner_before(vNodes[i])) {
                        vNodes.erase(vNodes.begin() + static_cast<long>(i));
                        return;
                }
        }
}

template <class TTasks>
void TBehaviorTreeDebugger<TTasks>::OnPostRenderUpdate(const Event&) {
        if (bTabbed || !bVisible) return;

        if (ImGui::Begin("Behavior Trees", &bVisible)) {
                DrawContents();
        }
        ImGui::End();
}

template <class TTasks>
void TBehaviorTreeDebugger<TTasks>::DrawTab() {
        if (!ImGui::BeginTabItem("Behavior Trees")) { return; }
        DrawContents();
        ImGui::EndTabItem();
}

template <class TTasks>
void TBehaviorTreeDebugger<TTasks>::DrawContents() {
        for (size_t i = 0; i < vNodes.size();) {
                auto pNode = vNodes[i].lock();
                if (!pNode) {
                        vNodes.erase(vNodes.begin() + static_cast<long>(i));   // expired entity
                        continue;
                }
                ++i;

                auto* pComp = pNode->template GetComponent<TComp>();
                if (!pComp) continue;

                const auto* pAsset = pComp->GetAsset();
                if (!pAsset) continue;

                // label formatting is the per-frame cost here — the tree/tick
                // header renders only while its entity header is open
                ImGui::PushID(static_cast<int>(pNode->GetID()));
                if (ImGui::CollapsingHeader(pNode->GetName().c_str())) {
                        ImGui::Text("tree: %s  ticks: %u  last dt: %.3f",
                                    pAsset->Str().c_str(), pComp->TickCount(),
                                    pComp->LastTickDt());
                        ImGui::Text("interval: %.3f", pComp->GetTickInterval());
                        DrawNode(pAsset, pComp, 0);
                }
                ImGui::PopID();
        }
}

template <class TTasks>
void TBehaviorTreeDebugger<TTasks>::DrawNode(const TBehaviorTreeAsset<TTasks>* pAsset,
                                             const TComp* pComp, uint16_t idx) {
        const auto& nodes  = pAsset->GetNodes();
        if (idx >= nodes.size()) return;

        const SNode&     n  = nodes[idx];
        const SNodeState& st = pComp->State(idx);

        ImVec4 color(0.65f, 0.65f, 0.65f, 1.f);   // Idle grey
        if (st.status == EBTNodeStatus::Running)   color = ImVec4(0.3f, 0.9f, 0.3f, 1.f);
        if (st.status == EBTNodeStatus::Succeeded) color = ImVec4(0.35f, 0.55f, 1.f, 1.f);
        if (st.status == EBTNodeStatus::Failed)    color = ImVec4(0.95f, 0.35f, 0.3f, 1.f);

        ImGui::PushStyleColor(ImGuiCol_Text, color);

        const bool bHasChildren =
                (n.type == FlatBuffers::BTNodeU::BTComposite && n.child_count > 0) ||
                (n.type == FlatBuffers::BTNodeU::BTDecorator);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
        if (!bHasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        const std::string label = fmt::format(
                        "{} {}", n.name == StrID{} ? "" : "[named] ",
                        n.type == FlatBuffers::BTNodeU::BTComposite
                                ? "Composite"
                                : (n.type == FlatBuffers::BTNodeU::BTDecorator ? "Decorator" : "Task"));

        const bool open = ImGui::TreeNodeEx(reinterpret_cast<const void*>(uintptr_t(idx)), flags,
                                             "%s", label.c_str());
        ImGui::PopStyleColor();

        if (bHasChildren && open) {
                if (n.type == FlatBuffers::BTNodeU::BTComposite) {
                        const auto & idx = pAsset->GetChildIndices();
                        for (uint16_t k = 0; k < n.child_count; ++k) {
                                DrawNode(pAsset, pComp, static_cast<uint16_t>(idx[n.child_first + k]));
                        }
                } else {
                        DrawNode(pAsset, pComp, static_cast<uint16_t>(n.decorated_child));
                }
                ImGui::TreePop();
        }

        // Right-hover details: state + binding.
        if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("node %u  status %d  timer %.2f  counter %u", idx,
                            int(st.status), st.timer, st.counter);
                if (n.type == FlatBuffers::BTNodeU::BTDecorator &&
                    n.decorator_kind == FlatBuffers::EBTDecoratorKind::BlackboardCondition) {
                        const auto& b = pComp->Binding(idx);
                        ImGui::TextUnformatted(b.valid ? "condition bound" : "condition UNBOUND (false)");
                }
                ImGui::EndTooltip();
        }
}

} // namespace SE

#endif // APP_AI_BEHAVIOR_TREE_DEBUGGER_TCC_INCLUDED

#endif // SE_IMPL
