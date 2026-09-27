#!/usr/bin/env python3
"""Generates the world_demo village content.

Outputs (all deterministic, rerunnable):
  resource/world/village_gen/*.seassembly   sub-level + NPC-template manifests
  resource/world/world_demo.json            streaming world (compiled by convert_world.sh)
  resource/world/world_demo_no_streaming.json  same content, streaming disabled
  resource/nav/world_demo.json              nav_bake config

Buildings are assembled from Medieval Village MegaKit pieces (piece scenes
converted by import_kit.sh), placed on exact terrain heights via the shared
height oracle from make_heightmap.py.
"""

import json
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_heightmap import height_at  # noqa: E402

GEN_DIR = "resource/world/village_gen"
KIT = "resource/model/village"
NATURE = "resource/model/nature"
NPC = "resource/model/npc"
AI_DIR = "resource/ai/world_demo"
ANIM_GRAPH = "resource/anim_graph/character.seag"

WORLD_SEED = 20260917

# ---------------------------------------------------------------------------
# Piece dimension table (from glTF POSITION accessor bounds; full extents).
# dims = (dx, dy, dz); ymin = min corner above the piece origin.
# ---------------------------------------------------------------------------
PIECE = {
    "Wall_Plaster_Straight":            (2.00, 3.12, 0.41, 0.00),
    "Wall_Plaster_Door_Flat":           (2.00, 3.12, 0.41, 0.00),
    "Wall_Plaster_Window_Wide_Flat":    (2.00, 3.12, 0.41, 0.00),
    "Wall_Plaster_Straight_Base":       (2.00, 3.12, 0.41, 0.00),
    "Wall_Arch":                        (2.00, 3.00, 0.06, 0.00),
    "Wall_UnevenBrick_Straight":        (2.00, 3.12, 0.41, 0.00),
    "Wall_UnevenBrick_Door_Flat":       (2.00, 3.12, 0.41, 0.00),
    "Wall_UnevenBrick_Window_Thin_Round": (2.00, 3.12, 0.41, 0.00),
    "Wall_UnevenBrick_Window_Wide_Flat": (2.00, 3.12, 0.41, 0.00),
    "Corner_Exterior_Brick":            (0.53, 3.02, 0.58, -0.01),
    "Corner_Exterior_Wood":             (0.21, 3.00, 0.24, 0.00),
    "Floor_Brick":                      (2.00, 0.02, 2.00, -0.01),
    "Floor_UnevenBrick":                (2.00, 0.02, 2.00, -0.01),
    "Floor_WoodDark":                   (2.00, 0.02, 2.00, -0.01),
    "Floor_WoodLight":                  (2.00, 0.02, 2.00, -0.01),
    "Roof_RoundTiles_4x6":              (5.51, 4.23, 7.57, -0.52),
    "Roof_RoundTiles_6x6":              (8.25, 5.67, 8.03, -0.78),
    "Roof_RoundTiles_6x8":              (8.25, 5.67, 9.68, -0.78),
    "Roof_RoundTiles_8x8":              (9.95, 6.78, 10.33, -0.78),
    "Roof_Tower_RoundTiles":            (5.65, 7.36, 5.43, -0.57),
    # gable wall pieces closing the roof ends; the suffix matches the wall-run
    # width in meters (local X = width, thickness lip extends toward +z)
    "Roof_Front_Brick4":                (4.40, 3.07, 1.13, -0.13),
    "Roof_Front_Brick6":                (6.69, 4.52, 1.13, -0.13),
    "Roof_Front_Brick8":                (8.51, 5.45, 1.13, -0.13),
    "Stairs_Exterior_Straight":         (2.00, 1.20, 2.08, -0.20),
    "Door_1_Flat":                      (1.12, 2.10, 0.12, 0.04),
    "Door_2_Flat":                      (1.11, 2.10, 0.26, 0.04),
    "Door_1_Round":                     (1.10, 2.10, 0.12, 0.04),
    "DoorFrame_Round_Brick":            (1.60, 2.59, 0.48, 0.00),
    "DoorFrame_Flat_WoodDark":          (1.60, 2.59, 0.48, 0.00),
    "Window_Wide_Flat1":                (1.61, 1.58, 0.68, 0.94),
    "Window_Thin_Round1":               (1.20, 1.60, 0.40, 0.90),
    "WindowShutters_Wide_Flat_Open":    (1.35, 1.42, 0.14, 1.09),
    "WindowShutters_Thin_Round_Open":   (1.10, 1.40, 0.14, 1.05),
    "Stairs_Exterior_Straight":         (2.00, 1.20, 2.08, -0.20),
    "Balcony_Simple_Straight":          (2.00, 1.23, 0.21, -0.11),
    "Overhang_Plaster_Long":            (2.00, 3.03, 2.20, 0.00),
    "Overhang_Plaster_Short":           (2.00, 3.03, 1.20, 0.00),
    "Prop_WoodenFence_Single":          (2.06, 0.84, 0.12, -0.03),
    "Prop_WoodenFence_Extension1":      (2.06, 0.84, 0.12, -0.03),
    "Prop_Crate":                       (1.08, 1.06, 1.08, 0.00),
    "Prop_Wagon":                       (1.95, 1.53, 4.02, 0.00),
    "Prop_Chimney":                     (0.95, 3.18, 1.00, 0.00),
    "Prop_Vine1":                       (1.54, 2.60, 0.22, -2.12),
    "Prop_Vine2":                       (1.30, 2.40, 0.22, -2.12),
    "Prop_Brick1":                      (1.20, 0.60, 0.90, 0.00),
    "Prop_Support":                     (0.30, 3.00, 0.30, 0.00),
}

# Nature pieces: (footprint, height, base_scale) — the source kit authors the
# small props at ~2 m (Flower_3 is 2.05 m tall), so dressing scatter multiplies
# the per-piece base_scale to bring flowers/grass/bushes below human height.
NATURE_PIECE = {
    "CommonTree_1": (2.5, 7.0, 1.0), "CommonTree_3": (2.8, 8.0, 1.0), "CommonTree_5": (3.0, 9.0, 1.0),
    "Pine_2": (3.5, 9.0, 1.0), "Pine_4": (3.5, 10.0, 1.0), "DeadTree_2": (3.0, 7.0, 1.0),
    "TwistedTree_1": (3.5, 6.0, 1.0), "Bush_Common": (2.0, 1.0, 0.5), "Bush_Common_Flowers": (2.0, 1.0, 0.5),
    "Grass_Common_Tall": (0.8, 0.6, 0.35), "Grass_Common_Short": (0.7, 0.4, 0.45),
    "Grass_Wispy_Tall": (0.8, 0.7, 0.35), "Fern_1": (1.0, 0.7, 0.3),
    "Flower_3_Group": (0.6, 0.4, 0.3), "Flower_4_Group": (0.6, 0.4, 0.25),
    "Mushroom_Common": (0.4, 0.4, 0.7), "Pebble_Round_1": (0.5, 0.3, 1.0),
    "Pebble_Round_2": (0.5, 0.3, 1.0), "Pebble_Round_3": (0.4, 0.25, 1.0),
    "Rock_Medium_1": (2.6, 1.2, 1.0), "Rock_Medium_2": (2.2, 1.0, 1.0),
    "RockPath_Round_Wide": (2.2, 0.15, 1.0), "RockPath_Round_Small_1": (1.0, 0.12, 1.0),
}

_bounds = {}  # per-manifest AABB accumulation


def _track(tag, x, y, z, rx, ry, rz):
    lo, hi = _bounds.setdefault(tag, ([1e9] * 3, [-1e9] * 3))
    lo[0] = min(lo[0], x - rx); hi[0] = max(hi[0], x + rx)
    lo[1] = min(lo[1], y - 0.5); hi[1] = max(hi[1], y + ry)
    lo[2] = min(lo[2], z - rz); hi[2] = max(hi[2], z + rz)


