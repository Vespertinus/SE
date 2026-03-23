# Sound Event System

## 1. Overview

`SoundEventSystem` is the cue-based event layer that sits above `AudioSystem`. Where `AudioSystem` provides raw clip playback via `Play(hClip, flags, pos, vel)`, `SoundEventSystem` lets game code post named events (`"character.footstep.stone"`) and handles everything else: variation selection, randomised volume/pitch, parameter-driven modulation, cooldowns, and polyphony limits.

The separation means game code never references `AudioClip` handles or `PlayFlags` directly — it fills in a context struct and calls `Post()`.

### Architecture

```
Game code
   │
   │  Post<TCtx>("character.footstep.stone", ctx)
   ▼
SoundEventSystem
   ├── ResolveCue()         dot-notation fallback: "a.b.c" → "a.b" → "a"
   ├── CheckCooldown()      per-(emitter, cue) timestamp gate
   ├── CheckPolyphony()     per-(emitter, cue) active voice count
   ├── SelectVariation()    RANDOM / ROUND_ROBIN / SHUFFLE / PARAMETER / SEQUENTIAL
   ├── ApplyModulators()    parameter → linear remap → volume / pitch multiplier
   └── AudioSystem::Play()  → VoiceHandle
```

**Core rule:** `SoundEventSystem` calls `AudioSystem::Play()` on the game thread, exactly as direct callers do. It inherits the same SPSC-queue and voice-pool constraints described in [Audio.md](Audio.md).

---

## 2. SoundCue — the cue definition

A `SoundCue` (`core/SoundEventSystem.h`) is the runtime representation of one entry in a `.secl` library. It is identified by a dot-notation string ID (e.g. `"character.footstep.stone"`), and contains all the parameters needed to pick and play a sound at event time.

### `SoundCue` fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `id` | `StrID` | — | Hashed cue identifier |
| `vHierarchy` | `vector<StrID>` | — | Pre-built prefix chain for fallback resolution — computed at load time, not stored in the schema |
| `selection_mode` | `SelectionMode` | `SHUFFLE` | How a variation is chosen (see §4) |
| `play_mode` | `PlayMode` | `ONE_SHOT` | Looping and exclusivity behaviour (see §6) |
| `vVariations` | `vector<SoundVariation>` | — | Candidate clips |
| `volume_min` | `float` | `1.0` | Lower bound of uniform volume randomisation |
| `volume_max` | `float` | `1.0` | Upper bound of uniform volume randomisation |
| `pitch_min` | `float` | `1.0` | Lower bound of uniform pitch randomisation |
| `pitch_max` | `float` | `1.0` | Upper bound of uniform pitch randomisation |
| `ref_dist` | `float` | `1.0` | OpenAL reference distance for attenuation |
| `max_dist` | `float` | `40.0` | Distance at which gain reaches its minimum |
| `bus` | `MixBusId` | `SFX` | Target mix bus |
| `cooldown` | `float` | `0.0` | Minimum seconds between consecutive fires (per emitter + cue pair) |
| `max_polyphony` | `int` | `4` | Maximum simultaneous voices for this cue from the same emitter |
| `vModulators` | `vector<Modulator>` | — | Parameter-driven volume/pitch scalars (see §5) |

### `SoundVariation` fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `hClip` | `H<AudioClip>` | — | Resource handle to the clip |
| `weight` | `float` | `1.0` | Reserved for weighted selection (parsed but not yet used in RANDOM/SHUFFLE) |
| `volume_scale` | `float` | `1.0` | Multiplied into the final volume for this specific clip |
| `pitch_scale` | `float` | `1.0` | Multiplied into the final pitch for this specific clip |
| `param_min` | `float` | `0.0` | Lower bound of the parameter range (PARAMETER mode) |
| `param_max` | `float` | `1.0` | Upper bound of the parameter range (PARAMETER mode) |

