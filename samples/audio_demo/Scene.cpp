
#include <Global.h>
#include <GlobalTypes.h>
#include <Camera.h>
#include <AudioSystem.h>
#include <AudioListener.h>
#include "Scene.h"

namespace SE {

Scene::Scene(const Settings& oSettings) :
        hSceneTree(CreateResource<TSceneTree>("AudioScene", true)) {

        pSceneTree = GetResource(hSceneTree);
        se_assert(pSceneTree);

        // 1. Camera node (renderer only, no AudioListener)
        pCameraNode = pSceneTree->Create("Camera");
        auto res = pCameraNode->CreateComponent<Camera>(oSettings.oCamSettings);
        if (res != uSUCCESS) {
                throw std::runtime_error("failed to create Camera component");
        }
        pCamera = pCameraNode->GetComponent<Camera>();
        se_assert(pCamera);

        pCamera->SetPos(0, 5, 20);
        pCamera->LookAt(0, 0, 0);
        GetSystem<TRenderer>().SetCamera(pCamera);

        // 2. Listener node (carries AudioListener, position driven by UI)
        pListenerNode = pSceneTree->Create("Listener");
        pListenerNode->CreateComponent<AudioListener>(1.0f);
        pListenerNode->SetPos(vListenerPos);

        // 3. Four looping spatial sound sources
        struct SourceDef {
                const char* label;
                const char* path;
                glm::vec3   pos;
        };
        static const SourceDef kDefs[] = {
                { "Town Theme",  "resource/audio/music/TownTheme.seac",         {-12.f,  0.f,   0.f} },
                { "Item Coins",  "resource/audio/sfx/item_coins_04.seac",       { 12.f,  0.f,   0.f} },
                { "Rain",        "resource/audio/sfx/rain.seac",                {  0.f,  0.f,  12.f} },
                { "Spell Fire",  "resource/audio/sfx/spell_fire_03.seac",       {  0.f,  0.f, -12.f} },
        };

        for (const auto& d : kDefs) {
                SoundSource s;
                s.label = d.label;
                s.pos   = d.pos;

                s.pNode = pSceneTree->Create(d.label);
                s.pNode->SetPos(d.pos);

                AudioEmitterDesc desc;
                desc.sClipPath      = d.path;
                desc.auto_play      = true;
                desc.oFlags.loop    = true;
                desc.oFlags.spatial = true;
                desc.oFlags.bus     = MixBusId::SFX;
                s.pNode->CreateComponent<AudioEmitter>(desc);

                vSources.push_back(std::move(s));
        }

        // 4. One-shot clips
        struct OneshotDef { const char* label; const char* path; };
        static const OneshotDef kOneshots[] = {
                { "Hit",    "resource/audio/sfx/hit_01.seac"   },
                { "Ambient","resource/audio/sfx/loop_ambient_01.seac" },
                { "Battle Theme","resource/audio/music/battleThemeA.seac" },
        };

        for (const auto& d : kOneshots) {
                ClipEntry c;
                c.label = d.label;
                c.path  = d.path;
                c.hClip = CreateResource<AudioClip>(d.path);
                vOneshotClips.push_back(std::move(c));
        }

        // 5. One-shot flags defaults
        oOneShotFlags.loop    = false;
        oOneShotFlags.spatial = false;
        oOneShotFlags.bus     = MixBusId::SFX;
        oOneShotFlags.gain    = 1.0f;
        oOneShotFlags.pitch   = 1.0f;
}



Scene::~Scene() noexcept {
        for (auto& v : vManualVoices) {
                if (v.hVoice.IsValid()) {
                        GetSystem<AudioSystem>().Stop(v.hVoice);
                }
        }
}



void Scene::Process() {

        // Update listener world position — triggers TargetTransformChanged on AudioListener
        pListenerNode->SetPos(vListenerPos);

        ShowGUI();
}



void Scene::ShowGUI() {

        // ---------------------------------------------------------------
        // Window 1: Sound Sources
        // ---------------------------------------------------------------
        ImGui::Begin("Sound Sources");

        ImGui::Separator(); ImGui::Text("Listener");
        ImGui::DragFloat3("Position##listener", &vListenerPos[0], 0.1f, -90.f, 90.f);
        ImGui::Text("(AudioListener component on 'Listener' node)");

        for (int i = 0; i < static_cast<int>(vSources.size()); ++i) {
                auto& s = vSources[i];
                ImGui::PushID(i);
                ImGui::Separator(); ImGui::Text("%s", s.label.c_str());

                if (ImGui::DragFloat3("Position", &s.pos[0], 0.1f, -30.f, 30.f))
                        s.pNode->SetPos(s.pos);   // → TargetTransformChanged → PostUpdateEmitter

                if (s.enabled) {
                        if (ImGui::Button("Stop")) {
                                s.pNode->GetComponent<AudioEmitter>()->Disable();
                                s.enabled = false;
                        }
                } else {
                        if (ImGui::Button("Play")) {
                                s.pNode->GetComponent<AudioEmitter>()->Enable();
                                s.enabled = true;
                        }
                }

                ImGui::SameLine();
                if (ImGui::SliderFloat("Gain", &s.gain, 0.f, 2.f)) {
                        auto* pEmitter = s.pNode->GetComponent<AudioEmitter>();
                        GetSystem<AudioSystem>().PostSetVoiceGain(pEmitter->GetVoice(), s.gain);
                }

                ImGui::PopID();
        }

        ImGui::End();

        // ---------------------------------------------------------------
        // Window 2: Manual Playback
        // ---------------------------------------------------------------
        ImGui::Begin("Manual Playback");

        ImGui::Separator(); ImGui::Text("Clip");
        for (auto& c : vOneshotClips) {
                if (ImGui::Button(c.label)) {
                        VoiceHandle hVoice = GetSystem<AudioSystem>().Play(c.hClip, oOneShotFlags);
                        if (hVoice.IsValid()) {
                                vManualVoices.push_back({c.label, hVoice, false});
                        }
                }
                ImGui::SameLine();
        }
        ImGui::NewLine();

        ImGui::Separator(); ImGui::Text("PlayFlags");
        ImGui::Checkbox("Loop",    &oOneShotFlags.loop);
        ImGui::Checkbox("Spatial", &oOneShotFlags.spatial);
        ImGui::SliderFloat("Gain",  &oOneShotFlags.gain,  0.f, 2.f);
        ImGui::SliderFloat("Pitch", &oOneShotFlags.pitch, 0.1f, 3.f);

        {
                int bus = static_cast<int>(oOneShotFlags.bus);
                ImGui::Combo("Bus", &bus, "Master\0Music\0SFX\0Voice\0UI\0");
                oOneShotFlags.bus = static_cast<MixBusId>(bus);
        }

        ImGui::Separator(); ImGui::Text("Active Voices");
        for (int i = static_cast<int>(vManualVoices.size()) - 1; i >= 0; --i) {
                auto& v = vManualVoices[i];
                ImGui::PushID(i);
                ImGui::Text("%s", v.label.c_str());
                ImGui::SameLine();
                if (ImGui::Button("Stop")) {
                        GetSystem<AudioSystem>().Stop(v.hVoice);
                        vManualVoices.erase(vManualVoices.begin() + i);
                        ImGui::PopID();
                        continue;
                }
                ImGui::SameLine();
                if (ImGui::Button(v.paused ? "Resume" : "Pause")) {
                        GetSystem<AudioSystem>().Pause(v.hVoice, !v.paused);
                        v.paused = !v.paused;
                }
                ImGui::PopID();
        }

        if (ImGui::Button("Stop All")) {
                for (auto& v : vManualVoices) {
                        GetSystem<AudioSystem>().Stop(v.hVoice);
                }
                vManualVoices.clear();
        }

        ImGui::End();

        // ---------------------------------------------------------------
        // Window 3: Bus Mixer
        // ---------------------------------------------------------------
        ImGui::Begin("Bus Mixer");

        static const char* kBusNames[] = {"Master", "Music", "SFX", "Voice", "UI"};

        ImGui::SliderFloat("Fade Time (s)", &fFadeTime, 0.f, 5.f);

        for (int i = 0; i < 5; ++i) {
                if (ImGui::SliderFloat(kBusNames[i], &aBusGains[i], 0.f, 2.f)) {
                        GetSystem<AudioSystem>().SetBusGain(static_cast<MixBusId>(i), aBusGains[i], fFadeTime);
                }
        }

        ImGui::End();
}

} // namespace SE
