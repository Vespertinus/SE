#!/usr/bin/env bash
# Convert a character FBX to a single self-contained .sesc prefab.
# Usage: convert_character.sh --input <file.fbx> --output <stem> [additional options]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONVERT="${CONVERT_BIN:-${SCRIPT_DIR}/../convert}"

if [[ ! -x "$CONVERT" ]]; then
    echo "ERROR: convert not found at '$CONVERT'" >&2
    echo "       Set CONVERT_BIN to the correct path." >&2
    exit 1
fi

exec "$CONVERT" \
    --skin=true \
    --animations=true \
    --inline_all=true \
    "$@"
