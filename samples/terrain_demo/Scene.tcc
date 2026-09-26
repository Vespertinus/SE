
#ifdef SE_IMPL

#include "Scene.h"

#include <Logging.h>

#include <EventManager.h>
#include <CommonEvents.h>
#include <InputCodes.h>
#include <InputEvents.h>
#include <StaticModel.h>
#include <RigidBody.h>
#include <TextureBuilder.h>
#include "../pbr_demo/SphereBuilder.h"
#include <world/WorldSystem.h>
#include <terrain/Terrain.h>
#include <terrain/TerrainSystem.h>
#include <terrain/TerrainDebugger.h>
#include <world/StreamingDebugger.h>

#include <cmath>

namespace SE {

// ---------------------------------------------------------------------------

Scene::Scene(const Settings & oSettings)
        : hSceneTree(CreateResource<TSceneTree>("TerrainDemoScene", true)) {

        pSceneTree = GetResource(hSceneTree);

        auto & oEM = GetSystem<EventManager>();
        oEM.AddListener<EUpdate,  &Scene::OnUpdate>(this);
        oEM.AddListener<EKeyDown, &Scene::OnKeyDown>(this);
        oEM.AddListener<EMouseButtonUp, &Scene::OnMouseButtonUp>(this);

        // --- camera ---
        pCameraNode = pSceneTree->Create("MainCamera");
        pCameraNode->CreateComponent<Camera>(oSettings.oCamSettings);
        pCamera = pCameraNode->GetComponent<Camera>();
        pCameraNode->SetPos({ 512.0f, 160.0f, -160.0f });
        pCamera->LookAt({ 512.0f, 40.0f, 512.0f });
        GetSystem<TRenderer>().SetCamera(pCamera);

        // --- world + terrain grid ---
        // LoadWorld fires EWorldLoaded; TerrainSystem auto-attaches the grid from
        // World::terrain_grid. The Terrain component below joins the same (deduped)
        // registration and carries the render side.
        auto & oWorlds = GetSystem<WorldSystem>();
        oWorlds.SetSceneTree(pSceneTree);
        if (!oWorlds.LoadWorld("resource/world/terrain_demo.seworld")) {
                log_e("terrain_demo: failed to load world");
        }

        pTerrainNode = pSceneTree->Create("TerrainGrid");
        Terrain::SDesc oDesc;
        oDesc.sManifest   = "resource/terrain/demo/demo.seterrain";
        oDesc.b_grid_mode = true;
        pTerrainNode->CreateComponent<Terrain>(oDesc);

        // --- dropped-sphere visuals (physics_demo pattern) ---
        hSphereMesh = CreateSphereMesh(16, 32);   // unit sphere (radius 1)
        {
                auto hWhiteTex  = TextureBuilder(1, 1).Fill(255, 255, 255).Upload("td_white");
                auto hNormalTex = TextureBuilder(1, 1).Fill(128, 128, 255).Upload("td_normal");
                auto hSpecTex   = TextureBuilder(1, 1).Fill(  0, 255, 255).Upload("td_spec");
                hMatBall = CreateResource<Material>("resource/material/pbr_demo.semt");
                auto * pMat = GetResource(hMatBall);
                pMat->SetTexture(TextureUnit::DIFFUSE,  hWhiteTex);
                pMat->SetTexture(TextureUnit::NORMAL,   hNormalTex);
                pMat->SetTexture(TextureUnit::SPECULAR, hSpecTex);
        }

        // the camera is the streaming focus; its radius overrides the world's
        oWorlds.RegisterFocus(pCameraNode, 1400.0f, 0.0f);

        // page the initial ring in before the first frame (loading-screen seam)
        oWorlds.FlushStreaming();

        log_i("terrain_demo: scene ready");
}

// ---------------------------------------------------------------------------

Scene::~Scene() noexcept {

        auto & oEM = GetSystem<EventManager>();
        oEM.RemoveListener<EUpdate,  &Scene::OnUpdate>(this);
        oEM.RemoveListener<EKeyDown, &Scene::OnKeyDown>(this);
        oEM.RemoveListener<EMouseButtonUp, &Scene::OnMouseButtonUp>(this);

        // releasing the nodes destroys their RigidBody components (and bodies)
        // while the physics system is still alive
        for (auto & pNode : vSpheres) { pNode.reset(); }
        vSpheres.clear();

        GetSystem<WorldSystem>().SetSceneTree(nullptr);
}

// ---------------------------------------------------------------------------

void Scene::Process() {
        // per-frame work happens in OnUpdate; Application drives the loop
        
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
        ImGui::Text("Texture cnt: %zu", GetSystem<TResourceManager>().Size<TTexture>());
        ImGui::Text("Mesh cnt: %zu", GetSystem<TResourceManager>().Size<TMesh>());
        ImGui::Text("Materials cnt: %zu", GetSystem<TResourceManager>().Size<Material>());
        ImGui::Text("Shader component cnt: %zu", GetSystem<TResourceManager>().Size<ShaderComponent>());
        ImGui::Text("Shader program cnt: %zu", GetSystem<TResourceManager>().Size<ShaderProgram>());
        ImGui::Separator();
        auto & oEntityMgr = GetSystem<EntityManager>();
        ImGui::Text("Entity spawn queue:   %u", oEntityMgr.SpawnQueueDepth());
        ImGui::Text("Entity destroy queue: %u", oEntityMgr.DestroyQueueDepth());
        ImGui::End();
}

void Scene::OnUpdate(const Event & oEvent) {

        const auto & e = oEvent.Get<EUpdate>();
        const float dt = e.last_frame_time;

        UpdateCamera(dt);

        // LOD overlay (DebugRenderer tile boxes colored by selected LOD)
        if (bLodOverlay && pTerrainNode) {
                if (auto * pTerrain = pTerrainNode->GetComponent<Terrain>()) {
                        pTerrain->DrawDebug();
                }
        }
}

void Scene::UpdateCamera(const float dt) {

        if (!pCamera) { return; }
        auto & oInput = GetSystem<InputManager>();

        if (oInput.GetKeyDown(Keys::ESCAPE) && bMouseGrabbed) {
                oInput.SetMouseGrabbed(false);
                oInput.SetMouseVisible(true);
                bMouseGrabbed = false;
        }

        if (bMouseGrabbed) {
                const glm::ivec2 vMove = oInput.GetMouseMove();
                yaw   -= float(vMove.x) * 0.0022f;
                pitch -= float(vMove.y) * 0.0022f;
                pitch = std::clamp(pitch, -1.5f, 1.5f);
        }

        const glm::vec3 vForward(std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch));
        // camera right for the LookAt convention: cross(forward, worldUp)
        const glm::vec3 vRight(-std::cos(yaw), 0.0f, std::sin(yaw));