def piece_node(tag, name, piece, pos, yaw=0.0, pitch=0.0, collider=False,
               friction=0.6, out_dir=KIT):
    """A StaticModel node referencing a converted piece scene (+ optional static box)."""
    dims = PIECE.get(piece)
    if dims is None:
        raise KeyError(f"piece not in table: {piece}")
    dx, dy, dz, ymin = dims
    # yaw extents swap for quarter turns
    c, s = abs(math.cos(math.radians(yaw))), abs(math.sin(math.radians(yaw)))
    rx = (dx * c + dz * s) / 2.0
    rz = (dx * s + dz * c) / 2.0
    _track(tag, pos[0], pos[1] + ymin, pos[2], rx, dy - ymin + 0.6, rz)
    n = {
        "name": name,
        "from": {"scene": f"{out_dir}/{piece}.sesc", "node": f"RootNode|{piece}"},
        "translation": [round(pos[0], 3), round(pos[1], 3), round(pos[2], 3)],
        "rotation": [pitch, yaw, 0.0],
    }
    if collider:
        n["components"] = {"RigidBody": {
            "collider": {"type": "box", "half_extents": [round(dx / 2, 3), round(dy / 2, 3), round(dz / 2, 3)]},
            "is_static": True, "friction": friction,
        }}
    return n


def nature_node(tag, name, piece, pos, yaw=0.0, scale=1.0, collider=False):
    base_scale = NATURE_PIECE[piece][2]
    s = scale * base_scale
    fx, fy = NATURE_PIECE[piece][0], NATURE_PIECE[piece][1]
    _track(tag, pos[0], pos[1], pos[2], fx * s / 2, fy * s, fx * s / 2)
    n = {
        "name": name,
        "from": {"scene": f"{NATURE}/{piece}.sesc", "node": f"RootNode|{piece}"},
        "translation": [round(pos[0], 3), round(pos[1], 3), round(pos[2], 3)],
        "rotation": [0.0, yaw, 0.0],
        "scale": [round(s, 3), round(s, 3), round(s, 3)],
    }
    if collider:  # trunk-only box collider
        n["components"] = {"RigidBody": {
            "collider": {"type": "box", "half_extents": [0.3, fy * s / 2, 0.3]},
            "is_static": True,
        }}
    return n


def plain_node(name, components=None, children=None, pos=None, yaw=0.0):
    n = {"name": name}
    if pos:
        n["translation"] = [round(pos[0], 3), round(pos[1], 3), round(pos[2], 3)]
    if yaw:
        n["rotation"] = [0.0, yaw, 0.0]
    if components:
        n["components"] = components
    if children:
        n["children"] = children
    return n


def hash01(*vals):
    """Deterministic 01 hash for scatter placement."""
    h = WORLD_SEED
    for v in vals:
        h ^= (int(v * 8191) + 0x9E3779B9) & 0xFFFFFFFF
        h = (h * 0x85EBCA6B) & 0xFFFFFFFF
        h ^= h >> 13
    return (h & 0xFFFFFF) / float(0xFFFFFF)


# ---------------------------------------------------------------------------
# Building constructors (append nodes to a manifest list)
# ---------------------------------------------------------------------------

def wall_run(tag, out, name, piece, start, end, y, height=None, collider=True):
    """Place 2 m wall pieces along a straight X or Z run, spanning start→end
    corner to corner: segment centres tile the full run (a slight overlap at
    non-even lengths is hidden under the corner posts, instead of a gap)."""
    nodes = []
    (x0, z0), (x1, z1) = start, end
    horizontal = abs(z1 - z0) < 1e-6
    length = abs((x1 - x0) if horizontal else (z1 - z0))
    n_seg = max(1, int(round(length / 2.0)))
    seg = length / n_seg
    yaw = 0.0 if horizontal else 90.0
    for k in range(n_seg):
        t = (seg * (k + 0.5)) / length
        px = x0 + (x1 - x0) * t
        pz = z0 + (z1 - z0) * t
        nodes.append(piece_node(tag, f"{name}_{k}", piece, (px, y, pz), yaw=yaw,
                                collider=collider, out_dir=out))
    return nodes


def wall_slots(tag, out, name, piece, axis, fixed, lo, y, n, door=False,
               door_piece="Door_1_Flat",
               window_slot=None, window_piece=None,
               collider=True, raised_collider_index=None):
    """Tile a wall with n modular 2 m pieces edge to edge on the 2 m grid —
    no corner gaps and no coplanar overlap between tiles. One slot may be a
    door wall (side pillars + lintel colliders frame the ~1.3 x 2.1 m opening),
    one a window wall piece. Doors ship CLOSED: the leaf yaw equals the wall
    yaw, the demo swings it open around the hinge (leaf-local +x) on interact.
    axis 'x': tiles along X at z=fixed, covering [lo, lo + 2n].
    axis 'z': tiles along Z at x=fixed, covering [lo, lo + 2n].
    raised_collider_index: for the column stacked ABOVE a door (upper course),
    mesh-align the box — wall boxes are centered on the tile node while the
    mesh rises from it, so an upper-course box hangs 1.56 m below its mesh and
    shears the doorway's headroom down to ~1.5 m (the invisible inn blocker)."""
    nodes = []
    door_i = (n - 1) // 2 if door else None
    for i in range(n):
        pos = lo + 1.0 + i * 2.0
        p = (pos, y, fixed) if axis == "x" else (fixed, y, pos)
        yaw = 0.0 if axis == "x" else 90.0
        if i == raised_collider_index:
            # box follows the mesh: centred 1.56 up from the base pivot
            tile = piece_node(tag, f"{name}_{i}", piece, p, yaw=yaw,
                              collider=False, out_dir=out)
            tile["children"] = [{
                "name": f"{name}_{i}_Box",
                "translation": [0.0, 1.56, 0.0],
                "components": {"RigidBody": {
                    "collider": {"type": "box",
                                 "half_extents": [1.0, 1.56, 0.205]},
                    "is_static": True, "friction": 0.6}}},
            ]
            nodes.append(tile)
            continue
        if i == door_i:
            # The full-tile box would seal the doorway — the wall is split
            # instead: two side-pillar children + a lintel child (leaf-local
            # space, the node yaw rotates them) frame the mesh opening
            # (x in [-0.648, 0.65], y up to ~2.15). Pillars reach 4 cm into
            # the opening so no collider seam gap opens at the jambs.
            # The lintel box bottom sits at 2.25 m over the floor (2.78 local
            # on a 1.98 base, half 0.51): the 2.0 m player capsule + its
            # 0.05 collision skin needs the headroom — at 2.61 the box bottom
            # grazed the capsule top and wedged it in every doorway.
            dw = piece_node(tag, f"{name}_DoorW",
                            piece.replace("Straight", "Door_Flat"),
                            p, yaw=yaw, collider=False, out_dir=out)
            pillar_l = {"name": f"{name}_PillarL", "translation": [-0.82, 1.56, 0.0],
                        "components": {"RigidBody": {
                            "collider": {"type": "box",
                                         "half_extents": [0.18, 1.56, 0.205]},
                            "is_static": True, "friction": 0.6}}}
            pillar_r = {"name": f"{name}_PillarR", "translation": [0.82, 1.56, 0.0],
                        "components": {"RigidBody": {
                            "collider": {"type": "box",
                                         "half_extents": [0.18, 1.56, 0.205]},
                            "is_static": True, "friction": 0.6}}}
            lintel = {"name": f"{name}_Lintel",
                      "translation": [0.0, 2.78, 0.0],
                      "components": {"RigidBody": {
                          "collider": {"type": "box",
                                       "half_extents": [1.0, 0.51, 0.205]},
                          "is_static": True, "friction": 0.6}}}
            dw["children"] = [pillar_l, pillar_r, lintel]
            nodes.append(dw)
            # leaf centred in the wall slab (the mesh's z=0 node plane is the
            # +z face, slab spans z in [-0.2, 0]) — flush from either side.
            # The hinge sits at the opening's edge, not its middle: an open
            # leaf swings perpendicular through the doorway, and a hinge at
            # -0.5 left only a ~34 cm passage beside it — the player caught
            # on the leaf fin even though the door was fully open.
            if axis == "x":
                leaf = (pos - 0.60, y, fixed - 0.1)
            else:
                leaf = (fixed - 0.1, y, pos - 0.60)
            leaf_node = piece_node(tag, f"{name}_Door", door_piece, leaf,
                                   yaw=yaw, out_dir=out)
            # kinematic leaf body: authored in leaf-local space at the slab
            # centre, so the demo can swing the leaf around the hinge and the
            # RigidBody node-listener drags the collider along (MoveKinematic)
            ldx, ldy, _, _ = PIECE[door_piece]
            hx, hy = round(ldx / 2, 3), round(ldy / 2, 3)
            leaf_node["children"] = [{
                "name": f"{name}_DoorBody",
                "translation": [hx, hy, 0.0],
                "components": {"RigidBody": {
                    "collider": {"type": "box",
                                 "half_extents": [hx, hy, 0.07]},
                    "is_kinematic": True, "friction": 0.8}}}]
            nodes.append(leaf_node)
        elif window_slot is not None and i == window_slot and window_piece:
            nodes.append(piece_node(tag, f"{name}_WinW{i}", window_piece, p,
                                    yaw=yaw, collider=collider, out_dir=out))
        else:
            nodes.append(piece_node(tag, f"{name}_{i}", piece, p, yaw=yaw,
                                    collider=collider, out_dir=out))
    return nodes


