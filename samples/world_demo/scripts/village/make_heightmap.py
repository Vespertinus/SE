#!/usr/bin/env python3
"""Deterministic 16-bit grayscale PNG heightmap for the world_demo terrain.

Output: resource/terrain/world_demo_src.png — 513x513 px covering a 640x640 m
area (16x16 tiles of 40 m, origin -320,-320). Standard authoring-tool exchange
format: pixel value is NORMALIZED (0..65535), meters = value * 1/1024
(see "height_scale" in resource/terrain/world_demo.json).

Layout (world coords, x east, z south):
  - flat village plateau   center (0,0), radius 55 m at +2 m
  - farm plain             center (0,+62), radius 34 m at +2.4 m
  - stream valley          along x ~ -42 (between village and west forest)
  - tower knoll            center (0,-62), radius 20 m at +9 m
  - hill rim               rises to +20 m near the map edge (natural boundary)
  - rolling FBM hills elsewhere (hash-based value noise, seed fixed)

A hand-authored heightmap replaces this file by dropping a png16 at the same
path (normalized 0..65535, "height_scale" in the bake config maps it to
meters) and re-running scripts/village/build_all.sh terrain.
"""

import math
import struct
import sys
import zlib

SIZE   = 513          # square grid samples (512 intervals = 16x16 tiles of 32 cells)
SPAN   = 640.0        # meters covered by the map (16x16 tiles of 40 m)
WORLD0 = -SPAN / 2.0  # world coordinate of pixel (0,0)

# normalized 16-bit storage: meters = raw / 1024 (H_MAX = 63.999 m full scale)
H_SCALE_INV = 1024

PLATEAU_H   = 2.0
PLATEAU_R   = 55.0
PLATEAU_BLEND = 60.0  # plateau mask falls off over [R, R+BLEND]
FARM_C      = (0.0, 62.0)
FARM_R      = 34.0
FARM_H      = 2.4
TOWER_C     = (0.0, -62.0)
TOWER_R     = 20.0
TOWER_H     = 9.0
STREAM_X    = -42.0
STREAM_W    = 14.0     # gaussian width
STREAM_DEPTH = 3.5     # carve depth below the local base
RIM_START   = 190.0
RIM_END     = 260.0
RIM_H       = 20.0
NOISE_AMP   = 2.4
NOISE_AMP_EDGE = 6.0
SEED        = 1147


def hash2(xi: int, yi: int, seed: int) -> float:
    """Deterministic value noise lattice hash -> [-1, 1]."""
    h = (seed & 0xFFFFFFFFFFFFFFFF)
    h ^= (xi + 0x9E3779B97F4A7C15 + (h << 6) + (h >> 2)) & 0xFFFFFFFFFFFFFFFF
    h ^= (yi + 0xBF58476D1CE4E5B9 + (h << 11) + (h >> 5)) & 0xFFFFFFFFFFFFFFFF
    h = (h * 0x94D049BB133111EB) & 0xFFFFFFFFFFFFFFFF
    h ^= h >> 31
    return (h & 0xFFFFFF) / float(0xFFFFFF) * 2.0 - 1.0


def smooth(t: float) -> float:
    return t * t * (3.0 - 2.0 * t)


def value_noise(u: float, v: float, freq: float, octave: int) -> float:
    fx, fy = u * freq, v * freq
    x0, y0 = math.floor(fx), math.floor(fy)
    tx, ty = smooth(fx - x0), smooth(fy - y0)
    s = SEED + octave * 7919
    n00 = hash2(x0, y0, s)
    n10 = hash2(x0 + 1, y0, s)
    n01 = hash2(x0, y0 + 1, s)
    n11 = hash2(x0 + 1, y0 + 1, s)
    a = n00 + (n10 - n00) * tx
    b = n01 + (n11 - n01) * tx
    return a + (b - a) * ty


