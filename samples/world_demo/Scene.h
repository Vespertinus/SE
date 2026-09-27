
#ifndef __WORLD_DEMO_SCENE_H__
#define __WORLD_DEMO_SCENE_H__ 1

#include <string>
#include <vector>

#include <ImGui.h>

#include <GlobalTypes.h>
#include <Light.h>
#include <glm/vec3.hpp>

namespace SE {

class Camera;
class Animator;
class CharacterController;
class InputState;
class StateMachine;

// World demo scene — integration (world/streaming + terrain + spawners
// + nav + crowd + blackboards + behavior trees).
//
// Controls:
//   WASD           — move (camera-relative)
//   Mouse          — look / orbit camera (grabbed at startup; RMB or ESC unlock)
//   Space          — jump
//   Left Shift     — sprint
//   Mouse wheel    — zoom
//   E              — open/close the nearest door (inn loads its interior;
//                    the tower crypt swaps the streamed world)
//   ESC            — unlock cursor / quit if already unlocked
//   F3             — toggle ALL debug panels (terrain_demo pattern)
//   G              — terrain quadtree/LOD overlay
//   B              — navmesh debug draw (tiles/polys/paths)
//   P              — stress-spawn guards at the camp (crowd load test)
class Scene {
public:
        struct Settings {
                Camera::Settings oCamSettings;
                std::string sWorldPath      {"resource/world/world_demo.seworld"};
                std::string sCharScenePath  {"resource/model/male_character.sesc"};
                std::string sPlayerHSMPath  {"resource/hsm/player_behavior.sesm"};
                std::string sAnimGraphPath  {"resource/anim_graph/character.seag"};

                // scene_viewer IBL flags (optional; absent => DirLight only)
                std::string sIblIrrPath;
                std::string sIblLdPath;
                float       ibl_scale    {1.0f};
                float       ibl_rotation {0.0f};
        };

        explicit Scene(const Settings & oSettings);
        ~Scene() noexcept;
        void Process();

private:
        // per-NPC facing state (visual node rotates toward velocity)
        struct NpcVisual {
                TSceneTree::TSceneNodeWeak pNode;    // template root (owns the CC)
                TSceneTree::TSceneNodeWeak pVisual;  // armature child that rotates
                CharacterController*       pCC {nullptr};
                float                      facing_deg = 0.f;
        };

        // --- interactive doors ------------------------------------------------
        // Door leaves are hinge-origin nodes named "*_Door"; a child node carries
        // the kinematic leaf body, so rotating the leaf here moves the collider
        // (RigidBody node-listener → MoveKinematic). Kind decides what opening
        // the door DOES besides swinging.
        struct Door {
                enum class Kind { Plain, Inn, CryptEnter, CryptExit };
                TSceneTree::TSceneNodeWeak pNode;   // leaf node (rotates)
                std::string                sName;   // bare node name ("Inn_FW_Door")
                Kind                       kind = Kind::Plain;
                bool                       open = false;
                float                      closed_yaw = 0.f;  // authored closed leaf yaw
                // crypt doors: >= 0 while a teleport swap is armed (fires
                // kCryptSwapDelay after the leaf started swinging)
                float                      swap_timer = -1.f;
        };
        struct DoorAnim {   // 0.4 s yaw tween, node is authoritative
                TSceneTree::TSceneNodeWeak pNode;
                float                      from = 0.f;
                float                      to   = 0.f;
                float                      t     = 0.f;
        };
        struct Toast {      // bottom-left event feedback line
                std::string sText;
                float       life = 0.f;   // seconds until gone
        };

        void CollectDoors(const TSceneTree::TSceneNodeWeak& pRoot);
        void ToggleNearestDoor();
        void SetDoorOpen(Door & d, bool open);
        void SilentCloseDoor(Door & d);
        void UpdateDoors(float dt);
        void EnterCrypt();
        void ExitCrypt();
        void AddToast(const std::string & sText);
        // FlushStreaming progress callback: pump + drop stale SDL events so the
        // window stays responsive (and no input burst replays) during a flush.
        static void FlushPump(uint32_t /*wanted*/, uint32_t /*active*/);
        static Door::Kind KindOf(const std::string & sName);
        static std::string PrettyDoorName(const std::string & sName);

