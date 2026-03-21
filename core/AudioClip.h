
#ifndef __AUDIO_CLIP_H__
#define __AUDIO_CLIP_H__ 1

#include <string>
#include <vector>
#include <cstdint>

#include <ResourceHolder.h>

// Forward-declare the AL buffer type so we don't pull openal headers here.
// The actual AL/al.h include is in AudioClip.tcc (audio thread only).
typedef unsigned int ALuint;

namespace SE::FlatBuffers { struct AudioClip; }

namespace SE {

/**
 * AudioClip — in-memory decoded PCM audio asset, or streamed Opus bitstream.
 *
 * Two tiers:
 *   Resident (bStreamed == false): float PCM decoded on construction (game
 *     thread), uploaded lazily to an OpenAL static buffer on the audio thread.
 *   Streamed  (bStreamed == true):  raw Opus payload stored in vOpusPayload;
 *     the audio thread decodes on-the-fly with ping-pong AL buffers.
 *
 * Both tiers support loop points and per-clip bus hints from the .audioclip
 * FlatBuffer header.
 *
 * Follows the ResourceHolder pattern: ctor takes (sName, rid).
 */
class AudioClip : public ResourceHolder {

        // --- Resident PCM tier ---
        std::vector<float> vSamples;    // interleaved normalized float [-1, 1]

        // --- Resident upload state (audio thread only) ---
        ALuint             al_buffer   = 0;
        bool               uploaded    = false;

        void Decode(const std::string& path);
        void LoadAudioClipFile(const std::string& path);  // parse .audioclip
        void LoadFromFB(const SE::FlatBuffers::AudioClip* pFB);  // parse inline FlatBuffer
        void Upload();   // audio thread only — resident tier

public:
        // --- Common ---
        uint32_t  sample_rate = 0;
        uint32_t  channels    = 0;

        // --- Streaming (Opus tier) ---
        bool                   bStreamed    = false;
        std::vector<uint8_t>   vOpusPayload;   // raw Opus bitstream (length-prefixed packets)
        uint32_t               pre_skip    = 0; // samples to discard after decoder init

        // Seek table (250 ms keyframes): sample_offset → byte_offset in vOpusPayload
        struct SeekEntry { uint64_t sample_offset; uint64_t byte_offset; };
        std::vector<SeekEntry> vSeekTable;

        // --- Loop points (both tiers) ---
        uint64_t  loop_start_sample     = 0;
        uint64_t  loop_end_sample       = 0;    // 0 = no loop
        uint32_t  loop_crossfade_samples = 0;

        // Total frames (set from .audioclip header; used for streamed duration).
        uint64_t  total_samples = 0;

        // --- Bus hint from asset ---
        uint8_t   bus_hint = 2;   // SFX

        AudioClip(const std::string& sName, rid_t rid);
        AudioClip(const std::string& sName, rid_t rid,
                  const SE::FlatBuffers::AudioClip* pFB);
        ~AudioClip() noexcept;

        // Called from audio thread — uploads if needed, returns AL buffer id.
        // Returns 0 for streamed clips (use vOpusPayload instead).
        ALuint GetALBuffer();

        uint32_t SampleRate()  const { return sample_rate; }
        uint32_t Channels()    const { return channels; }
        uint32_t SampleCount() const { return static_cast<uint32_t>(vSamples.size()); }
        bool     IsStreamed()   const { return bStreamed; }
        float    DurationSeconds() const;

        // Resident PCM data (non-empty only for resident tier).
        const std::vector<float>& Samples() const { return vSamples; }

        std::string Str() const;
};

} // namespace SE

#endif
