# Audio System

## 1. Overview

The audio system provides spatial and non-spatial sound playback backed by **OpenAL Soft**.
It is enabled at configure time with the `SE_AUDIO_ENABLED` CMake flag (see §7).

### Architecture

```
Game Thread                 Audio Thread
-----------                 ------------
AudioSystem  --[SPSCQueue]--> AudioThread --> OpenAL API --> OS Device

AudioEmitter / AudioListener
(scene-node listeners; post transform events when the node moves)
```

**Core rule:** the game thread never calls OpenAL.
All AL state is owned exclusively by the audio thread.
The game thread communicates only via `SPSCQueue<AudioEvent, 1024>`.

---

## 2. Systems

### `AudioSystem` (`core/AudioSystem.h`)

The game-thread facade. Retrieve it with `GetSystem<AudioSystem>()`.

| Method | Description |
|--------|-------------|
| `Play(hClip, flags, pos, vel)` | Start playback; returns a `VoiceHandle`. Returns an invalid handle if audio is disabled or the queue is full. |
| `Stop(h)` | Stop a voice. Safe to call with an invalid or stale handle. |
| `Pause(h, pause)` | Pause (`true`) or resume (`false`) a voice. |
| `SetBusGain(bus, gain, fadeTime)` | Set bus volume. `fadeTime > 0` triggers a linear fade over that many seconds; `0` is instant. |
| `PostUpdateEmitter(h, pos, vel)` | Update the 3-D source position/velocity. |
| `PostUpdateListener(pos, fwd, up, vel)` | Update listener orientation. |
| `PostSetVoiceGain(h, gain)` | Per-voice gain override (applied on top of bus gain). |

Events are enqueued into `SPSCQueue<AudioEvent, 1024>`. If the queue is full the event is dropped and a warning is logged; no blocking occurs.

---

## 3. Resource — `AudioClip` (`core/AudioClip.h`)

`AudioClip` is an audio asset managed via `TResourceManager`. It supports two storage tiers selected at load time: **resident PCM** (decoded to RAM) and **streamed Opus** (decoded on the audio thread).

### Constructors

| Constructor | When used |
|-------------|-----------|
| `AudioClip(sName, rid)` | Normal resource load — dispatches via file extension |
| `AudioClip(sName, rid, pFB)` | Inline FlatBuffer data (deserialization path, no file I/O) |

```cpp
// Normal load
auto hClip = CreateResource<AudioClip>("resource/audio/explosion.seac");

// Inline FlatBuffer (from deserialization)
auto hClip = CreateResource<AudioClip>("explosion", pFlatBufferTable);
```

**Supported formats on load:**
- `.wav`, `.mp3`, `.flac`, `.ogg` — decoded eagerly to resident PCM
- `.seac` — baked FlatBuffer container; tier is selected by the `format` field inside the file

### Two Tiers

| Tier | Condition | Storage | Playback |
|------|-----------|---------|----------|
| Resident | `format == PCM16` or raw decode | `vSamples` (float) | Uploaded lazily to an OpenAL static buffer on first `GetALBuffer()` call |
| Streamed | `format == Opus` | `vOpusPayload` (raw Opus bytes) | Audio thread decodes on-the-fly with 4 096-frame chunks and ping-pong AL buffers |

### Public Fields

| Field | Type | Description |
|-------|------|-------------|
| `sample_rate` | `uint32_t` | Sample rate in Hz |
| `channels` | `uint32_t` | Channel count (1 or 2) |
| `bStreamed` | `bool` | `true` for Opus streaming tier |
| `vOpusPayload` | `vector<uint8_t>` | Raw Opus bitstream (Opus tier only) |
| `vSeekTable` | `vector<SeekEntry>` | 250 ms keyframes: `{sample_offset, byte_offset}` |
| `pre_skip` | `uint32_t` | Opus encoder pre-skip frames to discard at decode start |
| `loop_start_sample` | `uint64_t` | Loop start in frames (0 = beginning) |
| `loop_end_sample` | `uint64_t` | Loop end in frames (0 = no loop) |
| `loop_crossfade_samples` | `uint32_t` | Blend window at loop point |
| `total_samples` | `uint64_t` | Total frame count (from header; used for streamed duration) |
| `bus_hint` | `uint8_t` | Default bus from asset (see `MixBusId`; default `2` = SFX) |

### Accessors

