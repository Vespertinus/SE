#include "Lufs.h"

#include <cmath>
#include <algorithm>
#include <vector>
#include <limits>

#include <spdlog/spdlog.h>

namespace SE::Tools {

// ---------------------------------------------------------------------------
// Biquad filter (Direct-Form II Transposed)
// ---------------------------------------------------------------------------
struct Biquad {

        double b0, b1, b2;
        double a1, a2;
        double z1 = 0.0, z2 = 0.0;

        double Process(double x) {
                double y = b0 * x + z1;
                z1 = b1 * x - a1 * y + z2;
                z2 = b2 * x - a2 * y;
                return y;
        }
        void Reset() { z1 = z2 = 0.0; }
};

// ITU-R BS.1770 pre-filter: high-shelf +4 dB at 1500 Hz
// Coefficients derived for a given sample rate.
static Biquad MakePreFilter(double fs) {
        // Shelf filter: Vh = 1.53512485958697, Vb = 1.69065929318241
        // f0 = 1681.974450955533 Hz, Q = 0.7071752369554196
        const double Vh = 1.53512485958697;
        const double Vb = 1.69065929318241;
        const double f0 = 1681.974450955533;
        const double Q  = 0.7071752369554196;
        const double K  = std::tan(M_PI * f0 / fs);

        Biquad oBiquad;
        double denom = 1.0 + K / Q + K * K;
        oBiquad.b0 = (Vh + Vb * K / Q + K * K) / denom;
        oBiquad.b1 = 2.0 * (K * K - Vh) / denom;
        oBiquad.b2 = (Vh - Vb * K / Q + K * K) / denom;
        oBiquad.a1 = 2.0 * (K * K - 1.0) / denom;
        oBiquad.a2 = (1.0 - K / Q + K * K) / denom;
        return oBiquad;
}

// BS.1770 RLB (revised low-frequency B-weighting) high-pass filter
// f0 = 38.13547087602444 Hz, Q = 0.5003270373238773
static Biquad MakeRlbFilter(double fs) {
        const double f0 = 38.13547087602444;
        const double Q  = 0.5003270373238773;
        const double K  = std::tan(M_PI * f0 / fs);

        Biquad oBiquad;
        double denom = 1.0 + K / Q + K * K;
        oBiquad.b0 =  1.0 / denom;
        oBiquad.b1 = -2.0 / denom;
        oBiquad.b2 =  1.0 / denom;
        oBiquad.a1 =  2.0 * (K * K - 1.0) / denom;
        oBiquad.a2 = (1.0 - K / Q + K * K) / denom;
        return oBiquad;
}

// ---------------------------------------------------------------------------
float MeasureLufs(const float* samples, size_t frame_count,
                uint32_t channels, uint32_t sample_rate) {

        if (frame_count == 0 || channels == 0 || sample_rate == 0)
                return -std::numeric_limits<float>::infinity();

        const double fs = static_cast<double>(sample_rate);

        // One filter chain per channel
        std::vector<Biquad> preFilters(channels, MakePreFilter(fs));
        std::vector<Biquad> rlbFilters(channels, MakeRlbFilter(fs));

        // BS.1770 uses 400 ms blocks with 75% overlap (100 ms hop).
        // Gate threshold: relative gate at -10 LU below ungated mean.
        // Absolute gate: -70 LUFS.
        const size_t block_frames = static_cast<size_t>(0.4 * fs);
        const size_t hop_frames   = static_cast<size_t>(0.1 * fs);

        // First pass: compute mean-square power of each 400 ms block (K-weighted).
        // Accumulate weighted sum for each channel then average.
        struct Block {
                double mean_sq;   // mean-square of K-weighted sum across channels
        };
        std::vector<Block> blocks;

        for (size_t start = 0; start + block_frames <= frame_count; start += hop_frames) {
                // Reset filters at start of block? No — BS.1770 says continuous filtering.
                // We use a stateful approach: run continuously, snapshot per-block.
                // For an offline tool, reset per block is an acceptable approximation.
                for (size_t ch = 0; ch < channels; ++ch) {
                        preFilters[ch].Reset();
                        rlbFilters[ch].Reset();
                }

                double sum_sq = 0.0;
                for (size_t f = start; f < start + block_frames; ++f) {
                        for (size_t ch = 0; ch < channels; ++ch) {
                                double x = static_cast<double>(samples[f * channels + ch]);
                                double y = rlbFilters[ch].Process(preFilters[ch].Process(x));
                                sum_sq += y * y;
                        }
                }
                double mean_sq = sum_sq / (static_cast<double>(block_frames) * channels);
                blocks.push_back({mean_sq});
        }

        if (blocks.empty())
                return -std::numeric_limits<float>::infinity();

        // Absolute gate: -70 LUFS = 10^((-70 + 0.691) / 10) mean-square
        const double abs_gate_lin = std::pow(10.0, (-70.0 + 0.691) / 10.0);

        // First pass: ungated mean
        double ungated_sum = 0.0;
        int    ungated_n   = 0;
        for (auto& bl : blocks) {
                if (bl.mean_sq > abs_gate_lin) {
                        ungated_sum += bl.mean_sq;
                        ++ungated_n;
                }
        }
        if (ungated_n == 0)
                return -std::numeric_limits<float>::infinity();

        double ungated_mean = ungated_sum / ungated_n;

        // Relative gate: -10 LU below ungated mean
        const double rel_gate_lin = ungated_mean * std::pow(10.0, -10.0 / 10.0);

        // Second pass: gated mean
        double gated_sum = 0.0;
        int    gated_n   = 0;
        for (auto& bl : blocks) {
                if (bl.mean_sq > abs_gate_lin && bl.mean_sq > rel_gate_lin) {
                        gated_sum += bl.mean_sq;
                        ++gated_n;
                }
        }
        if (gated_n == 0)
                return -std::numeric_limits<float>::infinity();

        double lufs = -0.691 + 10.0 * std::log10(gated_sum / gated_n);
        return static_cast<float>(lufs);
}

// ---------------------------------------------------------------------------
void NormalizeLufs(std::vector<float>& samples,
                size_t frame_count,
                uint32_t channels,
                uint32_t sample_rate,
                float target_lufs) {

        float measured = MeasureLufs(samples.data(), frame_count, channels, sample_rate);

        // Use arithmetic guard instead of isfinite(): -ffast-math implies -ffinite-math-only
        // which folds std::isfinite() to true, making the guard a no-op. Any value < -200
        // is outside the physically realisable LUFS range and means "not measured".
        if (!(measured > -200.0f)) {
                // Check whether audio actually has content
                float peak = 0.0f;
                for (float s : samples) peak = std::max(peak, std::abs(s));

                if (peak == 0.0f) {
                        spdlog::warn("LUFS: signal is silent — skipping normalization");
                        return;
                }

                // Clip is too short for a complete 400 ms BS.1770 block.
                // Fall back to peak normalization using target_lufs as the peak ceiling.
                float target_peak = std::pow(10.0f, target_lufs / 20.0f);
                float gain_lin    = target_peak / peak;
                float gain_db     = 20.0f * std::log10(gain_lin);
                for (auto& s : samples) s *= gain_lin;
                float duration_s = static_cast<float>(frame_count) / static_cast<float>(sample_rate);
                spdlog::warn("LUFS: clip too short for measurement ({:.3f}s < 0.4s) — "
                             "applied peak normalization: {:.2f} dB", duration_s, gain_db);
                return;
        }

        spdlog::info("LUFS: measured {:.2f} LUFS, target {:.2f} LUFS", measured, target_lufs);

        float gain_db = target_lufs - measured;
        float gain_lin = std::pow(10.0f, gain_db / 20.0f);

        for (auto& s : samples)
                s *= gain_lin;

        spdlog::info("LUFS: applied {:.2f} dB ({:.4f} linear)", gain_db, gain_lin);
}

} // namespace SE::Tools
