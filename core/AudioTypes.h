
#ifndef __AUDIO_TYPES_H__
#define __AUDIO_TYPES_H__ 1

#include <cstdint>
#include <glm/glm.hpp>

namespace SE {

// ---------------------------------------------------------------------------
// VoiceHandle — stale-handle protection (same pattern as BodyHandle)
// ---------------------------------------------------------------------------
struct VoiceHandle {
        uint32_t index      = 0;
        uint32_t generation = 0;

        bool IsValid() const { return generation != 0; }

        bool operator==(const VoiceHandle& o) const {
                return index == o.index && generation == o.generation;
        }
        bool operator!=(const VoiceHandle& o) const { return !(*this == o); }
};

// ---------------------------------------------------------------------------
// Mix bus identifiers
// ---------------------------------------------------------------------------
enum class MixBusId : uint8_t {
        Master = 0,
        Music  = 1,
        SFX    = 2,
        Voice  = 3,
        UI     = 4,
        COUNT
};

// ---------------------------------------------------------------------------
// PlayFlags — per-voice settings passed to AudioSystem::Play
// ---------------------------------------------------------------------------
struct PlayFlags {
        bool      loop      = false;
        bool      spatial   = true;
        MixBusId  bus       = MixBusId::SFX;
        float     pitch     = 1.0f;
        float     gain      = 1.0f;
        float     ref_dist  = 1.0f;
        float     max_dist  = 100.0f;
        float     rolloff   = 1.0f;
};

// ---------------------------------------------------------------------------
// Event structs — trivially copyable so they can live in the SPSC queue
// ---------------------------------------------------------------------------

struct EvtPlayClip {
        uint32_t  clip_index;       // ResourceHandle raw index
        uint32_t  clip_gen;         // ResourceHandle raw generation
        VoiceHandle hVoice;
        PlayFlags   oFlags;
        glm::vec3   vPos;
        glm::vec3   vVel;
};

struct EvtStopVoice {
        VoiceHandle hVoice;
};

struct EvtPauseVoice {
        VoiceHandle hVoice;
        bool        pause;
};

struct EvtUpdateEmitter {
        VoiceHandle hVoice;
        glm::vec3   vPos;
        glm::vec3   vVel;
};

struct EvtUpdateListener {
        glm::vec3 vPos;
        glm::vec3 vFwd;
        glm::vec3 vUp;
        glm::vec3 vVel;
};

struct EvtSetBusGain {
        MixBusId bus;
        float    target_gain;
        float    fade_rate;   // gain units per second; 0 = instant
};

struct EvtSetVoiceGain {
        VoiceHandle hVoice;
        float       gain;
};

struct EvtShutdown {};

// ---------------------------------------------------------------------------
// Tagged union event
// ---------------------------------------------------------------------------
enum class AudioEventTag : uint8_t {
        PlayClip,
        StopVoice,
        PauseVoice,
        UpdateEmitter,
        UpdateListener,
        SetBusGain,
        SetVoiceGain,
        Shutdown
};

struct AudioEvent {

        AudioEventTag tag = AudioEventTag::Shutdown;

        // Explicit default ctor required because union members have glm::vec3
        // which is non-trivial.
        AudioEvent() noexcept {}

        union {
                EvtPlayClip       play;
                EvtStopVoice      stop;
                EvtPauseVoice     pause;
                EvtUpdateEmitter  update_emitter;
                EvtUpdateListener update_listener;
                EvtSetBusGain     set_bus_gain;
                EvtSetVoiceGain   set_voice_gain;
                EvtShutdown       shutdown;
        };

        static AudioEvent MakePlay(const EvtPlayClip& e)
        { AudioEvent ev; ev.tag = AudioEventTag::PlayClip; ev.play = e; return ev; }
        static AudioEvent MakeStop(VoiceHandle h)
        { AudioEvent ev; ev.tag = AudioEventTag::StopVoice; ev.stop = {h}; return ev; }
        static AudioEvent MakePause(VoiceHandle h, bool p)
        { AudioEvent ev; ev.tag = AudioEventTag::PauseVoice; ev.pause = {h, p}; return ev; }
        static AudioEvent MakeUpdateEmitter(const EvtUpdateEmitter& e)
        { AudioEvent ev; ev.tag = AudioEventTag::UpdateEmitter; ev.update_emitter = e; return ev; }
        static AudioEvent MakeUpdateListener(const EvtUpdateListener& e)
        { AudioEvent ev; ev.tag = AudioEventTag::UpdateListener; ev.update_listener = e; return ev; }
        static AudioEvent MakeSetBusGain(MixBusId bus, float gain, float rate)
        { AudioEvent ev; ev.tag = AudioEventTag::SetBusGain; ev.set_bus_gain = {bus, gain, rate}; return ev; }
        static AudioEvent MakeSetVoiceGain(VoiceHandle h, float gain)
        { AudioEvent ev; ev.tag = AudioEventTag::SetVoiceGain; ev.set_voice_gain = {h, gain}; return ev; }
        static AudioEvent MakeShutdown()
        { AudioEvent ev; ev.tag = AudioEventTag::Shutdown; ev.shutdown = {}; return ev; }
};

// ---------------------------------------------------------------------------
// AudioEmitterDesc — plain descriptor used by AudioEmitter component ctor
// ---------------------------------------------------------------------------
struct AudioEmitterDesc {
        std::string sClipPath;
        PlayFlags   oFlags;
        bool        auto_play = false;
};

} // namespace SE

#endif
