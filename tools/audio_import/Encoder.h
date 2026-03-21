#pragma once

#include <vector>
#include <cstdint>
#include <string>

namespace SE::Tools {

enum class AudioTier { Resident, Stream };

struct EncodeResult {
        AudioTier        tier          = AudioTier::Resident;
        std::vector<uint8_t> payload;                  // PCM16 or Opus bitstream
        uint32_t         pre_skip     = 0;             // Opus pre-skip samples (0 for PCM)

        // 250 ms seek table entries: [sample_offset, byte_offset]
        // Only populated for Opus tier.
        struct SeekEntry {
                uint64_t sample_offset;
                uint64_t byte_offset;
        };
        std::vector<SeekEntry> seek_table;
};

struct EncodeParams {
        AudioTier tier          = AudioTier::Resident;
        uint32_t  channels      = 1;
        uint32_t  sample_rate   = 48000;
        int       opus_bitrate  = 128000;   // bits/s
        // Seek table interval for Opus streaming: 250 ms
        uint32_t  seek_interval_ms = 250;
};

// Encode interleaved float32 PCM to the target tier.
// samples: interleaved float [-1, 1], frame_count * channels elements.
bool Encode(const std::vector<float>& samples,
            uint64_t frame_count,
            const EncodeParams& params,
            EncodeResult& out);

} // namespace SE::Tools
