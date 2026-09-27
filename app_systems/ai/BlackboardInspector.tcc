#ifdef SE_IMPL

#include <GlobalTypes.h>

#include <ai/BlackboardInspector.h>
#include <ai/BlackboardSystem.h>
#include <ai/Blackboard.h>
#include <ai/BlackboardCore.h>
#include <Logging.h>

#include <imgui.h>

namespace SE {

namespace {

void DrawKeyTable(const BlackboardCore& oCore) {

        const BlackboardAsset* pAsset = oCore.GetAsset();
        if (!pAsset) {
                ImGui::TextUnformatted("uninitialized");
                return;
        }

        if (!ImGui::BeginTable("bb_keys", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                return;
        }
        ImGui::TableSetupColumn("key");
        ImGui::TableSetupColumn("type");
        ImGui::TableSetupColumn("value");
        ImGui::TableHeadersRow();

        for (const BlackboardAsset::SKey& k : pAsset->GetKeys()) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(k.s_name.c_str());
                ImGui::TableNextColumn();
                switch (k.slot.type) {
                        case EBBType::Float:  ImGui::TextUnformatted("float");  break;
                        case EBBType::Bool:   ImGui::TextUnformatted("bool");   break;
                        case EBBType::Int:    ImGui::TextUnformatted("int");    break;
                        case EBBType::Vec3:   ImGui::TextUnformatted("vec3");   break;
                        case EBBType::Entity: ImGui::TextUnformatted("entity"); break;
                        default:              ImGui::TextUnformatted("?");      break;
                }
                ImGui::TableNextColumn();
                switch (k.slot.type) {
                        case EBBType::Float:
                                ImGui::Text("%.3f", static_cast<double>(oCore.GetFloat(k.slot)));
                                break;
                        case EBBType::Bool:
                                ImGui::TextUnformatted(oCore.GetBool(k.slot) ? "true" : "false");
                                break;
                        case EBBType::Int:
                                ImGui::Text("%d", oCore.GetInt(k.slot));
                                break;
                        case EBBType::Vec3: {
                                const glm::vec3 v = oCore.GetVec3(k.slot);
                                ImGui::Text("(%.2f, %.2f, %.2f)", static_cast<double>(v.x),
                                                static_cast<double>(v.y), static_cast<double>(v.z));
                                break;
                        }
                        case EBBType::Entity:
                                ImGui::TextUnformatted(oCore.GetEntity(k.slot).expired() ? "none" : "live");
                                break;
                        default: break;
                }
        }
        ImGui::EndTable();
}

} // namespace

BlackboardInspector::BlackboardInspector() {
        GetSystem<EventManager>().AddListener<EPostRenderUpdate, &BlackboardInspector::OnPostRenderUpdate>(this);
}

BlackboardInspector::~BlackboardInspector() noexcept {
        GetSystem<EventManager>().RemoveListener<EPostRenderUpdate, &BlackboardInspector::OnPostRenderUpdate>(this);
}

void BlackboardInspector::OnPostRenderUpdate(const Event&) {

        if (bTabbed || !visible) { return; }

        if (!ImGui::Begin("Blackboards", &visible)) {
                ImGui::End();
                return;
        }

        DrawContents();

        ImGui::End();
}

void BlackboardInspector::DrawTab() {

        if (!ImGui::BeginTabItem("Blackboards")) { return; }

        DrawContents();

        ImGui::EndTabItem();
}

void BlackboardInspector::DrawContents() {

        auto & oBBSystem = GetSystem<BlackboardSystem>();

        // --- world-level (global) -------------------------------------------------
        if (ImGui::CollapsingHeader("global", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (oBBSystem.HasGlobal()) {
                        DrawKeyTable(oBBSystem.GetGlobal());
                } else {
                        ImGui::TextUnformatted("no world blackboard loaded");
                }
        }

        // --- per-entity instances -------------------------------------------------
        uint32_t count = 0;
        oBBSystem.ForEachComponent([&count](Blackboard* pBB) {
                ++count;
                const BlackboardCore& oCore = pBB->GetCore();
                if (!oCore.IsInitialized()) { return; }
                const BlackboardAsset* pAsset = oCore.GetAsset();
                const std::string sLabel = fmt::format("{} #{}", pAsset ? pAsset->Name() : "?", count);
                if (ImGui::CollapsingHeader(sLabel.c_str())) {
                        DrawKeyTable(oCore);
                }
        });

        if (count == 0) {
                ImGui::TextUnformatted("no entity blackboards registered");
        }
}

} // namespace SE

#else

#include <ai/BlackboardInspector.h>

namespace SE {

BlackboardInspector::BlackboardInspector() = default;
BlackboardInspector::~BlackboardInspector() noexcept = default;
void BlackboardInspector::OnPostRenderUpdate(const Event&) {}

} // namespace SE

#endif // SE_IMPL
