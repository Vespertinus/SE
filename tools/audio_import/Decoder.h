#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace SE::Tools {

struct DecodedAudio {

        std::vector<float> samples;   // interleaved float [-1, 1]
        uint32_t           sample_rate = 0;
        uint32_t           channels    = 0;

        uint64_t FrameCount() const {
                if (channels == 0) return 0;
                return samples.size() / channels;
        }
        float DurationSeconds() const {
                if (sample_rate == 0 || channels == 0) return 0.0f;
                return static_cast<float>(samples.size()) /
                        (static_cast<float>(sample_rate) * static_cast<float>(channels));
        }
};

// Decode any supported source audio file to interleaved float PCM.
// Supports: .wav, .mp3, .flac, .ogg
// Returns false on failure (error logged to stderr).
bool DecodeFile(const std::string& path, DecodedAudio& out);

} // namespace SE::Tools
