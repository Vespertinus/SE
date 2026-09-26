
#ifdef SE_IMPL

#include <terrain/TerrainDebugger.h>

#include <Logging.h>
#include <GlobalTypes.h>
#include <EventManager.h>
#include <CommonEvents.h>

#include <terrain/Terrain.h>

#include <cstdio>

#include <imgui.h>

namespace SE {

TerrainDebugger::TerrainDebugger() {
        GetSystem<EventManager>().AddListener<EPostRenderUpdate, &TerrainDebugger::OnPostRenderUpdate>(this);
}

TerrainDebugger::~TerrainDebugger() noexcept {
        GetSystem<EventManager>().RemoveListener<EPostRenderUpdate, &TerrainDebugger::OnPostRenderUpdate>(this);
}

void TerrainDebugger::OnPostRenderUpdate(const Event &) {

        if (bTabbed || !visible) { return; }

        if (ImGui::Begin("Terrain", &visible)) {
                DrawContents();
        }
        ImGui::End();
}

void TerrainDebugger::DrawTab() {
        if (!ImGui::BeginTabItem("Terrain")) { return; }
        DrawContents();
        ImGui::EndTabItem();
}

void TerrainDebugger::DrawContents() {

        auto & oTerrain = GetSystem<TerrainSystem>();
        Terrain * pTer = oTerrain.GetRenderable();
        oTerrain.GetDebugTiles(vTiles);

        if (pTer) {
                // wireframe material swap — makes the baked LOD chain visible
                bool wireframe = pTer->IsDebugWireframe();
                if (ImGui::Checkbox("Wireframe", &wireframe)) {
                        pTer->SetDebugWireframe(wireframe);
                }
                ImGui::Separator();
        }

        {
                const auto oStats = oTerrain.GetStats();
                // scene-wide totals (all passes, all renderables)
                ImGui::Text("rendered tris (scene): %llu",
                                (unsigned long long)GetSystem<TRenderer>().GetLastFrameTriangleCount());
                ImGui::Separator();
                if (ImGui::CollapsingHeader("Streaming", ImGuiTreeNodeFlags_DefaultOpen)) {
                        ImGui::Text("registrations: %u", oStats.registrations);
                        ImGui::Text("active tiles:  %u", oStats.active_tiles);
                        ImGui::Text("visible cmds:  %u", oStats.visible_cmds);
                        ImGui::Text("visible tris:  %llu", (unsigned long long)oStats.visible_tris);
                        if (pTer) {
                                ImGui::Text("in view:       %u", pTer->GetLastInViewCount());
                                ImGui::Text("no gpu:        %u", pTer->GetLastNoGpuCount());
                        }
                        ImGui::Text("requested:     %u", oStats.requested);
                        ImGui::Text("loading:       %u", oStats.loading);
                        ImGui::Text("in-flight:     %u", oStats.in_flight_reads);
                        ImGui::Text("build budget:  %.2f / %.2f ms",
                                        oStats.budget_spent_ms, oTerrain.Config().main_budget_ms);
                }

                if (ImGui::CollapsingHeader("Tiles", ImGuiTreeNodeFlags_DefaultOpen)) {
                        if (ImGui::BeginTable("tiles", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
                                ImGui::TableSetupColumn("x");
                                ImGui::TableSetupColumn("y");
                                ImGui::TableSetupColumn("state");
                                ImGui::TableSetupColumn("mode");
                                ImGui::TableSetupColumn("gpu");
                                ImGui::TableSetupColumn("lod");
                                ImGui::TableHeadersRow();
                                uint32_t tile_idx = 0;
                                for (const auto & t : vTiles) {
                                        ImGui::TableNextRow();
                                        ImGui::TableNextColumn(); ImGui::Text("%u", t.x);
                                        ImGui::TableNextColumn(); ImGui::Text("%u", t.y);
                                        ImGui::TableNextColumn();
                                        switch (t.status) {
                                        case 0: ImGui::TextUnformatted("unloaded"); break;
                                        case 1: ImGui::TextUnformatted("requested"); break;
                                        case 2: ImGui::TextUnformatted("loading"); break;
                                        case 3: ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "active"); break;
                                        default: ImGui::Text("?"); break;
                                        }
                                        ImGui::TableNextColumn(); ImGui::TextUnformatted(t.b_paged ? "paged" : "component");
                                        ImGui::TableNextColumn();
                                        const bool gpu = pTer && t.status == 3 && pTer->HasTileGpu(tile_idx);
                                        if (t.status == 3 && !gpu) {
                                                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "MISSING");
                                        }
                                        else {
                                                ImGui::TextUnformatted(gpu ? "yes" : "-");
                                        }
                                        ImGui::TableNextColumn();
                                        if (gpu) {
                                                const uint8_t lod = pTer->GetTileSelectedLod(tile_idx);
                                                const int8_t forced = pTer->GetLodOverride(tile_idx);
                                                if (forced >= 0) { ImGui::Text("%u*", uint32_t(lod)); }
                                                else             { ImGui::Text("%u", uint32_t(lod)); }
                                        }
                                        else {
                                                ImGui::TextUnformatted("-");
                                        }
                                        ++tile_idx;
                                }
                                ImGui::EndTable();
                        }
                }

                if (ImGui::CollapsingHeader("LOD override")) {
                        if (!pTer || pTer->GetTileCount() == 0 || vTiles.empty()) {
                                ImGui::TextUnformatted("no renderable terrain");
                        }
                        else {
                                if (iOverrideTile >= int(vTiles.size())) { iOverrideTile = 0; }

                                auto TileLabel = [](const TerrainSystem::STileDebug & t) {
                                        char buf[32];
                                        std::snprintf(buf, sizeof(buf), "(%u, %u)", t.x, t.y);
                                        return std::string(buf);
                                };
                                if (ImGui::BeginCombo("tile", TileLabel(vTiles[size_t(iOverrideTile)]).c_str())) {
                                        for (int i = 0; i < int(vTiles.size()); ++i) {
                                                if (ImGui::Selectable(TileLabel(vTiles[size_t(i)]).c_str(), i == iOverrideTile)) {
                                                        iOverrideTile = i;
                                                }
                                        }
                                        ImGui::EndCombo();
                                }

                                const int8_t cur = pTer->GetLodOverride(uint32_t(iOverrideTile));
                                const int sel = cur < 0 ? 0 : int(cur) + 1;
                                char sCurBuf[16];
                                if (sel > 0) { std::snprintf(sCurBuf, sizeof(sCurBuf), "LOD%d", sel - 1); }
                                const char * sCur = sel == 0 ? "auto" : sCurBuf;
                                if (ImGui::BeginCombo("lod", sCur)) {
                                        if (ImGui::Selectable("auto", sel == 0)) {
                                                pTer->SetLodOverride(uint32_t(iOverrideTile), -1);
                                        }
                                        const int lod_count = int(pTer->GetLodCount());
                                        for (int l = 0; l < lod_count; ++l) {
                                                char buf[16];
                                                std::snprintf(buf, sizeof(buf), "LOD%d", l);
                                                if (ImGui::Selectable(buf, sel == l + 1)) {
                                                        pTer->SetLodOverride(uint32_t(iOverrideTile), int8_t(l));
                                                }
                                        }
                                        ImGui::EndCombo();
                                }

                                // what the picked tile renders right now
                                const uint8_t eff = pTer->GetTileSelectedLod(uint32_t(iOverrideTile));
                                ImGui::Text("rendered:  LOD%u", uint32_t(eff));
                                ImGui::Text("tris:      %llu",
                                                (unsigned long long)pTer->GetLodTriCount(eff));
                                if (!pTer->HasTileGpu(uint32_t(iOverrideTile))) {
                                        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f),
                                                        "tile has no GPU buffers");
                                }
                                if (ImGui::Button("reset overrides")) {
                                        for (uint32_t i = 0; i < pTer->GetTileCount(); ++i) {
                                                pTer->SetLodOverride(i, -1);
                                        }
                                }
                        }
                }

                if (ImGui::CollapsingHeader("LOD legend")) {
                        ImGui::ColorButton("l0", ImVec4(0.2f, 1.0f, 0.2f, 1.0f)); ImGui::SameLine(); ImGui::Text("LOD0 (near)");
                        ImGui::ColorButton("l1", ImVec4(1.0f, 1.0f, 0.2f, 1.0f)); ImGui::SameLine(); ImGui::Text("LOD1");
                        ImGui::ColorButton("l2", ImVec4(1.0f, 0.6f, 0.2f, 1.0f)); ImGui::SameLine(); ImGui::Text("LOD2");
                        ImGui::ColorButton("l3", ImVec4(1.0f, 0.2f, 0.2f, 1.0f)); ImGui::SameLine(); ImGui::Text("LOD3+");
                        ImGui::ColorButton("lf", ImVec4(1.0f, 1.0f, 1.0f, 1.0f)); ImGui::SameLine(); ImGui::Text("overridden");
                }
        }
}

} // namespace SE

#endif // SE_IMPL