### `SoundCue::Modulator` fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `param_id` | `StrID` | — | Parameter name to read from the context (e.g. `"speed"`, `"rpm_normalized"`) |
| `target` | `Target` | `VOLUME` | What to scale: `VOLUME`, `PITCH`, or `PITCH_AND_VOLUME` |
| `input_min` | `float` | `0.0` | Parameter value that maps to `output_min` |
| `input_max` | `float` | `1.0` | Parameter value that maps to `output_max` |
| `output_min` | `float` | `0.0` | Output multiplier at the low end of the input range |
| `output_max` | `float` | `1.0` | Output multiplier at the high end of the input range |
| `clamp` | `bool` | `true` | Clamp the normalised `t` to `[0, 1]` before remapping |

---

## 3. `SoundEventSystem` — API (`core/SoundEventSystem.h`)

Retrieve the system with `GetSystem<SoundEventSystem>()`.

| Method | Description |
|--------|-------------|
| `LoadCues(sPath)` | Load all cues from a `.secl` FlatBuffers binary. Multiple libraries can be loaded; cues are merged into `mCues` by ID. Returns `false` if the file cannot be opened or fails FlatBuffers verification. |
| `RegisterCue(cue)` | Register a `SoundCue` directly from code (no file I/O). Overwrites any existing cue with the same ID. |
| `Post<TCtx>(ids, ctx)` | Hot-path overload. `ids` is a pre-hashed `vector<StrID>` hierarchy built with `BuildHierarchy()`. No string operations at call time. |
| `Post<TCtx>(sEventId, ctx)` | Convenience overload. Builds the hierarchy from a `string_view` on each call; suitable for infrequent events or prototyping. |
| `PostEx<TCtx>(ids, ctx, oResult)` | Debug variant of the hot-path overload. Fills `oResult` with resolution diagnostics. Prefer `Post()` in hot paths. |
| `PostEx<TCtx>(sEventId, ctx, oResult)` | Debug variant of the convenience overload. |
| `Update(dt)` | Advance the internal clock, expire finished cooldowns, and prune completed voices from per-emitter state. Call once per frame on the game thread. |
| `BuildHierarchy(sv)` | Static helper. Converts `"a.b.c"` to `[StrID("a"), StrID("a.b"), StrID("a.b.c")]`. Store the result in a `SoundEmitter` or similar object to avoid per-call allocation. |
| `ReleaseEmitter(pEmitter)` | Remove all per-emitter state (shuffle orders, round-robin indices, active voice lists) keyed to `pEmitter`. Must be called from `SoundEmitter`'s destructor. |

All four `Post`/`PostEx` overloads funnel through a single private `PostImpl<TCtx>()` that performs the full resolution pipeline.

### Internal state

| Member | Description |
|--------|-------------|
| `mCues` | `unordered_map<StrID, SoundCue>` — the registered cue library |
| `mEmitterState` | `unordered_map<uint64_t, PerEmitterState>` — per-emitter shuffle orders, round-robin indices, and active voice lists keyed by emitter pointer cast to `uint64_t` (key `0` = no emitter) |
| `mCooldowns` | `unordered_map<uint64_t, float>` — per-(emitter, cue) expiry timestamps; key is `emitter_key XOR (cue_id * 0x9e3779b97f4a7c15)` |
| `current_time` | Monotonic clock advanced by `Update(dt)` |
| `oRng` | `mt19937` seeded from `random_device` at construction |

---

## 4. Variation Selection

`SelectVariation()` is called once per `Post()` and returns a pointer to one `SoundVariation`. All state except the RNG is stored per-emitter, so two emitters posting the same cue maintain independent sequences.

| Mode | Behaviour |
|------|-----------|
| `RANDOM` | Uniform random pick from `[0, n)` on every call. No state. |
| `ROUND_ROBIN` / `SEQUENTIAL` | Cycle through variations in order, wrapping at the end. Per-emitter counter stored in `mRoundRobinIdx`. Both enum values share the same implementation. |
| `SHUFFLE` | Exhausts a random permutation before reshuffling. On each reshuffle, if the first slot would repeat the last-played index (stored in `mLastIdx`), the first two slots are swapped to guarantee a different opening pick. State: `mShuffleOrder`, `mShuffleIdx`, `mLastIdx`. |
| `PARAMETER` | Selects the first variation whose `[param_min, param_max]` interval contains the context parameter value. The parameter ID is taken from `modulators[0].param_id` if modulators are present; otherwise falls back to the literal name `"selection"`. Returns `vVariations[0]` if no range matches. |

