#!/bin/bash

# $1 resource_root   $2 project_root

echo "convert sound cues to secl"
for i in `ls $1/sound_cue/*.json`; do
    echo " $i"
    flatc -b -o "$1/sound_cue/" "$2/misc/SoundCue.fbs" $i
done
