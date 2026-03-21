#pragma once

#include <vector>
#include <cstdint>

namespace SE::Tools {

// Resample interleaved float PCM using libsoxr (sinc interpolation).
// in_rate / out_rate: sample rates in Hz.
// channels: number of interleaved channels.
// Returns true on success.
bool Resample(const std::vector<float>& in,
              uint32_t in_rate,
              uint32_t out_rate,
              uint32_t channels,
              std::vector<float>& out);

} // namespace SE::Tools
