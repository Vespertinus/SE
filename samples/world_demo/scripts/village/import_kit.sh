#!/usr/bin/env bash
# Converts the curated asset subsets for the world_demo village:
#   - Medieval Village MegaKit pieces  -> resource/model/village/*.sesc
#   - Stylized Nature MegaKit pieces   -> resource/model/nature/*.sesc
#   - Modular Character Outfits        -> resource/model/npc/*.sesc (skinned)
# Textures stay as raw PNG references (the engine loads images directly, like
# the character pipeline). Rerunnable; overwrites outputs.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CONVERT="${CONVERT_BIN:-${SCRIPT_DIR}/../../convert}"
MEDIEVAL="resource/raw/model/Medieval Village MegaKit[Standard]/glTF"
NATURE="resource/raw/model/Stylized Nature MegaKit[Standard]/glTF"
OUTFITS="resource/raw/model/Modular Character Outfits - Fantasy[Standard]/Exports/glTF (Godot-Unreal)/Outfits"
MODPARTS="resource/raw/model/Modular Character Outfits - Fantasy[Standard]/Exports/glTF (Godot-Unreal)/Modular Parts"

if [[ ! -x "$CONVERT" ]]; then
    echo "ERROR: convert not found at '$CONVERT' (build the 'convert' target)" >&2
    exit 1
fi

mkdir -p resource/model/village resource/model/nature resource/model/npc

convert_piece() { # <source-dir> <name> <out-dir> [extra flags...]
    local dir="$1" name="$2" out="$3"; shift 3
    echo "-- $name"
    # --cut_path strips the leading "resource/" the engine's ResourceManager
    # re-adds at load time; without it the stored paths double the prefix.
    "$CONVERT" --input "$dir/$name.gltf" --output "$out/$name" --to_scene true \
        --cut_path "resource/" "$@" --level warn
}

echo "=== Medieval Village MegaKit ==="
# walls (2 m segments, 3.12 m high)
for p in Wall_Plaster_Straight Wall_Plaster_Door_Flat Wall_Plaster_Window_Wide_Flat \
         Wall_Plaster_Straight_Base Wall_Arch \
         Wall_UnevenBrick_Straight Wall_UnevenBrick_Door_Flat \
         Wall_UnevenBrick_Window_Thin_Round Wall_UnevenBrick_Window_Wide_Flat; do
    convert_piece "$MEDIEVAL" "$p" resource/model/village
done
# corners, floors, roofs (Roof_Front_* are the gable wall pieces that close
# the roof ends; the 4/6/8 suffix matches the wall run width in meters)
for p in Corner_Exterior_Brick Corner_Exterior_Wood \
         Floor_Brick Floor_UnevenBrick Floor_WoodDark Floor_WoodLight \
         Roof_RoundTiles_4x6 Roof_RoundTiles_6x6 Roof_RoundTiles_6x8 Roof_RoundTiles_8x8 \
         Roof_Tower_RoundTiles \
         Roof_Front_Brick4 Roof_Front_Brick6 Roof_Front_Brick8; do
    convert_piece "$MEDIEVAL" "$p" resource/model/village
done
# doors, windows, shutters, stairs, balcony, overhangs
for p in Door_1_Flat Door_2_Flat Door_1_Round \
         DoorFrame_Round_Brick DoorFrame_Flat_WoodDark \
         Window_Wide_Flat1 Window_Thin_Round1 \
         WindowShutters_Wide_Flat_Open WindowShutters_Thin_Round_Open \
         Stairs_Exterior_Straight Balcony_Simple_Straight \
         Overhang_Plaster_Long Overhang_Plaster_Short; do
    convert_piece "$MEDIEVAL" "$p" resource/model/village
done
# props
for p in Prop_WoodenFence_Single Prop_WoodenFence_Extension1 Prop_Crate Prop_Wagon \
         Prop_Chimney Prop_Vine1 Prop_Vine2 Prop_Brick1 Prop_Support; do
    convert_piece "$MEDIEVAL" "$p" resource/model/village
done

echo "=== Stylized Nature MegaKit ==="
for p in CommonTree_1 CommonTree_3 CommonTree_5 Pine_2 Pine_4 DeadTree_2 TwistedTree_1 \
         Bush_Common Bush_Common_Flowers \
         Grass_Common_Tall Grass_Common_Short Grass_Wispy_Tall Fern_1 \
         Flower_3_Group Flower_4_Group Mushroom_Common \
         Pebble_Round_1 Pebble_Round_2 Pebble_Round_3 \
         Rock_Medium_1 Rock_Medium_2 \
         RockPath_Round_Wide RockPath_Round_Small_1; do
    convert_piece "$NATURE" "$p" resource/model/nature
done

echo "=== Modular Character Outfits (skinned) ==="
for p in Male_Peasant Female_Peasant Male_Ranger; do
    convert_piece "$OUTFITS" "$p" resource/model/npc --skin true
done

# Skinned head pieces: the Outfits exports are clothing-only (the pack ships no
# Peasant heads), so villagers wear the Ranger hood parts on the shared
# 65-joint skeleton. Attached to the Armature by build_village.py npc_template.
echo "=== Modular Character Outfits (head parts, skinned) ==="
for p in Male_Ranger_Head_Hood Female_Ranger_Head_Hood; do
    convert_piece "$MODPARTS" "$p" resource/model/npc --skin true
done

# Bare heads: the hoods are cloth-only. The Universal Base Superhero FullBody
# exports share the exact 65-joint skeleton and carry the head skin inside the
# body mesh plus separate Eyes/Eyebrows nodes — grafted alongside the hood so
# NPCs get a real face. npc_template() names the grafts Head_Base/Head_Eyes.
echo "=== Universal Base Characters (heads, skinned) ==="
BASECHAR="resource/raw/model/character/Universal Base Characters[Standard]/Base Characters/Godot - UE"
for p in Superhero_Male_FullBody Superhero_Female_FullBody; do
    convert_piece "$BASECHAR" "$p" resource/model/npc --skin true
done

echo "import_kit: done"
