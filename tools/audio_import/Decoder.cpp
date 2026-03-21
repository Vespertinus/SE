#include "Decoder.h"

#include <algorithm>
#include <cstring>
#include <cstdio>

// Single-file library implementations.
#define DR_WAV_IMPLEMENTATION
#define DR_MP3_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#define STB_VORBIS_IMPLEMENTATION

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmissing-declarations"

#include <dr_wav.h>
#include <dr_mp3.h>
#include <dr_flac.h>
#include <stb/stb_vorbis.h>

#pragma GCC diagnostic pop

// stb_vorbis defines single-letter macros that pollute the namespace.
#ifdef L
#undef L
#endif
#ifdef R
#undef R
#endif
#ifdef C
#undef C
#endif

#include <spdlog/spdlog.h>

namespace SE::Tools {

bool DecodeFile(const std::string& path, DecodedAudio& out) {

        const auto ext_pos = path.rfind('.');
        if (ext_pos == std::string::npos) {
                spdlog::error("Decoder: no file extension in '{}'", path);
                return false;
        }
        std::string sExt = path.substr(ext_pos + 1);
        std::transform(sExt.begin(), sExt.end(), sExt.begin(), ::tolower);

        if (sExt == "wav") {
                drwav_uint64 frame_count = 0;
                drwav_uint32 sr = 0, ch = 0;
                float* pRaw = drwav_open_file_and_read_pcm_frames_f32(
                                path.c_str(), &ch, &sr, &frame_count, nullptr);
                if (!pRaw) {
                        spdlog::error("Decoder: drwav failed to decode '{}'", path);
                        return false;
                }
                out.sample_rate = sr;
                out.channels    = ch;
                out.samples.assign(pRaw, pRaw + frame_count * ch);
                drwav_free(pRaw, nullptr);

        } else if (sExt == "mp3") {
                drmp3_config oCfg{};
                drmp3_uint64 frame_count = 0;
                float* pRaw = drmp3_open_file_and_read_pcm_frames_f32(
                                path.c_str(), &oCfg, &frame_count, nullptr);
                if (!pRaw) {
                        spdlog::error("Decoder: drmp3 failed to decode '{}'", path);
                        return false;
                }
                out.sample_rate = oCfg.sampleRate;
                out.channels    = oCfg.channels;
                out.samples.assign(pRaw, pRaw + frame_count * oCfg.channels);
                drmp3_free(pRaw, nullptr);

        } else if (sExt == "flac") {
                drflac_uint64 frame_count = 0;
                unsigned int  sr = 0, ch = 0;
                float* pRaw = drflac_open_file_and_read_pcm_frames_f32(
                                path.c_str(), &ch, &sr, &frame_count, nullptr);
                if (!pRaw) {
                        spdlog::error("Decoder: drflac failed to decode '{}'", path);
                        return false;
                }
                out.sample_rate = sr;
                out.channels    = ch;
                out.samples.assign(pRaw, pRaw + frame_count * ch);
                drflac_free(pRaw, nullptr);

        } else if (sExt == "ogg") {
                int sr = 0, ch = 0;
                short* pRawS = nullptr;
                int frame_count = stb_vorbis_decode_filename(path.c_str(), &ch, &sr, &pRawS);
                if (frame_count < 0 || !pRawS) {
                        spdlog::error("Decoder: stb_vorbis failed to decode '{}'", path);
                        return false;
                }
                out.sample_rate = static_cast<uint32_t>(sr);
                out.channels    = static_cast<uint32_t>(ch);
                const size_t total = static_cast<size_t>(frame_count) * ch;
                out.samples.resize(total);
                for (size_t i = 0; i < total; ++i)
                        out.samples[i] = pRawS[i] / 32768.0f;
                free(pRawS);

        } else {
                spdlog::error("Decoder: unsupported format '{}' for '{}'", sExt, path);
                return false;
        }

        spdlog::info("Decoder: loaded '{}' {} Hz {} ch {:.2f}s",
                        path, out.sample_rate, out.channels, out.DurationSeconds());
        return true;
}

} // namespace SE::Tools
