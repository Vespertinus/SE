#pragma once

#include "Encoder.h"
#include <string>
#include <cstdint>

namespace SE::Tools {

struct ImportManifest {
        std::string source;
        std::string tier         = "auto";   // "auto", "resident", "stream"
        std::string format       = "auto";
        float       target_lufs  = -18.0f;
        uint32_t    resample_to  = 48000;
        std::string channels     = "source"; // "source", "mono", "stereo"
        std::string bus_hint     = "sfx";    // "sfx", "music", "voice", "ambient", "ui"
        int         opus_bitrate = 128;      // kbps
        uint64_t    loop_start   = 0;
        uint64_t    loop_end     = 0;
        uint32_t    crossfade_samples = 2048;
        bool        trim_silence      = true;
        float       silence_threshold_db = -60.0f;
};

// Read manifest from a .audioimport JSON sidecar file.
// Returns false on parse error.
bool ReadManifest(const std::string& path, ImportManifest& out);

// Write (or overwrite) a .audioimport JSON sidecar file.
bool WriteManifest(const std::string& path, const ImportManifest& manifest);

// Derive the sidecar path from an audio source file path:
//   foo/bar.wav  →  foo/bar.audioimport
std::string SidecarPath(const std::string& source_path);

// Parse bus_hint string to uint8_t (BusHint enum value).
uint8_t ParseBusHint(const std::string& s);

// Determine tier from manifest fields and audio duration.
AudioTier DetermineTier(const ImportManifest& m, float duration_seconds);

} // namespace SE::Tools
