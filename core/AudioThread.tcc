
#include <GlobalTypes.h>
#include <AudioThread.h>
#include <AudioClip.h>
#include <Logging.h>

#ifdef SE_AUDIO_ENABLED
#include <AL/al.h>
#include <AL/alc.h>
#endif

#ifdef SE_OPUS_ENABLED
#include <opus/opus.h>
#endif

#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace SE {

AudioThread::AudioThread() {

        // Initialize bus gains to 1.0 (already done by aggregate init)
        for (auto& oBus : vBuses) {
                oBus = BusState{1.0f, 1.0f, 0.0f};
        }
}

AudioThread::~AudioThread() noexcept {
        if (oRunning.load()) {
                Stop();
        }
}

bool AudioThread::Start() {

        std::promise<bool> ready;
        auto oFut = ready.get_future();
        oRunning.store(true);
        oThread = std::thread(&AudioThread::ThreadFunc, this, std::move(ready));
        return oFut.get();
}

void AudioThread::Stop() {
        oQueue.TryPush(AudioEvent::MakeShutdown());
        if (oThread.joinable())
                oThread.join();
        oRunning.store(false);
}

bool AudioThread::IsVoiceActive(VoiceHandle h) const {
        if (!h.IsValid()) return false;
        for (uint32_t i = 0; i < MAX_VOICES; ++i) {
                if (vActiveGen[i].load(std::memory_order_relaxed) == h.generation)
                        return true;
        }
        return false;
}

