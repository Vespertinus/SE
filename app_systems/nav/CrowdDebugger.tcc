
#ifdef SE_IMPL

#include <GlobalTypes.h>

#include <nav/CrowdDebugger.h>
#include <nav/CrowdSystem.h>
#include <nav/PathFollower.h>
#include <Logging.h>

#include <imgui.h>

#include <vector>

namespace SE {

namespace {
const char * StatusName(const uint8_t s) {
        switch (static_cast<PathStatus>(s)) {
                case PathStatus::Following: return "Following";
                case PathStatus::Arrived:   return "Arrived";
                case PathStatus::Failed:    return "Failed";
                default:                    return "Idle";
        }
}
} // namespace

CrowdDebugger::CrowdDebugger() {
        GetSystem<EventManager>().AddListener<EPostRenderUpdate, &CrowdDebugger::OnPostRenderUpdate>(this);
}

CrowdDebugger::~CrowdDebugger() noexcept {
        GetSystem<EventManager>().RemoveListener<EPostRenderUpdate, &CrowdDebugger::OnPostRenderUpdate>(this);
}

void CrowdDebugger::OnPostRenderUpdate(const Event &) {
        if (bTabbed || !visible) { return; }   // panel hidden — no ImGui work at all
        if (!ImGui::Begin("Crowd")) { ImGui::End(); return; }
        DrawContents();
        ImGui::End();
}

void CrowdDebugger::DrawTab() {
        if (!ImGui::BeginTabItem("Crowd")) { return; }
        DrawContents();
        ImGui::EndTabItem();
}

void CrowdDebugger::DrawContents() {

        CrowdSystem & oCrowd = GetSystem<CrowdSystem>();
        const CrowdSystem::SStats & oStats = oCrowd.GetStats();

        ImGui::Text("solver: %u ticks this frame (%.3f ms last)", oStats.solver_ticks, oStats.solver_ms);
        ImGui::Text("arrivals: %u   repaths: %u", oStats.arrivals, oStats.repaths);
        ImGui::Text("followers: %u   avoided agents: %u", oStats.followers, oStats.avoided);
        ImGui::Text("backend classes: %zu", oCrowd.Backend().ClassCount());

        ImGui::Separator();
        std::vector<CrowdSystem::SFollowerDebug> vFollowers;
        oCrowd.GetDebugFollowers(vFollowers);
        for (const CrowdSystem::SFollowerDebug & f : vFollowers) {
                const ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnDoubleClick;
                if (ImGui::TreeNodeEx(f.sName.c_str(), flags)) {
                        ImGui::Text("status: %s", StatusName(f.status));
                        ImGui::Text("goal: (%.1f, %.1f, %.1f)  dist: %.2f",
                                    f.goal.x, f.goal.y, f.goal.z, f.dist_to_goal);
                        ImGui::Text("desired:  (%.2f, %.2f)", f.vDesired.x, f.vDesired.z);
                        ImGui::Text("adjusted: (%.2f, %.2f)  %s",
                                    f.vAdjusted.x, f.vAdjusted.z,
                                    f.avoided ? "[avoided]" : "[unavioded]");
                        ImGui::TreePop();
                }
        }
}

} // namespace SE

#endif // SE_IMPL
