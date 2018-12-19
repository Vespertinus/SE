#include <Global.h>
#include <GlobalTypes.h>
#include <Camera.h>
#include "Scene.h"


namespace SE {
namespace TOOLS {

static const uint8_t NODE_HIDE = 0x1;

Scene::Scene(const Settings & oNewSettings, SE::Camera & oCurCamera) :
        oCamera(oCurCamera),
        pSceneTree(SE::CreateResource<SE::TSceneTree>(oNewSettings.sScenePath)),
        oSettings(oNewSettings) {

        TInputManager::Instance().AddKeyListener   (&oImGui, "ImGui");
        TInputManager::Instance().AddMouseListener (&oImGui, "ImGui");


        oCamera.SetPos(8, 8, 4);
        oCamera.LookAt(0.1, 0.1, 0.1);

        if (oSettings.enable_all) {
                pSceneTree->EnableAll();
        }

        pSceneTree->Print();
}



Scene::~Scene() throw() { ;; }



void Scene::Process() {

        oImGui.NewFrame();

        SE::CheckOpenGLError();
        //SE::HELPERS::DrawAxes(10);

        TRenderState::Instance().SetViewProjection(oCamera.GetMVPMatrix());

        //pSceneTree->Draw();
        /*
        pSceneTree->GetRoot()->DepthFirstWalk([](SE::TSceneTree::TSceneNode & oNode) {


                if (oNode.GetFlags() & NODE_HIDE) {
                        return false;
                }

                oNode.DrawSelf();

                return true;
        });*/

        if (oSettings.vdebug) {

                pSceneTree->GetRoot()->DepthFirstWalk([](SE::TSceneTree::TSceneNodeExact & oNode) {

                        if (oNode.GetFlags() & NODE_HIDE) {
                                return false;
                        }

                        if (oNode.GetComponentsCnt() == 0) {
                                return true;
                        }


                        TRenderState::Instance().SetTransform(oNode.GetTransform().GetWorld());
                        TVisualHelpers::Instance().DrawLocalAxes();

                        return true;
                });

                pSceneTree->GetRoot()->DepthFirstWalk([](SE::TSceneTree::TSceneNodeExact & oNode) {

                        if (oNode.GetFlags() & NODE_HIDE) {
                                return false;
                        }

                        if (oNode.GetComponentsCnt() == 0) {
                                return true;
                        }


                        /*
                        auto * pMesh = oNode.GetEntity<TMesh>(0);

                        TRenderState::Instance().SetTransform(oNode.GetTransform().GetWorld());
                        TVisualHelpers::Instance().DrawBBox(pMesh->GetBBox());
                        */
                        return true;
                });
        }

        ShowGUI();

}

void Scene::PostRender() {

        oImGui.Render();
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
        ImGui::Text("Frame time: %f, FPS: %f", TRenderState::Instance().GetLastFrameTime(), TSimpleFPS::Instance().GetFPS());
        ImGui::Separator();
        ImGui::Text("Texture cnt: %zu", TResourceManager::Instance().Size<TTexture>());
        ImGui::Text("Mesh cnt: %zu", TResourceManager::Instance().Size<TMesh>());
        ImGui::Text("Shader component cnt: %zu", TResourceManager::Instance().Size<ShaderComponent>());
        ImGui::Text("Shader program cnt: %zu", TResourceManager::Instance().Size<ShaderProgram>());
        ImGui::Separator();
        ImGui::Text("Scene tree: '%s'", oSettings.sScenePath.c_str());
        ImGui::End();

        //scene tree
        static SE::TSceneTree::TSceneNodeExact * pCurNode;
        ImGui::SetNextWindowBgAlpha(0.9);
        ImGui::Begin("Scene tree", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

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

        }

        ImGui::Separator();
        if (ImGui::TreeNode("Nodes:")) {

                pSceneTree->GetRoot()->DepthFirstWalkEx([](SE::TSceneTree::TSceneNodeExact & oNode) {

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

                        bool hided = oNode.GetFlags() & NODE_HIDE;
                        if (ImGui::Button((hided) ? "show" : "hide")) {
                                if (hided) {
                                        oNode.ClearFlags(NODE_HIDE);
                                }
                                else {
                                        oNode.SetFlags(NODE_HIDE);
                                }
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


} //namespace SAMPLES
} //namespace SE