def roof_gables(tag, out, name, width, cx, cz, d, y, course=0.0):
    """Gable wall pieces closing the two roof ends. The ridge runs along Z,
    so the gables sit on the ±z walls; the piece's thickness lip points into
    the building (yaw 180 mirrors it on the +z end)."""
    piece = {4: "Roof_Front_Brick4", 6: "Roof_Front_Brick6", 8: "Roof_Front_Brick8"}[int(width)]
    _, _, _, ymin = PIECE[piece]
    nodes = []
    for sz in (-1, 1):
        nodes.append(piece_node(tag, f"{name}_Gable{sz:+d}", piece,
                                (cx, y + course + 3.12 - ymin, cz + sz * d / 2),
                                yaw=0.0 if sz < 0 else 180.0, out_dir=out))
    return nodes


def simple_building(tag, out, name, cx, cz, w, d, wall_front, wall_side,
                    roof, door_side="+z", h=None, collider=True, with_door=True,
                    door_piece="Door_1_Flat"):
    """One-story gable building. w along X, d along Z. Returns top-of-wall height."""
    y = h if h is not None else height_at(cx, cz) - 0.02
    nodes = []

    # floor grid
    k = 0
    for ix in range(int(w // 2)):
        for iz in range(int(d // 2)):
            fx = cx - w / 2 + 1 + ix * 2
            fz = cz - d / 2 + 1 + iz * 2
            nodes.append(piece_node(tag, f"{name}_Floor_{k}", "Floor_Brick" if (ix + iz) % 2 == 0 else "Floor_UnevenBrick",
                                    (fx, y + 0.02, fz), collider=True, out_dir=out))
            k += 1

    # corner posts
    for sx in (-1, 1):
        for sz in (-1, 1):
            nodes.append(piece_node(tag, f"{name}_Corner{sx}{sz}", "Corner_Exterior_Wood",
                                    (cx + sx * w / 2, y, cz + sz * d / 2),
                                    yaw=45 if (sx * sz > 0) else -45, collider=True, out_dir=out))

    # front (door side) and back walls — modular 2 m tiles, corner to corner
    front_z = cz + d / 2 if door_side == "+z" else cz - d / 2
    back_z = cz - d / 2 if door_side == "+z" else cz + d / 2
    sign = 1.0 if door_side == "+z" else -1.0
    n_fx = int(w // 2)
    if with_door:
        nodes += wall_slots(tag, out, f"{name}_F", wall_front, "x", front_z, cx - w / 2, y, n_fx,
                            door=True, door_piece=door_piece, collider=collider)
    else:
        nodes += wall_slots(tag, out, f"{name}_F", wall_front, "x", front_z, cx - w / 2, y, n_fx,
                            collider=collider)
    nodes += wall_slots(tag, out, f"{name}_B", wall_front.replace("Door_Flat", "Straight").replace("Window_Wide_Flat", "Straight"),
                        "x", back_z, cx - w / 2, y, n_fx, collider=collider)

    # side walls (2 m tiles) with the window wall at the middle slot
    for sx in (-1, 1):
        x = cx + sx * w / 2
        nodes += wall_slots(tag, out, f"{name}_S{sx}", wall_side, "z", x, cz - d / 2, y, int(d // 2),
                            window_slot=(int(d // 2) - 1) // 2, window_piece=wall_side,
                            collider=collider)
        nodes.append(piece_node(tag, f"{name}_Win{sx}", "Window_Wide_Flat1",
                                (x + (0.35 * -sx), y, cz),
                                yaw=90.0 if sx > 0 else -90.0, out_dir=out))

    # window inserts on front/back where window walls exist
    # (front flanks are plain walls for 4 m buildings; back gets one window)
    nodes.append(piece_node(tag, f"{name}_WinB", "Window_Wide_Flat1", (cx, y, back_z + (0.35 if door_side == "+z" else -0.35)),
                            yaw=0.0 if door_side == "+z" else 180.0, out_dir=out))

    # roof + gables closing the roof ends + chimney + vine
    ry, _, rz, roof_ymin = PIECE[roof]
    # no collider on roofs: their bounding boxes reach far below the visible
    # surface and would box in the interior headroom
    nodes.append(piece_node(tag, f"{name}_Roof", roof, (cx, y + 3.12 - roof_ymin, cz), out_dir=out))
    nodes += roof_gables(tag, out, name, w, cx, cz, d, y)
    nodes.append(piece_node(tag, f"{name}_Chimney", "Prop_Chimney",
                            (cx + w / 2 - 0.55, y + 3.0, cz - d / 2 + 0.55), out_dir=out))
    nodes.append(piece_node(tag, f"{name}_Vine", "Prop_Vine1",
                            (cx - w / 4, y + 2.95, front_z + (0.28 if door_side == "+z" else -0.28)),
                            out_dir=out))
    return nodes


def add_portal(node, level, act=22.0, rel=34.0):
    """Attach a Portal component (door → Manual sub-level link) to a node dict.
    The engine stages `level` while any streaming focus is inside `act` metres
    and releases it past `rel` (hysteresis band)."""
    node.setdefault("components", {})["Portal"] = {
        "level_path": level, "activate_radius": act, "release_radius": rel}
    return node


def find_node(nodes, name):
    for n in nodes:
        if n.get("name") == name:
            return n
    raise KeyError(f"node '{name}' not found")


def inn_building(tag, out, name, cx, cz):
    """Two-story inn, 8x8, ground UnevenBrick + upper Plaster, balcony, door faces -z (street)."""
    y = height_at(cx, cz) - 0.02
    w = d = 8.0
    nodes = []

    # ground + upper floors
    k = 0
    for ix in range(4):
        for iz in range(4):
            fx = cx - 3 + ix * 2
            fz = cz - 3 + iz * 2
            mat = "Floor_UnevenBrick" if (ix + iz) % 2 == 0 else "Floor_Brick"
            nodes.append(piece_node(tag, f"{name}_GF_{k}", mat, (fx, y + 0.02, fz), collider=True, out_dir=out))
            nodes.append(piece_node(tag, f"{name}_UF_{k}", "Floor_WoodDark", (fx, y + 3.14, fz), collider=True, out_dir=out))
            k += 1

    for sx in (-1, 1):
        for sz in (-1, 1):
            nodes.append(piece_node(tag, f"{name}_Corner{sx}{sz}", "Corner_Exterior_Brick",
                                    (cx + sx * 4.0, y, cz + sz * 4.0),
                                    yaw=45 if (sx * sz > 0) else -45, collider=True, out_dir=out))

    front_z = cz - 4.0   # faces street (z=0)
    back_z = cz + 4.0
    # ground course: door front, windows elsewhere (4 modular tiles per 8 m wall)
    nodes += wall_slots(tag, out, f"{name}_GF_F", "Wall_UnevenBrick_Straight", "x", front_z, cx - 4.0, y, 4,
                        door=True, door_piece="Door_2_Flat")
    nodes += wall_slots(tag, out, f"{name}_GF_B", "Wall_UnevenBrick_Straight", "x", back_z, cx - 4.0, y, 4)
    for sx in (-1, 1):
        x = cx + sx * 4.0
        nodes += wall_slots(tag, out, f"{name}_GF_S{sx}", "Wall_UnevenBrick_Straight", "z", x, cz - 4.0, y, 4,
                            window_slot=1, window_piece="Wall_UnevenBrick_Window_Thin_Round")
        nodes.append(piece_node(tag, f"{name}_GF_WinS{sx}", "Window_Thin_Round1", (x - sx * 0.35, y, cz - 1.0), yaw=90.0 if sx < 0 else -90.0, out_dir=out))
    nodes.append(piece_node(tag, f"{name}_GF_WinB", "Window_Thin_Round1", (cx, y, back_z - 0.35), yaw=180.0, out_dir=out))

    # upper course: plaster + wide windows, balcony over the door. The column
    # stacked above the GF door gets a mesh-aligned box (see wall_slots) so
    # the doorway keeps its full 2.1 m headroom.
    y2 = y + 3.14
    nodes += wall_slots(tag, out, f"{name}_UF_F", "Wall_Plaster_Window_Wide_Flat", "x", front_z, cx - 4.0, y2, 4,
                        raised_collider_index=(4 - 1) // 2)
    nodes += wall_slots(tag, out, f"{name}_UF_B", "Wall_Plaster_Straight", "x", back_z, cx - 4.0, y2, 4)
    for sx in (-1, 1):
        x = cx + sx * 4.0
        nodes += wall_slots(tag, out, f"{name}_UF_S{sx}", "Wall_Plaster_Straight", "z", x, cz - 4.0, y2, 4,
                            window_slot=1, window_piece="Wall_Plaster_Window_Wide_Flat")
        nodes.append(piece_node(tag, f"{name}_UF_WinS{sx}", "Window_Wide_Flat1", (x - sx * 0.35, y2, cz - 1.0), yaw=90.0 if sx < 0 else -90.0, out_dir=out))
    nodes.append(piece_node(tag, f"{name}_UF_WinF0", "Window_Wide_Flat1", (cx - 1.0, y2, front_z - 0.35), yaw=180.0, out_dir=out))
    nodes.append(piece_node(tag, f"{name}_UF_WinF1", "Window_Wide_Flat1", (cx + 1.0, y2, front_z - 0.35), yaw=180.0, out_dir=out))
    nodes.append(piece_node(tag, f"{name}_Balcony", "Balcony_Simple_Straight", (cx, y2 + 0.13, front_z - 1.0), yaw=180.0, out_dir=out))

    # roof + gables, chimneys, vines
    _, _, _, roof_ymin = PIECE["Roof_RoundTiles_8x8"]
    nodes.append(piece_node(tag, f"{name}_Roof", "Roof_RoundTiles_8x8", (cx, y2 + 3.12 - roof_ymin, cz), out_dir=out))
    nodes += roof_gables(tag, out, name, 8, cx, cz, 8, y, course=3.14)
    nodes.append(piece_node(tag, f"{name}_Chim0", "Prop_Chimney", (cx - 3.4, y2 + 3.0, cz + 3.4), out_dir=out))
    nodes.append(piece_node(tag, f"{name}_Chim1", "Prop_Chimney", (cx + 3.4, y2 + 3.0, cz + 3.4), out_dir=out))
    nodes.append(piece_node(tag, f"{name}_Vine0", "Prop_Vine1", (cx + 3.9, y2 + 2.9, front_z - 0.28), out_dir=out))
    nodes.append(piece_node(tag, f"{name}_Vine1n", "Prop_Vine2", (cx - 3.9, y + 2.9, front_z - 0.28), out_dir=out))
    return nodes


# ---------------------------------------------------------------------------
# Sub-level manifests
# ---------------------------------------------------------------------------

def build_village():
    tag = "village"
    children = []

    # main street paving + market plaza (plateau is flat at +2.0)
    y = 2.0
    k = 0
    for i, x in enumerate(range(-32, 33, 2)):
        piece = "Floor_Brick" if i % 2 == 0 else "Floor_UnevenBrick"
        children.append(piece_node(tag, f"Street_{k}", piece, (x + 1.0, y + 0.02, 0.0), out_dir=KIT))
        k += 1
    for ix in range(3):
        for iz in range(3):
            children.append(piece_node(tag, f"Plaza_{ix}_{iz}",
                                       "Floor_UnevenBrick" if (ix + iz) % 2 else "Floor_Brick",
                                       (-2.0 + ix * 2, y + 0.02, 5.0 + iz * 2), out_dir=KIT))

    # buildings
    children += simple_building(tag, KIT, "HouseA", -14, -8, 4, 6,
                                "Wall_Plaster_Straight", "Wall_Plaster_Window_Wide_Flat",
                                "Roof_RoundTiles_4x6", door_side="+z")
    children += simple_building(tag, KIT, "HouseB", -16, 8, 4, 6,
                                "Wall_Plaster_Straight", "Wall_Plaster_Window_Wide_Flat",
                                "Roof_RoundTiles_4x6", door_side="-z")
    inn_nodes = inn_building(tag, KIT, "Inn", 10, -10)
    children += inn_nodes
    # the inn interior streams in while the player approaches the front door
    add_portal(find_node(inn_nodes, "Inn_GF_F_Door"), "wd_inn_interior")
    # (the inn door is interactive now — E swings it and loads the interior)

    # market props
    children.append(piece_node(tag, "Wagon", "Prop_Wagon", (4.0, y, 6.5), yaw=95.0, collider=True, out_dir=KIT))
    children.append(piece_node(tag, "Crate0", "Prop_Crate", (7.0, y, 9.0), yaw=15.0, collider=True, out_dir=KIT))
    children.append(piece_node(tag, "Crate1", "Prop_Crate", (8.2, y, 9.3), yaw=-20.0, collider=True, out_dir=KIT))
    children.append(piece_node(tag, "Crate2", "Prop_Crate", (7.5, y + 1.06, 9.1), yaw=40.0, collider=True, out_dir=KIT))
    children.append(piece_node(tag, "Bricks", "Prop_Brick1", (-6.5, y, 8.0), yaw=10.0, collider=True, out_dir=KIT))

    # bell post (DoorFrame as a bell arch) with trigger + blackboard bindings
    bell_comps = {
        "TriggerVolume": {
            "collider": {"type": "box", "half_extents": [1.2, 1.5, 1.2]},
            "on_enter_event": "bell_ring", "one_shot": False,
            "collision_layer": 1, "collision_mask": 1,
        },
        "TriggerBinding": {"bindings": [
            {"on_event": "bell_ring", "target": "global", "key": "noise_pos", "write_position": True},
            {"on_event": "bell_ring", "target": "global", "key": "alert", "bool": True},
        ]},
        "SpatialEntry": {"tag": "bell", "radius": 1.0},
    }
    children.append(plain_node("BellPost", components=bell_comps, pos=(0.0, y, 10.0)))
    children.append(piece_node(tag, "BellArch", "DoorFrame_Round_Brick", (0.0, y, 10.0), collider=True, out_dir=KIT))

    # entrance fences (south, gap for the street)
    for i, x in enumerate(range(-30, 1, 2)):
        children.append(piece_node(tag, f"FenceS0_{i}", "Prop_WoodenFence_Single", (x, y, 8.0), collider=True, out_dir=KIT))
    for i, x in enumerate(range(3, 31, 2)):
        children.append(piece_node(tag, f"FenceS1_{i}", "Prop_WoodenFence_Single", (x, y, 8.0), collider=True, out_dir=KIT))
    # short wing fences along the street west edge
    for i, z in enumerate(range(2, 8, 2)):
        children.append(piece_node(tag, f"FenceW_{i}", "Prop_WoodenFence_Single", (-30.0, y, z), yaw=90.0, collider=True, out_dir=KIT))

    # village dressing (grass/bush/flower/pebble scatter on the plateau edge ring)
    dn = 0
    for i in range(46):
        ang = hash01(1, i) * math.tau
        rad = 18.0 + hash01(2, i) * 20.0
        x = math.cos(ang) * rad
        z = math.sin(ang) * rad
        if abs(x) < 3.5 and abs(z) < 12.0:  # keep street clear
            continue
        y_d = height_at(x, z)
        roll = hash01(3, i)
        if roll < 0.34:
            piece = "Grass_Common_Tall" if roll < 0.17 else "Grass_Common_Short"
        elif roll < 0.52:
            piece = "Bush_Common" if roll < 0.43 else "Bush_Common_Flowers"
        elif roll < 0.70:
            piece = "Flower_3_Group" if roll < 0.61 else "Flower_4_Group"
        elif roll < 0.88:
            piece = "Pebble_Round_1" if roll < 0.76 else "Pebble_Round_2"
        else:
            piece = "RockPath_Round_Small_1"
        children.append(nature_node(tag, f"Dress_{dn}", piece, (x, y_d, z),
                                    yaw=hash01(4, i) * 360.0, scale=0.8 + hash01(5, i) * 0.6))
        dn += 1

    # spawner geometry + population (ambient villagers).
    # Geometry nodes must live UNDER the population node — Population resolves
    # member geometry by FindChild on its own subtree.
    sv = []
    for i, (px, pz) in enumerate([(-8.0, 2.5), (8.0, 2.5), (-4.0, -4.0), (4.0, -4.0)]):
        sv.append(plain_node(f"VP{i + 1}", components={
            "SpawnVolume": {"size": [1.6, 1.0, 1.6], "distribution": "best_candidate",
                            "candidate_count": 6, "seed_offset": 11 + i},
            "SpatialEntry": {"tag": "spawn", "radius": 1.0},
        }, pos=(px, 0.0, pz)))
    pop = plain_node("Villagers", components={
        "Population": {
            "id": "villagers",
            "members": [
                {"template_id": "villager_a", "count": 2, "weight": 1.0, "geometry": "VP1"},
                {"template_id": "villager_a", "count": 1, "weight": 1.0, "geometry": "VP2"},
                {"template_id": "villager_b", "count": 2, "weight": 1.0, "geometry": "VP3"},
                {"template_id": "villager_b", "count": 1, "weight": 1.0, "geometry": "VP4"},
            ],
            "max_active": 6, "respawn_delay_s": 45.0, "tick_interval_s": 0.25,
            "lifetime": "sub_level", "snap": "navmesh",
        },
    }, pos=(0.0, y, 0.0))
    pop["children"] = sv

    return {"name": "Village", "children": children + [pop]}, _bounds[tag]


def build_inn_interior():
    tag = "inn"
    y = 2.0
    children = []
    k = 0
    for ix in range(4):
        for iz in range(4):
            fx = 10 - 3 + ix * 2
            fz = -10 - 3 + iz * 2
            children.append(piece_node(tag, f"IFloor_{k}", "Floor_WoodDark", (fx, y + 0.04, fz), out_dir=KIT))
            k += 1
    # sparse furnishings — kept clear of the doorway corridor (x [8.35, 9.65],
    # z [-14, -12.5]): the crate pile lives against the west wall instead.
    children.append(piece_node(tag, "ICrate0", "Prop_Crate", (7.2, y, -7.6), yaw=12.0, collider=True, out_dir=KIT))
    children.append(piece_node(tag, "ICrate1", "Prop_Crate", (7.4, y, -9.4), yaw=-15.0, collider=True, out_dir=KIT))
    children.append(piece_node(tag, "ICrate2", "Prop_Crate", (7.35, y + 1.06, -7.75), yaw=45.0, collider=True, out_dir=KIT))
    children.append(piece_node(tag, "ICrates3", "Prop_Crate", (13.0, y, -8.0), yaw=70.0, collider=True, out_dir=KIT))
    children.append(piece_node(tag, "IBricks", "Prop_Brick1", (12.5, y, -12.5), yaw=0.0, collider=True, out_dir=KIT))
    return {"name": "InnInterior", "children": children}, _bounds[tag]


def scatter_forest(tag, cx, seed_off, tree_count, camp):
    children = []
    trees = []
    attempts = 0
    while len(trees) < tree_count and attempts < tree_count * 30:
        attempts += 1
        x = cx - 29 + hash01(seed_off, attempts, 1) * 58.0
        z = -42 + hash01(seed_off, attempts, 2) * 84.0
        if math.hypot(x, z) > 118:  # keep inside the hill rim
            continue
        if abs(x + 42) < 9 and abs(z) < 60:  # keep the stream bed clear
            continue
        if camp and math.hypot(x - camp[0], z - camp[1]) < 10.0:  # camp clearing
            continue
        if any(math.hypot(x - tx, z - tz) < 5.0 for tx, tz in trees):
            continue
        trees.append((x, z))
        y = height_at(x, z)
        roll = hash01(seed_off, attempts, 3)
        if roll < 0.30:
            piece = "CommonTree_1" if roll < 0.10 else ("CommonTree_3" if roll < 0.20 else "CommonTree_5")
        elif roll < 0.52:
            piece = "Pine_2" if roll < 0.41 else "Pine_4"
        elif roll < 0.68:
            piece = "DeadTree_2" if roll < 0.60 else "TwistedTree_1"
        else:
            piece = "CommonTree_1"
        scale = 0.8 + hash01(seed_off, attempts, 4) * 0.5
        children.append(nature_node(tag, f"Tree{len(trees)}", piece, (x, y, z),
                                    yaw=hash01(seed_off, attempts, 5) * 360.0,
                                    scale=scale, collider=True))

    # forest floor dressing
    for i in range(40):
        x = cx - 28 + hash01(seed_off, 900, i) * 56.0
        z = -41 + hash01(seed_off, 901, i) * 82.0
        if camp and math.hypot(x - camp[0], z - camp[1]) < 8.0:
            continue
        y = height_at(x, z)
        roll = hash01(seed_off, 902, i)
        if roll < 0.40:
            piece = "Grass_Wispy_Tall" if roll < 0.2 else "Grass_Common_Short"
        elif roll < 0.58:
            piece = "Fern_1"
        elif roll < 0.70:
            piece = "Bush_Common" if roll < 0.64 else "Bush_Common_Flowers"
        elif roll < 0.82:
            piece = "Mushroom_Common" if roll < 0.76 else "Pebble_Round_3"
        elif roll < 0.92:
            piece = "Pebble_Round_1" if roll < 0.87 else "Pebble_Round_2"
        else:
            piece = "Flower_3_Group"
        children.append(nature_node(tag, f"FDress{i}", piece, (x, y, z),
                                    yaw=hash01(seed_off, 903, i) * 360.0,
                                    scale=0.8 + hash01(seed_off, 904, i) * 0.6))

    # rocks
    for i in range(6):
        x = cx - 25 + hash01(seed_off, 800, i) * 50.0
        z = -39 + hash01(seed_off, 801, i) * 78.0
        children.append(nature_node(tag, f"Rock{i}", "Rock_Medium_1" if i % 2 else "Rock_Medium_2",
                                    (x, height_at(x, z), z), yaw=hash01(seed_off, 802, i) * 360.0,
                                    scale=0.9 + hash01(seed_off, 803, i) * 0.8))

    if camp:
        ccx, ccz = camp
        cy = height_at(ccx, ccz) - 0.02
        children.append(piece_node(tag, "CampWagon", "Prop_Wagon", (ccx + 2.5, cy, ccz + 2.0), yaw=210.0, collider=True, out_dir=KIT))
        children.append(piece_node(tag, "CampCrate0", "Prop_Crate", (ccx - 2.5, cy, ccz - 1.0), yaw=30.0, collider=True, out_dir=KIT))
        children.append(piece_node(tag, "CampCrate1", "Prop_Crate", (ccx - 1.3, cy, ccz - 1.8), yaw=-10.0, collider=True, out_dir=KIT))
        children.append(piece_node(tag, "CampCrate2", "Prop_Crate", (ccx - 2.0, cy + 1.06, ccz - 1.4), yaw=75.0, collider=True, out_dir=KIT))
        # brick pile sits SE of the camp center: the guard post is (ccx, ccz),
        # and at +0.5/-0.5 the bricks' AABB sat 5 cm from it (arrival radius
        # 0.45 + 0.4 nav erosion could never be satisfied).
        children.append(piece_node(tag, "CampBricks", "Prop_Brick1", (ccx + 2.0, cy, ccz - 2.4), collider=True, out_dir=KIT))
        children.append(piece_node(tag, "CampSupport", "Prop_Support", (ccx - 3.5, cy, ccz + 2.5), yaw=15.0, collider=True, out_dir=KIT))

        children.append(plain_node("CampGuards", components={
            "Population": {
                "id": "camp_guards",
                "members": [],
                "waves": [{"on_event": "camp_alarm",
                           "members": [{"template_id": "guard", "count": 8, "weight": 2.0,
                                        "geometry": "CampVolume"}]}],
                "max_active": 8, "respawn_delay_s": -1.0, "tick_interval_s": 0.25,
                # persistent: chase guards spawn under the world dynamics root so
                # the sub-level's teardown (player leaving the camp's streaming
                # radius mid-chase) can't delete them at the territory edge
                "lifetime": "persistent", "snap": "navmesh", "seed_offset": 5,
            },
        }, pos=(ccx, cy, ccz),
            children=[plain_node("CampVolume", components={
                "SpawnVolume": {"size": [8.0, 2.0, 8.0], "distribution": "best_candidate",
                                "candidate_count": 12, "seed_offset": 31},
                "SpatialEntry": {"tag": "camp", "radius": 6.0},
            }, pos=(0.0, 0.0, 0.0))]))
        tx, tz = camp[0] + 14.0, camp[1]
        children.append(plain_node("CampTrigger", components={
            "TriggerVolume": {
                "collider": {"type": "sphere", "radius": 13.0},
                "on_enter_event": "camp_alarm", "one_shot": False,
                "collision_layer": 1, "collision_mask": 1,
            },
            "TriggerBinding": {"bindings": [
                {"on_event": "camp_alarm", "target": "global", "key": "alarm_raised", "bool": True},
            ]},
            "SpatialEntry": {"tag": "camp_trigger", "radius": 13.0},
        }, pos=(tx, height_at(tx, tz), tz)))
    return children


def build_forest_w():
    tag = "forest_w"
    children = scatter_forest(tag, -70.0, 100, 42, camp=(-76.0, 4.0))
    return {"name": "ForestW", "children": children}, _bounds[tag]


def build_forest_e():
    tag = "forest_e"
    children = scatter_forest(tag, 70.0, 200, 46, camp=None)
    return {"name": "ForestE", "children": children}, _bounds[tag]


def build_farm():
    tag = "farm"
    children = []
    cx, cz = 0.0, 58.0
    y = height_at(cx, cz) - 0.02

    # barn (6x8, brick, big door facing village/north = -z)
    bx, bz = -8.0, 58.0
    by = height_at(bx, bz) - 0.02
    k = 0
    for ix in range(3):
        for iz in range(4):
            children.append(piece_node(tag, f"Barn_F_{k}", "Floor_UnevenBrick",
                                       (bx - 2 + ix * 2, by + 0.02, bz - 3 + iz * 2), collider=True, out_dir=KIT))
            k += 1
    for sx in (-1, 1):
        for sz in (-1, 1):
            children.append(piece_node(tag, f"Barn_C{sx}{sz}", "Corner_Exterior_Brick",
                                       (bx + sx * 3.0, by, bz + sz * 4.0),
                                       yaw=45 if (sx * sz > 0) else -45, collider=True, out_dir=KIT))
    children += wall_slots(tag, KIT, "Barn_FW", "Wall_UnevenBrick_Straight", "x", bz - 4.0, bx - 3.0, by, 3,
                           door=True, door_piece="Door_2_Flat")
    children += wall_slots(tag, KIT, "Barn_BW", "Wall_UnevenBrick_Straight", "x", bz + 4.0, bx - 3.0, by, 3)
    for sx in (-1, 1):
        children += wall_slots(tag, KIT, f"Barn_S{sx}", "Wall_UnevenBrick_Straight", "z", bx + sx * 3.0, bz - 4.0, by, 4,
                               window_slot=1, window_piece="Wall_UnevenBrick_Window_Thin_Round")
    _, _, _, roof_ymin = PIECE["Roof_RoundTiles_6x8"]
    children.append(piece_node(tag, "Barn_Roof", "Roof_RoundTiles_6x8", (bx, by + 3.12 - roof_ymin, bz), out_dir=KIT))
    children += roof_gables(tag, KIT, "Barn", 6, bx, bz, 8, by)

    # two tilled plots with fence perimeters
    for p, (px, pz) in enumerate([(3.0, 56.0), (3.0, 63.0)]):
        py = height_at(px, pz) - 0.02
        for ix in range(3):
            for iz in range(2):
                children.append(piece_node(tag, f"Plot{p}_S_{ix}_{iz}", "Floor_WoodDark",
                                           (px - 2 + ix * 2, py + 0.03, pz - 1 + iz * 2), out_dir=KIT))
        for i, x in enumerate(range(-2, 3, 2)):
            children.append(piece_node(tag, f"Plot{p}_Fz{i}", "Prop_WoodenFence_Single", (px + x, py, pz - 2.0), collider=True, out_dir=KIT))
            children.append(piece_node(tag, f"Plot{p}_Fz2{i}", "Prop_WoodenFence_Single", (px + x, py, pz + 2.0), collider=True, out_dir=KIT))
        for i, z in enumerate(range(-2, 3, 2)):
            children.append(piece_node(tag, f"Plot{p}_Fx{i}", "Prop_WoodenFence_Single", (px - 3.0, py, pz + z), yaw=90.0, collider=True, out_dir=KIT))
            children.append(piece_node(tag, f"Plot{p}_Fx2{i}", "Prop_WoodenFence_Single", (px + 3.0, py, pz + z), yaw=90.0, collider=True, out_dir=KIT))

    # farmer spawner — geometry volumes live under the population node (local pos)
    children.append(plain_node("Farmers", components={
        "Population": {
            "id": "farmers",
            "members": [
                {"template_id": "farmer", "count": 1, "geometry": "FP1"},
                {"template_id": "farmer", "count": 1, "geometry": "FP2"},
            ],
            "max_active": 2, "respawn_delay_s": 90.0,
            "lifetime": "sub_level", "snap": "navmesh",
        },
    }, pos=(cx, y, cz), children=[
        plain_node("FP1", components={
            "SpawnVolume": {"size": [1.6, 1.0, 1.6], "seed_offset": 61},
            "SpatialEntry": {"tag": "spawn", "radius": 1.0},
        }, pos=(3.0 - cx, height_at(3.0, 54.0) - y, 54.0 - cz)),
        plain_node("FP2", components={
            "SpawnVolume": {"size": [1.6, 1.0, 1.6], "seed_offset": 62},
            "SpatialEntry": {"tag": "spawn", "radius": 1.0},
        }, pos=(3.0 - cx, height_at(3.0, 65.0) - y, 65.0 - cz)),
    ]))
    return {"name": "Farm", "children": children}, _bounds[tag]


def build_tower():
    tag = "tower"
    children = []
    cx, cz = 0.0, -58.0
    y = height_at(cx, cz) - 0.02

    k = 0
    for ix in range(2):
        for iz in range(2):
            children.append(piece_node(tag, f"Tw_F_{k}", "Floor_UnevenBrick",
                                       (cx - 1 + ix * 2, y + 0.02, cz - 1 + iz * 2), collider=True, out_dir=KIT))
            k += 1
    for sx in (-1, 1):
        for sz in (-1, 1):
            children.append(piece_node(tag, f"Tw_C{sx}{sz}", "Corner_Exterior_Brick",
                                       (cx + sx * 2.0, y, cz + sz * 2.0),
                                       yaw=45 if (sx * sz > 0) else -45, collider=True, out_dir=KIT))
    front_z = cz + 2.0  # faces village (south)
    children += wall_slots(tag, KIT, "Tw_FW", "Wall_UnevenBrick_Straight", "x", front_z, cx - 2.0, y, 2,
                           door=True, door_piece="Door_1_Round")
    children += wall_slots(tag, KIT, "Tw_B", "Wall_UnevenBrick_Straight", "x", cz - 2.0, cx - 2.0, y, 2)
    for sx in (-1, 1):
        children += wall_slots(tag, KIT, f"Tw_S{sx}", "Wall_UnevenBrick_Straight", "z", cx + sx * 2.0, cz - 2.0, y, 2,
                               window_slot=0 if sx < 0 else 1,
                               window_piece="Wall_UnevenBrick_Window_Thin_Round")
    children.append(piece_node(tag, "Tw_Win0", "Window_Thin_Round1", (cx - 2.35, y, cz - 1.0), yaw=90.0, out_dir=KIT))
    children.append(piece_node(tag, "Tw_Win1", "Window_Thin_Round1", (cx + 2.35, y, cz + 1.0), yaw=-90.0, out_dir=KIT))
    _, _, _, roof_ymin = PIECE["Roof_Tower_RoundTiles"]
    children.append(piece_node(tag, "Tw_Roof", "Roof_Tower_RoundTiles", (cx, y + 3.12 - roof_ymin, cz), out_dir=KIT))
    children.append(piece_node(tag, "Tw_Stairs", "Stairs_Exterior_Straight", (cx, y - 0.05, front_z + 1.3), yaw=180.0, out_dir=KIT))

    # crypt entrance: a stone portal butted against the tower's east wall
    # (wall face at x = cx + 2.0; the frame's local z spans [-0.295, 0.182],
    # so px = cx + 2.295 seats its back face flush on the wall). The leaf is
    # hinged at the arch opening's south edge, closed at yaw -90 (local +x ->
    # +z covers the ~1.2 m opening); the demo swings +90 from the closed yaw,
    # so it opens to yaw 0, perpendicular to the wall. The leaf is registry-
    # named "Crypt_Door" — the demo swaps the streamed world when it opens.
    # CryptExit anchors where the player re-appears on return.
    px, pz = cx + 2.295, cz
    py = height_at(px, pz) - 0.02
    children.append(piece_node(tag, "Crypt_Portal", "DoorFrame_Round_Brick",
                               (px, py, pz), yaw=90.0, collider=True, out_dir=KIT))
    portal = piece_node(tag, "Crypt_Door", "Door_1_Round", (px, py, pz - 0.55), yaw=-90.0, out_dir=KIT)
    add_portal(portal, "wd_crypt")
    portal["children"] = [{
        "name": "Crypt_DoorBody",
        "translation": [0.55, 1.17, 0.0],
        "components": {"RigidBody": {
            "collider": {"type": "box", "half_extents": [0.55, 1.17, 0.07]},
            "is_kinematic": True, "friction": 0.8}}}]
    children.append(portal)
    children.append(plain_node("CryptExit", pos=(px + 0.9, py + 0.05, pz)))
    return {"name": "Tower", "children": children}, _bounds[tag]


def build_crypt():
    """Small brick crypt under the watchtower — a Manual sub-level that
    demonstrates an interior streamed tile with its own navmesh layer: the
    demo loads it when the portal door opens and unloads the exterior with
    it (world swap), then reverses both on the exit door. Dim by design: the
    sun is the only light and nothing reaches underground."""
    tag = "crypt"
    children = []
    cx, cz = 0.0, -58.0          # under the tower
    y = -6.0                      # underground floor level

    # floor 8x6 m
    k = 0
    for ix in range(4):
        for iz in range(3):
            children.append(piece_node(tag, f"Cr_F_{k}", "Floor_UnevenBrick",
                                       (cx - 3 + ix * 2, y + 0.02, cz - 2 + iz * 2),
                                       collider=True, out_dir=KIT))
            k += 1
    # walls (exit door on the +z end wall, "up the stairs" toward the portal).
    # The floor/ceiling slabs span [cz-3, cz+3] x [cx-4, cx+4], so the end
    # walls sit at cz±3 and the side walls run lo=cz-3, n=3 — any other pitch
    # leaves open slits to the sky around the slab edges.
    children += wall_slots(tag, KIT, "Cr_N", "Wall_UnevenBrick_Straight", "x", cz - 3.0, cx - 4.0, y, 4)
    children += wall_slots(tag, KIT, "Crypt_Exit", "Wall_UnevenBrick_Straight", "x", cz + 3.0, cx - 4.0, y, 4,
                           door=True, door_piece="Door_1_Round")
    for sx in (-1, 1):
        children += wall_slots(tag, KIT, f"Cr_S{sx}", "Wall_UnevenBrick_Straight", "z", cx + sx * 4.0, cz - 3.0, y, 3,
                               window_slot=1, window_piece="Wall_UnevenBrick_Window_Thin_Round")
    # ceiling (floor pieces as the vault) + props + spawn anchor
    k = 0
    for ix in range(4):
        for iz in range(3):
            children.append(piece_node(tag, f"Cr_Ceil_{k}", "Floor_UnevenBrick",
                                       (cx - 3 + ix * 2, y + 3.14, cz - 2 + iz * 2),
                                       collider=True, out_dir=KIT))
            k += 1
    children.append(piece_node(tag, "Cr_Crate0", "Prop_Crate", (cx - 2.5, y, cz - 1.5), yaw=15.0, collider=True, out_dir=KIT))
    children.append(piece_node(tag, "Cr_Crate1", "Prop_Crate", (cx - 1.6, y, cz - 1.9), yaw=-30.0, collider=True, out_dir=KIT))
    children.append(piece_node(tag, "Cr_Bricks", "Prop_Brick1", (cx + 3.0, y, cz - 2.0), collider=True, out_dir=KIT))
    children.append(piece_node(tag, "Cr_Support", "Prop_Support", (cx + 2.5, y, cz + 2.0), yaw=100.0, collider=True, out_dir=KIT))
    children.append(plain_node("CryptSpawn", pos=(cx, y + 0.05, cz + 2.2)))
    return {"name": "Crypt", "children": children}, _bounds[tag]


# ---------------------------------------------------------------------------
# NPC templates (.sesc -> template_export -> .setm)
# ---------------------------------------------------------------------------

def npc_template(name, outfit, bb_asset, initial, bt_asset):
    # The Outfits exports are clothing-only (no head mesh — 'Head' is a bare
    # joint) and their hoods are cloth-only shells. Two graft sources fill the
    # head, both on the identical 65-joint skeleton so one Armature drives
    # everything: the skinned Ranger hood parts (headwear) and the Universal
    # Base Superhero FullBody parts — the head skin lives inside the body mesh
    # there, with separate Eyes/Eyebrows nodes for the face. The Male_Ranger
    # outfit already carries its own hood, so it skips the hood graft.
    sex = "Male" if outfit.startswith("Male") else "Female"
    base_scene = f"{NPC}/Superhero_{sex}_FullBody.sesc"
    body_node = "SuperHero_Male" if sex == "Male" else "Superhero_Female"

    def base_graft(node_name, graft_name):
        return {"name": graft_name,
                "from": {"scene": base_scene,
                         "node": f"RootNode|Armature|{node_name}"}}

    head_parts = [base_graft(body_node, "Head_Base"),
                  base_graft("Eyes", "Head_Eyes"),
                  base_graft("Eyebrows", "Head_Brows")]

    hood = None
    if outfit != "Male_Ranger":
        hood = {
            "name": "Head_Hood",
            "from": {"scene": f"{NPC}/{sex}_Ranger_Head_Hood.sesc",
                     "node": f"RootNode|Armature|{sex}_Ranger_Head_Hood"},
        }
    armature_children = ([hood] if hood else []) + head_parts
    return {
        "version": 1,
        "output": f"{GEN_DIR}/wd_{name}.sesc",
        "nodes": [{
            "name": name,
            "components": {
                "NavAgent": {"agent_class": "default", "avoidance": True},
                "PathFollower": {"arrival_radius": 0.45, "slow_radius": 2.2, "repath_interval": 0.6},
                "Blackboard": {"bb": f"{AI_DIR}/{bb_asset}.sebb", "initial": initial},
                "BehaviorTree": {"tree": f"{AI_DIR}/bt_{bt_asset}.sebt", "tick_interval": 0.2},
            },
            "children": [{
                # kinematic capsule so the player cannot walk through NPCs:
                # raised 1.0 m (node origin is at the feet, the capsule spans
                # 2*(0.65+0.35) = 2.0 m centred on the child node); the body
                # follows the crowd-driven node via the kinematic listener
                "name": "Physics",
                "translation": [0.0, 1.0, 0.0],
                "components": {"RigidBody": {
                    "collider": {"type": "capsule", "radius": 0.35, "half_height": 0.65},
                    "is_kinematic": True}},
            }, {
                "name": "Armature",
                "from": {"scene": f"{NPC}/{outfit}.sesc", "node": "RootNode|Armature"},
                "children": armature_children,
            }],
        }],
        "overrides": [{
            "node": f"{name}|Armature|{outfit}_Body",
            "components": {"Animator": {"animation_graph": ANIM_GRAPH}},
        }],
    }


# ---------------------------------------------------------------------------
# World JSONs + nav_bake config
# ---------------------------------------------------------------------------

def write_json(path, data):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        json.dump(data, f, indent=2)
        f.write("\n")
    print(f"wrote {path}")


def main():
    os.makedirs(GEN_DIR, exist_ok=True)

    manifests = {}
    manifests["wd_village"], b_village = build_village()
    manifests["wd_inn_interior"], b_inn = build_inn_interior()
    manifests["wd_forest_w"], b_fw = build_forest_w()
    manifests["wd_forest_e"], b_fe = build_forest_e()
    manifests["wd_farm"], b_farm = build_farm()
    manifests["wd_tower"], b_tower = build_tower()
    manifests["wd_crypt"], b_crypt = build_crypt()

    # Tile-aligned, non-overlapping sub-level boxes. The nav bake merges world
    # terrain tiles (40 m) into the sub-level that contains them and claims
    # 20 m bake tiles by these bounds — 40-aligned boxes keep both grids
    # conflict-free (see artifacts/gameplay_systems_stage_3_step_11_001.md).
    BOXES = {
        "wd_village":       [-40.0, 40.0, -40.0, 40.0],
        "wd_forest_w":      [-120.0, -40.0, -80.0, 80.0],
        "wd_forest_e":      [40.0, 120.0, -80.0, 80.0],
        "wd_farm":          [-40.0, 40.0, 40.0, 80.0],
        "wd_tower":         [-40.0, 40.0, -80.0, -40.0],
    }

    def box_for(name, b):
        if name in BOXES:
            x0, x1, z0, z1 = BOXES[name]
            # y spans the terrain relief (bake heights 0..33) plus structures
            return [x0, 0.0, z0], [x1, 36.0, z1]
        m = 1.0
        return [round(b[0][0] - m, 2), round(b[0][1] - 0.5, 2), round(b[0][2] - m, 2)], \
               [round(b[1][0] + m, 2), round(b[1][1] + 2.0, 2), round(b[1][2] + m, 2)]

    # scene manifest keys already carry the wd_ prefix — output mirrors them
    for name, tree in manifests.items():
        write_json(f"{GEN_DIR}/{name}.seassembly",
                   {"version": 1, "output": f"{GEN_DIR}/{name}.sesc", "nodes": [tree]})

    # NPC templates
    npc_specs = [
        ("villager_a", "Male_Peasant", "bb_villager",
         [{"key": "wp_a", "vec3": [-20.0, 2.0, 0.0]}, {"key": "wp_b", "vec3": [16.0, 2.0, 0.0]}],
         "villager_patrol"),
        ("villager_b", "Female_Peasant", "bb_villager",
         [{"key": "wp_a", "vec3": [0.0, 2.0, -12.0]}, {"key": "wp_b", "vec3": [0.0, 2.0, 10.0]}],
         "villager_patrol"),
        ("farmer", "Female_Peasant", "bb_villager",
         [{"key": "wp_a", "vec3": [-4.0, 2.4, 54.0]}, {"key": "wp_b", "vec3": [8.0, 2.4, 66.0]}],
         "villager_patrol"),
        ("guard", "Male_Ranger", "bb_guard",
         [{"key": "post", "vec3": [-76.0, 2.0, 4.0]},
          {"key": "post_a", "vec3": [-79.0, 2.0, 1.0]},
          {"key": "post_b", "vec3": [-72.0, 2.0, 0.5]}],
         "camp_guard"),
    ]
    for name, outfit, bb, initial, bt in npc_specs:
        write_json(f"{GEN_DIR}/wd_{name}.seassembly", npc_template(name, outfit, bb, initial, bt))

    # --- world JSONs ---
    sub_levels = [
        {"path": f"{GEN_DIR}/wd_village.sesc",
         "bounds_min": box_for("wd_village", b_village)[0],
         "bounds_max": box_for("wd_village", b_village)[1],
         "layer": "core"},
        {"path": f"{GEN_DIR}/wd_inn_interior.sesc",
         "bounds_min": box_for("wd_inn_interior", b_inn)[0],
         "bounds_max": box_for("wd_inn_interior", b_inn)[1],
         "flags": 2 | 8, "layer": "interior"},
        {"path": f"{GEN_DIR}/wd_crypt.sesc",
         "bounds_min": box_for("wd_crypt", b_crypt)[0],
         "bounds_max": box_for("wd_crypt", b_crypt)[1],
         "flags": 2 | 8, "layer": "interior"},
        {"path": f"{GEN_DIR}/wd_forest_w.sesc",
         "bounds_min": box_for("wd_forest_w", b_fw)[0],
         "bounds_max": box_for("wd_forest_w", b_fw)[1],
         "layer": "wilderness"},
        {"path": f"{GEN_DIR}/wd_forest_e.sesc",
         "bounds_min": box_for("wd_forest_e", b_fe)[0],
         "bounds_max": box_for("wd_forest_e", b_fe)[1],
         "layer": "wilderness"},
        {"path": f"{GEN_DIR}/wd_farm.sesc",
         "bounds_min": box_for("wd_farm", b_farm)[0],
         "bounds_max": box_for("wd_farm", b_farm)[1],
         "layer": "core"},
        {"path": f"{GEN_DIR}/wd_tower.sesc",
         "bounds_min": box_for("wd_tower", b_tower)[0],
         "bounds_max": box_for("wd_tower", b_tower)[1],
         "layer": "core"},
    ]

    streaming_block = {
        "enabled": True, "cell_size": 40.0,
        # 96/140: distant statics (houses, fences) must outlive the terrain's
        # LOD rings — at 55/85 buildings popped while terrain still drew to
        # the horizon, which read as a rendering bug.
        "load_radius": 96.0, "unload_radius": 140.0,
        "max_activations_per_frame": 1, "main_budget_ms": 2.0,
        "unload_frame_delay": 60,
    }
    common = {
        "id": "world_demo", "version": 1,
        "sub_levels": sub_levels,
        "initial_focus": {"x": 0.0, "y": 2.0, "z": 0.0},
        "seed": WORLD_SEED,
        "terrain_grid": {"manifest": "resource/terrain/world_demo/world_demo.seterrain"},
        "nav": {
            "meshes": [{"agent_class": "default",
                        "mesh": {"path": "resource/nav/world_demo_default.senm"}}],
            "default_class": "default",
        },
        "global_blackboard": {"path": f"{AI_DIR}/bb_world.sebb"},
    }
    write_json("resource/world/world_demo.json", dict(common, streaming=streaming_block))
    write_json("resource/world/world_demo_no_streaming.json",
               dict(common, id="world_demo_no_streaming",
                    streaming=dict(streaming_block, enabled=False)))

    # --- nav_bake config ---
    write_json("resource/nav/world_demo.json", {
        "world": "resource/world/world_demo.seworld",
        "classes": [{"name": "default", "radius": 0.4, "height": 1.8,
                     "max_slope_deg": 50.0, "step_height": 0.4}],
        "tile_dim": 20.0, "cell_size": 40.0,
        "voxel_size": 0.2, "voxel_height": 0.2,
        "output": "resource/nav/world_demo",
    })

    print("build_village: manifests + world configs generated")


if __name__ == "__main__":
    main()
