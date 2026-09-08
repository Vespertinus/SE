#!/bin/bash
set -e
cd "$(dirname "$0")/.."
mkdir -p resource/hsm

# Compile every HSM definition (behavior state machines) to .sesm flatbuffers.
for json in resource/hsm/*.json; do
    flatc --binary -o resource/hsm/ misc/StateMachine.fbs "$json"
    echo "Generated ${json%.json}.sesm"
done
