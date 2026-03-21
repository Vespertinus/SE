
#include <GlobalTypes.h>
#include <AudioClip.h>
#include <Logging.h>

// Single-file library implementations — must be defined once.
#define DR_WAV_IMPLEMENTATION
#define DR_MP3_IMPLEMENTATION
#define DR_FLAC_IMPLEMENTATION
#define STB_VORBIS_IMPLEMENTATION

// Suppress warnings from third-party headers.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wmissing-declarations"

#include <dr_wav.h>
#include <dr_mp3.h>
#include <dr_flac.h>
#include <stb/stb_vorbis.h>

#pragma GCC diagnostic pop

// stb_vorbis defines single-letter macros L, R, C that pollute the global
// namespace and conflict with engine identifiers (e.g. Keys::R).
#ifdef L
#undef L
#endif
#ifdef R
#undef R
#endif
#ifdef C
#undef C
#endif

#ifdef SE_AUDIO_ENABLED
#include <AL/al.h>
#include <AL/alc.h>
#endif

#include <AudioClip_generated.h>
#include <flatbuffers/flatbuffers.h>

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace SE {

AudioClip::AudioClip(const std::string& sName, rid_t rid)
        : ResourceHolder(rid, sName) {

        Decode(sName);
}

AudioClip::AudioClip(const std::string& sName, rid_t rid,
                     const SE::FlatBuffers::AudioClip* pFB)
        : ResourceHolder(rid, sName) {

        LoadFromFB(pFB);
}

AudioClip::~AudioClip() noexcept {
#ifdef SE_AUDIO_ENABLED
        if (uploaded && al_buffer != 0) {
                alDeleteBuffers(1, &al_buffer);
        }
#endif
}

// ---------------------------------------------------------------------------
// Parse a deserialized FlatBuffer AudioClip table (no file I/O)
// ---------------------------------------------------------------------------
void AudioClip::LoadFromFB(const SE::FlatBuffers::AudioClip* fb) {

        if (!fb || !fb->payload()) {
                log_e("AudioClip: LoadFromFB: null fb or missing payload in '{}'", sName);
                return;
        }

        sample_rate            = fb->sample_rate();
        channels               = fb->channel_count();
        total_samples          = fb->total_samples();
        loop_start_sample      = fb->loop_start_sample();
        loop_end_sample        = fb->loop_end_sample();
        loop_crossfade_samples = fb->loop_crossfade_samples();
        pre_skip               = fb->pre_skip_samples();
        bus_hint               = static_cast<uint8_t>(fb->bus_hint());

        // Seek table
        if (fb->seek_table()) {
                for (auto* se : *fb->seek_table()) {
                        vSeekTable.push_back({se->sample_offset(), se->byte_offset()});
                }
        }

        const auto* payload = fb->payload();

        if (fb->format() == SE::FlatBuffers::AudioFormat::PCM16) {
                bStreamed = false;
                const size_t n_samples = payload->size() / sizeof(int16_t);
                vSamples.resize(n_samples);
                const int16_t* src = reinterpret_cast<const int16_t*>(payload->data());
                for (size_t i = 0; i < n_samples; ++i)
                        vSamples[i] = src[i] / 32768.0f;

                size = static_cast<uint32_t>(vSamples.size() * sizeof(float));
                log_i("AudioClip: loaded resident '{}' {} Hz {} ch {:.2f}s",
                      sName, sample_rate, channels, DurationSeconds());

        } else if (fb->format() == SE::FlatBuffers::AudioFormat::Opus) {
                bStreamed = true;
                vOpusPayload.assign(payload->begin(), payload->end());

                size = static_cast<uint32_t>(vOpusPayload.size());
                log_i("AudioClip: loaded streamed '{}' {} Hz {} ch {} bytes Opus",
                      sName, sample_rate, channels, vOpusPayload.size());

        } else {
                log_e("AudioClip: unknown format {} in '{}'",
                      static_cast<int>(fb->format()), sName);
        }
}

// ---------------------------------------------------------------------------
// Load a baked .audioclip FlatBuffer file
// ---------------------------------------------------------------------------
void AudioClip::LoadAudioClipFile(const std::string& path) {

        std::ifstream ifs(path, std::ios::binary | std::ios::ate);
        if (!ifs) {
                log_e("AudioClip: cannot open '{}'", path);
                return;
        }
        const size_t file_size = static_cast<size_t>(ifs.tellg());
        ifs.seekg(0);
        std::vector<uint8_t> buf(file_size);
        ifs.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(file_size));

        // Verify "SEAC" file identifier
        flatbuffers::Verifier verifier(buf.data(), buf.size());
        if (!SE::FlatBuffers::VerifyAudioClipBuffer(verifier)) {
                log_e("AudioClip: FlatBuffer verify failed for '{}'", path);
                return;
        }

        const SE::FlatBuffers::AudioClip* fb =
                SE::FlatBuffers::GetAudioClip(buf.data());

        LoadFromFB(fb);
}

