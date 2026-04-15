#include <Global.h>
#include <GlobalTypes.h>
#include <Camera.h>
#include <Light.h>
#include <Animator.h>
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
                        auto pChild = vJointNodes[i].lock();
                        if (!pChild) continue;

                        const glm::vec3 vChild = pChild->GetTransform().GetWorldPos();

                        oDbg.DrawSphere(vChild, 0.015f, vJointColor);

                        const uint16_t pi = vJoints[i].parentIndex;
                        if (pi == Skeleton::kNoParent || pi >= vJointNodes.size()) continue;

                        auto pParent = vJointNodes[pi].lock();
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
        ImGui::Text("Frame time: %f, FPS: %f", GetSystem<GraphicsState>().GetLastFrameTime(), TSimpleFPS::Instance().GetFPS());
        ImGui::Separator();
        ImGui::Text("Texture cnt: %zu", TResourceManager::Instance().Size<TTexture>());
        ImGui::Text("Mesh cnt: %zu", TResourceManager::Instance().Size<TMesh>());
        ImGui::Text("Materials cnt: %zu", TResourceManager::Instance().Size<Material>());
        ImGui::Text("Shader component cnt: %zu", TResourceManager::Instance().Size<ShaderComponent>());
        ImGui::Text("Shader program cnt: %zu", TResourceManager::Instance().Size<ShaderProgram>());
        ImGui::Separator();
        ImGui::Text("Scene tree: '%s'", oSettings.sScenePath.c_str());
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
                        ImGui::Text("Animations");

                        auto& inst = pAnimator->GetInstance();

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
                        }

                        static std::vector<std::string> vStateNames;
                        vStateNames.clear();
                        inst.GetStateNames(vStateNames);

                        ImGui::Text("States (%zu):", vStateNames.size());

                        // Bind Pose button — shows the model in its rest pose
                        const bool bindPoseActive = pAnimator->IsShowingBindPose();
                        if (bindPoseActive) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                        if (ImGui::Button("Bind Pose")) {
                                pAnimator->SetShowBindPose(true);
                                inst.SetPaused(true);
                        }
                        if (bindPoseActive) ImGui::PopStyleColor();
                        ImGui::SameLine();

                        const StrID cur = inst.CurrentStateName();
                        for (const auto& sName : vStateNames) {
                                const bool active = (StrID(sName) == cur) && !pAnimator->IsShowingBindPose();
                                if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                                if (ImGui::Button(sName.c_str())) {
                                        pAnimator->SetShowBindPose(false);
                                        inst.ForceSetState(StrID(sName));
                                        inst.SetPaused(false);
                                }
                                if (active) ImGui::PopStyleColor();
                        }

                        ImGui::Separator();

                        // Skeleton overlay toggle
                        const bool skelActive = show_skeleton;
                        if (skelActive)
                                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.9f, 1.0f));
                        if (ImGui::Button("Skeleton"))
                                show_skeleton = !show_skeleton;
                        if (skelActive)
                                ImGui::PopStyleColor();

                        // Dump pose to log
                        ImGui::SameLine();
                        if (ImGui::Button("Dump Pose")) {
                                auto* pAnimModel = pCurNode->GetComponent<SE::AnimatedModel>();
                                const Skeleton* pSkel = pAnimModel
                                        ? GetResource(pAnimModel->GetSkeletonHandle())
                                        : nullptr;
                                if (pSkel && pAnimModel) {
                                        const auto& vBones    = pSkel->Bones();
                                        const auto& vJNodes   = pAnimModel->JointNodes();
                                        log_i("=== Pose dump: node '{}' t={:.3f}s ===",
                                              pCurNode->GetName(), inst.GetCurrentTime());
                                        for (uint32_t bi = 0; bi < static_cast<uint32_t>(vBones.size()) && bi < vJNodes.size(); ++bi) {
                                                auto pJN = vJNodes[bi].lock();
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
}

void Scene::OnMouseButtonUp(const Event & oEvent) {

        if (oEvent.Get<EMouseButtonUp>().button == MouseB::RIGHT) {
                toggle_controller = true;
        }
}


} //namespace SAMPLES
} //namespace SE




