#!/usr/bin/env bash
# Imports the world_demo sound effects and compiles the village cue bank:
#   bell_02.ogg  -> resource/audio/sfx/village_bell.seac   (village bell / noise)
#   alarm_01.ogg -> resource/audio/sfx/camp_alarm.seac     (camp raid horn)
# then writes resource/sound_cue/village.json and compiles it to .secl.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="${SCRIPT_DIR}/../.."
cd "$ROOT"

SFX100="resource/raw/audio/sfx_100_v2"
CC0="resource/raw/audio/100-CC0-SFX"

mkdir -p resource/audio/sfx

"$ROOT/audio_import" "$CC0/bell_02.ogg"  --bus sfx --out resource/audio/sfx/village_bell.seac
"$ROOT/audio_import" "$SFX100/sfx100v2_alarm_01.ogg" --bus sfx --out resource/audio/sfx/camp_alarm.seac \
        || "$ROOT/audio_import" "resource/raw/audio/sfx_loops/alarm_01.ogg" --bus sfx --out resource/audio/sfx/camp_alarm.seac

cat > resource/sound_cue/village.json <<'EOF'
{
  "cues": [
    {
      "id": "village.bell",
      "selection_mode": "RANDOM",
      "play_mode": "ONE_SHOT",
      "volume_min": 0.8,
      "volume_max": 0.9,
      "bus": "sfx",
      "variations": [
        { "clip": "resource/audio/sfx/village_bell.seac", "weight": 1.0 }
      ]
    },
    {
      "id": "camp.alarm",
      "selection_mode": "RANDOM",
      "play_mode": "ONE_SHOT",
      "volume_min": 0.9,
      "volume_max": 1.0,
      "bus": "sfx",
      "variations": [
        { "clip": "resource/audio/sfx/camp_alarm.seac", "weight": 1.0 }
      ]
    }
  ]
}
EOF

bash scripts/convert_sound_cues.sh
echo "import_audio: done"
