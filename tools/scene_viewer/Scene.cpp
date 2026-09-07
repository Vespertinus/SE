#include <Global.h>
#include <GlobalTypes.h>
#include <Camera.h>
#include <Light.h>
#include <Animator.h>
#include <AnimationGraph_generated.h>
#include "Scene.h"


namespace SE {
namespace TOOLS {

static const uint8_t NODE_HIDE = 0x1;

Scene::Scene(const Settings & oNewSettings) :
        hSceneTree(SE::CreateResource<SE::TSceneTree>(oNewSettings.sScenePath)),
        oSettings(oNewSettings) {

        pSceneTree = SE::GetResource(hSceneTree);

        pCameraNode = pSceneTree->Create("MainCamera");
        auto res = pCameraNode->CreateComponent<Camera>(oSettings.oCamSettings);
        if (res != uSUCCESS) {
                throw("failed to create Camera component");
        }
        pCamera = pCameraNode->GetComponent<Camera>();
        se_assert(pCamera);

        res = pCameraNode->CreateComponent<BasicController>();
        if (res != uSUCCESS) {
                throw("failed to create BasicController component");
        }

        //init cam
        pCamera->SetPos(8, 4, 8);
        pCamera->LookAt(0.1, 0.1, 0.1);

        TEngine::Instance().Get<TRenderer>().SetCamera(pCamera);

#if SE_DEFERRED_RENDERER
        {
                DirLight oDir;
                oDir.direction = glm::normalize(glm::vec3(-1.f, -2.f, -1.f));
                oDir.intensity = 0.8f;
                TEngine::Instance().Get<TRenderer>().SetDirLight(oDir);
        }

        if (!oSettings.sIblIrrPath.empty() || !oSettings.sIblLdPath.empty()) {
                H<TTexture> hIrr, hLd;
                if (!oSettings.sIblIrrPath.empty())
                        hIrr = CreateResource<TTexture>(oSettings.sIblIrrPath,
                                        StoreTextureCubeMap::Settings{});
                if (!oSettings.sIblLdPath.empty())
                        hLd  = CreateResource<TTexture>(oSettings.sIblLdPath,
                                        StoreTextureCubeMap::Settings{});
                TEngine::Instance().Get<TRenderer>().SetIBL(
                        hIrr.IsValid() ? hIrr : H<TTexture>::Null(),
                        hLd.IsValid()  ? hLd  : H<TTexture>::Null());
        }
        TEngine::Instance().Get<TRenderer>().SetIBLScale(oSettings.ibl_scale);
        TEngine::Instance().Get<TRenderer>().SetIBLRotation(oSettings.ibl_rotation);
#endif

        if (oSettings.enable_all) {
                pSceneTree->EnableAll();
        }

        pSceneTree->Print();

        GetSystem<EventManager>().AddListener<EMouseButtonUp, &Scene::OnMouseButtonUp>(this);

}



Scene::~Scene() noexcept {

        GetSystem<EventManager>().RemoveListener<EMouseButtonUp, &Scene::OnMouseButtonUp>(this);
}



void Scene::Process() {

        auto & oClock = GetSystem<AppClock>();
        if (step_one_frame) {
                oClock.Pause();
                step_one_frame = false;
        }

        SE::CheckOpenGLError();

        if (toggle_controller) {
                pCameraNode->ToggleEnabled();
                toggle_controller = false;
        }

        if (oSettings.vdebug) {

                auto & oDebugRenderer = GetSystem<DebugRenderer>();
                oDebugRenderer.DrawGrid(pSceneTree->GetRoot()->GetTransform());

                pSceneTree->GetRoot()->DepthFirstWalk([](SE::TSceneTree::TSceneNodeExact & oNode) {

                        if (oNode.GetFlags() & NODE_HIDE) {
                                return false;
                        }

                        if (oNode.GetComponentsCnt() == 0) {
                                return true;
                        }

                        oNode.DrawDebug();


                        return true;
                });
        }

        if (show_skeleton) {
                DrawSkeletonOverlay();
        }

        //DEBUG
        //SE::GetSystem<SE::TRenderer>().Print();

        ShowGUI();
}

void Scene::DrawSkeletonOverlay() {

        auto& oDbg = GetSystem<DebugRenderer>();

        // Bone line color (yellow) and joint sphere color (white)
        const glm::vec4 vBoneColor  = glm::vec4(1.0f, 0.85f, 0.1f, 1.0f);
        const glm::vec4 vJointColor = glm::vec4(1.0f, 1.0f,  1.0f, 1.0f);

        pSceneTree->GetRoot()->DepthFirstWalk([&](SE::TSceneTree::TSceneNodeExact& oNode) {

                auto* pAnimModel = oNode.GetComponent<SE::AnimatedModel>();
                if (!pAnimModel) return true;

                const Skeleton* pSkel = GetResource(pAnimModel->GetSkeletonHandle());
                if (!pSkel) return true;

                const auto& vJoints    = pSkel->Bones();
                const auto& vJointNodes = pAnimModel->JointNodes();
                const uint32_t boneCount = static_cast<uint32_t>(vJoints.size());

                for (uint32_t i = 0; i < boneCount && i < vJointNodes.size(); ++i) {
                        auto pChild = vJointNodes[i];
                        if (!pChild) continue;

                        const glm::vec3 vChild = pChild->GetTransform().GetWorldPos();

                        oDbg.DrawSphere(vChild, 0.015f, vJointColor);

                        const uint16_t pi = vJoints[i].parentIndex;
                        if (pi == Skeleton::kNoParent || pi >= vJointNodes.size()) continue;

                        auto pParent = vJointNodes[pi];
                        if (!pParent) continue;

                        const glm::vec3 vParent = pParent->GetTransform().GetWorldPos();
                        oDbg.DrawLine(vParent, vChild, vBoneColor);
                }

                return true;
        });
}

void Scene::ShowGUI() {

        static const uint8_t NODE_HIDE = 0x1;

        //basic info
        const float indent = 10;
        ImVec2 window_pos = ImVec2(ImGui::GetIO().DisplaySize.x - indent, indent);
        ImVec2 window_pos_pivot = ImVec2(1.0f, 0.0f);
        ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        ImGui::SetNextWindowBgAlpha(0.7f);
        ImGui::Begin(
                        "Info",
                        nullptr,
                        ImGuiWindowFlags_NoMove |
                        ImGuiWindowFlags_NoTitleBar |
                        ImGuiWindowFlags_NoResize |
                        ImGuiWindowFlags_AlwaysAutoResize |
                        ImGuiWindowFlags_NoSavedSettings |
                        ImGuiWindowFlags_NoFocusOnAppearing |
                        ImGuiWindowFlags_NoNav);

        auto & oFpsTracker = GetSystem<FpsTracker>();
        ImGui::Text("Frame time: %.2f, FPS: %.2f", oFpsTracker.FrameMs(), oFpsTracker.Fps());
        ImGui::Text("Frame min time: %.2f, max time: %.2f", oFpsTracker.MinFrameMs(), oFpsTracker.MaxFrameMs());

        ImGui::Separator();
        ImGui::Text("Texture cnt: %zu", TResourceManager::Instance().Size<TTexture>());
        ImGui::Text("Mesh cnt: %zu", TResourceManager::Instance().Size<TMesh>());
        ImGui::Text("Materials cnt: %zu", TResourceManager::Instance().Size<Material>());
        ImGui::Text("Shader component cnt: %zu", TResourceManager::Instance().Size<ShaderComponent>());
        ImGui::Text("Shader program cnt: %zu", TResourceManager::Instance().Size<ShaderProgram>());
        ImGui::Separator();
        ImGui::Text("Scene tree: '%s'", oSettings.sScenePath.c_str());
        ImGui::Separator();
        auto & oEntityMgr = GetSystem<EntityManager>();
        ImGui::Text("Entity spawn queue:   %u", oEntityMgr.SpawnQueueDepth());
        ImGui::Text("Entity destroy queue: %u", oEntityMgr.DestroyQueueDepth());
        ImGui::End();

        //scene tree
        static SE::TSceneTree::TSceneNodeExact * pCurNode;
        ImGui::SetNextWindowBgAlpha(0.9);
        ImGui::Begin("Scene tree", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        //ImGui::SetWindowFontScale(2); //for 4K resolution

        if (pCurNode) {

                ImGui::Text("Node: '%s'", pCurNode->GetFullName().c_str());

                const auto & oTransform = pCurNode->GetTransform();
                glm::vec3 local_pos   = oTransform.GetPos();
                glm::vec3 local_rot   = oTransform.GetRotationDeg();
                glm::vec3 prev_local_rot = local_rot;
                glm::vec3 local_scale = oTransform.GetScale();

                static glm::vec3 cur_rot_around(0);
                static glm::vec3 cur_point(0);

                auto [vWorldTranslation, vWorldRotation, vWorldScale] = pCurNode->GetTransform().GetWorldDecomposedEuler();

                ImGui::Text("Global:");
                ImGui::Text("\t %.3f \t%.3f \t%.3f pos",
                                vWorldTranslation.x,
                                vWorldTranslation.y,
                                vWorldTranslation.z);
                ImGui::Text(" \t%.3f \t%.3f \t%.3f rot",
                                vWorldRotation.x,
                                vWorldRotation.y,
                                vWorldRotation.z);
                ImGui::Text(" \t%.3f \t%.3f \t%.3f scale",
                                vWorldScale.x,
                                vWorldScale.y,
                                vWorldScale.z);
                ImGui::Separator();

                ImGui::Text("Local:");

                ImGui::DragFloat3("pos",   &local_pos[0], 0.1, -100, 100);
                ImGui::DragFloat3("rot",   &local_rot[0], 0.2, -180, 180);
                ImGui::DragFloat3("scale", &local_scale[0], 0.1, 0.01, 100);

                ImGui::Separator();
                ImGui::DragFloat3("point", &cur_point[0], 0.1, -100, 100);
                ImGui::DragFloat3("angle", &cur_rot_around[0], 0.2, -180, 180);

                if (local_pos != oTransform.GetPos()) {

                        pCurNode->SetPos(local_pos);
                }
                if (local_rot != prev_local_rot) {
                        pCurNode->SetRotation(local_rot);
                }
                if (local_scale != oTransform.GetScale()) {
                        pCurNode->SetScale(local_scale);
                }

                if (ImGui::Button("rotate around point")) {

                        pCurNode->RotateAround(cur_point, cur_rot_around);
                        cur_point = glm::vec3(0);
                        cur_rot_around = glm::vec3(0);
                }

                //blendshape values:
                if (auto * pComponent = pCurNode->GetComponent<SE::AnimatedModel>(); pComponent) {
                        ImGui::Separator();
                        static std::vector<float> vWeights;
                        vWeights.clear();
                        vWeights.reserve(pComponent->BlendShapesCnt());

                        for (uint8_t i = 0; i < pComponent->BlendShapesCnt(); ++i) {
                                vWeights.emplace_back(pComponent->GetWeight(i));
                                ImGui::DragFloat(fmt::format("blendshape[{}]", i).c_str(),   &vWeights[i], 0.01, 0, 1);
                        }

                        for (uint8_t i = 0; i < pComponent->BlendShapesCnt(); ++i) {
                                if (vWeights[i] != pComponent->GetWeight(i)) {
                                        pComponent->SetWeight(i, vWeights[i]);
                                }
                        }
                }

                // Animator panel
                if (auto* pAnimator = pCurNode->GetComponent<SE::Animator>(); pAnimator) {
                        ImGui::Separator();

                        auto& inst = pAnimator->GetInstance();

                        static std::vector<std::string>  vStateNames;
                        static std::vector<ActiveStateInfo> vActive;
                        static std::vector<ParamInfo>    vParams;
                        vStateNames.clear();
                        vActive.clear();
                        vParams.clear();
                        inst.GetStateNames(vStateNames);
                        inst.GetActiveStates(vActive);
                        inst.GetParams(vParams);

                        if (ImGui::BeginTabBar("##anim_tabs")) {

                                if (ImGui::BeginTabItem("Animation")) {

                                        if (pAnimator->IsShowingBindPose()) {
                                                ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "Bind Pose");
                                        } else {
                                                if (inst.IsPaused()) {
                                                        if (ImGui::Button("Play"))  inst.SetPaused(false);
                                                } else {
                                                        if (ImGui::Button("Pause")) inst.SetPaused(true);
                                                }
                                                ImGui::SameLine();
                                                ImGui::Text("t = %.2f s", inst.GetCurrentTime());
                                                if (inst.IsTransitioning()) {
                                                        ImGui::SameLine();
                                                        ImGui::Text("(-> %.0f%%)", inst.TransitionProgress() * 100.0f);
                                                }

                                                if (!vActive.empty() && vActive[0].hClip.IsValid()) {
                                                        const AnimClip* pClip = GetResource(vActive[0].hClip);
                                                        if (pClip) {
                                                                const float dur = pClip->Duration();
                                                                ImGui::Text("dur: %.3f s  loop: %s  dTrans: %s  ch: %zu  ev: %zu",
                                                                        dur,
                                                                        pClip->Looping()           ? "yes" : "no",
                                                                        pClip->DeltaTranslations() ? "yes" : "no",
                                                                        pClip->Channels().size(),
                                                                        pClip->Events().size());
                                                                if (dur > 0.f)
                                                                        ImGui::ProgressBar(inst.GetCurrentTime() / dur,
                                                                                ImVec2(-1.f, 4.f), "");
                                                        }
                                                }
                                        }

                                        ImGui::Text("States (%zu):", vStateNames.size());

                                        const StrID cur = inst.CurrentStateName();
                                        int selected_idx = -1;
                                        if (pAnimator->IsShowingBindPose()) {
                                                selected_idx = 0;
                                        } else {
                                                for (int i = 0; i < (int)vStateNames.size(); ++i) {
                                                        if (StrID(vStateNames[i]) == cur) { selected_idx = i + 1; break; }
                                                }
                                        }

                                        const int total_items  = 1 + (int)vStateNames.size();
                                        const int visible_rows = std::min(total_items, 8);
                                        const float list_height = ImGui::GetTextLineHeightWithSpacing() * (visible_rows + 0.25f)
                                                               + ImGui::GetStyle().FramePadding.y * 2.0f;
                                        if (ImGui::BeginListBox("##states", ImVec2(0.f, list_height))) {
                                                if (ImGui::Selectable("Bind Pose", selected_idx == 0)) {
                                                        pAnimator->SetShowBindPose(true);
                                                        inst.SetPaused(true);
                                                }
                                                if (selected_idx == 0) ImGui::SetItemDefaultFocus();

                                                for (int i = 0; i < (int)vStateNames.size(); ++i) {
                                                        const bool is_selected = (selected_idx == i + 1);
                                                        if (ImGui::Selectable(vStateNames[i].c_str(), is_selected)) {
                                                                pAnimator->SetShowBindPose(false);
                                                                inst.ForceSetState(StrID(vStateNames[i]));
                                                                inst.SetPaused(false);
                                                        }
                                                        if (is_selected) ImGui::SetItemDefaultFocus();
                                                }
                                                ImGui::EndListBox();
                                        }

                                        ImGui::Separator();

                                        const bool skelActive = show_skeleton;
                                        if (skelActive)
                                                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.9f, 1.0f));
                                        if (ImGui::Button("Skeleton"))
                                                show_skeleton = !show_skeleton;
                                        if (skelActive)
                                                ImGui::PopStyleColor();

                                        ImGui::SameLine();
                                        if (ImGui::Button("Dump Pose")) {
                                                auto* pAnimModel = pCurNode->GetComponent<SE::AnimatedModel>();
                                                const Skeleton* pSkel = pAnimModel
                                                        ? GetResource(pAnimModel->GetSkeletonHandle())
                                                        : nullptr;
                                                if (pSkel && pAnimModel) {
                                                        const auto& vBones  = pSkel->Bones();
                                                        const auto& vJNodes = pAnimModel->JointNodes();
                                                        log_i("=== Pose dump: node '{}' t={:.3f}s ===",
                                                              pCurNode->GetName(), inst.GetCurrentTime());
                                                        for (uint32_t bi = 0; bi < static_cast<uint32_t>(vBones.size()) && bi < vJNodes.size(); ++bi) {
                                                                auto pJN = vJNodes[bi];
                                                                if (!pJN) continue;
                                                                const glm::vec3 lp = pJN->GetTransform().GetPos();
                                                                const glm::vec3 wp = pJN->GetTransform().GetWorldPos();
                                                                log_i("  bone[{:2d}] '{}':  local=({:.3f},{:.3f},{:.3f})  world=({:.3f},{:.3f},{:.3f})",
                                                                      bi, vBones[bi].name,
                                                                      lp.x, lp.y, lp.z,
                                                                      wp.x, wp.y, wp.z);
                                                        }
                                                }
                                        }

                                        ImGui::EndTabItem();
                                }

                                if (ImGui::BeginTabItem("Graph")) {

                                        // Active states with blend weights
                                        ImGui::Text("Active states:");
                                        for (const auto& s : vActive) {
                                                const char* sname = "?";
                                                for (const auto& n : vStateNames) {
                                                        if (StrID(n) == s.state_name) { sname = n.c_str(); break; }
                                                }
                                                ImGui::Text("  %s  w=%.2f  t=%.3fs", sname, s.weight, s.local_time);
                                        }

                                        // Transition bar
                                        if (inst.IsTransitioning()) {
                                                ImGui::Separator();
                                                const StrID tgt_id = inst.TransitionTargetName();
                                                const char* tgt_str = "?";
                                                for (const auto& n : vStateNames) {
                                                        if (StrID(n) == tgt_id) { tgt_str = n.c_str(); break; }
                                                }
                                                ImGui::Text("Transition -> %s", tgt_str);
                                                const float prog = inst.TransitionProgress();
                                                ImGui::ProgressBar(prog, ImVec2(-1.f, 0.f),
                                                        fmt::format("{:.0f}%", prog * 100.f).c_str());
                                        }

                                        // Parameter table
                                        if (!vParams.empty()) {
                                                ImGui::Separator();
                                                ImGui::Text("Parameters (%zu):", vParams.size());
                                                for (const auto& p : vParams) {
                                                        switch (p.type) {
                                                        case AnimParamStore::Type::Float:
                                                                ImGui::Text("  [F] %s = %.3f", p.name, p.float_val);
                                                                break;
                                                        case AnimParamStore::Type::Bool:
                                                                ImGui::Text("  [B] %s = %s", p.name, p.bool_val ? "true" : "false");
                                                                break;
                                                        case AnimParamStore::Type::Int:
                                                                ImGui::Text("  [I] %s = %d", p.name, p.int_val);
                                                                break;
                                                        case AnimParamStore::Type::Trigger:
                                                                ImGui::Text("  [T] %s = %s", p.name, p.triggered ? "ARMED" : "off");
                                                                break;
                                                        }
                                                }
                                        }

                                        ImGui::EndTabItem();
                                }

                                if (ImGui::BeginTabItem("Topology")) {

                                        const auto* pFB = inst.GetGraphFB();
                                        if (!pFB) {
                                                ImGui::TextDisabled("No graph loaded");
                                        } else {

                                                const StrID cur_sid = inst.CurrentStateName();
                                                const StrID tgt_sid = inst.TransitionTargetName();

                                                // -- States --
                                                const auto* fb_states = pFB->states();
                                                ImGui::Text("States (%u):", fb_states ? fb_states->size() : 0u);
                                                if (fb_states) {
                                                        for (flatbuffers::uoffset_t i = 0; i < fb_states->size(); ++i) {
                                                                const auto* s = fb_states->Get(i);
                                                                if (!s || !s->id()) continue;
                                                                const StrID sid(s->id()->c_str());
                                                                const bool is_cur = (sid == cur_sid);
                                                                const bool is_tgt = inst.IsTransitioning() && (sid == tgt_sid);
                                                                if (is_cur)
                                                                        ImGui::TextColored({0.3f,1.f,0.3f,1.f}, "> %s", s->id()->c_str());
                                                                else if (is_tgt)
                                                                        ImGui::TextColored({1.f,0.85f,0.f,1.f}, "~ %s", s->id()->c_str());
                                                                else
                                                                        ImGui::Text("  %s", s->id()->c_str());
                                                        }
                                                }

                                                // -- Transitions --
                                                ImGui::Separator();
                                                const auto* fb_trans = pFB->transitions();
                                                ImGui::Text("Transitions (%u):", fb_trans ? fb_trans->size() : 0u);
                                                if (fb_trans) {

                                                        auto FindParam = [&](const char* name) -> const ParamInfo* {
                                                                StrID sid(name);
                                                                for (const auto& p : vParams)
                                                                        if (StrID(p.name) == sid) return &p;
                                                                return nullptr;
                                                        };

                                                        for (flatbuffers::uoffset_t i = 0; i < fb_trans->size(); ++i) {
                                                                const auto* t = fb_trans->Get(i);
                                                                if (!t || !t->from() || !t->to()) continue;

                                                                const bool from_cur = (StrID(t->from()->c_str()) == cur_sid);

                                                                // Header: "From -> To  0.20s  [exit=0.80]  [intr]"
                                                                std::string hdr = fmt::format("{} -> {}  {:.2f}s",
                                                                        t->from()->c_str(), t->to()->c_str(), t->duration());
                                                                if (t->has_exit_time())
                                                                        hdr += fmt::format("  exit={:.2f}", t->exit_time());
                                                                if (t->can_interrupt())
                                                                        hdr += "  [intr]";

                                                                if (from_cur)
                                                                        ImGui::TextColored({0.3f,1.f,0.3f,1.f}, "%s", hdr.c_str());
                                                                else
                                                                        ImGui::TextDisabled("%s", hdr.c_str());

                                                                // Conditions
                                                                if (t->conditions()) {
                                                                        using Op = SE::FlatBuffers::ConditionOp;
                                                                        for (flatbuffers::uoffset_t ci = 0; ci < t->conditions()->size(); ++ci) {
                                                                                const auto* c = t->conditions()->Get(ci);
                                                                                if (!c || !c->parameter()) continue;
                                                                                const ParamInfo* pP = FindParam(c->parameter()->c_str());

                                                                                bool met = false;
                                                                                const char* op_str = "?";
                                                                                bool show_thr = true;
                                                                                switch (c->op()) {
                                                                                case Op::Greater:   op_str = ">";         met = pP && pP->float_val >  c->threshold(); break;
                                                                                case Op::Less:      op_str = "<";         met = pP && pP->float_val <  c->threshold(); break;
                                                                                case Op::Equal:     op_str = "==";        met = pP && std::abs(pP->float_val - c->threshold()) < 1e-4f; break;
                                                                                case Op::NotEqual:  op_str = "!=";        met = pP && std::abs(pP->float_val - c->threshold()) >= 1e-4f; break;
                                                                                case Op::IsTrue:    op_str = "== true";   met = pP && pP->bool_val;   show_thr = false; break;
                                                                                case Op::IsFalse:   op_str = "== false";  met = pP && !pP->bool_val;  show_thr = false; break;
                                                                                case Op::Triggered: op_str = "triggered"; met = pP && pP->triggered;  show_thr = false; break;
                                                                                }

                                                                                // Current value string
                                                                                char cur_val[32] = "";
                                                                                if (pP) {
                                                                                        using T = AnimParamStore::Type;
                                                                                        switch (pP->type) {
                                                                                        case T::Float:   snprintf(cur_val, sizeof(cur_val), " (%.3f)", pP->float_val); break;
                                                                                        case T::Bool:    snprintf(cur_val, sizeof(cur_val), " (%s)",   pP->bool_val ? "true" : "false"); break;
                                                                                        case T::Int:     snprintf(cur_val, sizeof(cur_val), " (%d)",   pP->int_val); break;
                                                                                        case T::Trigger: snprintf(cur_val, sizeof(cur_val), " (%s)",   pP->triggered ? "ARMED" : "off"); break;
                                                                                        }
                                                                                }

                                                                                std::string cond = show_thr
                                                                                        ? fmt::format("    {} {} {:.3f}{}", c->parameter()->c_str(), op_str, c->threshold(), cur_val)
                                                                                        : fmt::format("    {} {}{}", c->parameter()->c_str(), op_str, cur_val);

                                                                                if (met)
                                                                                        ImGui::TextColored({0.3f,1.f,0.3f,1.f}, "%s", cond.c_str());
                                                                                else
                                                                                        ImGui::TextDisabled("%s", cond.c_str());
                                                                        }
                                                                }
                                                        }
                                                }
                                        }
                                        ImGui::EndTabItem();
                                }

                                ImGui::EndTabBar();
                        }
                }

        }