        const float boost = oInput.GetKeyDown(Keys::X) ? 4.0f : 1.0f;
        glm::vec3 vVel{};
        if (oInput.GetKeyDown(Keys::W)) { vVel += vForward; }
        if (oInput.GetKeyDown(Keys::S)) { vVel -= vForward; }
        if (oInput.GetKeyDown(Keys::A)) { vVel -= vRight; }
        if (oInput.GetKeyDown(Keys::D)) { vVel += vRight; }
        if (oInput.GetKeyDown(Keys::E) || oInput.GetKeyDown(Keys::SPACE)) { vVel += glm::vec3(0, 1, 0); }
        if (oInput.GetKeyDown(Keys::Q)) { vVel -= glm::vec3(0, 1, 0); }

        const glm::vec3 vPos = pCameraNode->GetTransform().GetWorldPos();
        const glm::vec3 vNewPos = vPos + vVel * (speed * boost * dt);
        pCameraNode->SetPos(vNewPos);

        if (glm::length2(vVel) > 0.0f) {
                pCamera->SetPos(vNewPos.x, vNewPos.y, vNewPos.z);
        }
        pCamera->LookAt(vNewPos + vForward);
}

void Scene::OnMouseButtonUp(const Event & oEvent) {

        // scene_viewer pattern: RMB toggles mouse grab, leaving LMB for ImGui
        if (oEvent.Get<EMouseButtonUp>().button == MouseB::RIGHT) {
                bMouseGrabbed = !bMouseGrabbed;
                auto & oInput = GetSystem<InputManager>();
                oInput.SetMouseGrabbed(bMouseGrabbed);
                oInput.SetMouseVisible(!bMouseGrabbed);
        }
}

void Scene::OnKeyDown(const Event & oEvent) {

        const auto key = oEvent.Get<EKeyDown>().key;

        if (key == Keys::F3) {
                auto & oStreamDbg = GetSystem<StreamingDebugger>();
                auto & oTerrainDbg = GetSystem<TerrainDebugger>();
                oStreamDbg.SetVisible(!oStreamDbg.Visible());
                oTerrainDbg.SetVisible(!oTerrainDbg.IsVisible());
        }
        else if (key == Keys::G) {
                bLodOverlay = !bLodOverlay;
        }
        else if (key == Keys::R) {
                DropSphere();
        }
}

void Scene::DropSphere() {

        if (!hSphereMesh.IsValid() || !hMatBall.IsValid()) { return; }
        const glm::vec3 vForward(std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch));
        const glm::vec3 vPos = pCameraNode->GetTransform().GetWorldPos() + vForward * 20.0f;

        float h = 0.0f;
        if (GetSystem<TerrainSystem>().TryGetHeight(vPos, h)) {
                log_i("terrain_demo: drop at ({:.1f},{:.1f},{:.1f}), terrain height {:.1f}",
                      vPos.x, vPos.y, vPos.z, h);
        }

        // full names are unique per tree — SceneTree::Create returns nullptr on a
        // name collision (AddChild failure), so never reuse a bare "DropSphere"
        const std::string sName = "DropSphere_" + std::to_string(++iSphereSeq);
        auto pNode = pSceneTree->Create(sName);
        if (!pNode) {
                log_e("terrain_demo: failed to create node '{}'", sName);
                return;
        }
        pNode->SetPos(vPos);
        pNode->SetScale({ 3.0f, 3.0f, 3.0f });   // unit sphere mesh → radius 3
        pNode->CreateComponent<StaticModel>(hSphereMesh, hMatBall);

        RigidBodyDesc oDesc;
        oDesc.oCollider.type   = ColliderDesc::Sphere;
        oDesc.oCollider.radius = 3.0f;
        oDesc.vInitialPosition = vPos;
        oDesc.mass             = 80.0f;
        pNode->CreateComponent<RigidBody>(oDesc);

        vSpheres.push_back(pNode);
        if (vSpheres.size() > 16) {
                vSpheres.front().reset();   // RigidBody dtor destroys the body
                vSpheres.erase(vSpheres.begin());
        }
}

} // namespace SE

#endif // SE_IMPL