// ---------------------------------------------------------------------------
void AudioClip::Decode(const std::string& path) {

        // Determine format by extension.
        const auto ext_pos = path.rfind('.');
        if (ext_pos == std::string::npos) {
                log_e("AudioClip: no file extension in path '{}'", path);
                return;
        }
        std::string ext = path.substr(ext_pos + 1);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        // Baked binary container — dispatch to FlatBuffer loader.
        if (ext == "seac") {
                LoadAudioClipFile(path);
                return;
        }

        if (ext == "wav") {
                drwav_uint64 frame_count = 0;
                drwav_uint32 sr = 0, ch = 0;
                float* pRaw = drwav_open_file_and_read_pcm_frames_f32(
                                path.c_str(), &ch, &sr, &frame_count, nullptr);
                if (!pRaw) {
                        log_e("AudioClip: drwav failed to decode '{}'", path);
                        return;
                }
                sample_rate = sr;
                channels    = ch;
                vSamples.assign(pRaw, pRaw + frame_count * ch);
                drwav_free(pRaw, nullptr);

        } else if (ext == "mp3") {
                drmp3_config oCfg{};
                drmp3_uint64 frame_count = 0;
                float* pRaw = drmp3_open_file_and_read_pcm_frames_f32(
                                path.c_str(), &oCfg, &frame_count, nullptr);
                if (!pRaw) {
                        log_e("AudioClip: drmp3 failed to decode '{}'", path);
                        return;
                }
                sample_rate = oCfg.sampleRate;
                channels    = oCfg.channels;
                vSamples.assign(pRaw, pRaw + frame_count * oCfg.channels);
                drmp3_free(pRaw, nullptr);

        } else if (ext == "flac") {
                drflac_uint64 frame_count = 0;
                unsigned int  sr = 0, ch = 0;
                float* pRaw = drflac_open_file_and_read_pcm_frames_f32(
                                path.c_str(), &ch, &sr, &frame_count, nullptr);
                if (!pRaw) {
                        log_e("AudioClip: drflac failed to decode '{}'", path);
                        return;
                }
                sample_rate = sr;
                channels    = ch;
                vSamples.assign(pRaw, pRaw + frame_count * ch);
                drflac_free(pRaw, nullptr);

        } else if (ext == "ogg") {
                int sr = 0, ch = 0;
                short* pRawS = nullptr;
                int frame_count = stb_vorbis_decode_filename(path.c_str(), &ch, &sr, &pRawS);
                if (frame_count < 0 || !pRawS) {
                        log_e("AudioClip: stb_vorbis failed to decode '{}'", path);
                        return;
                }
                sample_rate = static_cast<uint32_t>(sr);
                channels    = static_cast<uint32_t>(ch);
                const size_t total = static_cast<size_t>(frame_count) * ch;
                vSamples.resize(total);
                for (size_t i = 0; i < total; ++i)
                        vSamples[i] = pRawS[i] / 32768.0f;
                free(pRawS);
        } else {
                log_e("AudioClip: unsupported format '{}' for '{}'", ext, path);
                return;
        }

        size = static_cast<uint32_t>(vSamples.size() * sizeof(float));
        log_i("AudioClip: loaded '{}' {} Hz {} ch {} frames",
                        path, sample_rate, channels,
                        vSamples.size() / std::max(1u, channels));
}

void AudioClip::Upload() {
#ifdef SE_AUDIO_ENABLED
        if (uploaded || vSamples.empty()) return;

        // Convert float samples to 16-bit signed PCM for maximum compatibility.
        const size_t n = vSamples.size();
        std::vector<int16_t> vPcm16(n);
        for (size_t i = 0; i < n; ++i) {
                float s = vSamples[i];
                if (s >  1.0f) s =  1.0f;
                if (s < -1.0f) s = -1.0f;
                vPcm16[i] = static_cast<int16_t>(s * 32767.0f);
        }

        ALenum format = (channels == 2) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;

        alGenBuffers(1, &al_buffer);
        alBufferData(al_buffer, format,
                        vPcm16.data(),
                        static_cast<ALsizei>(vPcm16.size() * sizeof(int16_t)),
                        static_cast<ALsizei>(sample_rate));

        ALenum err = alGetError();
        if (err != AL_NO_ERROR) {
                log_e("AudioClip: alBufferData error {} for '{}'", err, sName);
                alDeleteBuffers(1, &al_buffer);
                al_buffer = 0;
                return;
        }

        uploaded = true;
        log_i("AudioClip: uploaded AL buffer {} for '{}'", al_buffer, sName);
#endif
}

ALuint AudioClip::GetALBuffer() {
        Upload();
        return al_buffer;
}

float AudioClip::DurationSeconds() const {
        if (sample_rate == 0) return 0.0f;
        if (bStreamed) {
                // Use total_samples from the header (frames, not interleaved samples).
                return static_cast<float>(total_samples) / static_cast<float>(sample_rate);
        }
        if (channels == 0) return 0.0f;
        return static_cast<float>(vSamples.size()) /
                (static_cast<float>(sample_rate) * static_cast<float>(channels));
}

std::string AudioClip::Str() const {
        return fmt::format("AudioClip['{}' {}Hz {}ch {:.2f}s]",
                        sName,
                        sample_rate,
                        channels,
                        DurationSeconds());
}

} // namespace SE