---

## 5. Modulators

Modulators apply after the initial random volume/pitch draw and the per-variation scale. The formula is a linear remap:

```
t      = (context.GetParameter(param_id) - input_min) / (input_max - input_min)
t      = clamp(t, 0, 1)   [if clamp == true]
output = output_min + t * (output_max - output_min)
```

`output` is then multiplied into `volume` (target `VOLUME`), `pitch` (target `PITCH`), or both (target `PITCH_AND_VOLUME`).

All modulators on a cue are applied in order. The combined gain chain for a single Post() call is:

```
volume = rand(volume_min, volume_max) × variation.volume_scale × mod₁_output × mod₂_output × …
pitch  = rand(pitch_min,  pitch_max)  × variation.pitch_scale  × mod₁_output × mod₂_output × …
```

If the context sets `volume_override >= 0` or `pitch_override >= 0`, those values replace the computed result entirely (modulation is bypassed for that axis).

---

## 6. PlayMode

| Mode | Behaviour |
|------|-----------|
| `ONE_SHOT` | Sets `PlayFlags::loop = false`. The voice is released when the clip finishes. |
| `LOOPING` | Sets `PlayFlags::loop = true`. The caller must eventually `Stop()` the returned `VoiceHandle`. |
| `ONE_PER_EMITTER` | Schema value reserved for future enforcement. Currently behaves as `ONE_SHOT`; authors use `max_polyphony = 1` to approximate the intent. |
| `EXCLUSIVE` | Schema value reserved for future enforcement. Not currently implemented. |

---

## 7. `SoundEventContext` and Context Types

### `SoundEventContext` (`core/SoundContext.h`)

The base context struct. `TCtx` must derive from it and shadow `GetParameter` non-virtually (static dispatch via the `Post<TCtx>` template — no vtable, no virtual calls).

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `pEmitter` | `SoundEmitter*` | `nullptr` | Spatial anchor; used as the emitter key for per-emitter state. Set to `nullptr` for non-spatial (2-D) events. |
| `vPosition` | `glm::vec3` | `{}` | World position of the sound source |
| `vVelocity` | `glm::vec3` | `{}` | World velocity for Doppler (passed to OpenAL) |
| `volume_override` | `float` | `-1.0` | If `>= 0`, replaces the modulated volume entirely |
| `pitch_override` | `float` | `-1.0` | If `>= 0`, replaces the modulated pitch entirely |
| `bus_override` | `MixBusId` | `COUNT` | If `!= COUNT`, overrides the cue's bus assignment |

The spatial flag passed to `AudioSystem::Play()` is derived automatically: `spatial = (pEmitter != nullptr || vPosition != vec3(0))`.

### `CharacterSoundContext` (`units/SoundContextTypes.h`)

| Field | Type | Parameter name |
|-------|------|----------------|
| `speed` | `float` | `"speed"` |
| `weight` | `float` | `"weight"` |
| `wetness` | `float` | `"wetness"` |
| `impact` | `float` | `"impact"` |
| `surface` | `SurfaceType` | — (not a named parameter; callers select the cue ID based on surface) |

### `VehicleSoundContext` (`units/SoundContextTypes.h`)

| Field | Type | Parameter name |
|-------|------|----------------|
| `rpm_normalized` | `float` | `"rpm_normalized"` |
| `engine_load` | `float` | `"engine_load"` |
| `lateral_slip` | `float` | `"lateral_slip"` |
| `longitudinal_slip` | `float` | `"longitudinal_slip"` |
| `impact` | `float` | `"impact"` |

Extending the system with a new context type requires only adding a struct that inherits `SoundEventContext` and shadows `GetParameter(StrID)`.

### Usage example

