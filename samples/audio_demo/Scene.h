
#ifndef __AUDIO_DEMO_SCENE_H__
#define __AUDIO_DEMO_SCENE_H__ 1

#include <string>
#include <vector>

#include <ImGui.h>
#include <AudioTypes.h>
#include <GlobalTypes.h>

namespace SE {

class Scene {
public:
        struct Settings { SE::Camera::Settings oCamSettings; };

        Scene(const Settings&);
        ~Scene() noexcept;
        void Process();

private:
        void ShowGUI();

        // --- Engine objects ---
        SE::Camera*                 pCamera     {nullptr};
        SE::TSceneTree::TSceneNode  pCameraNode;
        SE::TSceneTree::TSceneNode  pListenerNode;
        SE::H<SE::TSceneTree>       hSceneTree;
        SE::TSceneTree*             pSceneTree  {nullptr};
        SE::HELPERS::ImGuiWrapper   oImGui;

        // --- Sound sources (spatial, looping) ---
        struct SoundSource {
                std::string                label;
                glm::vec3                  pos;
                SE::TSceneTree::TSceneNode pNode;   // owns the AudioEmitter component
                float                      gain    {1.0f};
                bool                       enabled {true};
        };
        std::vector<SoundSource> vSources;

        // Listener position (world space)
        glm::vec3 vListenerPos {0.f, 0.f, 0.f};

        // --- One-shot / manual playback ---
        struct ClipEntry {
                const char*      label;
                std::string      path;
                SE::H<AudioClip> hClip;
        };
        std::vector<ClipEntry> vOneshotClips;

        SE::PlayFlags oOneShotFlags;

        struct VoiceInfo {
                std::string     label;
                SE::VoiceHandle hVoice;
                bool            paused {false};
        };
        std::vector<VoiceInfo> vManualVoices;

        // --- Bus mixer ---
        float aBusGains[5] = {1,1,1,1,1};
        float fFadeTime    = 0.f;
};

} // namespace SE

#endif
