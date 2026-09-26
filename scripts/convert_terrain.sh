#!/bin/bash
set -e
cd "$(dirname "$0")/.."
mkdir -p resource/terrain

# Bake every terrain config into .seterrain + .setile assets.
TERRAIN_BAKE_BIN=${TERRAIN_BAKE_BIN:-../terrain_bake}

for cfg in resource/terrain/*.json; do
    [ -e "$cfg" ] || continue
    "$TERRAIN_BAKE_BIN" "$cfg"
done
