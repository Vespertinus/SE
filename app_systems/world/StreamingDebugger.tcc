
#ifdef SE_IMPL

#include <GlobalTypes.h>

#include <world/StreamingDebugger.h>
#include <world/WorldSystem.h>
#include <world/WorldAsset.h>
#include <Logging.h>

#include <imgui.h>

#include <algorithm>
#include <limits>

namespace SE {

StreamingDebugger::StreamingDebugger() {
        GetSystem<EventManager>().AddListener<EPostRenderUpdate, &StreamingDebugger::OnPostRenderUpdate>(this);
}

StreamingDebugger::~StreamingDebugger() noexcept {
        GetSystem<EventManager>().RemoveListener<EPostRenderUpdate, &StreamingDebugger::OnPostRenderUpdate>(this);
}

void StreamingDebugger::OnPostRenderUpdate(const Event&) {

        if (bTabbed || !visible) { return; }

        if (!ImGui::Begin("Streaming", &visible)) {
                ImGui::End();
                return;
        }

        DrawContents();

        ImGui::End();
}

void StreamingDebugger::DrawTab() {
        if (!ImGui::BeginTabItem("Streaming")) { return; }
        DrawContents();
        ImGui::EndTabItem();
}

void StreamingDebugger::DrawContents() {

        auto & oWorldSystem = GetSystem<WorldSystem>();
        const auto oStats = oWorldSystem.GetStreamingStats();

        // DrawContents is called inside an open window/tab: the early-out
        // below becomes a plain return (no End on this path any more)
        if (!GetSystem<WorldSystem>().GetWorld().IsValid()) {
                ImGui::TextUnformatted("no world loaded");
                return;
        }

        // --- snapshot ------------------------------------------------------------
        ImGui::Text("wanted %u   active %u   requested %u   loading %u   unloading %u",
                        oStats.wanted, oStats.active, oStats.requested,
                        oStats.loading, oStats.unloading);
        ImGui::Text("in-flight reads: %u   pins: %u", oStats.in_flight_reads, oStats.pinned_total);
        ImGui::Text("main-thread budget: %.3f / %.3f ms",
                        double(oStats.budget_spent_ms), double(oStats.budget_limit_ms));

        const auto hWorld = oWorldSystem.GetWorld();
        const auto * pWorldAsset = hWorld.IsValid() ? GetResource(hWorld) : nullptr;

        if (!pWorldAsset) {
                ImGui::TextUnformatted("no world loaded");
                return;
        }

        const auto & vEntries = pWorldAsset->GetSubLevels();

        // --- per-level table -------------------------------------------------------
        if (ImGui::BeginTable("levels", 4, ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("level");
                ImGui::TableSetupColumn("state");
                ImGui::TableSetupColumn("deps");
                ImGui::TableSetupColumn("dist");
                ImGui::TableHeadersRow();

                const auto & vFocuses = oWorldSystem.GetFocuses();

                for (uint32_t i = 0; i < oWorldSystem.GetSubLevelCount(); ++i) {

                        const char * sState = "?";
                        switch (oWorldSystem.GetLevelState(i)) {
                                case WorldSystem::ELevelState::Unloaded:  sState = "unloaded";  break;
                                case WorldSystem::ELevelState::Requested: sState = "requested"; break;
                                case WorldSystem::ELevelState::Loading:   sState = "loading";   break;
                                case WorldSystem::ELevelState::Active:    sState = "active";    break;
                                case WorldSystem::ELevelState::Unloading: sState = "unloading"; break;
                        }

                        float dist = -1.f;
                        if (!vFocuses.empty()) {
                                dist = std::numeric_limits<float>::max();
                                for (const auto & f : vFocuses) {
                                        dist = std::min(dist, StreamingDirector::DistanceToBounds(
                                                        f.pos, vEntries[i].bounds_min, vEntries[i].bounds_max));
                                }
                        }

                        uint32_t deps_done = 0, deps_total = 0;
                        const bool bStaged = oWorldSystem.GetLevelDepProgress(i, deps_done, deps_total);

                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(vEntries[i].path.c_str());
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(sState);
                        ImGui::TableNextColumn();
                        if (bStaged) { ImGui::Text("%u/%u", deps_done, deps_total); }
                        else         { ImGui::TextUnformatted("-"); }
                        ImGui::TableNextColumn();
                        if (dist >= 0.f) { ImGui::Text("%.1f", double(dist)); }
                        else             { ImGui::TextUnformatted("-"); }
                }
                ImGui::EndTable();
        }

        // --- top-down map ------------------------------------------------------------
        const auto & oStreaming = pWorldAsset->GetStreaming();
        const float kMapSize = 260.f;

        // world bounds over eligible levels (map view extent)
        glm::vec3 vMin( std::numeric_limits<float>::max());
        glm::vec3 vMax(-std::numeric_limits<float>::max());
        for (const auto & e : vEntries) {
                vMin = glm::min(vMin, e.bounds_min);
                vMax = glm::max(vMax, e.bounds_max);
        }
        // pad by the unload radius so focus circles stay visible at the edges
        const float pad = oStreaming.unload_radius + 8.f;
        vMin -= glm::vec3(pad, 0.f, pad);
        vMax += glm::vec3(pad, 0.f, pad);

        const float extent = std::max(vMax.x - vMin.x, vMax.z - vMin.z);
        if (extent > 0.f) {

                const ImVec2 vTopLeft = ImGui::GetCursorScreenPos();
                ImDrawList * pDraw = ImGui::GetWindowDrawList();
                pDraw->AddRectFilled(vTopLeft, { vTopLeft.x + kMapSize, vTopLeft.y + kMapSize },
                                IM_COL32(24, 24, 24, 200));

                auto MapX = [&](float x) {
                        return vTopLeft.x + (x - vMin.x) / extent * kMapSize;
                };
                auto MapY = [&](float z) {
                        return vTopLeft.y + kMapSize - (z - vMin.z) / extent * kMapSize;
                };

                for (uint32_t i = 0; i < oWorldSystem.GetSubLevelCount(); ++i) {

                        ImU32 color = IM_COL32(90, 90, 90, 200);   // unloaded
                        switch (oWorldSystem.GetLevelState(i)) {
                                case WorldSystem::ELevelState::Requested:
                                case WorldSystem::ELevelState::Loading:
                                        color = IM_COL32(210, 170, 40, 220); break;
                                case WorldSystem::ELevelState::Active:
                                        color = IM_COL32(70, 190, 90, 220); break;
                                case WorldSystem::ELevelState::Unloading:
                                        color = IM_COL32(190, 90, 60, 220); break;
                                default: break;
                        }

                        const ImVec2 a(MapX(vEntries[i].bounds_min.x), MapY(vEntries[i].bounds_max.z));
                        const ImVec2 b(MapX(vEntries[i].bounds_max.x), MapY(vEntries[i].bounds_min.z));
                        pDraw->AddRectFilled(a, b, color);
                        pDraw->AddRect(a, b, IM_COL32(255, 255, 255, 90));
                }

                // focus points (+ effective load radius circles)
                for (const auto & f : oWorldSystem.GetFocuses()) {
                        const float r = (f.radius > 0.f ? f.radius : oStreaming.load_radius)
                                        / extent * kMapSize;
                        pDraw->AddCircle({ MapX(f.pos.x), MapY(f.pos.z) }, r,
                                        IM_COL32(120, 170, 255, 180));
                        pDraw->AddCircleFilled({ MapX(f.pos.x), MapY(f.pos.z) }, 3.f,
                                        IM_COL32(170, 210, 255, 255));
                }

                ImGui::Dummy({ kMapSize, kMapSize });
        }

        // --- scene trees -----------------------------------------------------------
        // Read-only node trees (name + local pos/rot/scale; scene_viewer's panel
        // minus the editing): the app's own nodes first — the world-root subtree is
        // skipped here since every sub-level gets its own tree below — then one
        // tree per instantiated sub-level.
        if (ImGui::CollapsingHeader("scene trees")) {

                if (TSceneTree * pTree = oWorldSystem.GetSceneTree()) {

                        auto DrawNode = [](TSceneTree::TSceneNodeExact & oNode) {
                                const bool open = ImGui::TreeNodeEx(oNode.GetName().c_str(),
                                                ImGuiTreeNodeFlags_OpenOnArrow |
                                                ImGuiTreeNodeFlags_OpenOnDoubleClick);
                                if (!open) { return false; }
                                const auto & t = oNode.GetTransform();
                                const glm::vec3 vPos = t.GetPos();
                                const glm::vec3 vRot = t.GetRotationDeg();
                                const glm::vec3 vScl = t.GetScale();
                                ImGui::Text("pos (%.2f, %.2f, %.2f)  rot (%.1f, %.1f, %.1f)  scale (%.2f, %.2f, %.2f)",
                                                vPos.x, vPos.y, vPos.z,
                                                vRot.x, vRot.y, vRot.z,
                                                vScl.x, vScl.y, vScl.z);
                                return true;
                        };
                        auto PopNode = [](TSceneTree::TSceneNodeExact &) { ImGui::TreePop(); };

                        const auto pWorldRoot = oWorldSystem.GetWorldRoot().lock();

                        pTree->GetRoot()->DepthFirstWalkEx(
                                [&](TSceneTree::TSceneNodeExact & oNode) {
                                        if (pWorldRoot && &oNode == pWorldRoot.get()) {
                                                ImGui::BulletText("%s (sub-levels below)", oNode.GetName().c_str());
                                                return false;
                                        }
                                        return DrawNode(oNode);
                                },
                                PopNode);

                        for (uint32_t i = 0; i < oWorldSystem.GetSubLevelCount(); ++i) {
                                if (oWorldSystem.GetLevelState(i) != WorldSystem::ELevelState::Active) { continue; }
                                const auto pLevelRoot = oWorldSystem.GetLevelRoot(i).lock();
                                if (!pLevelRoot) { continue; }
                                if (ImGui::TreeNodeEx(vEntries[i].path.c_str(), ImGuiTreeNodeFlags_OpenOnArrow)) {
                                        pLevelRoot->DepthFirstWalkEx(DrawNode, PopNode);
                                        ImGui::TreePop();
                                }
                        }

                        if (oWorldSystem.GetSubLevelCount() == 0) {
                                ImGui::TextDisabled("no sub-levels");
                        }
                }
        }
}

} // namespace SE

#endif
