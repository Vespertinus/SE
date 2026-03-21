#include "Encoder.h"

#include <algorithm>
#include <cstring>
#include <cmath>
#include <cassert>

#include <opus/opus.h>
#include <spdlog/spdlog.h>

namespace SE::Tools {

// ---------------------------------------------------------------------------
// PCM16 tier: convert float → int16, interleaved
// ---------------------------------------------------------------------------
static bool EncodePcm16(const std::vector<float>& samples,
                        uint64_t /*frame_count*/,
                        const EncodeParams& /*params*/,
                        EncodeResult& out) {

        const size_t n = samples.size();
        out.payload.resize(n * sizeof(int16_t));

        auto* dst = reinterpret_cast<int16_t*>(out.payload.data());
        for (size_t i = 0; i < n; ++i) {
                float s = std::max(-1.0f, std::min(1.0f, samples[i]));
                dst[i] = static_cast<int16_t>(s * 32767.0f);
        }

        out.tier      = AudioTier::Resident;
        out.pre_skip  = 0;
        spdlog::info("Encoder: PCM16 payload {} bytes ({} samples)",
                        out.payload.size(), n);
        return true;
}

// ---------------------------------------------------------------------------
// Opus tier
// ---------------------------------------------------------------------------
// Opus frame size options (samples per channel at 48 kHz):
//   2.5 ms=120, 5 ms=240, 10 ms=480, 20 ms=960, 40 ms=1920, 60 ms=2880
static constexpr int      OPUS_FRAME_SIZE = 960;// 20 ms at 48 kHz
static constexpr uint32_t OPUS_PRE_SKIP = 312;  // standard encoder pre-skip @ 48 kHz

static bool EncodeOpus(const std::vector<float>& samples,
                       uint64_t frame_count,
                       const EncodeParams& params,
                       EncodeResult& out) {

        int err = 0;
        OpusEncoder* enc = opus_encoder_create(
                        static_cast<opus_int32>(params.sample_rate),
                        static_cast<int>(params.channels),
                        OPUS_APPLICATION_AUDIO,
                        &err);

        if (err != OPUS_OK || !enc) {
                spdlog::error("Encoder: opus_encoder_create failed: {}", opus_strerror(err));
                return false;
        }

        opus_encoder_ctl(enc, OPUS_SET_BITRATE(params.opus_bitrate));
        opus_encoder_ctl(enc, OPUS_SET_COMPLEXITY(10));
        opus_encoder_ctl(enc, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));

        // Maximum output buffer per Opus packet (generous upper bound)
        const int MAX_PACKET = 4000;
        std::vector<uint8_t> vPacketBuf(MAX_PACKET);

        const uint32_t seek_interval_samples =
                params.seek_interval_ms * params.sample_rate / 1000;

        out.tier     = AudioTier::Stream;
        out.pre_skip = OPUS_PRE_SKIP;
        out.payload.clear();
        out.seek_table.clear();

        uint64_t sample_head = 0;  // current position in output samples
        size_t   frame_head  = 0;  // position in input samples array

        while (frame_head < samples.size()) {
                // Build one Opus frame of OPUS_FRAME_SIZE frames (pad with zeros at end)
                std::vector<float> frameBuf(static_cast<size_t>(OPUS_FRAME_SIZE) * params.channels, 0.0f);
                size_t toCopy = std::min(
                                static_cast<size_t>(OPUS_FRAME_SIZE) * params.channels,
                                samples.size() - frame_head);
                std::memcpy(frameBuf.data(), samples.data() + frame_head, toCopy * sizeof(float));

                // Seek table entry every seek_interval_samples
                if (sample_head == 0 ||
                                sample_head % seek_interval_samples < static_cast<uint64_t>(OPUS_FRAME_SIZE)) {
                        EncodeResult::SeekEntry se;
                        se.sample_offset = sample_head;
                        se.byte_offset   = out.payload.size();
                        out.seek_table.push_back(se);
                }

                // Store packet length prefix (4 bytes LE) then packet data
                opus_int32 bytes = opus_encode_float(
                                enc,
                                frameBuf.data(),
                                OPUS_FRAME_SIZE,
                                vPacketBuf.data(),
                                MAX_PACKET);

                if (bytes < 0) {
                        spdlog::error("Encoder: opus_encode_float failed: {}", opus_strerror(bytes));
                        opus_encoder_destroy(enc);
                        return false;
                }

                uint32_t len = static_cast<uint32_t>(bytes);
                // Length prefix (little-endian)
                out.payload.push_back(static_cast<uint8_t>(len & 0xFF));
                out.payload.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
                out.payload.push_back(static_cast<uint8_t>((len >> 16) & 0xFF));
                out.payload.push_back(static_cast<uint8_t>((len >> 24) & 0xFF));
                out.payload.insert(out.payload.end(), vPacketBuf.begin(), vPacketBuf.begin() + bytes);

                frame_head  += static_cast<size_t>(OPUS_FRAME_SIZE) * params.channels;
                sample_head += static_cast<uint64_t>(OPUS_FRAME_SIZE);
        }

        opus_encoder_destroy(enc);

        spdlog::info("Encoder: Opus payload {} bytes, {} packets, {} seek entries",
                        out.payload.size(),
                        frame_count / OPUS_FRAME_SIZE + 1,
                        out.seek_table.size());
        return true;
}

// ---------------------------------------------------------------------------
bool Encode(const std::vector<float>& samples,
            uint64_t frame_count,
            const EncodeParams& params,
            EncodeResult& out) {

        if (params.tier == AudioTier::Resident)
                return EncodePcm16(samples, frame_count, params, out);
        else
                return EncodeOpus(samples, frame_count, params, out);
}

} // namespace SE::Tools
