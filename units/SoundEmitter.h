
#ifndef __SOUND_EMITTER_H__
#define __SOUND_EMITTER_H__ 1

#include <string>

#include <AudioTypes.h>
#include <SoundContext.h>
#include <SoundEventSystem.h>
#include <SoundEmitter_generated.h>

namespace SE {

class SoundEmitter {

        TSceneTree::TSceneNodeExact* pNode;
        std::string                  sCueName;
        std::vector<StrID>           vCueHierarchy;  // precomputed from sCueName at construction
        VoiceHandle                  hVoice;
        glm::vec3                    vPrevPos  {0.0f};
        glm::vec3                    vVelocity {0.0f};
        bool                         first_tick = true;

public:
        using TSerialized = FlatBuffers::SoundEmitter;

        SoundEmitter(TSceneTree::TSceneNodeExact* pNode, const SoundEmitterDesc& desc);
        SoundEmitter(TSceneTree::TSceneNodeExact* pNode,
                     const SE::FlatBuffers::SoundEmitter* pFB);
        ~SoundEmitter() noexcept;

        void Enable();
        void Disable();

        /** Post the cue event using this emitter as spatial anchor. */
        template<class TCtx = SoundEventContext>
        VoiceHandle Play(const TCtx& ctx_in = {}) {
                TCtx ctx      = ctx_in;
                ctx.pEmitter  = this;
                ctx.vPosition = GetPosition();
                ctx.vVelocity = vVelocity;
                hVoice = GetSystem<SoundEventSystem>().Post(vCueHierarchy, ctx);
                first_tick = false;
                log_d("SoundEmitter: play cue '{}' voice gen={}", sCueName, hVoice.generation);
                return hVoice;
        }

        void TargetTransformChanged(TSceneTree::TSceneNodeExact* pNode);

        VoiceHandle GetVoice()    const { return hVoice; }
        bool        IsPlaying()   const { return hVoice.IsValid(); }
        glm::vec3   GetPosition() const;
        glm::vec3   GetVelocity() const { return vVelocity; }

        std::string Str() const;
        void DrawDebug() const {}
};

} // namespace SE

#endif
