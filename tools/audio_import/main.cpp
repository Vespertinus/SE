#include "Decoder.h"
#include "Lufs.h"
#include "Resampler.h"
#include "Encoder.h"
#include "Writer.h"
#include "Manifest.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Trim leading/trailing silence below threshold_db.
// ---------------------------------------------------------------------------
static void TrimSilence(SE::Tools::DecodedAudio& audio, float threshold_db) {

        const float  thresh = std::pow(10.0f, threshold_db / 20.0f);
        const size_t ch     = audio.channels;
        const size_t n      = audio.samples.size();
        const size_t frames = n / ch;

        auto oFrameMax = [&](size_t f) -> float {
                float m = 0.0f;
                for (size_t c = 0; c < ch; ++c)
                        m = std::max(m, std::abs(audio.samples[f * ch + c]));
                return m;
        };

        size_t start = 0;
        while (start < frames && oFrameMax(start) < thresh) ++start;
        if (start == frames) { audio.samples.clear(); return; }

        size_t end = frames;
        while (end > start && oFrameMax(end - 1) < thresh) --end;

        if (start > 0 || end < frames) {
                std::vector<float> vTrimmed(audio.samples.begin() + start * ch,
                                audio.samples.begin() + end   * ch);
                audio.samples = std::move(vTrimmed);
                spdlog::info("TrimSilence: {} -> {} frames", frames, end - start);
        }
}

// ---------------------------------------------------------------------------
// Downmix to mono: average all channels
// ---------------------------------------------------------------------------
static void DownmixMono(SE::Tools::DecodedAudio& audio) {
        if (audio.channels <= 1) return;
        const size_t ch     = audio.channels;
        const size_t frames = audio.samples.size() / ch;
        std::vector<float> vMono(frames);
        for (size_t f = 0; f < frames; ++f) {
                float sum = 0.0f;
                for (size_t c = 0; c < ch; ++c)
                        sum += audio.samples[f * ch + c];
                vMono[f] = sum / static_cast<float>(ch);
        }
        audio.samples  = std::move(vMono);
        audio.channels = 1;
}

// ---------------------------------------------------------------------------
// CLI option structure
// ---------------------------------------------------------------------------
struct CliOptions {
        std::vector<std::string> inputs;
        std::string out_path;
        std::string tier       = "auto";
        bool        mono       = false;
        uint32_t    rate       = 48000;
        float       lufs       = -18.0f;
        int         opus_kbps  = 128;
        uint64_t    loop_start = 0;
        uint64_t    loop_end   = 0;
        uint32_t    crossfade  = 2048;
        std::string bus        = "sfx";
        bool        dry_run    = false;
};

static void PrintUsage(const char* argv0) {
        fprintf(stderr,
                        "Usage: %s <input> [<input2> ...] [options]\n"
                        "\n"
                        "  -o, --out <path>                 Output file or directory (default: beside input)\n"
                        "  --tier <resident|stream|auto>    Force tier (default: auto)\n"
                        "  --mono                           Downmix to mono\n"
                        "  --rate <hz>                      Override resample target (default 48000)\n"
                        "  --lufs <val>                     Normalisation target (default -18.0)\n"
                        "  --opus-bitrate <kbps>            Opus bitrate in kbps (default 128)\n"
                        "  --loop-start <sample>            Loop start sample index\n"
                        "  --loop-end <sample>              Loop end sample index\n"
                        "  --crossfade <samples>            Loop crossfade window (default 2048)\n"
                        "  --bus <sfx|music|voice|ui>       Mix bus hint\n"
                        "  --dry-run                        Parse and analyse only, no output written\n"
                        "\n", argv0);
}

static bool ParseArgs(int argc, char** argv, CliOptions& opts) {
        
        for (int i = 1; i < argc; ++i) {
                std::string a(argv[i]);
                if (a == "-o" || a == "--out") {
                        if (++i >= argc) return false;
                        opts.out_path = argv[i];
                } else if (a == "--tier") {
                        if (++i >= argc) return false;
                        opts.tier = argv[i];
                } else if (a == "--mono") {
                        opts.mono = true;
                } else if (a == "--rate") {
                        if (++i >= argc) return false;
                        opts.rate = static_cast<uint32_t>(std::stoul(argv[i]));
                } else if (a == "--lufs") {
                        if (++i >= argc) return false;
                        opts.lufs = std::stof(argv[i]);
                } else if (a == "--opus-bitrate") {
                        if (++i >= argc) return false;
                        opts.opus_kbps = std::stoi(argv[i]);
                } else if (a == "--loop-start") {
                        if (++i >= argc) return false;
                        opts.loop_start = std::stoull(argv[i]);
                } else if (a == "--loop-end") {
                        if (++i >= argc) return false;
                        opts.loop_end = std::stoull(argv[i]);
                } else if (a == "--crossfade") {
                        if (++i >= argc) return false;
                        opts.crossfade = static_cast<uint32_t>(std::stoul(argv[i]));
                } else if (a == "--bus") {
                        if (++i >= argc) return false;
                        opts.bus = argv[i];
                } else if (a == "--dry-run") {
                        opts.dry_run = true;
                } else if (a[0] == '-') {
                        fprintf(stderr, "Unknown option: %s\n", a.c_str());
                        return false;
                } else {
                        opts.inputs.push_back(a);
                }
        }
        return !opts.inputs.empty();
}