        ImGui::Separator();
        if (ImGui::TreeNode("Nodes:")) {

                pSceneTree->GetRoot()->DepthFirstWalkEx([this](SE::TSceneTree::TSceneNodeExact & oNode) {

                        ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                                        ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                                        ((&oNode == pCurNode) ? ImGuiTreeNodeFlags_Selected : 0);
                        bool node_open = ImGui::TreeNodeEx(oNode.GetFullName().c_str(), node_flags, oNode.GetName().c_str());

                        if (ImGui::IsItemClicked()) {
                                pCurNode = &oNode;
                        }

                        if (!node_open) {
                                return false;
                        }


                        ImGui::Text("components cnt: %u", oNode.GetComponentsCnt());

                        oNode.ForEachComponent(
                                [&oNode](const auto & pComponent) {

                                        ImGui::Text(pComponent->Str().c_str());

                                }
                        );

                        bool show = oNode.GetFlags() & NODE_HIDE;
                        if (ImGui::Button((show) ? "show" : "hide")) {
                                if (show) {
                                        oNode.ClearFlags(NODE_HIDE);
                                        oNode.EnableRecursive();
                                }
                                else {
                                        oNode.SetFlags(NODE_HIDE);
                                        oNode.DisableRecursive();
                                }
                        }
                        if (ImGui::Button("look at")) {
                                pCamera->LookAt(oNode.GetTransform().GetWorldPos());
                        }

                        return true;
                },
                        [](SE::TSceneTree::TSceneNodeExact & oNode) {

                                ImGui::TreePop();
                });

                ImGui::TreePop();
        }

