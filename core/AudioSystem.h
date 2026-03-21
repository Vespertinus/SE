
#ifndef __AUDIO_SYSTEM_H__
#define __AUDIO_SYSTEM_H__ 1

#include <memory>
#include <atomic>
#include <string>

#include <AudioTypes.h>
#include <ResourceHandle.h>

namespace SE {

class AudioClip;
class AudioThread;

/**
 * AudioSystem — game-thread-facing facade.
 *
 * All OpenAL calls are forwarded through the SPSC queue to the audio thread.
 * This class never calls OpenAL directly.
 */
class AudioSystem {

public:
        AudioSystem();
        ~AudioSystem() noexcept;

        // -----------------------------------------------------------------------
        // Playback control
        // -----------------------------------------------------------------------

        /**
         * Begin playing a clip.
         * @return VoiceHandle that can be used to stop/pause the sound.
         *         Returns an invalid handle if AUDIO is disabled or queue is full.
         */
        VoiceHandle Play(H<AudioClip> hClip, const PlayFlags& flags,
                        glm::vec3 pos = {}, glm::vec3 vel = {});

        void Stop(VoiceHandle h);
        void Pause(VoiceHandle h, bool pause = true);

        // -----------------------------------------------------------------------
        // Bus control
        // -----------------------------------------------------------------------
        /** Set bus gain. fadeTime > 0 enables a linear fade in seconds. */
        void SetBusGain(MixBusId bus, float gain, float fadeTime = 0.0f);

        // -----------------------------------------------------------------------
        // Per-frame transform sync (called by AudioSyncSystem)
        // -----------------------------------------------------------------------
        void PostUpdateEmitter(VoiceHandle h, glm::vec3 pos, glm::vec3 vel);
        void PostUpdateListener(glm::vec3 pos, glm::vec3 fwd, glm::vec3 up, glm::vec3 vel);
        void PostSetVoiceGain(VoiceHandle h, float gain);

private:
        VoiceHandle AllocHandle();
        void        PostEvent(const AudioEvent& ev);

        std::unique_ptr<AudioThread> pAudioThread;
        std::atomic<uint32_t>        oNextGeneration{1};
};

} // namespace SE

#endif
