#!/bin/bash
# navmesh bake driver: one nav_bake config per world.
# Run AFTER convert_world.sh (nav_bake consumes the compiled .seworld).
set -e
cd "$(dirname "$0")/.."
mkdir -p resource/nav
NAV_BAKE_BIN=${NAV_BAKE_BIN:-./nav_bake}
for cfg in resource/nav/*.json; do
    [ -e "$cfg" ] || continue
    "$NAV_BAKE_BIN" "$cfg"
done