// ---------------------------------------------------------------------------
// Process one file
// ---------------------------------------------------------------------------
static bool ProcessFile(const std::string& input_path, const CliOptions& opts) {

        spdlog::info("=== Processing '{}' ===", input_path);

        // Try loading sidecar manifest first
        SE::Tools::ImportManifest oManifest;
        oManifest.source      = input_path;
        oManifest.tier        = opts.tier;
        oManifest.target_lufs = opts.lufs;
        oManifest.resample_to = opts.rate;
        oManifest.channels    = opts.mono ? "mono" : "source";
        oManifest.bus_hint    = opts.bus;
        oManifest.opus_bitrate = opts.opus_kbps;
        oManifest.loop_start  = opts.loop_start;
        oManifest.loop_end    = opts.loop_end;
        oManifest.crossfade_samples = opts.crossfade;

        std::string sidecar = SE::Tools::SidecarPath(input_path);
        SE::Tools::ReadManifest(sidecar, oManifest);  // overrides with sidecar if present

        // CLI --tier overrides sidecar
        if (opts.tier != "auto")
            oManifest.tier = opts.tier;

        // Decode
        SE::Tools::DecodedAudio oAudio;
        if (!SE::Tools::DecodeFile(input_path, oAudio))
                return false;

        // Trim silence
        if (oManifest.trim_silence)
                TrimSilence(oAudio, oManifest.silence_threshold_db);

        if (oAudio.samples.empty()) {
                spdlog::error("ProcessFile: audio is empty after trim for '{}'", input_path);
                return false;
        }

        // Downmix
        if (oManifest.channels == "mono")
                DownmixMono(oAudio);

        // Resample
        if (oAudio.sample_rate != oManifest.resample_to) {
                std::vector<float> vResampled;
                if (!SE::Tools::Resample(oAudio.samples, oAudio.sample_rate, oManifest.resample_to,
                                        oAudio.channels, vResampled))
                        return false;
                oAudio.samples     = std::move(vResampled);
                oAudio.sample_rate = oManifest.resample_to;
        }

        const uint64_t frame_count = oAudio.samples.size() / oAudio.channels;
        const float    duration_s  = static_cast<float>(frame_count) /
                static_cast<float>(oAudio.sample_rate);

        // LUFS normalisation
        SE::Tools::NormalizeLufs(oAudio.samples, frame_count, oAudio.channels,
                        oAudio.sample_rate, oManifest.target_lufs);

        // Determine tier
        SE::Tools::AudioTier tier = SE::Tools::DetermineTier(oManifest, duration_s);
        spdlog::info("Tier: {}", (tier == SE::Tools::AudioTier::Resident) ? "resident (PCM16)" : "stream (Opus)");

        if (opts.dry_run) {
                spdlog::info("Dry run — no output written.");
                return true;
        }

        // Determine output path
        std::string out_path = opts.out_path;
        if (out_path.empty() || fs::is_directory(out_path)) {
                fs::path oBase(input_path);
                std::string stem = oBase.stem().string();
                fs::path oDir = out_path.empty() ? oBase.parent_path() : fs::path(out_path);
                out_path = (oDir / (stem + ".seac")).string();
        }

        // Encode
        SE::Tools::EncodeParams oEp;
        oEp.tier         = tier;
        oEp.channels     = oAudio.channels;
        oEp.sample_rate  = oAudio.sample_rate;
        oEp.opus_bitrate = oManifest.opus_bitrate * 1000;

        SE::Tools::EncodeResult oEr;
        if (!SE::Tools::Encode(oAudio.samples, frame_count, oEp, oEr))
                return false;

        // Write
        SE::Tools::WriteParams oWp;
        oWp.out_path              = out_path;
        oWp.sample_rate           = oAudio.sample_rate;
        oWp.channel_count         = static_cast<uint8_t>(oAudio.channels);
        oWp.total_samples         = frame_count;
        oWp.normalized_loudness   = oManifest.target_lufs;
        oWp.loop_start_sample     = oManifest.loop_start;
        oWp.loop_end_sample       = oManifest.loop_end;
        oWp.loop_crossfade_samples = oManifest.crossfade_samples;
        oWp.bus_hint              = SE::Tools::ParseBusHint(oManifest.bus_hint);

        if (!SE::Tools::WriteAudioClip(oWp, oEr))
                return false;

        // Write/update sidecar
        SE::Tools::WriteManifest(sidecar, oManifest);

        return true;
}

// ---------------------------------------------------------------------------
int main(int argc, char** argv) {

        spdlog::set_level(spdlog::level::info);

        CliOptions oOpts;
        if (!ParseArgs(argc, argv, oOpts)) {
                PrintUsage(argv[0]);
                return 1;
        }

        int failures = 0;
        for (const auto& input : oOpts.inputs) {
                if (!ProcessFile(input, oOpts)) {
                        spdlog::error("Failed to process '{}'", input);
                        ++failures;
                }
        }

        return failures > 0 ? 1 : 0;
}
