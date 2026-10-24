#!/usr/bin/env python3
"""RE1's door scenes (ITEM_M1/*.DOR, dor.py) as SoH resources, for soh/src/code/z_nv_doorscene.c.

RE1 covers every room change with a short 3D scene: the door swinging open against black as
the camera moves through. Each one used by the converted rooms becomes "re1/doors/door_<n>"
(n: Door_set's animation index), a binary blob:

    "RDSC", u16 numRoutines, u16 scriptSize, u16 routine offsets[numRoutines], script bytes
    (RE1's door bytecode, the routines back to back; dor.py documents it)
    u16 texture width, height in 64x32 cells, then the cells (64x32 RGBA5551, big-endian
    as libultraship reads textures; the PSX's colour 0 is transparent)
    u16 numModels; per model: u16 numBatches; per batch: u16 cell, u16 numVerts, then
    numVerts Vtx (s16 x, y, z, u16 flag, s16 s, t (10.5, in the cell), u8 rgba), three per
    triangle: the model's triangles cut to the cells they cover

Models stay in RE1's units and axes (y down); the scene's camera is in the same space.
The cells keep each texture load within TMEM's 4 KB, as on the N64 (mod1's re_door.py).

    python3 re1/tools/soh_doors.py --re1 <RE1>/JPN --soh <folder> 3 22 ...
"""
import argparse
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dor  # noqa: E402
import rdt  # noqa: E402
import soh_scene  # noqa: E402

CELL_W, CELL_H = 64, 32
MAGIC = b"RDSC"

# RE1's door file table (Door_set's animation index -> file in ITEM_M1)
DOOR_FILES = (["door%02d.dor" % i for i in range(15)] +
              ["mon.dor", "ele03.dor", "ele01.dor", "ele01a.dor", "ele01b.dor", "ele02.dor", "ele04.dor",
               "kai01.dor", "kai03.dor", "kai02.dor", "kai04.dor", "lad01.dor", "lad02.dor",
               "door00k.dor", "door01k.dor", "door03k.dor", "door05k.dor", "door06k.dor", "door15.dor"])


def find_file(re1, anim):
    d = rdt.find_file(re1, "ITEM_M1")  # Item_m1 in the GOG release
    if not os.path.isdir(d):  # a partial install: the doors keep the plain fade
        return None
    want = DOOR_FILES[anim].upper()
    for fn in os.listdir(d):
        if fn.upper() == want:
            return os.path.join(d, fn)
    return None


def rgba5551(c):
    """PSX 15-bit colour -> RGBA5551; 0x0000 is the PSX's transparent colour."""
    if c == 0:
        return 0
    r, g, b = c & 31, (c >> 5) & 31, (c >> 10) & 31
    return (r << 11) | (g << 6) | (b << 1) | 1


def clip(poly, axis, value, keep_greater):
    """Sutherland-Hodgman against one texture-space line. poly: [(x, y, z, u, v)]."""
    out = []
    for i, a in enumerate(poly):
        b = poly[(i + 1) % len(poly)]
        ina = (a[axis] >= value) if keep_greater else (a[axis] <= value)
        inb = (b[axis] >= value) if keep_greater else (b[axis] <= value)
        if ina:
            out.append(a)
        if ina != inb:
            t = (value - a[axis]) / (b[axis] - a[axis])
            out.append(tuple(a[k] + (b[k] - a[k]) * t for k in range(5)))
    return out


def area_uv(p):
    return abs(sum(p[i][3] * p[(i + 1) % len(p)][4] - p[(i + 1) % len(p)][3] * p[i][4] for i in range(len(p)))) / 2


def model_cells(obj, cells_w, cells_h):
    """{cell index: [triangles as 3 (x, y, z, s, t), s and t relative to the cell]}"""
    cells = {}
    for tri in obj.triangles:
        poly0 = [tuple(obj.vertices[v]) + tuple(uv) for v, uv in zip(tri.verts, tri.uvs)]
        us = [p[3] for p in poly0]
        vs = [p[4] for p in poly0]
        for cy in range(max(0, int(min(vs)) // CELL_H), min(cells_h - 1, int(max(vs)) // CELL_H) + 1):
            for cx in range(max(0, int(min(us)) // CELL_W), min(cells_w - 1, int(max(us)) // CELL_W) + 1):
                x0, y0 = cx * CELL_W, cy * CELL_H
                p = poly0
                for axis, value, g in ((3, x0, True), (3, x0 + CELL_W, False), (4, y0, True), (4, y0 + CELL_H, False)):
                    if p:
                        p = clip(p, axis, value, g)
                if len(p) < 3 or area_uv(p) < 0.01:
                    continue
                for k in range(1, len(p) - 1):  # fan
                    cells.setdefault(cy * cells_w + cx, []).append(
                        [(q[0], q[1], q[2], q[3] - x0, q[4] - y0) for q in (p[0], p[k], p[k + 1])])
    return cells


def door_blob(path):
    D = dor.load(path)
    script = b"".join(D.routines)
    offs, o = [], 0
    for r in D.routines:
        offs.append(o)
        o += len(r)
    out = bytearray(MAGIC + struct.pack("<HH", len(offs), len(script)))
    out += struct.pack("<%dH" % len(offs), *offs) + script
    if len(out) % 2:
        out += b"\0"

    tim = D.tim
    w, h = tim.width, tim.height
    cw, ch = (w + CELL_W - 1) // CELL_W, (h + CELL_H - 1) // CELL_H
    pal = [rgba5551(c) for c in tim.cluts[0]]
    out += struct.pack("<HH", cw, ch)
    for cy in range(ch):
        for cx in range(cw):
            for y in range(CELL_H):
                for x in range(CELL_W):
                    px, py = cx * CELL_W + x, cy * CELL_H + y
                    c = pal[tim.pixels[py * w + px]] if px < w and py < h else 0
                    out += struct.pack(">H", c)

    out += struct.pack("<H", len(D.objects))
    for obj in D.objects:
        cells = model_cells(obj, cw, ch)
        out += struct.pack("<H", len(cells))
        for cell, tris in sorted(cells.items()):
            out += struct.pack("<HH", cell, 3 * len(tris))
            for t in tris:
                for x, y, z, s, tt in t:
                    out += struct.pack("<3hH2h4B", round(x), round(y), round(z), 0,
                                       round(s * 32), round(tt * 32), 255, 255, 255, 255)
    return bytes(out)


def write(re1, out, anims):
    """Write the door scenes for these animation indexes into the SoH folder; returns the ones written."""
    done = []
    d = os.path.join(out, "re1", "doors")
    os.makedirs(d, exist_ok=True)
    for anim in sorted(set(anims)):
        path = find_file(re1, anim) if anim < len(DOOR_FILES) else None
        if path is None:
            print("door scene %d: no file, left out (the plain cut)" % anim)
            continue
        blob = door_blob(path)
        open(os.path.join(d, "door_%d" % anim), "wb").write(
            soh_scene._otr_header(soh_scene.LUS_BLOB) + struct.pack("<I", len(blob)) + blob)
        done.append(anim)
    return done


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--re1", required=True, help="RE1 JPN directory")
    ap.add_argument("--soh", required=True, help="SoH resource folder (rdt2scene.py --soh)")
    ap.add_argument("anim", type=int, nargs="+")
    a = ap.parse_args()
    print("door scenes:", write(a.re1, a.soh, a.anim))


if __name__ == "__main__":
    main()
