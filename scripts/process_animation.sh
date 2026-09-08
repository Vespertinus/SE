#/bin/sh

./convert --input "resource/raw/animation/Universal Animation Library[Standard]/Unreal-Godot/UAL1_Standard.glb" --output resource/animation/ual1
./convert --input "resource/raw/model/character/Universal Base Characters[Standard]/Base Characters/Godot - UE/Superhero_Male_FullBody.gltf" --output resource/model/male_fullbody
./convert --input "resource/raw/model/character/Universal Base Characters[Standard]/Base Characters/Godot - UE/Superhero_Female_FullBody.gltf" --output resource/model/female_fullbody
./normalize_skel generate-canonical resource/animation/ual1_skeleton.sesk --output resource/animation/canonical_humanoid.sesk
./normalize_skel anim-source  resource/animation/ual1.sesc          --ref-skel resource/animation/canonical_humanoid.sesk
./normalize_skel character    resource/model/male_fullbody.sesc      --ref-skel resource/animation/canonical_humanoid.sesk
./normalize_skel character    resource/model/female_fullbody.sesc    --ref-skel resource/animation/canonical_humanoid.sesk
# Bone masks — spine_01 and up; root/pelvis/legs stay at 0 so locomotion keeps
# driving the legs under overlay layers (aiming etc.). Every skeleton used with
# character.seag needs this — the graph's aiming layer references "upper_body".
./normalize_skel add-mask resource/animation/ual1_skeleton.sesk          --mask-name upper_body --exclude-bones root,pelvis,thigh,calf,foot,ball,toe
./normalize_skel add-mask resource/animation/canonical_humanoid.sesk     --mask-name upper_body --exclude-bones root,pelvis,thigh,calf,foot,ball,toe
./normalize_skel add-mask resource/model/male_fullbody_skeleton.sesk     --mask-name upper_body --exclude-bones root,pelvis,thigh,calf,foot,ball,toe
./normalize_skel add-mask resource/model/female_fullbody_skeleton.sesk   --mask-name upper_body --exclude-bones root,pelvis,thigh,calf,foot,ball,toe
./compose_scene test_character.seassembly
./compose_scene test_04.seassembly
./compose_scene test_05.seassembly
./compose_scene character.seassembly
./compose_scene npc.seassembly

