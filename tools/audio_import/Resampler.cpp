#include "Resampler.h"

#include <soxr.h>
#include <spdlog/spdlog.h>
#include <cstring>

namespace SE::Tools {

bool Resample(const std::vector<float>& in,
              uint32_t in_rate,
              uint32_t out_rate,
              uint32_t channels,
              std::vector<float>& out) {

        if (in_rate == out_rate) {
                out = in;
                return true;
        }

        if (in.empty() || channels == 0) {
                spdlog::error("Resampler: empty input or zero channels");
                return false;
        }

        const size_t in_frames  = in.size() / channels;
        // Estimate output frames with a small margin.
        const size_t out_frames = static_cast<size_t>(
                        static_cast<double>(in_frames) * out_rate / in_rate + 2);

        out.resize(out_frames * channels);

        soxr_error_t err;
        soxr_io_spec_t io_spec = soxr_io_spec(SOXR_FLOAT32_I, SOXR_FLOAT32_I);
        soxr_quality_spec_t q_spec = soxr_quality_spec(SOXR_HQ, 0);
        soxr_runtime_spec_t rt_spec = soxr_runtime_spec(1);

        size_t out_actual = 0;
        soxr_t soxr = soxr_create(
                        static_cast<double>(in_rate),
                        static_cast<double>(out_rate),
                        channels,
                        &err,
                        &io_spec,
                        &q_spec,
                        &rt_spec);

        if (err || !soxr) {
                spdlog::error("Resampler: soxr_create failed: {}", soxr_strerror(err));
                return false;
        }

        err = soxr_process(
                        soxr,
                        in.data(),  in_frames,  nullptr,
                        out.data(), out_frames, &out_actual);

        soxr_delete(soxr);

        if (err) {
                spdlog::error("Resampler: soxr_process failed: {}", soxr_strerror(err));
                return false;
        }

        out.resize(out_actual * channels);
        spdlog::info("Resampler: {} -> {} Hz, {} frames -> {} frames",
                        in_rate, out_rate, in_frames, out_actual);
        return true;
}

} // namespace SE::Tools