def fbm(u: float, v: float, octaves: int = 5, frequency: float = 6.0) -> float:
    noise, amp, amp_sum = 0.0, 1.0, 0.0
    for o in range(octaves):
        noise += value_noise(u, v, frequency * (2.0 ** o), o) * amp
        amp_sum += amp
        amp *= 0.5
    return noise / amp_sum


def clamp01(t: float) -> float:
    return 0.0 if t < 0.0 else (1.0 if t > 1.0 else t)


def smoothstep(e0: float, e1: float, t: float) -> float:
    return smooth(clamp01((t - e0) / (e1 - e0)))


def height_at(wx: float, wz: float) -> float:
    dist_c = math.hypot(wx, wz)
    u, v = wx / SPAN, wz / SPAN

    # plateau / farm / tower masks (1 = fully flat at feature height)
    m_village = 1.0 - smoothstep(PLATEAU_R, PLATEAU_R + PLATEAU_BLEND, dist_c)
    m_farm    = 1.0 - smoothstep(FARM_R, FARM_R + 26.0, math.hypot(wx - FARM_C[0], wz - FARM_C[1]))
    m_tower   = 1.0 - smoothstep(TOWER_R, TOWER_R + 18.0, math.hypot(wx - TOWER_C[0], wz - TOWER_C[1]))

    # stream channel: meandering carve, suppressed where masks are high
    stream_x = STREAM_X + 8.0 * math.sin(wz / 55.0 + 1.3)
    d_stream = abs(wx - stream_x)
    m_stream = math.exp(-(d_stream * d_stream) / (2.0 * STREAM_W * STREAM_W))

    base = PLATEAU_H
    base = base * (1.0 - m_farm) + FARM_H * m_farm
    base = base * (1.0 - m_tower) + TOWER_H * m_tower

    # rolling hills: gentle inside the play area, stronger toward the rim
    edge_t   = smoothstep(RIM_START * 0.45, RIM_START, dist_c)
    amp      = NOISE_AMP + (NOISE_AMP_EDGE - NOISE_AMP) * edge_t
    hills    = fbm(u, v) * amp

    # hill rim at the map edge
    rim = smoothstep(RIM_START, RIM_END, dist_c) * RIM_H

    h = base + hills * (1.0 - m_village) * (1.0 - m_farm) * (1.0 - 0.6 * m_tower)
    h += rim
    h -= STREAM_DEPTH * m_stream * (1.0 - m_village)

    return max(0.05, h)


def write_png16(path: str, size: int, heights):
    """16-bit grayscale PNG, filter 0 scanlines, zlib-compressed."""
    def chunk(tag: bytes, data: bytes) -> bytes:
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))

    raw = bytearray()
    for i in range(size):
        raw.append(0)  # filter: none
        for j in range(size):
            # normalized storage: meters * 1024 -> 0..65535 (~1 mm resolution)
            h = heights[i * size + j] * H_SCALE_INV
            h = max(0.0, min(65535.0, h))
            raw += struct.pack(">H", int(round(h)))

    ihdr = struct.pack(">IIBBBBB", size, size, 16, 0, 0, 0, 0)
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", ihdr)
           + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
           + chunk(b"IEND", b""))
    with open(path, "wb") as f:
        f.write(png)


def main() -> int:
    out = sys.argv[1] if len(sys.argv) > 1 else "resource/terrain/world_demo_src.png"
    step = SPAN / (SIZE - 1)
    heights = [0.0] * (SIZE * SIZE)
    for i in range(SIZE):
        wz = WORLD0 + i * step
        for j in range(SIZE):
            heights[i * SIZE + j] = height_at(WORLD0 + j * step, wz)
    write_png16(out, SIZE, heights)

    lo = min(heights)
    hi = max(heights)
    center = height_at(0.0, 0.0)
    print(f"wrote {out}: {SIZE}x{SIZE}, heights {lo:.2f}..{hi:.2f} m, "
          f"village center {center:.2f} m")
    return 0


if __name__ == "__main__":
    sys.exit(main())
