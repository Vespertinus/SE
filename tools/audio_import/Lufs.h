#pragma once

#include <cstddef>
#include <vector>
#include <cstdint>

namespace SE::Tools {

// ITU-R BS.1770-4 integrated loudness measurement.
// K-weighted pre-filter (high-shelf) + RLB (high-pass) biquad chain.
// Mean-square power gated at -70 LUFS absolute threshold.
//
// Returns integrated loudness in LUFS.
// Returns -INFINITY if the signal is silent / below gate.
float MeasureLufs(const float* samples, size_t frame_count,
                  uint32_t channels, uint32_t sample_rate);

// Apply a linear gain so that the clip reads target_lufs after encoding.
// Modifies samples in-place.
void NormalizeLufs(std::vector<float>& samples,
                   size_t frame_count,
                   uint32_t channels,
                   uint32_t sample_rate,
                   float target_lufs = -18.0f);

} // namespace SE::Tools
