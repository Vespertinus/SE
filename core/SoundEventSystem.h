
#ifndef __SOUND_EVENT_SYSTEM_H__
#define __SOUND_EVENT_SYSTEM_H__ 1

#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <random>
#include <cstdint>
#include <algorithm>
#include <numeric>
#include <cmath>

#include <AudioTypes.h>
#include <ResourceHandle.h>
#include <StrID.h>
#include <SoundContext.h>
#include <AudioSystem.h>
#include <Logging.h>

namespace SE {

class AudioClip;
class SoundEmitter;

// ---------------------------------------------------------------------------
// SoundCue runtime types
// ---------------------------------------------------------------------------

enum class SelectionMode : uint8_t { RANDOM, ROUND_ROBIN, SHUFFLE, PARAMETER, SEQUENTIAL };
enum class PlayMode      : uint8_t { ONE_SHOT, LOOPING, ONE_PER_EMITTER, EXCLUSIVE };

struct SoundVariation {
        H<AudioClip> hClip;
        float weight       = 1.0f;
        float volume_scale = 1.0f;
        float pitch_scale  = 1.0f;
        float param_min    = 0.0f;
        float param_max    = 1.0f;
};

struct SoundCue {
        StrID                  id;
        std::vector<StrID>     vHierarchy;  // [StrID("a"), StrID("a.b"), StrID("a.b.c")] for "a.b.c"
        SelectionMode selection_mode = SelectionMode::SHUFFLE;
        PlayMode      play_mode      = PlayMode::ONE_SHOT;
        std::vector<SoundVariation> vVariations;
        float    volume_min    = 1.0f;
        float    volume_max    = 1.0f;
        float    pitch_min     = 1.0f;
        float    pitch_max     = 1.0f;
        float    ref_dist      = 1.0f;
        float    max_dist      = 40.0f;
        float    cooldown      = 0.0f;
        int      max_polyphony = 4;
        MixBusId bus           = MixBusId::SFX;

        struct Modulator {
                StrID  param_id;
                enum class Target : uint8_t { VOLUME, PITCH, PITCH_AND_VOLUME } target = Target::VOLUME;
                float  input_min  = 0.0f;
                float  input_max  = 1.0f;
                float  output_min = 0.0f;
                float  output_max = 1.0f;
                bool   clamp      = true;
        };
        std::vector<Modulator> vModulators;
};

// ---------------------------------------------------------------------------
// SoundEventResult — filled by Post() to expose resolution details
// ---------------------------------------------------------------------------

struct SoundEventResult {
        bool  fired           = false;
        bool  cooldown_block  = false;
        bool  polyphony_block = false;
        bool  fallback        = false;   // resolved cue differs from requested
        StrID resolved_cue;
        int   variation_idx   = -1;
        float final_volume    = 0.0f;
        float final_pitch     = 0.0f;
};

// ---------------------------------------------------------------------------
// SoundEventSystem
// ---------------------------------------------------------------------------

class SoundEventSystem {
public:
        SoundEventSystem();

        /** Load cues from a .secl FlatBuffers binary. */
        bool LoadCues(const std::string& sPath);

        /** Register a cue directly (e.g. from code). */
        void RegisterCue(SoundCue cue);

        /**
         * Post a sound event. Resolves cue, selects variation, applies
         * modulators, calls AudioSystem::Play().
         * Falls back to parent cue on miss ("a.b.c" → "a.b" → "a").
         * TCtx must inherit from SoundEventContext and shadow GetParameter.
         *
         * Hot-path overload: pass a pre-hashed StrID hierarchy (no string ops at call time).
         * Convenience overload: builds the hierarchy from sEventId string.
         */
        template<class TCtx = SoundEventContext>
        VoiceHandle Post(const std::vector<StrID>& ids, const TCtx& ctx);

        template<class TCtx = SoundEventContext>
        VoiceHandle Post(std::string_view sEventId, const TCtx& ctx);

        /**
         * Debug variant of Post — fills oResult with resolution diagnostics
         * (fired, fallback, block flags, variation index, final volume/pitch).
         * Use from tools and debug/logging code; prefer Post() in hot paths.
         */
        template<class TCtx = SoundEventContext>
        VoiceHandle PostEx(const std::vector<StrID>& ids, const TCtx& ctx,
                           SoundEventResult& oResult);

        template<class TCtx = SoundEventContext>
        VoiceHandle PostEx(std::string_view sEventId, const TCtx& ctx,
                           SoundEventResult& oResult);

        /** Advance cooldown timers and prune finished voices. Call once per frame. */
        void Update(float dt);

        /**
         * Build a pre-hashed StrID hierarchy from a dot-notation cue name.
         * "a.b.c" → [StrID("a"), StrID("a.b"), StrID("a.b.c")].
         * Store the result and pass it to Post() to avoid per-call string hashing.
         */
        static std::vector<StrID> BuildHierarchy(std::string_view sv);

