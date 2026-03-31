
#ifndef __UI_DEMO_SCENE_H__
#define __UI_DEMO_SCENE_H__ 1

#include <string>
#include <vector>
#include <GlobalTypes.h>
#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>
#include <ui/UITypes.h>

namespace SE {

struct HUDModel {
        int         health  {100};
        int         shield  {80};
        int         energy  {60};
        float       cam_x   {0.0f};
        float       cam_y   {0.0f};
        float       cam_z   {10.0f};
        float       cam_fov {45.0f};
        std::string cam_proj{"PERSPECTIVE"};
        int         score   {0};
        int         kills   {0};
        float       elapsed {0.0f};
};

enum class GameState { MENU, PLAYING, PAUSED };

struct CharacterStats {
        int health     {100};
        int max_health {100};
        int armor      {50};
        int level      {3};
};

struct BeaconState {
        bool        active {true};
        int         ping   {42};
        std::string label  {"OBJ-A"};
};

struct CreditsModel {
        std::vector<Rml::String> vFeatures;
};

class Scene {
public:
        struct Settings { SE::Camera::Settings oCamSettings; };

        Scene(const Settings &);
        ~Scene() noexcept;
        void Process();

private:
        void OnUIEvent (const Event & oEvent);
        void OnUpdate  (const Event & oEvent);
        void OnKeyDown (const Event & oEvent);
        void OnKeyUp   (const Event & oEvent);

        void UpdateCameraOrbit (float dt);
        void UpdateHUDModel    ();
        void UpdateHealthDrain (float dt);

        void StartMission   ();
        void ReturnToMenu   ();
        void OpenSettings   ();
        void CloseSettings  ();
        void OpenQuitConfirm();
        void CloseQuitConfirm();
        void OpenCredits    ();
        void CloseCredits   ();
        void ShowToast      ();
        void SetThemeAll    (const std::string & theme);
        void EnableWidgets  ();
        void DisableWidgets ();

        SE::Camera *                pCamera    {nullptr};
        SE::TSceneTree::TSceneNode  pCameraNode;
        SE::TSceneTree::TSceneNode  pChar1Node;
        SE::TSceneTree::TSceneNode  pChar2Node;
        SE::TSceneTree::TSceneNode  pBeaconNode;
        SE::H<SE::TSceneTree>       hSceneTree;
        SE::TSceneTree *            pSceneTree {nullptr};

        CharacterStats              oStats1;
        CharacterStats              oStats2;
        BeaconState                 oBeacon;

        GameState  eState            {GameState::MENU};
        bool       settings_open     {false};
        bool       quit_confirm_open {false};
        bool       credits_open      {false};

        float      cam_yaw           {0.0f};
        float      cam_pitch         {0.0f};
        float      cam_yaw_speed     {0.0f};
        float      cam_pitch_speed   {0.0f};
        float      cam_radius        {10.0f};
        float      current_fov       {45.0f};
        float      health_drain_accum{0.0f};
        float      beacon_ping_accum {0.0f};

        std::string           current_theme;
        std::string           callsign {"OPERATIVE"};
        float                 toast_timer {0.0f};
        UIDocumentId          toast_doc   {INVALID_UI_DOCUMENT};

        HUDModel              oHUD;
        Rml::DataModelHandle  hHUD;

        CreditsModel          oCredits;
        Rml::DataModelHandle  hCreditsModel;

        Rml::DataModelHandle  hDebugModel;
        UIDocumentId          debug_panel_doc{INVALID_UI_DOCUMENT};
        bool                  draw_grid {true};
        bool                  draw_debug{true};
};

} // namespace SE

#endif
