
#ifndef TERRAIN_DEMO_SCENE_H
#define TERRAIN_DEMO_SCENE_H

#include <glm/glm.hpp>

#include <GlobalTypes.h>
#include <application.h>
#include <Camera.h>
#include <ImGui.h>
#include <InputManager.h>
#include <PhysicsSystem.h>
#include <RigidBody.h>
#include <world/WorldSystem.h>
#include <terrain/Terrain.h>
#include <terrain/TerrainSystem.h>
#include <terrain/TerrainDebugger.h>
#include <world/StreamingDebugger.h>

namespace SE {

// ---------------------------------------------------------------------------
// terrain_demo — flight-camera over a paged procedural terrain grid (4×4 tiles of 512 m). 
// Validates the whole pipeline: 
//      baked LODs + skirts,
//      quadtree culling,
//      HeightFieldShape physics,
//      GetHeight queries,
//      tile streaming budgets.
//
// Controls:
//   WASD + QE (or SPACE)       fly
//   right mouse (toggle)       look (ESC also releases; LMB stays free for ImGui)
//   X                          boost
//   R                          drop a physics sphere at the camera aim point
//   F3                         toggle streaming + terrain debug windows
//   G                          toggle terrain LOD overlay
// ---------------------------------------------------------------------------

class Scene {
public:
        struct Settings {
                Camera::Settings oCamSettings;
        };

        explicit Scene(const Settings & oSettings);
        ~Scene() noexcept;

        void Process();

private:
        void UpdateCamera(const float dt);
        void DropSphere();

        void OnUpdate(const Event & oEvent);
        void OnKeyDown(const Event & oEvent);
        void OnMouseButtonUp(const Event & oEvent);

        // --- ImGui lifecycle ---
        HELPERS::ImGuiWrapper oImGui;

        // --- engine objects ---
        Camera *               pCamera    { nullptr };
        TSceneTree::TSceneNode pCameraNode;
        H<TSceneTree>          hSceneTree;
        TSceneTree *           pSceneTree { nullptr };

        TSceneTree::TSceneNode pTerrainNode;

        // --- fly camera state ---
        float yaw   = 0.0f;
        float pitch = -0.4f;
        float speed = 120.0f;

        // --- dropped props (node + visuals + body; node owns the body) ---
        std::vector<TSceneTree::TSceneNode> vSpheres;
        H<TMesh>    hSphereMesh;
        H<Material> hMatBall;
        uint32_t    iSphereSeq = 0;   // unique node names — full names must not collide

        bool bLodOverlay = true;
        bool bMouseGrabbed = false;
};

} // namespace SE

#ifdef SE_IMPL
#include "Scene.tcc"
#endif

#endif