```cpp
// Character footstep — hot path, pre-built hierarchy stored on SoldierState
CharacterSoundContext ctx;
ctx.pEmitter  = pSoldier->GetComponent<SoundEmitter>();
ctx.vPosition = pSoldierNode->GetWorldPosition();
ctx.vVelocity = vSoldierVelocity;
ctx.speed     = fMoveSpeed;
ctx.weight    = fCharacterWeight;

GetSystem<SoundEventSystem>().Post(vFootstepHierarchy, ctx);

// Vehicle engine — convenience overload, debug result capture
VehicleSoundContext vCtx;
vCtx.pEmitter       = pVehicle->GetComponent<SoundEmitter>();
vCtx.rpm_normalized = fRpm;
vCtx.engine_load    = fLoad;

SoundEventResult oRes;
VoiceHandle hEngine = GetSystem<SoundEventSystem>()
        .PostEx("vehicle.engine", vCtx, oRes);
```

---

## 8. `SoundEmitter` Component (`units/SoundEmitter.h`)

`SoundEmitter` is the scene-node component that integrates `SoundEventSystem` into the entity hierarchy. See [Audio.md §4](Audio.md#4-components) for the descriptor fields and construction API.

Key integration details:

- On construction, the cue ID string is passed to `BuildHierarchy()` and the result is stored as `vHierarchy` — a pre-hashed `vector<StrID>`. Subsequent `Play()` calls pass `vHierarchy` to `Post()`, so there are no string operations on the hot path.
- `Play<TCtx>(ctx)` fills `ctx.pEmitter`, `ctx.vPosition`, and `ctx.vVelocity` from the component's current state before forwarding to `SoundEventSystem::Post(vHierarchy, ctx)`.
- The destructor calls `SoundEventSystem::ReleaseEmitter(this)` to clean up all per-emitter state. Omitting this would leak entries in `mEmitterState` until the pointer happened to be reused.

---

## 9. Authoring Cues

### `.secl` binary format

`.secl` is the compiled cue library produced by running `flatc` on a JSON source file.

- **Schema:** `misc/SoundCue.fbs` → `generated/SoundCue_generated.h`
- **File identifier:** `SECL`
- **Extension:** `.secl`
- **Root type:** `SE::FlatBuffers::SoundCueList`

The buffer is verified with `VerifySoundCueListBuffer()` on load; a failed verification causes `LoadCues()` to return `false`.

**`SoundCue` schema fields** (from `misc/SoundCue.fbs`):

| Field | FBS type | Default | Notes |
|-------|----------|---------|-------|
| `id` | `string` (required) | — | Dot-notation cue name |
| `selection_mode` | `SelectionMode` | `SHUFFLE` | Enum: `RANDOM`, `ROUND_ROBIN`, `SHUFFLE`, `PARAMETER`, `SEQUENTIAL` |
| `play_mode` | `PlayMode` | `ONE_SHOT` | Enum: `ONE_SHOT`, `LOOPING`, `ONE_PER_EMITTER`, `EXCLUSIVE` |
| `variations` | `[SoundVariation]` | — | Clip list |
| `volume_min` / `volume_max` | `float` | `1.0` | Uniform volume range |
| `pitch_min` / `pitch_max` | `float` | `1.0` | Uniform pitch range |
| `ref_dist` / `max_dist` | `float` | `1.0` / `40.0` | OpenAL attenuation distances |
| `bus` | `string` | `null` → `"sfx"` | One of: `"master"`, `"music"`, `"sfx"`, `"voice"`, `"ui"` |
| `cooldown` | `float` | `0.0` | Seconds; `0` disables |
| `max_polyphony` | `int` | `4` | Per-emitter voice cap |
| `modulators` | `[Modulator]` | — | Parameter curves |

**`SoundVariation` schema fields:**

| Field | FBS type | Default |
|-------|----------|---------|
| `clip` | `string` (required) | — |
| `weight` | `float` | `1.0` |
| `volume_scale` | `float` | `1.0` |
| `pitch_scale` | `float` | `1.0` |
| `param_min` | `float` | `0.0` |
| `param_max` | `float` | `1.0` |

**`Modulator` schema fields:**

| Field | FBS type | Default |
|-------|----------|---------|
| `parameter` | `string` (required) | — |
| `target` | `ModulatorTarget` | `VOLUME` |
| `input_min` / `input_max` | `float` | `0.0` / `1.0` |
| `output_min` / `output_max` | `float` | `0.0` / `1.0` |
| `clamp` | `bool` | `true` |

### JSON source format

JSON files are compiled to `.secl` with:

```sh
flatc --binary --json -o resource/sound_cue/ misc/SoundCue.fbs -- resource/sound_cue/character.json
```

The JSON structure mirrors the FlatBuffers schema directly. The root object has a single `"cues"` array.

**Example — footstep with SHUFFLE + modulators:**

```json
{
  "cues": [
    {
      "id": "character.footstep.stone",
      "selection_mode": "SHUFFLE",
      "play_mode": "ONE_SHOT",
      "volume_min": 0.8, "volume_max": 1.0,
      "pitch_min": 0.95, "pitch_max": 1.05,
      "cooldown": 0.08, "max_polyphony": 3,
      "bus": "sfx",
      "variations": [
        { "clip": "resource/audio/sfx/footstep_stone_01.seac" },
        { "clip": "resource/audio/sfx/footstep_stone_02.seac" }
      ],
      "modulators": [
        { "parameter": "speed",  "target": "PITCH",
          "input_min": 1.0, "input_max": 6.0, "output_min": 0.9, "output_max": 1.15 },
        { "parameter": "weight", "target": "VOLUME",
          "input_min": 0.0, "input_max": 100.0, "output_min": 0.85, "output_max": 1.1 }
      ]
    },
    {
      "id": "character.footstep",
      "selection_mode": "SHUFFLE",
      "play_mode": "ONE_SHOT",
      "volume_min": 0.8, "volume_max": 1.0,
      "pitch_min": 0.95, "pitch_max": 1.05,
      "cooldown": 0.08, "max_polyphony": 3,
      "bus": "sfx",
      "variations": [
        { "clip": "resource/audio/sfx/footstep_concrete_01.seac" },
        { "clip": "resource/audio/sfx/footstep_concrete_02.seac" }
      ]
    }
  ]
}
```

> Both `"character.footstep.stone"` and the fallback cue `"character.footstep"` are registered in the same library. If the game posts `"character.footstep.sand"` and no such cue exists, the system resolves `"character.footstep"` instead.

**Example — looping engine with PARAMETER selection:**

```json
{
  "id": "vehicle.engine",
  "selection_mode": "PARAMETER",
  "play_mode": "LOOPING",
  "volume_min": 0.9, "volume_max": 1.0,
  "pitch_min": 1.0,  "pitch_max": 1.0,
  "cooldown": 0.0, "max_polyphony": 1,
  "bus": "sfx",
  "variations": [
    { "clip": "resource/audio/sfx/engine_idle.seac",    "param_min": 0.00, "param_max": 0.20 },
    { "clip": "resource/audio/sfx/engine_low.seac",     "param_min": 0.20, "param_max": 0.40 },
    { "clip": "resource/audio/sfx/engine_mid.seac",     "param_min": 0.40, "param_max": 0.60 },
    { "clip": "resource/audio/sfx/engine_high.seac",    "param_min": 0.60, "param_max": 0.85 },
    { "clip": "resource/audio/sfx/engine_redline.seac", "param_min": 0.85, "param_max": 1.00 }
  ],
  "modulators": [
    { "parameter": "rpm_normalized", "target": "PITCH",
      "input_min": 0.0, "input_max": 1.0, "output_min": 0.9, "output_max": 1.2 },
    { "parameter": "engine_load",    "target": "VOLUME",
      "input_min": 0.0, "input_max": 1.0, "output_min": 0.6, "output_max": 1.0 }
  ]
}
```

> `modulators[0].param_id` (`"rpm_normalized"`) is also used as the parameter source for PARAMETER variation selection. The clip that covers the current RPM range is chosen, and then pitch/volume are further modulated by RPM and engine load.

**Example — impact-selected landing:**

```json
{
  "id": "character.land",
  "selection_mode": "PARAMETER",
  "play_mode": "ONE_SHOT",
  "volume_min": 0.8, "volume_max": 1.0,
  "cooldown": 0.0, "max_polyphony": 2,
  "bus": "sfx",
  "variations": [
    { "clip": "resource/audio/sfx/character_land_soft.seac",
      "param_min": 0.0, "param_max": 0.5 },
    { "clip": "resource/audio/sfx/character_land_hard.seac",
      "param_min": 0.5, "param_max": 1.0 }
  ],
  "modulators": [
    { "parameter": "impact", "target": "VOLUME",
      "input_min": 0.0, "input_max": 1.0, "output_min": 0.5, "output_max": 1.0 }
  ]
}
```

---

## 10. Cooldowns and Polyphony

### Cooldowns

A cooldown is stored as an expiry timestamp in `mCooldowns`. The map key is:

```
key = emitter_key XOR (cue_id * 0x9e3779b97f4a7c15)
```

where `emitter_key` is the emitter pointer cast to `uint64_t` (or `0` for context-less posts). This means the same cue fired from two different emitters has two independent cooldown timers.

Expired entries are removed in `Update()`. A key is only added to the map if `cue.cooldown > 0`, so cues with no cooldown incur no map overhead.

### Polyphony

Active voices are tracked per `(emitter_key, cue_id)` pair in `PerEmitterState::mActiveVoices`. Before each `Post()`, stale handles are pruned by querying `AudioSystem::IsVoiceActive()`. If the live count is `>= cue.max_polyphony`, the event is blocked and `PostEx()` sets `polyphony_block = true`. `Update()` also prunes all per-emitter voice lists once per frame to bound map growth.

---

## 11. `SoundEventResult` — diagnostics

`SoundEventResult` is filled by `PostEx()` and exposes the outcome of a single post call.

| Field | Type | Description |
|-------|------|-------------|
| `fired` | `bool` | `true` if a voice was started |
| `cooldown_block` | `bool` | `true` if the call was suppressed by the cooldown timer |
| `polyphony_block` | `bool` | `true` if the call was suppressed by `max_polyphony` |
| `fallback` | `bool` | `true` if the resolved cue differs from the deepest requested cue (e.g. resolved `"a.b"` when `"a.b.c"` was requested) |
| `resolved_cue` | `StrID` | The cue that was actually used |
| `variation_idx` | `int` | Index into `vVariations` of the chosen clip (`-1` if no voice was started) |
| `final_volume` | `float` | Volume passed to `AudioSystem::Play()` after all modulation |
| `final_pitch` | `float` | Pitch passed to `AudioSystem::Play()` after all modulation |

Use `Post()` on hot paths (call site footprint is one `SoundEventResult` stack variable less). Use `PostEx()` for debug overlays, event logs, and tools. `log_d` messages are emitted by `PostImpl()` regardless of which variant is used.

---

## 12. Known Limitations

- **`ONE_PER_EMITTER` and `EXCLUSIVE` not enforced** — both play modes exist in the schema but the runtime treats them identically to `ONE_SHOT`. Authors approximate these semantics with `max_polyphony = 1`.
- **`weight` field not used** — the `weight` field is parsed and stored but `RANDOM` and `SHUFFLE` modes treat all variations as equally likely. Weighted selection is not implemented.
- **SHUFFLE last-played guard is index-based** — if multiple variations reference the same clip file, the guard still prevents index repetition but cannot prevent the same audio from playing twice in a row.
- **Emitter state leaks if `ReleaseEmitter` is not called** — entries in `mEmitterState` accumulate until `ReleaseEmitter(pEmitter)` is called. `SoundEmitter`'s destructor handles this automatically; raw `Post()` calls that pass a custom `pEmitter` pointer must clean up manually.
- **No weighted bus fan-out** — a cue targets exactly one bus. Routing the same event to multiple buses simultaneously is not supported.
