
#include <GlobalTypes.h>
#include <AudioSystem.h>
#include <AudioThread.h>
#include <AudioClip.h>
#include <Logging.h>

namespace SE {

AudioSystem::AudioSystem() {

        pAudioThread = std::make_unique<AudioThread>();
        if (!pAudioThread->Start()) {
                log_e("AudioSystem: failed to initialize audio thread / OpenAL context");
                pAudioThread.reset();
        } else {
                log_i("AudioSystem: initialized");
        }
}

AudioSystem::~AudioSystem() noexcept {
        if (pAudioThread) {
                pAudioThread->Stop();
                pAudioThread.reset();
        }
        log_i("AudioSystem: shut down");
}

VoiceHandle AudioSystem::AllocHandle() {
        VoiceHandle h;
        h.generation = oNextGeneration.fetch_add(1, std::memory_order_relaxed);
        // index is assigned by the audio thread (voice slot index is opaque to game thread)
        // We use generation as the key; index is 0 and matched via generation only.
        h.index = 0;
        return h;
}

void AudioSystem::PostEvent(const AudioEvent& ev) {
        if (!pAudioThread) return;
        if (!pAudioThread->Queue().TryPush(ev)) {
                log_w("AudioSystem: event queue full — dropping event");
        }
}

VoiceHandle AudioSystem::Play(
                H<AudioClip> hClip,
                const PlayFlags& flags,
                glm::vec3 pos,
                glm::vec3 vel) {

        if (!pAudioThread || !hClip.IsValid()) return VoiceHandle{};

        VoiceHandle h = AllocHandle();

        EvtPlayClip oEvt{};
        oEvt.clip_index = hClip.raw.index;
        oEvt.clip_gen   = hClip.raw.generation;
        oEvt.hVoice     = h;
        oEvt.oFlags     = flags;
        oEvt.vPos       = pos;
        oEvt.vVel       = vel;

        PostEvent(AudioEvent::MakePlay(oEvt));
        return h;
}

void AudioSystem::Stop(VoiceHandle h) {
        if (!h.IsValid()) return;
        PostEvent(AudioEvent::MakeStop(h));
}

void AudioSystem::Pause(VoiceHandle h, bool pause) {
        if (!h.IsValid()) return;
        PostEvent(AudioEvent::MakePause(h, pause));
}

void AudioSystem::SetBusGain(MixBusId bus, float gain, float fadeTime) {
        float rate = 0.0f;
        if (fadeTime > 0.0f)
                rate = std::abs(gain - 1.0f) / fadeTime;  // approx; audio thread refines
        PostEvent(AudioEvent::MakeSetBusGain(bus, gain, rate));
}

void AudioSystem::PostUpdateEmitter(VoiceHandle h, glm::vec3 pos, glm::vec3 vel) {
        if (!h.IsValid()) return;
        EvtUpdateEmitter oEvt{h, pos, vel};
        PostEvent(AudioEvent::MakeUpdateEmitter(oEvt));
}

void AudioSystem::PostUpdateListener(glm::vec3 pos, glm::vec3 fwd, glm::vec3 up, glm::vec3 vel) {
        EvtUpdateListener oEvt{pos, fwd, up, vel};
        PostEvent(AudioEvent::MakeUpdateListener(oEvt));
}

void AudioSystem::PostSetVoiceGain(VoiceHandle h, float gain) {
        if (!h.IsValid()) return;
        PostEvent(AudioEvent::MakeSetVoiceGain(h, gain));
}

bool AudioSystem::IsVoiceActive(VoiceHandle h) const {
        if (!pAudioThread || !h.IsValid()) return false;
        return pAudioThread->IsVoiceActive(h);
}

} // namespace SE
