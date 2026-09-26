
#ifdef SE_IMPL

#include <GlobalTypes.h>

#include <nav/NavigationDebugger.h>
#include <nav/NavigationSystem.h>
#include <Logging.h>

#include <imgui.h>

namespace SE {

NavigationDebugger::NavigationDebugger() {
        GetSystem<EventManager>().AddListener<EPostRenderUpdate, &NavigationDebugger::OnPostRenderUpdate>(this);
}

NavigationDebugger::~NavigationDebugger() noexcept {
        GetSystem<EventManager>().RemoveListener<EPostRenderUpdate, &NavigationDebugger::OnPostRenderUpdate>(this);
}

void NavigationDebugger::OnPostRenderUpdate(const Event &) {
        if (bTabbed || !visible) { return; }   // panel hidden — no ImGui work at all
        if (!ImGui::Begin("Navigation")) { ImGui::End(); return; }
        DrawContents();
        ImGui::End();
}

void NavigationDebugger::DrawTab() {
        if (!ImGui::BeginTabItem("Navigation")) { return; }
        DrawContents();
        ImGui::EndTabItem();
}

void NavigationDebugger::DrawContents() {

        NavigationSystem & oNav = GetSystem<NavigationSystem>();
        const NavigationSystem::SStats & oStats = oNav.GetStats();

        NavigationSystem::SDebug & oDbg = oNav.DebugConfig();
        ImGui::Checkbox("draw enabled", &oDbg.enabled);
        ImGui::Checkbox("tiles",   &oDbg.tiles);
        ImGui::Checkbox("polys",   &oDbg.polys);
        ImGui::Checkbox("paths",   &oDbg.paths);
        ImGui::Checkbox("markers", &oDbg.markers);

        ImGui::Separator();
        ImGui::Text("queries/frame: %u (rejected %u)", oStats.queries, oStats.budget_rejected);
        ImGui::Text("tiles: %u live (+%u/-%u this frame)", oStats.tiles_live, oStats.tiles_added, oStats.tiles_removed);
        ImGui::Text("agents: %u", oStats.agents);
        if (!oNav.DefaultClass().empty()) {
                ImGui::Text("default class: %s", oNav.DefaultClass().c_str());
        }

        ImGui::Separator();
        std::vector<NavigationSystem::SAgentDebug> vAgents;
        oNav.GetDebugAgents(vAgents);
        for (const NavigationSystem::SAgentDebug & a : vAgents) {
                const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnDoubleClick;
                if (ImGui::TreeNodeEx(a.sName.c_str(), flags)) {
                        ImGui::Text("class: %s", a.sClass.c_str());
                        ImGui::Text("pos: (%.1f, %.1f, %.1f)", a.pos.x, a.pos.y, a.pos.z);
                        ImGui::Text("path: %s (%zu points)",
                                    a.has_path ? "active" : "none", a.path_points);
                        ImGui::TreePop();
                }
        }
}

} // namespace SE

#endif // SE_IMPL