        /** Remove per-emitter state when a SoundEmitter is destroyed. */
        void ReleaseEmitter(SoundEmitter* pEmitter);

private:
        const SoundCue* ResolveCue(const std::vector<StrID>& ids,
                                   std::string_view sDebugName = {});
        MixBusId        ResolveBus(StrID bus_id);

        template<class TCtx>
        const SoundVariation* SelectVariation(const SoundCue&, const TCtx&, uint64_t emitter_key);

        template<class TCtx>
        void ApplyModulators(const SoundCue&, const TCtx&, float& volume, float& pitch);

        template<class TCtx>
        VoiceHandle PostImpl(const std::vector<StrID>& ids, std::string_view sDebugName,
                             const TCtx& ctx, SoundEventResult& oRes);

        std::unordered_map<StrID, SoundCue> mCues;

        struct PerEmitterState {
                std::unordered_map<StrID, std::vector<int>>         mShuffleOrder;
                std::unordered_map<StrID, int>                      mShuffleIdx;
                std::unordered_map<StrID, int>                      mLastIdx;
                std::unordered_map<StrID, int>                      mRoundRobinIdx;
                std::unordered_map<StrID, std::vector<VoiceHandle>> mActiveVoices;
        };
        std::unordered_map<uint64_t, PerEmitterState> mEmitterState;
        std::unordered_map<uint64_t, float>           mCooldowns;

