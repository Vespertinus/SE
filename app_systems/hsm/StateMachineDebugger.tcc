
#include <GlobalTypes.h>

#include <hsm/StateMachineDebugger.h>
#include <hsm/StateMachine.h>
#include <hsm/StateMachineAsset.h>
#include <Logging.h>

#include <imgui.h>

namespace SE {

StateMachineDebugger::StateMachineDebugger() {
        GetSystem<EventManager>().AddListener<EPostRenderUpdate, &StateMachineDebugger::OnPostRenderUpdate>(this);
}

StateMachineDebugger::~StateMachineDebugger() noexcept {
        GetSystem<EventManager>().RemoveListener<EPostRenderUpdate, &StateMachineDebugger::OnPostRenderUpdate>(this);
}

void StateMachineDebugger::SetSceneTree(TSceneTree* pTree) {

        vEntries.clear();
        if (!pTree) return;
        TSceneTree::TSceneNode pRoot = pTree->GetRoot();
        if (!pRoot) return;

        pRoot->DepthFirstWalk([this](auto& oNode) {
                        if (oNode.template GetComponent<StateMachine>())
                        Register(oNode.GetShared());
                        return true;
                        });
}

void StateMachineDebugger::Register(TSceneTree::TSceneNodeWeak pNode) {
        for (const auto& e : vEntries) {
                if (!e.pNode.expired() && e.pNode.lock() == pNode.lock()) return;
        }
        DebugEntry entry;
        entry.pNode = pNode;
        vEntries.push_back(std::move(entry));
}

void StateMachineDebugger::Unregister(TSceneTree::TSceneNodeWeak pNode) {
        auto target = pNode.lock();
        vEntries.erase(
                        std::remove_if(vEntries.begin(), vEntries.end(), [&target](const DebugEntry& e) {
                                return e.pNode.expired() || e.pNode.lock() == target;
                                }),
                        vEntries.end());
}

void StateMachineDebugger::OnPostRenderUpdate(const Event& e) {
        if (!visible) return;   // panel hidden — no ImGui work at all

        elapsed += e.Get<EPostRenderUpdate>().last_frame_time;

        if (!ImGui::Begin("State Machines")) { ImGui::End(); return; }

        for (auto& entry : vEntries) {
                auto pNode = entry.pNode.lock();
                if (!pNode) continue;

                auto* comp = pNode->GetComponent<StateMachine>();
                if (!comp) continue;

                const std::string label = fmt::format("{}###sm_{}", pNode->GetName(),
                                reinterpret_cast<uintptr_t>(comp));
                if (!ImGui::CollapsingHeader(label.c_str())) continue;

                ImGui::Text("Current state:  %zu", static_cast<uint64_t>(comp->GetCurrentState()));
                ImGui::Text("Previous state: %zu", static_cast<uint64_t>(comp->GetPreviousState()));
                ImGui::Text("Time in state:  %.3f s", comp->GetTimeInState());
                if (comp->IsInTransition())
                        ImGui::Text("Transition -> %zu  %.0f%%",
                                        static_cast<uint64_t>(comp->GetTransitionTarget()),
                                        static_cast<double>(comp->GetTransitionProgress() * 100.f));

                ImGui::Separator();
                ImGui::Text("Parameters:");

                const auto* pAsset = comp->GetAsset();
                if (pAsset) {
                        for (const auto& d : pAsset->GetDefaults()) {
                                const char* name = d.sName.c_str();
                                auto& params = comp->GetParams();

                                if (std::holds_alternative<float>(d.value)) {
                                        float v = params.GetFloat(d.name);
                                        if (ImGui::DragFloat(name, &v, 0.01f))
                                                params.SetFloat(d.name, v);
                                } else if (std::holds_alternative<bool>(d.value)) {
                                        bool v = params.GetBool(d.name);
                                        if (ImGui::Checkbox(name, &v))
                                                params.SetBool(d.name, v);
                                } else if (std::holds_alternative<int>(d.value)) {
                                        int v = params.GetInt(d.name);
                                        if (ImGui::InputInt(name, &v))
                                                params.SetInt(d.name, v);
                                } else if (std::holds_alternative<SMTrigger>(d.value)) {
                                        ImGui::Text("%s [trigger]", name);
                                        ImGui::SameLine();
                                        if (ImGui::SmallButton(fmt::format("Fire###{}", name).c_str()))
                                                params.SetTrigger(d.name);
                                }
                        }
                }

                ImGui::Separator();
                ImGui::Text("Transition history:");
                const size_t count = entry.history_count;
                for (size_t i = 0; i < count; ++i) {
                        const size_t idx = (entry.history_head + DebugEntry::kHistoryCapacity - count + i)
                                % DebugEntry::kHistoryCapacity;
                        const auto& h = entry.history[idx];
                        ImGui::Text("  %zu -> %zu  @ %.2f s",
                                        static_cast<uint64_t>(h.from),
                                        static_cast<uint64_t>(h.to),
                                        static_cast<double>(h.timestamp));
                }
        }

        ImGui::End();
}

} // namespace SE
