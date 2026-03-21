#pragma once

#include "Encoder.h"
#include <string>
#include <cstdint>

namespace SE::Tools {

struct WriteParams {
        std::string  out_path;
        uint32_t     sample_rate      = 48000;
        uint8_t      channel_count    = 1;
        uint64_t     total_samples    = 0;
        float        normalized_loudness = -18.0f;
        uint64_t     loop_start_sample   = 0;
        uint64_t     loop_end_sample     = 0;
        uint32_t     loop_crossfade_samples = 0;
        uint8_t      bus_hint         = 2;   // SFX
};

// Pack FlatBuffer header + encoded payload → .seac file.
// Returns true on success.
bool WriteAudioClip(const WriteParams& params, const EncodeResult& encoded);

} // namespace SE::Tools
