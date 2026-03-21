#include "Manifest.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <spdlog/spdlog.h>

namespace SE::Tools {

using TJson = nlohmann::json;

bool ReadManifest(const std::string& path, ImportManifest& out) {

        std::ifstream ifs(path);
        if (!ifs) {
                spdlog::info("Manifest: '{}' not found — using defaults", path);
                return false;
        }
        try {
                TJson j;
                ifs >> j;
                if (j.contains("source"))              out.source              = j["source"].get<std::string>();
                if (j.contains("tier"))                out.tier                = j["tier"].get<std::string>();
                if (j.contains("format"))              out.format              = j["format"].get<std::string>();
                if (j.contains("target_lufs"))         out.target_lufs         = j["target_lufs"].get<float>();
                if (j.contains("resample_to"))         out.resample_to         = j["resample_to"].get<uint32_t>();
                if (j.contains("channels"))            out.channels            = j["channels"].get<std::string>();
                if (j.contains("bus_hint"))            out.bus_hint            = j["bus_hint"].get<std::string>();
                if (j.contains("opus_bitrate"))        out.opus_bitrate        = j["opus_bitrate"].get<int>();
                if (j.contains("loop_start"))          out.loop_start          = j["loop_start"].get<uint64_t>();
                if (j.contains("loop_end"))            out.loop_end            = j["loop_end"].get<uint64_t>();
                if (j.contains("crossfade_samples"))   out.crossfade_samples   = j["crossfade_samples"].get<uint32_t>();
                if (j.contains("trim_silence"))        out.trim_silence        = j["trim_silence"].get<bool>();
                if (j.contains("silence_threshold_db"))out.silence_threshold_db= j["silence_threshold_db"].get<float>();
        } catch (const std::exception& e) {
                spdlog::error("Manifest: parse error in '{}': {}", path, e.what());
                return false;
        }
        return true;
}

bool WriteManifest(const std::string& path, const ImportManifest& m) {
        TJson j;
        j["source"]               = m.source;
        j["tier"]                 = m.tier;
        j["format"]               = m.format;
        j["target_lufs"]          = m.target_lufs;
        j["resample_to"]          = m.resample_to;
        j["channels"]             = m.channels;
        j["bus_hint"]             = m.bus_hint;
        j["opus_bitrate"]         = m.opus_bitrate;
        j["loop_start"]           = m.loop_start;
        j["loop_end"]             = m.loop_end;
        j["crossfade_samples"]    = m.crossfade_samples;
        j["trim_silence"]         = m.trim_silence;
        j["silence_threshold_db"] = m.silence_threshold_db;

        std::ofstream ofs(path);
        if (!ofs) {
                spdlog::error("Manifest: cannot write '{}'", path);
                return false;
        }
        ofs << j.dump(2) << "\n";
        spdlog::info("Manifest: wrote sidecar '{}'", path);
        return true;
}

std::string SidecarPath(const std::string& source_path) {
        const auto dot = source_path.rfind('.');
        const auto base = (dot != std::string::npos) ? source_path.substr(0, dot) : source_path;
        return base + ".audioimport";
}

uint8_t ParseBusHint(const std::string& s) {
        if (s == "master") return 0;
        if (s == "music")  return 1;
        if (s == "sfx")    return 2;
        if (s == "voice")  return 3;
        if (s == "ui")     return 4;
        return 2; // default SFX
}

AudioTier DetermineTier(const ImportManifest& m, float duration_seconds) {
        if (m.tier == "resident") return AudioTier::Resident;
        if (m.tier == "stream")   return AudioTier::Stream;
        // auto: stream if >= 5 s
        return (duration_seconds >= 5.0f) ? AudioTier::Stream : AudioTier::Resident;
}

} // namespace SE::Tools
