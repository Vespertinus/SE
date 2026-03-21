#include "Writer.h"

#include <flatbuffers/flatbuffers.h>
#include <AudioClip_generated.h>

#include <fstream>
#include <spdlog/spdlog.h>

namespace SE::Tools {

bool WriteAudioClip(const WriteParams& params, const EncodeResult& encoded) {

        flatbuffers::FlatBufferBuilder oBuilder(1024 * 1024);

        // Seek table
        std::vector<SE::FlatBuffers::SeekEntry> vSeekEntries;
        for (auto& se : encoded.seek_table) {
                vSeekEntries.emplace_back(se.sample_offset, se.byte_offset);
        }
        auto vSeekVec = oBuilder.CreateVectorOfStructs(vSeekEntries);

        // Payload
        auto vPayloadVec = oBuilder.CreateVector(encoded.payload);

        // Format enum
        SE::FlatBuffers::AudioFormat fmt = (encoded.tier == AudioTier::Resident)
                ? SE::FlatBuffers::AudioFormat::PCM16
                : SE::FlatBuffers::AudioFormat::Opus;

        SE::FlatBuffers::BusHint oBusHint = static_cast<SE::FlatBuffers::BusHint>(params.bus_hint);

        // Build table
        SE::FlatBuffers::AudioClipBuilder oClipBuilder(oBuilder);
        oClipBuilder.add_schema_version(1);
        oClipBuilder.add_format(fmt);
        oClipBuilder.add_sample_rate(params.sample_rate);
        oClipBuilder.add_channel_count(params.channel_count);
        oClipBuilder.add_total_samples(params.total_samples);
        oClipBuilder.add_normalized_loudness(params.normalized_loudness);
        oClipBuilder.add_loop_start_sample(params.loop_start_sample);
        oClipBuilder.add_loop_end_sample(params.loop_end_sample);
        oClipBuilder.add_loop_crossfade_samples(params.loop_crossfade_samples);
        oClipBuilder.add_pre_skip_samples(encoded.pre_skip);
        oClipBuilder.add_bus_hint(oBusHint);
        oClipBuilder.add_seek_table(vSeekVec);
        oClipBuilder.add_payload(vPayloadVec);

        auto clip = oClipBuilder.Finish();
        oBuilder.Finish(clip, "SEAC");

        // Write to file
        std::ofstream ofs(params.out_path, std::ios::binary | std::ios::trunc);
        if (!ofs) {
                spdlog::error("Writer: cannot open '{}' for writing", params.out_path);
                return false;
        }

        const uint8_t* pData = oBuilder.GetBufferPointer();
        const size_t   size  = oBuilder.GetSize();
        ofs.write(reinterpret_cast<const char*>(pData), static_cast<std::streamsize>(size));

        if (!ofs) {
                spdlog::error("Writer: write failed for '{}'", params.out_path);
                return false;
        }

        spdlog::info("Writer: wrote '{}' ({} bytes)", params.out_path, size);
        return true;
}

} // namespace SE::Tools
