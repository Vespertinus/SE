
// NOTE: SoundCue_generated.h must be generated before compiling this file:
//   flatc --cpp -o generated/ misc/SoundCue.fbs

#include <GlobalTypes.h>
#include <SoundEventSystem.h>
#include <AudioClip.h>
#include <Logging.h>

#include <SoundCue_generated.h>

#include <flatbuffers/flatbuffers.h>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <cmath>

namespace SE {

// ---------------------------------------------------------------------------
// SoundEventSystem
// ---------------------------------------------------------------------------

SoundEventSystem::SoundEventSystem() : oRng(std::random_device{}()) {
        log_i("SoundEventSystem: initialized");
}

void SoundEventSystem::RegisterCue(SoundCue cue) {
        StrID id = cue.id;
        mCues[id] = std::move(cue);
}

MixBusId SoundEventSystem::ResolveBus(StrID bus_id) {
        static const std::unordered_map<StrID, MixBusId> kBusMap = {
                { StrID("master"), MixBusId::Master },
                { StrID("music"),  MixBusId::Music  },
                { StrID("sfx"),    MixBusId::SFX    },
                { StrID("voice"),  MixBusId::Voice  },
                { StrID("ui"),     MixBusId::UI     },
        };
        auto it = kBusMap.find(bus_id);
        return (it != kBusMap.end()) ? it->second : MixBusId::SFX;
}

bool SoundEventSystem::LoadCues(const std::string& sPath) {
        std::ifstream f(sPath, std::ios::binary | std::ios::ate);
        if (!f) {
                log_e("SoundEventSystem: cannot open '{}'", sPath);
                return false;
        }
        const auto sz = f.tellg();
        f.seekg(0);
        std::vector<uint8_t> buf(static_cast<size_t>(sz));
        if (!f.read(reinterpret_cast<char*>(buf.data()), sz)) {
                log_e("SoundEventSystem: read error on '{}'", sPath);
                return false;
        }

        flatbuffers::Verifier v(buf.data(), buf.size());
        if (!SE::FlatBuffers::VerifySoundCueListBuffer(v)) {
                log_e("SoundEventSystem: invalid SECL buffer in '{}'", sPath);
                return false;
        }

        const auto* list = SE::FlatBuffers::GetSoundCueList(buf.data());
        if (!list->cues()) {
                log_i("SoundEventSystem: '{}' loaded (empty)", sPath);
                return true;
        }

        int loaded = 0;
        for (const auto* fbCue : *list->cues()) {
                if (!fbCue->id()) continue;

                SoundCue oCue;
                const std::string_view sCueId = fbCue->id()->c_str();
                oCue.id = StrID(sCueId);
                {
                        size_t start = 0;
                        while (true) {
                                const auto pos = sCueId.find('.', start);
                                if (pos == std::string_view::npos) {
                                        oCue.vHierarchy.push_back(StrID(sCueId));
                                        break;
                                }
                                oCue.vHierarchy.push_back(StrID(sCueId.substr(0, pos)));
                                start = pos + 1;
                        }
                }
                oCue.selection_mode = static_cast<SelectionMode>(fbCue->selection_mode());
                oCue.play_mode      = static_cast<PlayMode>(fbCue->play_mode());
                oCue.volume_min     = fbCue->volume_min();
                oCue.volume_max     = fbCue->volume_max();
                oCue.pitch_min      = fbCue->pitch_min();
                oCue.pitch_max      = fbCue->pitch_max();
                oCue.ref_dist       = fbCue->ref_dist();
                oCue.max_dist       = fbCue->max_dist();
                oCue.cooldown       = fbCue->cooldown();
                oCue.max_polyphony  = fbCue->max_polyphony();
                oCue.bus = fbCue->bus() ? ResolveBus(StrID(fbCue->bus()->c_str()))
                                        : MixBusId::SFX;

                if (fbCue->variations()) {
                        for (const auto* fbVar : *fbCue->variations()) {
                                SoundVariation oVar;
                                if (fbVar->clip()) {
                                        oVar.hClip = CreateResource<AudioClip>(fbVar->clip()->c_str());
                                }
                                oVar.weight       = fbVar->weight();
                                oVar.volume_scale = fbVar->volume_scale();
                                oVar.pitch_scale  = fbVar->pitch_scale();
                                oVar.param_min    = fbVar->param_min();
                                oVar.param_max    = fbVar->param_max();
                                oCue.vVariations.push_back(std::move(oVar));
                        }
                }

                if (fbCue->modulators()) {
                        for (const auto* fbMod : *fbCue->modulators()) {
                                if (!fbMod->parameter()) continue;
                                SoundCue::Modulator oMod;
                                oMod.param_id   = StrID(fbMod->parameter()->c_str());
                                oMod.target     = static_cast<SoundCue::Modulator::Target>(fbMod->target());
                                oMod.input_min  = fbMod->input_min();
                                oMod.input_max  = fbMod->input_max();
                                oMod.output_min = fbMod->output_min();
                                oMod.output_max = fbMod->output_max();
                                oMod.clamp      = fbMod->clamp();
                                oCue.vModulators.push_back(std::move(oMod));
                        }
                }

                RegisterCue(std::move(oCue));
                ++loaded;
        }

        log_i("SoundEventSystem: loaded {} cue(s) from '{}'", loaded, sPath);
        return true;
}

// ---------------------------------------------------------------------------

std::vector<StrID> SoundEventSystem::BuildHierarchy(std::string_view sv) {
        std::vector<StrID> ids;
        size_t start = 0;
        while (true) {
                const auto pos = sv.find('.', start);
                if (pos == std::string_view::npos) {
                        ids.push_back(StrID(sv));
                        break;
                }
                ids.push_back(StrID(sv.substr(0, pos)));
                start = pos + 1;
        }
        return ids;
}

const SoundCue* SoundEventSystem::ResolveCue(const std::vector<StrID>& ids, std::string_view sDebugName) {

        for (int i = static_cast<int>(ids.size()) - 1; i >= 0; --i) {
                auto it = mCues.find(ids[i]);
                if (it != mCues.end()) {
                        if (i < static_cast<int>(ids.size()) - 1) {
                                log_w("SoundEventSystem: '{}' fall back (depth {}->{})",
                                      sDebugName,
                                      static_cast<int>(ids.size()) - 1, i);
                        }
                        return &it->second;
                }
        }
        log_w("SoundEventSystem: cue '{}' not found", sDebugName);
        return nullptr;
}

// ---------------------------------------------------------------------------

void SoundEventSystem::Update(float dt) {
        current_time += dt;

        for (auto it = mCooldowns.begin(); it != mCooldowns.end(); ) {
                if (current_time >= it->second)
                        it = mCooldowns.erase(it);
                else
                        ++it;
        }

        for (auto& [emitter_key, oEmState] : mEmitterState) {
                for (auto& [cue_id, vVoices] : oEmState.mActiveVoices) {
                        vVoices.erase(
                                std::remove_if(vVoices.begin(), vVoices.end(),
                                        [](VoiceHandle h) {
                                                return !GetSystem<AudioSystem>().IsVoiceActive(h);
                                        }),
                                vVoices.end());
                }
        }
}

void SoundEventSystem::ReleaseEmitter(SoundEmitter* pEmitter) {
        if (!pEmitter) return;
        const uint64_t key = reinterpret_cast<uint64_t>(pEmitter);
        mEmitterState.erase(key);
}

} // namespace SE
