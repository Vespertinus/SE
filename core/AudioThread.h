
#ifndef __AUDIO_THREAD_H__
#define __AUDIO_THREAD_H__ 1

#include <thread>
#include <atomic>
#include <future>
#include <cstdint>
#include <memory>
#include <vector>

#include <AudioTypes.h>
#include <SPSCQueue.h>

// Forward-declare AL types so callers don't need openal headers.
typedef unsigned int ALuint;
struct ALCdevice;
struct ALCcontext;

// Forward-declare OpusDecoder (matches the typedef in opus/opus.h).
struct OpusDecoder;

namespace SE { class AudioClip; }

namespace SE {

static constexpr uint32_t MAX_VOICES   = 32;
static constexpr uint32_t QUEUE_SIZE   = 1024;

class AudioThread {

public:
        using TQueue = SPSCQueue<AudioEvent, QUEUE_SIZE>;

        // -----------------------------------------------------------------------
        // Public API — called from game thread only
        // -----------------------------------------------------------------------
        AudioThread();
        ~AudioThread() noexcept;

        /** Start the audio thread; blocks until OpenAL is initialized. */
        bool Start();

        /** Post shutdown event and join the thread. */
        void Stop();

        TQueue& Queue() { return oQueue; }

        // -----------------------------------------------------------------------
        // Streaming state (Opus tier) — one per active streamed voice.
        // Declared public so AudioThread.tcc helper functions can reference it.
        // -----------------------------------------------------------------------
        struct StreamState {
                OpusDecoder*         pDec       = nullptr;
                const AudioClip*     pClip      = nullptr;
                uint64_t             readSample = 0;   // decode head in output samples
                uint64_t             readByte   = 0;   // byte offset in vOpusPayload
                ALuint               alBufs[2]  = {};  // ping-pong AL buffers
                std::vector<float>   xfadeBuffer;      // crossfade accumulator
                bool                 looping    = false;
        };

private:
        // -----------------------------------------------------------------------
        // Voice slot
        // -----------------------------------------------------------------------
        struct VoiceSlot {
                ALuint       al_source  = 0;
                VoiceHandle  hVoice;
                float        priority   = 0.0f;
                bool         active     = false;
                float        gain       = 1.0f;   // user-requested gain (before bus)
                MixBusId     bus        = MixBusId::SFX;
                StreamState* pStream    = nullptr; // non-null for streamed voices
        };

        // -----------------------------------------------------------------------
        // Bus state
        // -----------------------------------------------------------------------
        struct BusState {
                float current_gain = 1.0f;
                float target_gain  = 1.0f;
                float fade_rate    = 0.0f;  // gain/second; 0 = instant
        };

        // -----------------------------------------------------------------------
        // Internal — audio thread only
        // -----------------------------------------------------------------------
        void   ThreadFunc(std::promise<bool> ready_promise);
        bool   InitOpenAL();
        void   ShutdownOpenAL();
        void   Drain();
        void   Tick(float dt);
        void   ReclaimFinished();
        void   TickStreaming();          // feed Opus streaming voices

        VoiceSlot* AllocVoice(float priority);
        VoiceSlot* FindVoice(VoiceHandle h);

        bool StartStreamVoice(VoiceSlot& slot, const AudioClip* clip, bool looping);
        void FreeStreamState(VoiceSlot& slot);

        void ExecPlay(const EvtPlayClip& e);
        void ExecStop(const EvtStopVoice& e);
        void ExecPause(const EvtPauseVoice& e);
        void ExecUpdateEmitter(const EvtUpdateEmitter& e);
        void ExecUpdateListener(const EvtUpdateListener& e);
        void ExecSetBusGain(const EvtSetBusGain& e);
        void ExecSetVoiceGain(const EvtSetVoiceGain& e);

        float EffectiveBusGain(MixBusId bus) const;
        void  ApplyVoiceGain(VoiceSlot& slot);

        // -----------------------------------------------------------------------
        // Data
        // -----------------------------------------------------------------------
        TQueue               oQueue;
        std::thread          oThread;
        std::atomic<bool>    oRunning{false};

        ALCdevice*           pDevice  = nullptr;
        ALCcontext*          pContext = nullptr;

        VoiceSlot            vVoices[MAX_VOICES];
        BusState             vBuses[static_cast<size_t>(MixBusId::COUNT)];
};

} // namespace SE

#endif
