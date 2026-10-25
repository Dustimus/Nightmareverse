#!/usr/bin/env python3
"""RE1's enemy models (ENEMY/EM10xx.EMD, emd.py) as SoH resources, for soh/src/code/z_nv_enemy.c.

Each enemy type used by the converted rooms becomes "re1/enemies/em<type>", a binary blob:

    "RENM", u16 numBones, u16 numBanks
    per bone: s16 x, y, z (offset from its parent), s16 parent (-1: the root)
    per bank (RE1's two animation sets: 0 the model's main one, 1 its extra one):
        u16 numFrames, then per frame s16 rootOffset[3], rootSpeed[3], angles[3 * numBones]
        (4096 = 360 degrees, rotated X then Y then Z: the GTE's RotMatrix)
        u16 numAnims, then per anim u16 numSteps and its steps as u16 frame, u16 flags
    u16 texture width, height in 64x32 cells, then the cells (RGBA5551, big-endian; the
    PSX's colour 0x0000 is transparent)
    per bone: u16 numBatches; per batch u16 cell, u16 numVerts, then numVerts Vtx (s16 x, y,
    z, u16 flag, s16 s, t (10.5, in the cell), u8 rgba): the bone's triangles cut to the cells

Models stay in RE1's units and axes (y down); z_nv_enemy.c places them in the room.

    python3 re1/tools/soh_enemy.py --re1 <RE1>/JPN --soh <folder> 17 ...
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import emd  # noqa: E402
import rdt  # noqa: E402
import soh_doors  # noqa: E402
import soh_scene  # noqa: E402

MAGIC = b"RENM"


def enemy_file(re1, enemy_type):
    """RE1's model for an enemy type (EntityModelLoader's path table: em10xx, xx the type)."""
    d = rdt.find_file(re1, "ENEMY")
    return rdt.find_file(d, "EM10%02X.EMD" % enemy_type)


def bank(skeleton, anims):
    out = bytearray(struct.pack("<H", len(skeleton.frames)))
    for f in skeleton.frames:
        vals = list(f.root_offset) + list(f.root_speed) + [a for ang in f.angles for a in ang]
        out += struct.pack("<%dh" % len(vals), *vals)
    out += struct.pack("<H", len(anims))
    for a in anims:
        out += struct.pack("<H", len(a.steps))
        for frame, flags in a.steps:
            out += struct.pack("<2H", frame, flags & 0xFFFF)
    return bytes(out)


def enemy_blob(path):
    e = emd.load(path)
    sk = e.skeleton
    n = len(sk.relpos)
    out = bytearray(MAGIC + struct.pack("<HH", n, 2 if e.extra_skeleton else 1))
    for i in range(n):
        out += struct.pack("<4h", *sk.relpos[i], sk.parents[i])
    out += bank(sk, e.animations)
    if e.extra_skeleton:
        out += bank(e.extra_skeleton, e.extra_animations)

    w, h, rgba = emd.tim_to_rgba(e.tim, emd.page_cluts(e))
    cw, ch = (w + soh_doors.CELL_W - 1) // soh_doors.CELL_W, (h + soh_doors.CELL_H - 1) // soh_doors.CELL_H
    out += struct.pack("<HH", cw, ch)
    for cy in range(ch):
        for cx in range(cw):
            for y in range(soh_doors.CELL_H):
                for x in range(soh_doors.CELL_W):
                    px, py = cx * soh_doors.CELL_W + x, cy * soh_doors.CELL_H + y
                    if px < w and py < h:
                        r, g, b, a = rgba[(py * w + px) * 4:(py * w + px) * 4 + 4]
                        c = ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | (1 if a else 0)
                    else:
                        c = 0
                    out += struct.pack(">H", c)

    for i in range(n):
        obj = e.objects[i] if i < len(e.objects) else emd.MeshObject([], [], [])
        cells = soh_doors.model_cells(obj, cw, ch)
        out += struct.pack("<H", len(cells))
        for cell, tris in sorted(cells.items()):
            out += struct.pack("<HH", cell, 3 * len(tris))
            for t in tris:
                for x, y, z, s, tt in t:
                    out += struct.pack("<3hH2h4B", round(x), round(y), round(z), 0,
                                       round(s * 32), round(tt * 32), 255, 255, 255, 255)
    return bytes(out)


def write(re1, out, types):
    """Write the models for these enemy types into the SoH folder; returns the ones written."""
    done = []
    d = os.path.join(out, "re1", "enemies")
    os.makedirs(d, exist_ok=True)
    for t in sorted(set(types)):
        path = enemy_file(re1, t)
        if not os.path.exists(path):
            print("enemy %d: no model file, left out" % t)
            continue
        blob = enemy_blob(path)
        open(os.path.join(d, "em%d" % t), "wb").write(
            soh_scene._otr_header(soh_scene.LUS_BLOB) + struct.pack("<I", len(blob)) + blob)
        done.append(t)
    return done


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--re1", required=True, help="RE1 data directory (JPN, or USA in the GOG release)")
    ap.add_argument("--soh", required=True, help="SoH resource folder (rdt2scene.py --soh)")
    ap.add_argument("type", type=int, nargs="+")
    a = ap.parse_args()
    print("enemies:", write(a.re1, a.soh, a.type))


if __name__ == "__main__":
    main()