        // crypt swap delay: the leaf swings 0.4 s, the teleport fires just after
        static constexpr float kCryptSwapDelay = 0.45f;

        void SpawnPlayer(const Settings& oSettings);
        TSceneTree::TSceneNodeWeak SpawnNpcEntity(const SpawnIntent& intent);
        void LoadNpcTemplates();

        void UpdateCamera(float dt);
        void UpdateCharFacing(float dt);
        void UpdateGlobalBlackboard(float dt);

        // IMC post_process hook: gates intent by player state and rotates the
        // raw device axes into camera space (runs inside the IMC's own
        // EInputUpdate listener, right after its flush — no listener-order race)
        void ApplyInputFrame (InputState & oInput);

        // --- event handlers ---
        void OnUpdate          (const Event& e);
        void OnMouseMove       (const Event& e);
        void OnKeyDown         (const Event& e);
        void OnMouseButtonUp   (const Event& e);
        void OnMouseWheel      (const Event& e);
        void OnTriggerFired    (const Event& e);
        void OnSubLevelActivated   (const Event& e);
        void OnSubLevelDeactivated (const Event& e);

        // --- ImGui lifecycle ---
        HELPERS::ImGuiWrapper      oImGui;
        // F3: single tabbed debug window (the panels render into its TabBar)
        bool                       debug_window { false };

        // --- engine objects ---
        Camera*                    pCamera     {nullptr};
        TSceneTree::TSceneNode     pCameraNode;
        H<TSceneTree>              hSceneTree;
        TSceneTree*                pSceneTree  {nullptr};

        // --- player ---
        TSceneTree::TSceneNode     pPlayerNode;
        CharacterController*       pPlayerCC    {nullptr};
        InputState*                pPlayerInput {nullptr};
        StateMachine*              pPlayerSM    {nullptr};
        TSceneTree::TSceneNode     pCharVisualNode;
        Animator*                  pPlayerAnimator {nullptr};
        float                      char_facing_deg = 0.f;
        float                      step_timer      = 0.f;

        // --- terrain grid render node ---
        TSceneTree::TSceneNode     pTerrainNode;

        // --- NPC facing state ---
        std::vector<NpcVisual>     vNpcVisuals;

        // --- one-shot spawn ground snap (terrain tiles activate post-load) ---
        bool                       spawn_snapped = false;

        // --- world / sub-levels ---
        std::string                sWorldPath;
        uint32_t                   inn_level_index  = UINT32_MAX;
        uint32_t                   crypt_level_index = UINT32_MAX;
        uint32_t                   tower_level_index = UINT32_MAX;
        bool                       inn_active      = false;
        bool                       crypt_active    = false;

        // --- doors / interaction ---
        std::vector<Door>          vDoors;
        std::vector<DoorAnim>      vDoorAnims;
        TSceneTree::TSceneNodeWeak pInteractDoor;   // nearest door in range (HUD)

        // --- streaming focus (the player focus doubles as the crypt grace
        //     zone: it moves underground with the teleport, so the area around
        //     the tower portal stays resident for the whole interior visit) ---
        uint64_t                   hPlayerFocus = 0;

        // --- HUD event toasts ---
        std::vector<Toast>         vToasts;

        // --- global blackboard writes ---
        float                      bb_pos_timer = 0.f;

        // --- stress spawn (P) ---
        uint32_t                   stress_seq = 0;

        // --- camera orbit ---
        float cam_yaw        = 200.0f;   // degrees
        float cam_pitch      = 18.0f;
        float cam_dist       = 7.0f;
        glm::vec3 v_cam_target_smooth {0.f};

        // --- directional light (shadow frustum follows the player) ---
        DirLight oDirLight;

        // --- mouse state ---
        bool mouse_locked = false;
        bool lod_overlay  = false;

        // --- settings kept for deferred spawns ---
        std::string sCharScenePath;
        std::string sPlayerHSMPath;
};

} // namespace SE

#include "Scene.tcc"
#endif