| Method | Description |
|--------|-------------|
| `SampleRate()` | Sample rate in Hz |
| `Channels()` | Channel count |
| `IsStreamed()` | `true` if Opus streaming tier |
| `DurationSeconds()` | Clip length in seconds |
| `Samples()` | Resident PCM data (non-empty only for resident tier) |
| `GetALBuffer()` | Audio-thread only — uploads if needed, returns AL buffer id; returns `0` for streamed clips |

---

### 3.1 `.seac` Binary Format

`.seac` is the baked asset container produced by the `audio_import` tool (see §9).

- **File identifier:** `SEAC` (FlatBuffers 4-byte file ID)
- **Extension:** `.seac`
- **Schema:** `misc/AudioClip.fbs` → `generated/AudioClip_generated.h`
- **Root type:** `SE::FlatBuffers::AudioClip`

**`AudioClip` schema fields:**

| Field | Type | Description |
|-------|------|-------------|
| `schema_version` | `uint32` | Currently 1 |
| `format` | `AudioFormat` | `PCM16` or `Opus` |
| `sample_rate` | `uint32` | Hz |
| `channel_count` | `uint8` | 1 or 2 |
| `total_samples` | `uint64` | Frame count |
| `normalized_loudness` | `float` | Measured integrated LUFS |
| `loop_start_sample` | `uint64` | 0 = beginning |
| `loop_end_sample` | `uint64` | 0 = no loop |
| `loop_crossfade_samples` | `uint32` | Blend window |
| `pre_skip_samples` | `uint32` | Opus pre-skip frames |
| `bus_hint` | `BusHint` | Default mix bus |
| `seek_table` | `[SeekEntry]` | 250 ms keyframes (Opus only) |
| `payload` | `[uint8]` | PCM-16 or Opus bitstream (required) |

**`AudioClipHolder` table** (used inside the `AudioEmitter` schema to reference a clip):

| Field | Description |
|-------|-------------|
| `path` | Filesystem path — runtime loads via `CreateResource<AudioClip>(path)` |
| `clip` | Inline `AudioClip` table — loaded via `CreateResource<AudioClip>(name, pFB)` |
| `name` | Asset name used when resolving an inline `clip` |

---

## 4. Components

### `AudioEmitter` (`units/AudioEmitter.h`)

Attach to any scene node that should emit sound.

```cpp
// From descriptor
AudioEmitterDesc desc;
desc.sClipPath = "resource/audio/engine.seac";
desc.oFlags.loop    = true;
desc.oFlags.spatial = true;
desc.oFlags.bus     = MixBusId::SFX;
desc.auto_play      = true;
pNode->CreateComponent<AudioEmitter>(desc);

// From FlatBuffer (deserialization)
pNode->CreateComponent<AudioEmitter>(pFlatBufferTable);
```

**`AudioEmitterDesc` fields:**

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `sClipPath` | `string` | — | Path to the audio asset |
| `oFlags` | `PlayFlags` | (see §5) | Playback settings |
| `auto_play` | `bool` | `false` | Start playing immediately on construction |

**FlatBuffer constructor — `AudioClipHolder` resolution:**

When deserializing from a FlatBuffer, the clip is resolved from the embedded `AudioClipHolder` in priority order:

```
1. holder->path() != nullptr  → CreateResource<AudioClip>(path)
2. holder->name() && holder->clip() → CreateResource<AudioClip>(name, pFB)
3. neither → throws std::runtime_error
```

The destructor automatically stops the voice. `IsPlaying()` returns `true` while a valid `VoiceHandle` is held.

`TargetTransformChanged(pNode)` is called by the scene node whenever its world transform changes. It estimates velocity as `(currentPos - prevPos) / dt`. A `first_tick` guard suppresses the velocity spike on the very first frame.

### `AudioListener` (`units/AudioListener.h`)

Marker component. Attach to the camera or player node.

```cpp
pCameraNode->CreateComponent<AudioListener>(/* master_gain = */ 1.0f);
```

Sets the master bus gain on construction. `TargetTransformChanged(pNode)` is called by the scene node whenever its world transform changes, posting `EvtUpdateListener` to the audio thread. `MasterGain()` returns the configured value.

> **Warning:** only one listener per scene is supported. With multiple listeners, updates are processed in scene-traversal order and the last one wins — resulting in undefined spatial behaviour.

---

## 5. PlayFlags & Mix Buses

### `PlayFlags`

