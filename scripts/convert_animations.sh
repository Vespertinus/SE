#!/bin/bash
# Convert FBX animation files to .seak (AnimationClip FlatBuffers) format.
#
# Usage:
#   convert_animations.sh <input.fbx> [output_prefix] [--rate 30]
#
# Options:
#   <input.fbx>       Path to the FBX file containing animation stacks.
#   [output_prefix]   Output file prefix (default: input stem).
#   [--rate N]        Sample rate in fps (default: 30).
#
# Output:
#   One .seak file per animation stack found in the FBX:
#     <output_prefix>_<stack_name>.seak
#
# Requires:
#   - The 'convert' tool built from tools/convert/
#   - The FBX file must have been exported with skinning (--skin=true)

set -e

INPUT="${1}"
OUTPUT="${2:-}"
RATE="${3:-30}"

if [ -z "${INPUT}" ]; then
    echo "Usage: $0 <input.fbx> [output_prefix] [--rate N]"
    exit 1
fi

if [ -z "${OUTPUT}" ]; then
    OUTPUT="$(basename "${INPUT}" .fbx)"
fi

echo "Converting animations from: ${INPUT}"
echo "  Output prefix : ${OUTPUT}"
echo "  Sample rate   : ${RATE} fps"

convert \
    --input  "${INPUT}" \
    --output "${OUTPUT}" \
    --skin=true \
    --animations=true \
    --anim_rate "${RATE}" \
    --skip_material=true \
    --to_scene=false

echo "Done."