        ImGui::End();

        ShowTimeGUI();
}

void Scene::ShowTimeGUI() {

        auto & oClock = GetSystem<AppClock>();
        const bool paused = oClock.IsPaused();

        ImGui::SetNextWindowBgAlpha(0.9f);
        ImGui::Begin("Game Time", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

        if (paused)
                ImGui::TextColored({1.f, 0.5f, 0.2f, 1.f}, "PAUSED");
        else
                ImGui::TextColored({0.3f, 1.f, 0.3f, 1.f}, "%.2fx", oClock.Scale());

        ImGui::Text("Total:  %.3f s", oClock.Total());
        ImGui::Text("Frame:  %llu",   (unsigned long long)oClock.Frame());
        ImGui::Text("dt game: %.4f s  raw: %.4f s", oClock.Delta(), oClock.RawDelta());

        ImGui::Separator();

        if (paused) {
                if (ImGui::Button("Resume"))   oClock.Resume();
                ImGui::SameLine();
                if (ImGui::Button("Step >>|")) { oClock.Resume(); step_one_frame = true; }
        } else {
                if (ImGui::Button("Pause"))    oClock.Pause();
        }

        ImGui::Separator();

        static constexpr float       kSpeeds[] = { 0.1f, 0.25f, 0.5f, 1.f, 2.f, 4.f };
        static constexpr const char* kLabels[] = { "0.1x","0.25x","0.5x","1x","2x","4x" };
        for (int i = 0; i < 6; ++i) {
                const bool active = !paused && std::abs(oClock.Scale() - kSpeeds[i]) < 0.001f;
                if (active) ImGui::PushStyleColor(ImGuiCol_Button, {0.2f, 0.5f, 0.9f, 1.f});
                if (ImGui::Button(kLabels[i])) { oClock.SetScale(kSpeeds[i]); if (paused) oClock.Resume(); }
                if (active) ImGui::PopStyleColor();
                if (i < 5) ImGui::SameLine();
        }

        ImGui::Separator();

        if (ImGui::Button("<<"))    oClock.SetScale(std::max(0.05f, oClock.Scale() * 0.5f));
        ImGui::SameLine();
        if (ImGui::Button(">>"))    oClock.SetScale(std::min(8.f,   oClock.Scale() * 2.0f));
        ImGui::SameLine();
        if (ImGui::Button("Reset")) { oClock.SetScale(1.f); oClock.Resume(); }

        ImGui::End();
}

void Scene::OnMouseButtonUp(const Event & oEvent) {

        if (oEvent.Get<EMouseButtonUp>().button == MouseB::RIGHT) {
                toggle_controller = true;
        }
}


} //namespace SAMPLES
} //namespace SE