        float        current_time = 0.0f;
        std::mt19937 oRng;
};

// ---------------------------------------------------------------------------
// Template definitions
// ---------------------------------------------------------------------------

// Forward declaration — full definition lives in SystemsImpl.tcc
template<class TSystem> TSystem& GetSystem();

template<class TCtx>
const SoundVariation* SoundEventSystem::SelectVariation(
        const SoundCue& cue,
        const TCtx&     ctx,
        uint64_t        emitter_key)
{
        if (cue.vVariations.empty()) return nullptr;
        const size_t n = cue.vVariations.size();
        if (n == 1) return &cue.vVariations[0];

        auto& state = mEmitterState[emitter_key];

        switch (cue.selection_mode) {
        case SelectionMode::RANDOM: {
                std::uniform_int_distribution<size_t> dist(0, n - 1);
                return &cue.vVariations[dist(oRng)];
        }
        case SelectionMode::SEQUENTIAL:
        case SelectionMode::ROUND_ROBIN: {
                auto& idx = state.mRoundRobinIdx[cue.id];
                const size_t selected = static_cast<size_t>(idx);
                idx = static_cast<int>((static_cast<size_t>(idx) + 1) % n);
                return &cue.vVariations[selected];
        }
        case SelectionMode::SHUFFLE: {
                auto& order = state.mShuffleOrder[cue.id];
                auto& pos   = state.mShuffleIdx[cue.id];
                auto& last  = state.mLastIdx[cue.id];

                if (order.empty() || pos >= static_cast<int>(order.size())) {
                        order.resize(n);
                        std::iota(order.begin(), order.end(), 0);
                        std::shuffle(order.begin(), order.end(), oRng);
                        if (n > 1 && order[0] == last) {
                                std::swap(order[0], order[1]);
                        }
                        pos = 0;
                }
                const int selected = order[pos++];
                last = selected;
                return &cue.vVariations[static_cast<size_t>(selected)];
        }
        case SelectionMode::PARAMETER: {
                const StrID kSelection("selection");
                const StrID param_id = cue.vModulators.empty()
                        ? kSelection
                        : cue.vModulators[0].param_id;
                const float val = ctx.GetParameter(param_id);
                for (const auto& v : cue.vVariations) {
                        if (val >= v.param_min && val <= v.param_max) return &v;
                }
                return &cue.vVariations[0];
        }
        }
        return &cue.vVariations[0];
}

template<class TCtx>
void SoundEventSystem::ApplyModulators(
        const SoundCue& cue,
        const TCtx&     ctx,
        float&          volume,
        float&          pitch)
{
        for (const auto& mod : cue.vModulators) {
                const float input = ctx.GetParameter(mod.param_id);
                const float range = mod.input_max - mod.input_min;
                float t = (range > 1e-9f) ? (input - mod.input_min) / range : 0.0f;
                if (mod.clamp) t = std::max(0.0f, std::min(1.0f, t));
                const float output = mod.output_min + t * (mod.output_max - mod.output_min);

                switch (mod.target) {
                case SoundCue::Modulator::Target::VOLUME:
                        volume *= output; break;
                case SoundCue::Modulator::Target::PITCH:
                        pitch  *= output; break;
                case SoundCue::Modulator::Target::PITCH_AND_VOLUME:
                        volume *= output;
                        pitch  *= output;
                        break;
                }
        }
}

template<class TCtx>
VoiceHandle SoundEventSystem::Post(const std::vector<StrID>& ids, const TCtx& ctx) {
        SoundEventResult oDummy;
        return PostImpl(ids, {}, ctx, oDummy);
}

template<class TCtx>
VoiceHandle SoundEventSystem::Post(std::string_view sEventId, const TCtx& ctx) {
        SoundEventResult oDummy;
        return PostImpl(BuildHierarchy(sEventId), sEventId, ctx, oDummy);
}

template<class TCtx>
VoiceHandle SoundEventSystem::PostEx(const std::vector<StrID>& ids, const TCtx& ctx,
                                     SoundEventResult& oResult) {
        return PostImpl(ids, {}, ctx, oResult);
}

template<class TCtx>
VoiceHandle SoundEventSystem::PostEx(std::string_view sEventId, const TCtx& ctx,
                                     SoundEventResult& oResult) {
        return PostImpl(BuildHierarchy(sEventId), sEventId, ctx, oResult);
}

template<class TCtx>
VoiceHandle SoundEventSystem::PostImpl(const std::vector<StrID>& ids,
                                       std::string_view sDebugName,
                                       const TCtx& ctx,
                                       SoundEventResult& oRes) {
        const SoundCue* pCue = ResolveCue(ids, sDebugName);
        if (!pCue) return VoiceHandle{};

        oRes.resolved_cue = pCue->id;
        if (pCue->id != ids.back()) oRes.fallback = true;

        const uint64_t emitter_key = ctx.pEmitter
                ? reinterpret_cast<uint64_t>(ctx.pEmitter)
                : 0ULL;

        const uint64_t ck = emitter_key
                ^ (static_cast<uint64_t>(pCue->id) * 0x9e3779b97f4a7c15ULL);
        if (pCue->cooldown > 0.0f) {
                auto cit = mCooldowns.find(ck);
                if (cit != mCooldowns.end() && current_time < cit->second) {
                        oRes.cooldown_block = true;
                        return VoiceHandle{};
                }
        }

        auto& oEmState      = mEmitterState[emitter_key];
        auto& vActiveVoices = oEmState.mActiveVoices[pCue->id];
        vActiveVoices.erase(
                std::remove_if(vActiveVoices.begin(), vActiveVoices.end(),
                        [](VoiceHandle h) {
                                return !GetSystem<AudioSystem>().IsVoiceActive(h);
                        }),
                vActiveVoices.end());

        if (static_cast<int>(vActiveVoices.size()) >= pCue->max_polyphony) {
                oRes.polyphony_block = true;
                return VoiceHandle{};
        }

        const SoundVariation* pVar = SelectVariation(*pCue, ctx, emitter_key);
        if (!pVar || !pVar->hClip.IsValid()) return VoiceHandle{};

        oRes.variation_idx = static_cast<int>(pVar - &pCue->vVariations[0]);

        std::uniform_real_distribution<float> volDist(pCue->volume_min, pCue->volume_max);
        std::uniform_real_distribution<float> pitDist(pCue->pitch_min,  pCue->pitch_max);
        float volume = volDist(oRng) * pVar->volume_scale;
        float pitch  = pitDist(oRng) * pVar->pitch_scale;

        ApplyModulators(*pCue, ctx, volume, pitch);

        oRes.final_volume = volume;
        oRes.final_pitch  = pitch;

        if (ctx.volume_override >= 0.0f) volume = ctx.volume_override;
        if (ctx.pitch_override  >= 0.0f) pitch  = ctx.pitch_override;

        PlayFlags oFlags;
        oFlags.gain     = volume;
        oFlags.pitch    = pitch;
        oFlags.loop     = (pCue->play_mode == PlayMode::LOOPING);
        oFlags.spatial  = (ctx.pEmitter != nullptr
                           || ctx.vPosition.x != 0.0f
                           || ctx.vPosition.y != 0.0f
                           || ctx.vPosition.z != 0.0f);
        oFlags.bus      = (ctx.bus_override != MixBusId::COUNT) ? ctx.bus_override : pCue->bus;
        oFlags.ref_dist = pCue->ref_dist;
        oFlags.max_dist = pCue->max_dist;

        VoiceHandle hVoice = GetSystem<AudioSystem>().Play(
                pVar->hClip, oFlags, ctx.vPosition, ctx.vVelocity);

        if (!hVoice.IsValid()) return VoiceHandle{};

        if (pCue->cooldown > 0.0f) {
                mCooldowns[ck] = current_time + pCue->cooldown;
        }
        vActiveVoices.push_back(hVoice);

        oRes.fired = true;
        log_d("SoundEventSystem::Post '{}' fired={} fallback={} cd_block={} poly_block={} "
              "var={} vol={:.3f} pitch={:.3f}",
              sDebugName,
              oRes.fired, oRes.fallback, oRes.cooldown_block, oRes.polyphony_block,
              oRes.variation_idx, oRes.final_volume, oRes.final_pitch);
        return hVoice;
}

} // namespace SE

#endif
