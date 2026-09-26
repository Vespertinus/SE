
#ifdef SE_IMPL

#include <spawner/SpawnerDebugger.h>
#include <spawner/SpawnerSystem.h>
#include <spawner/Population.h>

#include <EventManager.h>
#include <CommonEvents.h>
#include <Logging.h>

#include <imgui.h>

namespace SE {

SpawnerDebugger::SpawnerDebugger() {
        GetSystem<EventManager>().AddListener<EPostRenderUpdate, &SpawnerDebugger::OnPostRenderUpdate>(this);
}

SpawnerDebugger::~SpawnerDebugger() noexcept {
        GetSystem<EventManager>().RemoveListener<EPostRenderUpdate, &SpawnerDebugger::OnPostRenderUpdate>(this);
}

void SpawnerDebugger::OnPostRenderUpdate(const Event & /*e*/) {

        if (bTabbed || !visible) { return; }

        if (!ImGui::Begin("Spawner", &visible)) {
                ImGui::End();
                return;
        }

        DrawContents();

        ImGui::End();
}

void SpawnerDebugger::DrawTab() {
        if (!ImGui::BeginTabItem("Spawner")) { return; }
        DrawContents();
        ImGui::EndTabItem();
}

void SpawnerDebugger::DrawContents() {

        auto & oSpawner = GetSystem<SpawnerSystem>();

        const auto & st = oSpawner.GetStats();
        ImGui::Text("populations: %u   spawn points: %u", st.populations, st.spawn_points);
        ImGui::Text("spawns executed: %u   deferred: %u   despawns: %u",
                        st.spawns_executed, st.spawns_deferred, st.despawns);
        ImGui::Text("world slots: %u / %u resolved", st.world_slots_resolved, st.world_slots_total);
        ImGui::Separator();

        if (ImGui::BeginTable("populations", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("id");
                ImGui::TableSetupColumn("active");
                ImGui::TableSetupColumn("max");
                ImGui::TableSetupColumn("members");
                ImGui::TableSetupColumn("waves fired");
                ImGui::TableHeadersRow();

                for (auto * pPop : oSpawner.vPopulations) {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn(); ImGui::TextUnformatted(pPop->id_string.c_str());
                        ImGui::TableNextColumn(); ImGui::Text("%u", pPop->oRuntime.ActiveTotal());
                        ImGui::TableNextColumn(); ImGui::Text("%u", pPop->oRuntime.MaxActive());
                        ImGui::TableNextColumn(); ImGui::Text("%u", pPop->oRuntime.MemberCount());
                        uint32_t fired = 0;
                        for (uint32_t i = 0; i < pPop->oRuntime.WaveCount(); ++i) {
                                fired += pPop->oRuntime.WaveFired(i) ? 1u : 0u;
                        }
                        ImGui::TableNextColumn(); ImGui::Text("%u/%u", fired, pPop->oRuntime.WaveCount());
                }
                ImGui::EndTable();
        }
}

} // namespace SE

#endif
