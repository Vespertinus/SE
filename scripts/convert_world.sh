#!/bin/bash
set -e
cd "$(dirname "$0")/.."
mkdir -p resource/world

# Compile every world definition to .seworld flatbuffers.
for json in resource/world/*.json; do
    [ -e "$json" ] || continue
    flatc --binary -o resource/world/ misc/World.fbs "$json"
    echo "Generated ${json%.json}.seworld"
done