// ---------------------------------------------------------------------------
// Thread entry point
// ---------------------------------------------------------------------------
void AudioThread::ThreadFunc(std::promise<bool> ready_promise) {

#ifdef SE_AUDIO_ENABLED
        if (!InitOpenAL()) {
                ready_promise.set_value(false);
                return;
        }
        ready_promise.set_value(true);

        using TClock = std::chrono::steady_clock;
        auto prev = TClock::now();

        while (oRunning.load(std::memory_order_relaxed)) {
                auto now = TClock::now();
                float dt = std::chrono::duration<float>(now - prev).count();
                prev = now;

                Drain();
                Tick(dt);
                TickStreaming();
                ReclaimFinished();

                std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }

        ShutdownOpenAL();
#else
        ready_promise.set_value(true);
        // No-op loop when audio is disabled
        while (oRunning.load(std::memory_order_relaxed)) {
                AudioEvent oEv;
                while (oQueue.TryPop(oEv)) {
                        if (oEv.tag == AudioEventTag::Shutdown) {
                                oRunning.store(false);
                                return;
                        }
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
#endif
}

// ---------------------------------------------------------------------------
bool AudioThread::InitOpenAL() {
#ifdef SE_AUDIO_ENABLED
        pDevice = alcOpenDevice(nullptr);
        if (!pDevice) {
                log_e("AudioThread: alcOpenDevice failed");
                return false;
        }
        pContext = alcCreateContext(pDevice, nullptr);
        if (!pContext) {
                log_e("AudioThread: alcCreateContext failed");
                alcCloseDevice(pDevice);
                pDevice = nullptr;
                return false;
        }
        if (!alcMakeContextCurrent(pContext)) {
                log_e("AudioThread: alcMakeContextCurrent failed");
                alcDestroyContext(pContext);
                alcCloseDevice(pDevice);
                pContext = nullptr;
                pDevice  = nullptr;
                return false;
        }

        // Pre-generate all AL sources
        for (auto& oSlot : vVoices) {
                alGenSources(1, &oSlot.al_source);
                ALenum err = alGetError();
                if (err != AL_NO_ERROR) {
                        log_w("AudioThread: alGenSources failed for slot (err {})", err);
                        oSlot.al_source = 0;
                }
        }

        log_i("AudioThread: OpenAL initialized — {}",
                        alcGetString(pDevice, ALC_DEVICE_SPECIFIER));
        return true;
#else
        return true;
#endif
}

void AudioThread::ShutdownOpenAL() {
#ifdef SE_AUDIO_ENABLED
        for (auto& oSlot : vVoices) {
                if (oSlot.al_source) {
                        alSourceStop(oSlot.al_source);
                        alDeleteSources(1, &oSlot.al_source);
                        oSlot.al_source = 0;
                }
        }
        alcMakeContextCurrent(nullptr);
        if (pContext) { alcDestroyContext(pContext); pContext = nullptr; }
        if (pDevice)  { alcCloseDevice(pDevice);     pDevice  = nullptr; }
        log_i("AudioThread: OpenAL shut down");
#endif
}

// ---------------------------------------------------------------------------
// Drain + Tick
// ---------------------------------------------------------------------------
void AudioThread::Drain() {
        AudioEvent oEv;
        while (oQueue.TryPop(oEv)) {
                switch (oEv.tag) {
                        case AudioEventTag::PlayClip:
                                ExecPlay(oEv.play);
                                break;
                        case AudioEventTag::StopVoice:
                                ExecStop(oEv.stop);
                                break;
                        case AudioEventTag::PauseVoice:
                                ExecPause(oEv.pause);
                                break;
                        case AudioEventTag::UpdateEmitter:
                                ExecUpdateEmitter(oEv.update_emitter);
                                break;
                        case AudioEventTag::UpdateListener:
                                ExecUpdateListener(oEv.update_listener);
                                break;
                        case AudioEventTag::SetBusGain:
                                ExecSetBusGain(oEv.set_bus_gain);
                                break;
                        case AudioEventTag::SetVoiceGain:
                                ExecSetVoiceGain(oEv.set_voice_gain);
                                break;
                        case AudioEventTag::Shutdown:
                                oRunning.store(false);
                                return;
                }
        }
}

void AudioThread::Tick(float dt) {
#ifdef SE_AUDIO_ENABLED
        // Advance bus fades
        for (auto& oBus : vBuses) {
                if (oBus.fade_rate > 0.0f) {
                        float delta = oBus.fade_rate * dt;
                        if (oBus.current_gain < oBus.target_gain)
                                oBus.current_gain = std::min(oBus.current_gain + delta, oBus.target_gain);
                        else if (oBus.current_gain > oBus.target_gain)
                                oBus.current_gain = std::max(oBus.current_gain - delta, oBus.target_gain);
                        else
                                oBus.fade_rate = 0.0f;
                }
        }

        // Re-apply effective gain to all active voices after bus update
        for (auto& oSlot : vVoices) {
                if (oSlot.active && oSlot.al_source)
                        ApplyVoiceGain(oSlot);
        }
#endif
}

void AudioThread::ReclaimFinished() {
#ifdef SE_AUDIO_ENABLED
        for (auto& oSlot : vVoices) {
                if (!oSlot.active) continue;
                if (!oSlot.al_source) {           // stuck slot — clean up defensively
                        FreeStreamState(oSlot);
                        vActiveGen[static_cast<size_t>(&oSlot - vVoices)].store(0, std::memory_order_relaxed);
                        oSlot.active = false;
                        oSlot.hVoice = VoiceHandle{};
                        continue;
                }
                // Streamed voices are managed by TickStreaming; don't reclaim them here.
                if (oSlot.pStream) continue;

                ALint state = AL_STOPPED;
                alGetSourcei(oSlot.al_source, AL_SOURCE_STATE, &state);
                if (state == AL_STOPPED) {
                        alSourcei(oSlot.al_source, AL_BUFFER, 0);
                        vActiveGen[static_cast<size_t>(&oSlot - vVoices)].store(0, std::memory_order_relaxed);
                        oSlot.active = false;
                        oSlot.hVoice = VoiceHandle{};
                }
        }
#endif
}

// ---------------------------------------------------------------------------
// Voice allocation
// ---------------------------------------------------------------------------
AudioThread::VoiceSlot* AudioThread::AllocVoice(float priority) {

        // Find a free slot first
        for (auto& oSlot : vVoices) {
                if (!oSlot.active && oSlot.al_source) return &oSlot;   // require valid source
        }
        // Steal lowest-priority active slot
        VoiceSlot* pVictim = nullptr;
        for (auto& oSlot : vVoices) {
                if (!oSlot.al_source) continue;                         // skip bad slots
                if (!pVictim || oSlot.priority < pVictim->priority)
                        pVictim = &oSlot;
        }
        if (pVictim && pVictim->priority < priority) {
#ifdef SE_AUDIO_ENABLED
                alSourceStop(pVictim->al_source);
                alSourcei(pVictim->al_source, AL_BUFFER, 0);
#endif
                FreeStreamState(*pVictim);
                vActiveGen[static_cast<size_t>(pVictim - vVoices)].store(0, std::memory_order_relaxed);
                pVictim->active = false;
                pVictim->hVoice = VoiceHandle{};
                return pVictim;
        }
        return nullptr;  // nothing stealable
}

AudioThread::VoiceSlot* AudioThread::FindVoice(VoiceHandle h) {

        if (!h.IsValid()) return nullptr;
        for (auto& oSlot : vVoices) {
                if (oSlot.active && oSlot.hVoice == h)
                        return &oSlot;
        }
        return nullptr;
}

// ---------------------------------------------------------------------------
// Event executors
// ---------------------------------------------------------------------------
void AudioThread::ExecPlay(const EvtPlayClip& e) {
#ifdef SE_AUDIO_ENABLED
        // Resolve clip handle back to resource
        using RawHandle = H<AudioClip>;
        RawHandle hClip;
        hClip.raw.index      = e.clip_index;
        hClip.raw.generation = e.clip_gen;

        AudioClip* pClip = GetResource(hClip);
        if (!pClip) {
                log_w("AudioThread::ExecPlay: invalid clip handle");
                return;
        }

        VoiceSlot* pSlot = AllocVoice(e.oFlags.gain);
        if (!pSlot) {
                log_w("AudioThread::ExecPlay: no voice available — dropped");
                return;
        }

        pSlot->hVoice   = e.hVoice;
        pSlot->priority = e.oFlags.gain;
        pSlot->gain     = e.oFlags.gain;
        pSlot->bus      = e.oFlags.bus;
        pSlot->active   = true;
        vActiveGen[static_cast<size_t>(pSlot - vVoices)].store(e.hVoice.generation, std::memory_order_relaxed);

        if (pClip->IsStreamed()) {
                // Streamed Opus tier — set up ping-pong buffer queue
                if (!StartStreamVoice(*pSlot, pClip, e.oFlags.loop)) {
                        log_w("AudioThread::ExecPlay: failed to start stream for '{}'",
                              pClip->Name());
                        vActiveGen[static_cast<size_t>(pSlot - vVoices)].store(0, std::memory_order_relaxed);
                        pSlot->active = false;
                        pSlot->hVoice = VoiceHandle{};
                        return;
                }
        } else {
                ALuint buf = pClip->GetALBuffer();
                if (buf == 0) {
                        log_w("AudioThread::ExecPlay: clip '{}' has no AL buffer", pClip->Name());
                        vActiveGen[static_cast<size_t>(pSlot - vVoices)].store(0, std::memory_order_relaxed);
                        pSlot->active = false;
                        pSlot->hVoice = VoiceHandle{};
                        return;
                }
                alSourcei(pSlot->al_source, AL_BUFFER, static_cast<ALint>(buf));
                alSourcei(pSlot->al_source, AL_LOOPING, e.oFlags.loop ? AL_TRUE : AL_FALSE);
        }

        alSourcef(pSlot->al_source, AL_PITCH, e.oFlags.pitch);

        if (e.oFlags.spatial) {
                alSourcei(pSlot->al_source,  AL_SOURCE_RELATIVE, AL_FALSE);
                alSource3f(pSlot->al_source, AL_POSITION, e.vPos.x, e.vPos.y, e.vPos.z);
                alSource3f(pSlot->al_source, AL_VELOCITY, e.vVel.x, e.vVel.y, e.vVel.z);
                alSourcef(pSlot->al_source,  AL_REFERENCE_DISTANCE, e.oFlags.ref_dist);
                alSourcef(pSlot->al_source,  AL_MAX_DISTANCE,       e.oFlags.max_dist);
                alSourcef(pSlot->al_source,  AL_ROLLOFF_FACTOR,     e.oFlags.rolloff);
        } else {
                alSourcei(pSlot->al_source,  AL_SOURCE_RELATIVE, AL_TRUE);
                alSource3f(pSlot->al_source, AL_POSITION, 0.0f, 0.0f, 0.0f);
        }

        ApplyVoiceGain(*pSlot);
        alGetError();   // discard any errors accumulated by setup calls
        alSourcePlay(pSlot->al_source);

        ALenum err = alGetError();
        if (err != AL_NO_ERROR) {
                log_w("AudioThread::ExecPlay: alSourcePlay error {}", err);
                FreeStreamState(*pSlot);
                vActiveGen[static_cast<size_t>(pSlot - vVoices)].store(0, std::memory_order_relaxed);
                pSlot->active = false;
                pSlot->hVoice = VoiceHandle{};
        }
#endif
}

void AudioThread::ExecStop(const EvtStopVoice& e) {
#ifdef SE_AUDIO_ENABLED
        VoiceSlot* pSlot = FindVoice(e.hVoice);
        if (!pSlot) return;
        alSourceStop(pSlot->al_source);
        alSourcei(pSlot->al_source, AL_BUFFER, 0);
        FreeStreamState(*pSlot);
        vActiveGen[static_cast<size_t>(pSlot - vVoices)].store(0, std::memory_order_relaxed);
        pSlot->active = false;
        pSlot->hVoice = VoiceHandle{};
#endif
}

void AudioThread::ExecPause(const EvtPauseVoice& e) {
#ifdef SE_AUDIO_ENABLED
        VoiceSlot* pSlot = FindVoice(e.hVoice);

        if (!pSlot) return;

        if (e.pause)
                alSourcePause(pSlot->al_source);
        else
                alSourcePlay(pSlot->al_source);
#endif
}

void AudioThread::ExecUpdateEmitter(const EvtUpdateEmitter& e) {
#ifdef SE_AUDIO_ENABLED
        VoiceSlot* pSlot = FindVoice(e.hVoice);
        if (!pSlot) return;
        alSource3f(pSlot->al_source, AL_POSITION, e.vPos.x, e.vPos.y, e.vPos.z);
        alSource3f(pSlot->al_source, AL_VELOCITY, e.vVel.x, e.vVel.y, e.vVel.z);
#endif
}

void AudioThread::ExecUpdateListener(const EvtUpdateListener& e) {
#ifdef SE_AUDIO_ENABLED
        alListener3f(AL_POSITION, e.vPos.x, e.vPos.y, e.vPos.z);
        alListener3f(AL_VELOCITY, e.vVel.x, e.vVel.y, e.vVel.z);
        float vOri[6] = {
                e.vFwd.x, e.vFwd.y, e.vFwd.z,
                e.vUp.x,  e.vUp.y,  e.vUp.z
        };
        alListenerfv(AL_ORIENTATION, vOri);
#endif
}

void AudioThread::ExecSetBusGain(const EvtSetBusGain& e) {
        const auto idx = static_cast<size_t>(e.bus);
        if (idx >= static_cast<size_t>(MixBusId::COUNT)) return;
        auto& oBus = vBuses[idx];
        oBus.target_gain = e.target_gain;
        if (e.fade_rate <= 0.0f) {
                oBus.current_gain = e.target_gain;
                oBus.fade_rate    = 0.0f;
        } else {
                oBus.fade_rate = e.fade_rate;
        }
}

void AudioThread::ExecSetVoiceGain(const EvtSetVoiceGain& e) {
        VoiceSlot* pSlot = FindVoice(e.hVoice);
        if (!pSlot) return;
        pSlot->gain = e.gain;
        ApplyVoiceGain(*pSlot);
}

// ---------------------------------------------------------------------------
float AudioThread::EffectiveBusGain(MixBusId bus) const {

        const auto idx = static_cast<size_t>(bus);
        if (idx >= static_cast<size_t>(MixBusId::COUNT)) return 1.0f;
        // Chain: bus * Master
        float g = vBuses[idx].current_gain;
        if (bus != MixBusId::Master)
                g *= vBuses[static_cast<size_t>(MixBusId::Master)].current_gain;
        return g;
}

void AudioThread::ApplyVoiceGain(VoiceSlot& slot) {
#ifdef SE_AUDIO_ENABLED
        const float g = slot.gain * EffectiveBusGain(slot.bus);
        alSourcef(slot.al_source, AL_GAIN, g);
#endif
}

// ---------------------------------------------------------------------------
// Streaming — Opus ping-pong buffer support
// ---------------------------------------------------------------------------

// Number of PCM frames decoded per AL buffer fill
static constexpr int STREAM_CHUNK_FRAMES = 4096;

void AudioThread::FreeStreamState(VoiceSlot& slot) {
#if defined(SE_AUDIO_ENABLED) && defined(SE_OPUS_ENABLED)
        if (!slot.pStream) return;
        StreamState* st = slot.pStream;
        if (st->pDec) {
                opus_decoder_destroy(st->pDec);
                st->pDec = nullptr;
        }
        if (st->alBufs[0]) {
                // Stop source and unqueue all buffers before deleting
                alSourceStop(slot.al_source);
                ALint queued = 0;
                alGetSourcei(slot.al_source, AL_BUFFERS_QUEUED, &queued);
                if (queued > 0) {
                        ALuint tmp[2];
                        alSourceUnqueueBuffers(slot.al_source,
                                               std::min(queued, 2), tmp);
                }
                alDeleteBuffers(2, st->alBufs);
                st->alBufs[0] = st->alBufs[1] = 0;
        }
        delete st;
        slot.pStream = nullptr;
#else
        (void)slot;
#endif
}

// Decode the next STREAM_CHUNK_FRAMES of Opus data into pcmOut (interleaved float).
// Advances st.readSample and st.readByte.
// Returns number of frames decoded (0 = end of stream or error).
#if defined(SE_AUDIO_ENABLED) && defined(SE_OPUS_ENABLED)
static int DecodeNextChunk(AudioThread::StreamState& st,
                           std::vector<float>& pcmOut,
                           int channels) {
    const AudioClip* clip = st.pClip;
    const auto& payload   = clip->vOpusPayload;
    if (st.readByte >= payload.size()) return 0;

    pcmOut.resize(static_cast<size_t>(STREAM_CHUNK_FRAMES) * channels);

    // Read length prefix (4 bytes LE)
    if (st.readByte + 4 > payload.size()) return 0;
    uint32_t pktLen = 0;
    std::memcpy(&pktLen, payload.data() + st.readByte, 4);
    st.readByte += 4;

    if (st.readByte + pktLen > payload.size()) return 0;
    const uint8_t* pkt = payload.data() + st.readByte;

    int decoded = opus_decode_float(
        st.pDec, pkt, static_cast<opus_int32>(pktLen),
        pcmOut.data(), STREAM_CHUNK_FRAMES, 0);

    if (decoded < 0) {
        return 0;
    }
    st.readByte   += pktLen;
    st.readSample += static_cast<uint64_t>(decoded);
    pcmOut.resize(static_cast<size_t>(decoded) * channels);
    return decoded;
}
#endif

bool AudioThread::StartStreamVoice(VoiceSlot& slot,
                                    const AudioClip* clip,
                                    bool looping) {
#if defined(SE_AUDIO_ENABLED) && defined(SE_OPUS_ENABLED)
        int err = 0;
        OpusDecoder* dec = opus_decoder_create(
            static_cast<opus_int32>(clip->sample_rate),
            static_cast<int>(clip->channels),
            &err);
        if (err != OPUS_OK || !dec) {
                log_e("AudioThread: opus_decoder_create failed: {}", opus_strerror(err));
                return false;
        }

        StreamState* st = new StreamState();
        st->pDec       = dec;
        st->pClip      = clip;
        st->readSample = 0;
        st->readByte   = 0;
        st->looping    = looping;

        alGenBuffers(2, st->alBufs);
        if (alGetError() != AL_NO_ERROR) {
                log_e("AudioThread: alGenBuffers failed for streamed voice");
                opus_decoder_destroy(dec);
                delete st;
                return false;
        }

        slot.pStream = st;

        // Prime both AL buffers
        const ALenum fmt = (clip->channels == 2) ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
        for (int i = 0; i < 2; ++i) {
                std::vector<float> pcm;
                int frames = DecodeNextChunk(*st, pcm, static_cast<int>(clip->channels));
                if (frames <= 0) {
                        // Very short clip — just fill with zeros for second buffer
                        pcm.assign(static_cast<size_t>(STREAM_CHUNK_FRAMES) *
                                   clip->channels, 0.0f);
                        frames = STREAM_CHUNK_FRAMES;
                }
                // Convert float → int16
                std::vector<int16_t> pcm16(pcm.size());
                for (size_t k = 0; k < pcm.size(); ++k) {
                        float s = std::max(-1.0f, std::min(1.0f, pcm[k]));
                        pcm16[k] = static_cast<int16_t>(s * 32767.0f);
                }
                alBufferData(st->alBufs[i], fmt,
                             pcm16.data(),
                             static_cast<ALsizei>(pcm16.size() * sizeof(int16_t)),
                             static_cast<ALsizei>(clip->sample_rate));
        }

        alSourceQueueBuffers(slot.al_source, 2, st->alBufs);
        return alGetError() == AL_NO_ERROR;
#else
        (void)slot; (void)clip; (void)looping;
        return false;
#endif
}

void AudioThread::TickStreaming() {
#if defined(SE_AUDIO_ENABLED) && defined(SE_OPUS_ENABLED)
        for (auto& oSlot : vVoices) {
                if (!oSlot.active || !oSlot.pStream) continue;

                StreamState* st  = oSlot.pStream;
                const AudioClip* clip = st->pClip;
                if (!clip) continue;

                const ALenum fmt = (clip->channels == 2)
                                   ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;
                const int ch = static_cast<int>(clip->channels);

                ALint processed = 0;
                alGetSourcei(oSlot.al_source, AL_BUFFERS_PROCESSED, &processed);

                while (processed-- > 0) {
                        ALuint buf = 0;
                        alSourceUnqueueBuffers(oSlot.al_source, 1, &buf);

                        bool atEnd = (st->readByte >= clip->vOpusPayload.size());

                        if (atEnd && st->looping) {
                                // Seek to loop start using seek table
                                uint64_t targetSample = clip->loop_start_sample;
                                uint64_t seekByte = 0;
                                for (auto it = clip->vSeekTable.rbegin();
                                     it != clip->vSeekTable.rend(); ++it) {
                                        if (it->sample_offset <= targetSample) {
                                                seekByte = it->byte_offset;
                                                st->readSample = it->sample_offset;
                                                break;
                                        }
                                }
                                st->readByte = seekByte;
                                opus_decoder_ctl(st->pDec, OPUS_RESET_STATE);
                                atEnd = false;
                        }

                        if (!atEnd) {
                                std::vector<float> pcm;
                                int frames = DecodeNextChunk(*st, pcm, ch);
                                if (frames > 0) {
                                        std::vector<int16_t> pcm16(pcm.size());
                                        for (size_t k = 0; k < pcm.size(); ++k) {
                                                float s = std::max(-1.0f,
                                                          std::min(1.0f, pcm[k]));
                                                pcm16[k] = static_cast<int16_t>(s * 32767.0f);
                                        }
                                        alBufferData(buf, fmt,
                                                     pcm16.data(),
                                                     static_cast<ALsizei>(
                                                         pcm16.size() * sizeof(int16_t)),
                                                     static_cast<ALsizei>(clip->sample_rate));
                                        alSourceQueueBuffers(oSlot.al_source, 1, &buf);
                                        continue;
                                }
                        }

                        // End of stream — mark voice finished (ReclaimFinished will clean up)
                        ALint queued = 0;
                        alGetSourcei(oSlot.al_source, AL_BUFFERS_QUEUED, &queued);
                        if (queued == 0) {
                                FreeStreamState(oSlot);
                                vActiveGen[static_cast<size_t>(&oSlot - vVoices)].store(0, std::memory_order_relaxed);
                                oSlot.active = false;
                                oSlot.hVoice = VoiceHandle{};
                        }
                }

                // Restart source if it stalled (buffer underrun)
                ALint state = AL_STOPPED;
                alGetSourcei(oSlot.al_source, AL_SOURCE_STATE, &state);
                ALint queued = 0;
                alGetSourcei(oSlot.al_source, AL_BUFFERS_QUEUED, &queued);
                if (state == AL_STOPPED && queued > 0 && oSlot.active) {
                        log_w("AudioThread: stream underrun on '{}' — restarting",
                              clip->Name());
                        alSourcePlay(oSlot.al_source);
                }
        }
#endif
}

} // namespace SE
