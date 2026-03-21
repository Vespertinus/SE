
#ifndef __AUDIO_EMITTER_H__
#define __AUDIO_EMITTER_H__ 1

#include <string>

#include <AudioTypes.h>
#include <ResourceHandle.h>
#include <Audio_generated.h>

namespace SE {

class AudioClip;

class AudioEmitter {

        TSceneTree::TSceneNodeExact* pNode;
        H<AudioClip>                 hClip;
        PlayFlags                    oFlags;
        VoiceHandle                  hVoice;
        glm::vec3                    vPrevPos{0.0f};
        bool                         first_tick = true;

public:
        using TSerialized = FlatBuffers::AudioEmitter;

        AudioEmitter(TSceneTree::TSceneNodeExact* pNode, const AudioEmitterDesc& desc);
        AudioEmitter(TSceneTree::TSceneNodeExact* pNode, const SE::FlatBuffers::AudioEmitter* pFB);
        ~AudioEmitter() noexcept;

        void Enable();
        void Disable();

        void TargetTransformChanged(TSceneTree::TSceneNodeExact* pNode);

        VoiceHandle GetVoice() const { return hVoice; }
        bool        IsPlaying() const { return hVoice.IsValid(); }

        std::string Str() const;
        void DrawDebug() const {}
};

} // namespace SE

#endif
