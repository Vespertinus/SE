
#include <GlobalTypes.h>
#include <AudioEmitter.h>
#include <AudioClip.h>
#include <AudioSystem.h>
#include <GraphicsState.h>
#include <Logging.h>

namespace SE {

AudioEmitter::AudioEmitter(TSceneTree::TSceneNodeExact* pNewNode,
                           const SE::FlatBuffers::AudioEmitter* pFB)
        : pNode(pNewNode) {

        // --- clip loading via AudioClipHolder ---
        const auto* holder = pFB->clip();
        if (holder->path() != nullptr) {
                hClip = CreateResource<AudioClip>(holder->path()->c_str());
        } else if (holder->name() != nullptr && holder->clip() != nullptr) {
                hClip = CreateResource<AudioClip>(holder->name()->c_str(),
                                                   holder->clip());
        } else {
                throw std::runtime_error(
                        fmt::format("AudioEmitter: invalid AudioClipHolder "
                                    "(name {:p}, clip {:p})",
                                    (void*)holder->name(), (void*)holder->clip()));
        }

        // --- playback flags ---
        oFlags.loop     = pFB->loop();
        oFlags.spatial  = pFB->spatial();
        oFlags.gain     = pFB->gain();
        oFlags.pitch    = pFB->pitch();
        oFlags.ref_dist = pFB->ref_dist();
        oFlags.max_dist = pFB->max_dist();
        oFlags.rolloff  = pFB->rolloff();
        oFlags.bus      = static_cast<MixBusId>(pFB->bus());

        // --- auto-play ---
        if (pFB->auto_play() && hClip.IsValid()) {
                const glm::vec3 vPos = pNode->GetTransform().GetWorldPos();
                hVoice     = GetSystem<AudioSystem>().Play(hClip, oFlags, vPos, {});
                vPrevPos   = vPos;
                first_tick = false;
                log_i("AudioEmitter: auto-play voice gen={}", hVoice.generation);
        }

        pNode->AddListener(this);
}

AudioEmitter::AudioEmitter(TSceneTree::TSceneNodeExact* pNewNode, const AudioEmitterDesc& oDesc)
    : pNode(pNewNode), oFlags(oDesc.oFlags) {

        if (!oDesc.sClipPath.empty()) {
                hClip = CreateResource<AudioClip>(oDesc.sClipPath);
        }

        if (oDesc.auto_play && hClip.IsValid()) {
                const glm::vec3 vPos = pNode->GetTransform().GetWorldPos();
                hVoice = GetSystem<AudioSystem>().Play(hClip, oFlags, vPos, {});
                vPrevPos   = vPos;
                first_tick = false;
                log_i("AudioEmitter: auto-play '{}' voice gen={}", oDesc.sClipPath, hVoice.generation);
        }

        pNode->AddListener(this);
}

AudioEmitter::~AudioEmitter() noexcept {
        pNode->RemoveListener(this);
        if (hVoice.IsValid()) {
                GetSystem<AudioSystem>().Stop(hVoice);
                hVoice = VoiceHandle{};
        }
        if (hClip.IsValid()) {
                DestroyResource(hClip);
        }
}

void AudioEmitter::Enable() {
        if (hVoice.IsValid())
                GetSystem<AudioSystem>().Pause(hVoice, false);
}

void AudioEmitter::Disable() {
        if (hVoice.IsValid())
                GetSystem<AudioSystem>().Pause(hVoice, true);
}

void AudioEmitter::TargetTransformChanged(TSceneTree::TSceneNodeExact* /*pNode*/) {
        if (!hVoice.IsValid() || !oFlags.spatial) return;

        const float     dt   = GetSystem<GraphicsState>().GetLastFrameTime();
        const glm::vec3 vPos = pNode->GetTransform().GetWorldPos();
        glm::vec3 vVel(0.0f);

        if (!first_tick && dt > 0.0f)
                vVel = (vPos - vPrevPos) / dt;

        vPrevPos   = vPos;
        first_tick = false;

        GetSystem<AudioSystem>().PostUpdateEmitter(hVoice, vPos, vVel);
}

std::string AudioEmitter::Str() const {
        return fmt::format("AudioEmitter[clip={} voice_gen={}]",
                        hClip.IsValid() ? GetResource(hClip)->Name() : "<none>",
                        hVoice.generation);
}

} // namespace SE