All fields have defaults appropriate for a typical 3-D SFX:

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `loop` | `bool` | `false` | Loop indefinitely |
| `spatial` | `bool` | `true` | 3-D positional audio (false = ambient/2-D) |
| `bus` | `MixBusId` | `SFX` | Target mix bus |
| `pitch` | `float` | `1.0` | Pitch multiplier |
| `gain` | `float` | `1.0` | Voice gain (before bus gain) |
| `ref_dist` | `float` | `1.0` | Reference distance for attenuation |
| `max_dist` | `float` | `100.0` | Distance beyond which gain reaches minimum |
| `rolloff` | `float` | `1.0` | Rolloff factor (OpenAL inverse model) |

### `MixBusId`

| Enumerator | Value | Typical use |
|------------|-------|-------------|
| `Master` | 0 | Global volume |
| `Music` | 1 | Background music |
| `SFX` | 2 | World sound effects (default) |
| `Voice` | 3 | Dialogue / narration |
| `UI` | 4 | Interface sounds |

**Gain formula:** `effective_gain = voice.gain × bus.gain × Master.gain`

Buses chain to Master only; there are no sub-buses.

---

## 6. VoiceHandle

`VoiceHandle` is a `{index, generation}` pair that provides stale-handle safety. An invalid handle has `generation == 0`; `IsValid()` tests this.

All `AudioSystem` methods that accept a `VoiceHandle` silently ignore invalid or stale handles — it is always safe to pass a handle without checking it first.

**Voice pool:** 32 slots. When all slots are occupied and a new `Play` is requested, the voice with the lowest `gain` is evicted (voice stealing).

---

## 7. Build Configuration

```sh
# Default — audio enabled
cmake -B build -DAUDIO=ON

# Disable audio (e.g., headless server build)
cmake -B build -DAUDIO=OFF
```

When `AUDIO=ON`, OpenAL Soft is linked and `SE_AUDIO_ENABLED` is defined. When `AUDIO=OFF`, all `AudioSystem` methods compile to safe no-ops; no OpenAL dependency is introduced.

---

## 8. Known Limitations

- **Opus streaming only** — resident (PCM16) clips are fully decoded into RAM; streamed (Opus) clips decode on the audio thread with 4 096-frame chunks and ping-pong AL buffers. Streaming is limited to the Opus codec.
- **No reverb / EFX** — the OpenAL Effects Extension is not used.
- **No physics-based occlusion** — audio propagation is line-of-sight only (pure distance attenuation).
- **32 voice hard cap** — voice stealing mitigates this but priority control beyond gain is not available.
- **Flat bus hierarchy** — buses route directly to Master; no sub-bus grouping.
- **Estimated velocity** — `AudioEmitter::TargetTransformChanged` derives velocity from position delta; the first-frame spike is suppressed but fast teleportation will produce an erroneous Doppler shift for one frame.
- **Single listener** — multiple `AudioListener` components in the same scene produce undefined spatial behaviour.

---

## 9. `audio_import` Tool (`tools/audio_import/`)

`audio_import` is an offline converter that packages raw audio files into `.seac` baked containers ready for the runtime.

### Processing Pipeline

```
Source file → Decode (dr_libs/stb_vorbis) → Resample to 48 kHz (libsoxr)
→ Normalise to −18 LUFS (BS.1770 K-weighted) → Encode:
    Resident tier (< 5 s or transient): float → int16 PCM
    Streamed tier (≥ 5 s): libopus at configured bitrate + seek table
→ Pack FlatBuffer → write .seac
```

### CLI Synopsis

```sh
audio_import <input> [<input2>…] [options]

  -o, --out <path>               Output file or directory
  --tier <resident|stream|auto>  Force tier (default: auto)
  --mono                         Downmix to mono
  --rate <hz>                    Resample target (default 48000)
  --lufs <val>                   Normalisation target (default -18.0)
  --opus-bitrate <kbps>          Opus bitrate (default: 128 music/96 ambient/48 voice)
  --loop-start <sample>
  --loop-end   <sample>
  --crossfade  <samples>         Loop crossfade window (default 2048)
  --bus <sfx|music|voice|ui>
  --dry-run
```

### Bitrate Defaults

| Content | Channels | Tier | Bitrate |
|---------|----------|------|---------|
| Music | Stereo | Streamed | 128–192 kbps Opus |
| Ambient | Stereo | Streamed | 96–128 kbps Opus |
| Voice | Mono | Streamed | 32–48 kbps Opus |
| SFX < 5 s | Any | Resident | PCM 16-bit |
| Transient SFX | Any | Resident | PCM 16-bit (forced) |
