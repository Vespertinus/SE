
#ifndef __MOVING_ACTORS_SCENE_H__
#define __MOVING_ACTORS_SCENE_H__ 1

#include <string>
#include <vector>
#include <array>

#include <ImGui.h>

#include <GlobalTypes.h>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>
#include <RmlUi/Core/DataModelHandle.h>

namespace SE {

class Animator;
class CharacterController;
class InputState;
class SprintStamina;
class StateMachine;

// Moving Actors demo scene.
//
// Controls:
//   WASD           — move (camera-relative)
//   Mouse          — look / orbit camera (click window to lock cursor)
//   Space          — jump (coyote time + jump buffering)
//   Left Shift     — sprint (consumes stamina)
//   Mouse wheel    — zoom camera
//   ESC            — unlock cursor / quit if already unlocked
//   F3 (debug)     — toggle CharacterDebugger and StateMachineDebugger panels
//
// Goal: collect all 7 orbs scattered across the tiered level.
// A second NPC character is spawned by EntityManager 8 s after the game starts.
// Pressing Space while the NPC is alive spawns an additional NPC (event-driven).
class Scene {
public:
        struct Settings {
                Camera::Settings oCamSettings;
                std::string sCharScenePath  {"resource/model/male_character.sesc"};
                std::string sNPCScenePath   {"resource/model/npc.sesc"};
                std::string sPlayerHSMPath  {"resource/hsm/player_behavior.sesm"};
                std::string sNPCHSMPath     {"resource/hsm/npc_ai.sesm"};
        };

        explicit Scene(const Settings & oSettings);
        ~Scene() noexcept;
        void Process();

private:
        // --- Level geometry helpers ---
        void MakeStaticBox(const char* name, glm::vec3 vPos, glm::vec3 vHalf,
                           glm::vec3 vRotDeg, H<Material> hMat);
        void MakeOrb(int index, glm::vec3 vPos);

        // --- Character spawning ---
        void SpawnPlayer(const Settings& oSettings);
        TSceneTree::TSceneNodeWeak SpawnNPC(const SpawnIntent& intent);

        // --- Per-frame helpers ---
        void UpdateCamera(float dt);
        void UpdateStamina(float dt);
        void UpdateFootstepTimers(float dt);
        void UpdateHUDBindings();
        void UpdateCameraShake(float dt);
        void UpdateCharFacing(float dt);

        // --- Player ragdoll (HSM-driven pose-source seam demo) ---
        void TogglePlayerRagdoll();

        // --- Orb pickup ---
        void CheckOrbProximity();

        // --- Event handlers ---
        void OnUpdate         (const Event& e);
        void OnInputUpdate    (const Event& e);
        void OnMouseMove      (const Event& e);
        void OnKeyDown        (const Event& e);
        void OnMouseButtonDown(const Event& e);
        void OnMouseWheel     (const Event& e);
        void OnCharacterJumped(const Event& e);
        void OnCharacterLanded(const Event& e);
        void OnEntitySpawned  (const Event& e);
        void OnStateMachineEvent(const Event& e);

        // --- ImGui lifecycle ---
        HELPERS::ImGuiWrapper      oImGui;

        // --- Engine objects ---
        Camera*                    pCamera     {nullptr};
        TSceneTree::TSceneNode     pCameraNode;
        H<TSceneTree>              hSceneTree;
        TSceneTree*                pSceneTree  {nullptr};

        // --- Meshes shared by level and orbs ---
        H<TMesh>                   hBoxMesh;
        H<TMesh>                   hSphereMesh;
        H<Material>                hOrbMat;

        // --- Player character visual ---
        TSceneTree::TSceneNode     pCharVisualNode;
        Animator*                  pPlayerAnimator  {nullptr};
        float                      char_facing_deg  = 0.f;
        std::string                sNPCScenePath;
        std::string                sNPCHSMPath;

        // --- Player ---
        TSceneTree::TSceneNode     pPlayerNode;
        CharacterController*       pPlayerCC    {nullptr};
        InputState*                pPlayerInput {nullptr};
        SprintStamina*             pPlayerStamina {nullptr};
        StateMachine*              pPlayerSM    {nullptr};
        float                      step_timer   = 0.f;

        // --- NPCs (spawned dynamically) ---
        struct NPCEntry {
                TSceneTree::TSceneNodeWeak pNode;
                TSceneTree::TSceneNode     pVisualNode;
                Animator*                  pAnimator   {nullptr};
                CharacterController*       pCC         {nullptr};  // cached — avoids per-frame lookup
                float                      facing_deg  = 0.f;
                float                      step_timer  = 0.f;
        };
        std::vector<NPCEntry>      vNPCs;

        // --- Orbs ---
        static constexpr int kOrbCount = 7;
        std::array<TSceneTree::TSceneNodeWeak, kOrbCount> vOrbs;
        bool orbs_spawned = false;  // set when orbs are created — guards the per-frame proximity scan
        int orbs_remaining = kOrbCount;
        int orbs_collected = 0;

        // --- Timer ---
        float game_time     = 0.f;
        bool  game_running  = false;
        bool  game_complete = false;

        // --- NPC auto-spawn ---
        float npc_spawn_timer = 8.0f;  // seconds until first auto-spawn
        bool  first_npc_spawned = false;

        // --- Camera orbit ---
        float cam_yaw        = 30.0f;   // degrees
        float cam_pitch      = 20.0f;   // degrees
        float cam_dist       = 6.0f;
        float cam_fov_current = 60.0f;
        glm::vec3 v_cam_target_smooth {0.f};

        // --- Camera shake (landing) ---
        glm::vec3 v_shake_offset   {0.f};
        float     shake_timer       = 0.f;
        float     shake_magnitude   = 0.f;

        // --- Mouse state ---
        bool mouse_locked = false;

        // --- HUD data model ---
        Rml::DataModelHandle hHUD;

        char timer_str[32]    = "00:00.0";
        char complete_str[32] = "";

        // HUD change tracking — only re-dirty RmlUi bindings when a value changes.
        char hud_timer_prev[32] = "";
        int  hud_orbs_prev      = -1;
        int  hud_stamina_prev   = -1;
        bool hud_complete_prev  = false;
};

} // namespace SE

#include "Scene.tcc"
#endif
