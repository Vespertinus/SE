
#ifndef __SOUND_DEMO_SCENE_H__
#define __SOUND_DEMO_SCENE_H__ 1

#include <string>
#include <vector>
#include <deque>

#include <ImGui.h>
#include <AudioTypes.h>
#include <GlobalTypes.h>
#include <SoundEventSystem.h>
#include <SoundContextTypes.h>
#include <SoundEmitter.h>

namespace SE {

class Scene {
public:
        struct Settings { SE::Camera::Settings oCamSettings; };

        Scene(const Settings&);
        ~Scene() noexcept;
        void Process();

private:
        void ShowGUI();
        void UpdateSoldier(float dt);
        void UpdateVehicle(float dt);
        void FireFootstep();

        SurfaceType GetSurface(glm::vec2 vP) const;

        void LogEvent(const std::string& sCueId,
                      const std::vector<std::pair<std::string,float>>& vParams,
                      const SoundEventResult& oResult);

        void UpdateCooldownTracker(const std::string& sCueId, float duration);

        // --- Engine objects ---
        SE::Camera*                 pCamera     {nullptr};
        SE::TSceneTree::TSceneNode  pCameraNode;
        SE::TSceneTree::TSceneNode  pListenerNode;
        SE::H<SE::TSceneTree>       hSceneTree;
        SE::TSceneTree*             pSceneTree  {nullptr};
        SE::HELPERS::ImGuiWrapper   oImGui;

        float time = 0.0f;

        // --- Map ---
        static constexpr int kMapW = 40;
        static constexpr int kMapH = 30;
        SurfaceType tile_map[kMapH][kMapW];

        // --- Soldier ---
        struct SoldierState {
                glm::vec2   vPos          = {20.f, 15.f};
                float       speed         = 0.0f;
                float       weight        = 80.0f;
                SurfaceType surface       = SurfaceType::CONCRETE;
                float       step_timer    = 0.0f;
                int         ammo          = 30;
                bool        is_jumping    = false;
                float       jump_vel_y    = 0.0f;
                float       jump_height   = 0.0f;   // peak height reached
                float       jump_height_cur = 0.0f; // current height during flight
                bool        space_was_down = false;
                bool        r_was_down     = false;
                TSceneTree::TSceneNode pNode;
                SoundEmitter*          pEmitter = nullptr;
        } mSoldier;

        // --- Vehicle ---
        struct VehicleState {
                glm::vec2   vPos              = {10.f, 10.f};
                glm::vec2   vVelocity         = {};
                float       throttle          = 0.0f;
                float       rpm_normalized    = 0.0f;
                float       engine_load       = 0.0f;
                float       lateral_slip      = 0.0f;
                int         gear              = 1;
                int         engine_tier       = -1;
                VoiceHandle hEngineVoice;
                VoiceHandle hSquealVoice;
                bool        squeal_active     = false;
                float       heading           = 0.0f;
                bool        prev_on_boundary  = false;
                TSceneTree::TSceneNode pNode;
                SoundEmitter*          pEmitter = nullptr;
        } mVehicle;

        // --- Active entity (0=soldier, 1=vehicle) ---
        int  active_entity  = 0;
        bool tab_was_down   = false;

        // --- Event log ---
        struct EventLogEntry {
                float       time;
                std::string sCueId;
                std::vector<std::pair<std::string, float>> vParams;
                SoundEventResult oResult;
        };
        static constexpr int kMaxLog = 30;
        std::deque<EventLogEntry> vEventLog;

        // --- Cooldown tracker ---
        struct CooldownEntry {
                std::string sName;
                float       duration;
                float       remaining = 0.0f;
        };
        std::vector<CooldownEntry> vCooldownTracker;

        // --- Bus mixer ---
        float bus_gains[5]     = {1,1,1,1,1};
        float fade_time        = 0.f;
        float music_duck_timer = 0.0f;
};

} // namespace SE

#endif
