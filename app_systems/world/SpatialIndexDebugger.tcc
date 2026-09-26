
#ifdef SE_IMPL

#include <GlobalTypes.h>

#include <world/SpatialIndexDebugger.h>
#include <world/SpatialIndexSystem.h>
#include <world/WorldSystem.h>
#include <Logging.h>

#include <imgui.h>

#include <cstdio>

namespace SE {

SpatialIndexDebugger::SpatialIndexDebugger() {
        GetSystem<EventManager>().AddListener<EPostRenderUpdate, &SpatialIndexDebugger::OnPostRenderUpdate>(this);
}

SpatialIndexDebugger::~SpatialIndexDebugger() noexcept {
        GetSystem<EventManager>().RemoveListener<EPostRenderUpdate, &SpatialIndexDebugger::OnPostRenderUpdate>(this);
}

void SpatialIndexDebugger::OnPostRenderUpdate(const Event &) {

        if (bTabbed || !visible) { return; }

        if (!ImGui::Begin("SpatialIndex", &visible)) {
                ImGui::End();
                return;
        }

        DrawContents();

        ImGui::End();
}

void SpatialIndexDebugger::DrawTab() {
        if (!ImGui::BeginTabItem("Spatial Index")) { return; }
        DrawContents();
        ImGui::EndTabItem();
}

void SpatialIndexDebugger::DrawContents() {

        auto & oSystem = GetSystem<SpatialIndexSystem>();
        SpatialIndex & oIndex = oSystem.Get();

        // --- stats -----------------------------------------------------------------
        const auto oStats = oIndex.GetStats();
        ImGui::Text("entries %u   cells %u   tags %u   pending %u   refused %u",
                        oStats.entries, oStats.cells, oStats.tags,
                        oStats.pending, oStats.refused_queries);
        const auto & oCfg = oIndex.Config();
        ImGui::Text("cell size %.1f m   max query cells %u", double(oCfg.cell_size), oCfg.max_query_cells);

        // --- tag registry ------------------------------------------------------------
        if (ImGui::CollapsingHeader("tags")) {
                if (ImGui::BeginTable("tag_registry", 3, ImGuiTableFlags_Borders)) {
                        ImGui::TableSetupColumn("bit");
                        ImGui::TableSetupColumn("mask");
                        ImGui::TableSetupColumn("hash");
                        ImGui::TableHeadersRow();
                        for (uint32_t i = 0; i < oIndex.TagCount(); ++i) {
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::Text("%u", i);
                                ImGui::TableNextColumn();
                                ImGui::Text("0x%08x", 1u << i);
                                ImGui::TableNextColumn();
                                ImGui::Text("%llx", (unsigned long long)uint64_t(oIndex.TagName(i)));
                        }
                        ImGui::EndTable();
                }
        }

        // --- per-sub-level entry table -----------------------------------------------
        if (ImGui::CollapsingHeader("entries")) {

                std::vector<SpatialIndex::SDebugEntry> vEntries;
                oIndex.DebugDump(vEntries);

                // counts by sub-level attribution
                uint32_t counts_by_level[64] = {};
                for (const auto & e : vEntries) {
                        if (e.sub_level < 64) { ++counts_by_level[e.sub_level]; }
                }
                for (uint32_t lvl = 0; lvl < 64; ++lvl) {
                        if (counts_by_level[lvl] == 0) { continue; }
                        if (lvl == 0) { ImGui::BulletText("%u entries (dynamics / world-less)", counts_by_level[0]); }
                        else          { ImGui::BulletText("%u entries (sub-level %u)", counts_by_level[lvl], lvl); }
                }

                if (ImGui::BeginTable("entry_dump", 4, ImGuiTableFlags_Borders)) {
                        ImGui::TableSetupColumn("handle");
                        ImGui::TableSetupColumn("tags");
                        ImGui::TableSetupColumn("pos");
                        ImGui::TableSetupColumn("lvl");
                        ImGui::TableHeadersRow();
                        for (const auto & e : vEntries) {
                                ImGui::TableNextRow();
                                ImGui::TableNextColumn();
                                ImGui::Text("%llx", (unsigned long long)e.handle);
                                ImGui::TableNextColumn();
                                ImGui::Text("0x%llx", (unsigned long long)e.tags);
                                ImGui::TableNextColumn();
                                ImGui::Text("(%.1f, %.1f, %.1f)", double(e.pos.x), double(e.pos.y), double(e.pos.z));
                                ImGui::TableNextColumn();
                                ImGui::Text("%u", e.sub_level);
                        }
                        ImGui::EndTable();
                }
        }

        // --- query probe ----------------------------------------------------------------
        if (ImGui::CollapsingHeader("query probe")) {

                static glm::vec3 vProbePos(0.f, 0.f, 0.f);
                static float     probe_radius = 32.f;
                static int       probe_tag    = 0;

                ImGui::InputFloat3("pos", &vProbePos.x, "%.1f");
                ImGui::InputFloat("radius", &probe_radius, 1.f, 8.f, "%.1f");

                const uint32_t tag_count = oIndex.TagCount();
                if (tag_count > 0) {
                        probe_tag = probe_tag < int(tag_count) ? probe_tag : 0;
                        const std::string sTag = std::to_string(uint64_t(oIndex.TagName(uint32_t(probe_tag))));
                        if (ImGui::BeginCombo("tag", sTag.c_str())) {
                                for (uint32_t i = 0; i < tag_count; ++i) {
                                        const std::string sItem = std::to_string(uint64_t(oIndex.TagName(i)));
                                        if (ImGui::Selectable(sItem.c_str(), probe_tag == int(i))) { probe_tag = int(i); }
                                }
                                ImGui::EndCombo();
                        }

                        const uint64_t mask = 1ull << uint32_t(probe_tag);

                        uint32_t count = 0;
                        oIndex.ForEachInSphere(vProbePos, probe_radius, mask, [&count](const SpatialIndex::SEntry &) { ++count; });
                        ImGui::Text("in sphere: %u", count);

                        if (count > 0) {
                                SpatialIndex::SEntry near_entry;
                                if (oIndex.Nearest(vProbePos, probe_radius, mask, near_entry)) {
                                        ImGui::Text("nearest: (%.1f, %.1f, %.1f) h %llx",
                                                        double(near_entry.pos.x), double(near_entry.pos.y),
                                                        double(near_entry.pos.z), (unsigned long long)near_entry.handle);
                                }
                        }
                } else {
                        ImGui::TextDisabled("no tags registered");
                }
        }
}

} // namespace SE

#endif
