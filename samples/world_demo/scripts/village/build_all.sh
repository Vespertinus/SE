#!/usr/bin/env bash
# Full asset build for the world_demo village, in dependency order:
#   1. kit import (convert pieces)            — scripts/village/import_kit.sh
#   2. terrain heightmap + bake               — make_heightmap.py + terrainbake
#   3. content manifests (build_village.py)   — .seassembly + world JSONs + nav config
#   4. compose sub-levels + NPC templates     — compose_scene
#   5. NPC template export                    — template_export
#   6. AI assets (.sebb/.sebt)                — convert_ai.sh
#   7. world compile + navmesh bake           — convert_world.sh + navbake
#   8. sound cues                             — import_audio.sh
# Each stage is rerunnable; pass a stage name to run only that stage.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="${SCRIPT_DIR}/../.."
STAGE="${1:-all}"

compose_all() {
    for m in "$ROOT"/resource/world/village_gen/wd_*.seassembly; do
        "$ROOT/compose_scene" "$m"
    done
}

export_templates() {
    pushd "$ROOT" >/dev/null
    ./template_export resource/world/village_gen/wd_villager_a.sesc --node villager_a --id villager_a --out resource/world/village_gen/villager_a.setm
    ./template_export resource/world/village_gen/wd_villager_b.sesc --node villager_b --id villager_b --out resource/world/village_gen/villager_b.setm
    ./template_export resource/world/village_gen/wd_farmer.sesc     --node farmer --id farmer     --out resource/world/village_gen/farmer.setm    
    ./template_export resource/world/village_gen/wd_guard.sesc      --node guard --id guard      --out resource/world/village_gen/guard.setm     
    popd >/dev/null
}

run_kit()        { bash "$SCRIPT_DIR/import_kit.sh"; }
run_terrain()    { python3 "$SCRIPT_DIR/make_heightmap.py" && "$ROOT/terrain_bake" "$ROOT/resource/terrain/world_demo.json"; }
run_manifests()  { python3 "$SCRIPT_DIR/build_village.py"; }
run_compose()    { compose_all; }
run_templates()  { export_templates; }
run_ai()         { bash "$ROOT/scripts/convert_ai.sh"; }
run_world()      { bash "$ROOT/scripts/convert_world.sh" && "$ROOT/nav_bake" "$ROOT/resource/nav/world_demo.json"; }
run_audio()      { bash "$SCRIPT_DIR/import_audio.sh"; }

case "$STAGE" in
    all)            run_kit; run_terrain; run_manifests; run_compose
                    run_templates; run_ai; run_world; run_audio ;;
    kit)            run_kit ;;
    terrain)        run_terrain ;;
    manifests)      run_manifests ;;
    compose)        run_compose ;;
    templates)      run_templates ;;
    ai)             run_ai ;;
    world)          run_world ;;
    audio)          run_audio ;;
    *) echo "usage: $0 [all|kit|terrain|manifests|compose|templates|ai|world|audio]" >&2; exit 1 ;;
esac

echo "build_all: stage '$STAGE' done"
