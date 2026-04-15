
#ifdef SE_IMPL

#include <AnimClip.h>
#include <Logging.h>

#include <AnimationClip_generated.h>
#include <flatbuffers/flatbuffers.h>

#include <algorithm>
#include <fstream>
#include <vector>

namespace SE {

// ---------------------------------------------------------------------------
// Path-based constructor — read file, verify identifier, decode
// ---------------------------------------------------------------------------
AnimClip::AnimClip(const std::string& sName, rid_t rid)
        : ResourceHolder(rid, sName) {

        std::ifstream f(sName, std::ios::binary | std::ios::ate);
        if (!f.is_open()) {
                log_e("AnimClip: failed to open '{}'", sName);
                return;
        }
        size_t sz = static_cast<size_t>(f.tellg());
        f.seekg(0);
        std::vector<uint8_t> buf(sz);
        f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(sz));

        // Verify "SEAK" file identifier
        flatbuffers::Verifier verifier(buf.data(), buf.size());
        if (!SE::FlatBuffers::VerifyAnimationClipBuffer(verifier)) {
                log_e("AnimClip: FlatBuffer verify failed for '{}'", sName);
                return;
        }

        auto* fb = SE::FlatBuffers::GetAnimationClip(buf.data());
        LoadFromFB(fb);
        size = static_cast<uint32_t>(sz);
}

// ---------------------------------------------------------------------------
// Inline FlatBuffer constructor
// ---------------------------------------------------------------------------
AnimClip::AnimClip(const std::string& sName, rid_t rid,
                   const SE::FlatBuffers::AnimationClip* pFB)
        : ResourceHolder(rid, sName) {

        LoadFromFB(pFB);
}

// ---------------------------------------------------------------------------
// Decode FlatBuffer data into runtime structures
// ---------------------------------------------------------------------------
void AnimClip::LoadFromFB(const SE::FlatBuffers::AnimationClip* pFB) {

        if (!pFB) {
                log_e("AnimClip: LoadFromFB: null FlatBuffer pointer for '{}'", sName);
                return;
        }

        duration = pFB->duration();
        looping  = pFB->looping();

        // --- Channels ---
        if (pFB->channels()) {
                vChannels.reserve(pFB->channels()->size());
                for (const auto* fbCh : *pFB->channels()) {
                        if (!fbCh) continue;

                        CurveChannel ch;
                        ch.bone_index = fbCh->bone_index();
                        ch.target    = fbCh->target();

                        const SE::FlatBuffers::CurveFormat fbFmt = fbCh->format();
                        const bool isQuantized =
                                (fbFmt == SE::FlatBuffers::CurveFormat::Quantized16);

                        // Determine runtime format — Quantized16 becomes HermiteF32 after dequant
                        if (isQuantized || fbFmt == SE::FlatBuffers::CurveFormat::HermiteF32) {
                                ch.format = Format::HermiteF32;
                        } else if (fbFmt == SE::FlatBuffers::CurveFormat::StepF32) {
                                ch.format = Format::StepF32;
                        } else if (fbFmt == SE::FlatBuffers::CurveFormat::LinearF32) {
                                ch.format = Format::LinearF32;
                        } else {
                                ch.format = Format::ConstantF32;
                        }

                        // Copy times
                        if (fbCh->times()) {
                                ch.vTimes.assign(fbCh->times()->begin(), fbCh->times()->end());
                        }

                        // Copy / dequantize values
                        if (fbCh->values()) {
                                if (isQuantized) {
                                        const float qMin = fbCh->quant_min();
                                        const float qMax = fbCh->quant_max();
                                        const float range = qMax - qMin;
                                        ch.vValues.reserve(fbCh->values()->size());
                                        for (float raw : *fbCh->values()) {
                                                ch.vValues.push_back(raw * range + qMin);
                                        }
                                } else {
                                        ch.vValues.assign(fbCh->values()->begin(),
                                                         fbCh->values()->end());
                                }
                        }

                        // Copy tangents (absent for Constant/Step)
                        if (fbCh->tangents()) {
                                ch.vTangents.assign(fbCh->tangents()->begin(),
                                                   fbCh->tangents()->end());
                        }

                        vChannels.push_back(std::move(ch));
                }
        }

        // --- Events ---
        if (pFB->events()) {
                vEvents.reserve(pFB->events()->size());
                for (const auto* fbEv : *pFB->events()) {
                        if (!fbEv || !fbEv->name()) continue;

                        AnimEvent ev;
                        ev.time   = fbEv->time();
                        ev.name   = fbEv->name()->str();
                        ev.nameID = StrID(ev.name);
                        ev.value  = fbEv->value();
                        vEvents.push_back(ev);
                }
                // Sort by time (should already be sorted at export, but enforce at load)
                std::sort(vEvents.begin(), vEvents.end(),
                          [](const AnimEvent& a, const AnimEvent& b) {
                                  return a.time < b.time;
                          });
        }

        log_d("AnimClip: loaded '{}' dur={:.3f}s loop={} channels={} events={}",
              sName, duration, looping, vChannels.size(), vEvents.size());
}

// ---------------------------------------------------------------------------
std::string AnimClip::Str() const {
        return fmt::format("AnimClip['{}' dur={:.3f}s loop={} ch={} ev={}]",
                           sName, duration, looping,
                           vChannels.size(), vEvents.size());
}

} // namespace SE

#endif // SE_IMPL
