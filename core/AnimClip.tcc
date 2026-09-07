
#ifdef SE_IMPL

#include <AnimClip.h>
#include <Logging.h>

#include <AnimationClip_generated.h>
#include <flatbuffers/flatbuffers.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <unordered_map>
#include <vector>

namespace SE {

// ---------------------------------------------------------------------------
// Path-based constructor — read file, verify identifier, decode
// ---------------------------------------------------------------------------
AnimClip::AnimClip(const std::string& sName, rid_t rid)
        : ResourceHolder(rid, sName) {

        std::ifstream f(sName, std::ios::binary | std::ios::ate);
        if (!f.is_open()) {
                throw(std::runtime_error(fmt::format("AnimClip: failed to open '{}'", sName)));
        }
        size_t sz = static_cast<size_t>(f.tellg());
        f.seekg(0);
        std::vector<uint8_t> buf(sz);
        f.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(sz));

        // Verify "SEAK" file identifier
        flatbuffers::Verifier verifier(buf.data(), buf.size());
        if (!SE::FlatBuffers::VerifyAnimationClipBuffer(verifier)) {
                throw(std::runtime_error(fmt::format("AnimClip: FlatBuffer verify failed for '{}'", sName)));
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
                throw(std::runtime_error(fmt::format("AnimClip: LoadFromFB: null FlatBuffer pointer for '{}'", sName)));
                return;
        }

        duration           = pFB->duration();
        looping            = pFB->looping();
        delta_translations = pFB->delta_translations();
        src_pelvis_scale = pFB->src_pelvis_scale();

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

        EnforceQuatContinuity();

        log_d("AnimClip: loaded '{}' dur={:.3f}s loop={} channels={} events={}",
              sName, duration, looping, vChannels.size(), vEvents.size());
}

// ---------------------------------------------------------------------------
// Quaternion sign continuity (fixes the "limb sweeps through the wrong arc"
// artifact).
//
// Rotation channels (targets 3..6) are stored per component and interpolated
// per component at runtime (see SampleCurve). q and -q describe the same
// rotation, but a sign flip between adjacent keys makes that interpolation
// sweep the limb the long way round — e.g. thigh_r in the shipped UAL clips
// ("leg rotates behind the head" in jump_start/walk/sprint). Importers emit
// discontinuous quaternions whenever their internal euler wrap flips
// hemisphere, so the fix belongs at decode: walk each bone's 4 rotation
// channels in key order and negate every key whose quaternion has a negative
// dot product with its predecessor — values AND hermite tangents, so cubic
// segments keep their shape. This mirrors what UE5/Unity importers do
// (quaternion continuity fixup at import time).
// ---------------------------------------------------------------------------
void AnimClip::EnforceQuatContinuity() {

        constexpr uint8_t kRotTarget0 = 3;   // targets 3,4,5,6 = rot x,y,z,w
        constexpr size_t  kRotAxes    = 4;

        // Gather the 4 rotation channels of each bone (bone -> [x,y,z,w])
        std::unordered_map<uint16_t, std::array<CurveChannel*, kRotAxes>> mBones;
        mBones.reserve(vChannels.size() / kRotAxes);
        for (auto& ch : vChannels) {
                if (ch.target >= kRotTarget0 && ch.target < kRotTarget0 + kRotAxes) {
                        mBones[ch.bone_index][ch.target - kRotTarget0] = &ch;
                }
        }

        for (auto& [bone, vAxes] : mBones) {

                bool complete = true;
                const size_t key_cnt = vAxes[0] ? vAxes[0]->vValues.size() : 0;
                for (size_t axis = 0; axis < kRotAxes; ++axis) {
                        if (!vAxes[axis] || vAxes[axis]->vValues.size() != key_cnt) {
                                complete = false;
                                break;
                        }
                }
                if (!complete || key_cnt < 2) continue;   // partial rig — leave as-is

                for (size_t k = 1; k < key_cnt; ++k) {

                        float dot = 0.0f;
                        for (size_t axis = 0; axis < kRotAxes; ++axis) {
                                dot += vAxes[axis]->vValues[k - 1] * vAxes[axis]->vValues[k];
                        }
                        if (dot >= 0.0f) continue;

                        // Same rotation, opposite hemisphere — flip key k in place
                        for (size_t axis = 0; axis < kRotAxes; ++axis) {
                                CurveChannel& ch = *vAxes[axis];
                                ch.vValues[k] = -ch.vValues[k];
                                if (ch.vTangents.size() == key_cnt) {
                                        ch.vTangents[k] = -ch.vTangents[k];
                                }
                        }
                }
        }
}

// ---------------------------------------------------------------------------
std::string AnimClip::Str() const {
        return fmt::format("AnimClip['{}' dur={:.3f}s loop={} ch={} ev={}]",
                           sName, duration, looping,
                           vChannels.size(), vEvents.size());
}

} // namespace SE

#endif // SE_IMPL
