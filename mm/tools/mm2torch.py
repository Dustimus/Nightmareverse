"""Torch configs for MM scenes, worked out from the player's own ROM.

SoH's extractor (Torch) only exports what a config declares: every display list, vertex array
and texture, with its format and size (SoH's own OoT configs were generated that way from ZAPD).
This walks an MM scene's room meshes the same way ZAPD does and writes those declarations, so
Torch can export MM's rooms in the formats SoH already loads.

    python mm/tools/mm2torch.py <rom> <out yml dir> [scene ...]   (default: Clock Town)

Writes <scene>.yml (what the rooms use from the scene file, segment 2) and
<scene>_room_NN.yml (the room, segment 3), all under resource paths mm/scenes/<scene>_scene/.
Only offsets, sizes and formats are written: no game data.
"""

import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mm_rom  # noqa: E402
import yaz0  # noqa: E402

FMT = {0: "RGBA", 2: "CI", 3: "IA", 4: "I"}
SIZ = {0: 4, 1: 8, 2: 16, 3: 32}
G_VTX, G_DL, G_ENDDL = 0x01, 0xDE, 0xDF
G_SETTIMG, G_SETTILE, G_SETTILESIZE = 0xFD, 0xF5, 0xF2
G_LOADBLOCK, G_LOADTILE, G_LOADTLUT = 0xF3, 0xF4, 0xF0
G_MTX, G_RDPHALF_1 = 0xDA, 0xE1  # (RDPHALF_1 carries G_BRANCH_Z's display list)


class Assets:
    """What one file (one segment) needs declared."""

    def __init__(self, seg, data):
        self.seg = seg
        self.data = data
        self.dls = set()
        self.vtx = []          # (start, end) byte ranges
        self.tex = {}          # offset -> (format, width, height, tlut offset or None)
        self.tlut = {}         # offset -> colour count
        self.mtx = set()

    def vtx_arrays(self):
        """Touching or overlapping vertex loads merged into arrays (as ZAPD does)."""
        out = []
        for s, e in sorted(self.vtx):
            if out and s <= out[-1][1]:
                out[-1][1] = max(out[-1][1], e)
            else:
                out.append([s, e])
        return out


class Walker:
    def __init__(self, files):
        self.files = files      # segment -> Assets
        self.seen = set()
        self.other_segments = set()

    def target(self, addr):
        seg, off = addr >> 24, addr & 0xFFFFFF
        if seg not in self.files:
            self.other_segments.add(seg)
            return None, off
        return self.files[seg], off

    def walk(self, addr):
        a, off = self.target(addr)
        if a is None or addr in self.seen:
            return
        self.seen.add(addr)
        a.dls.add(off)
        timg = None             # (Assets, offset, fmt, siz) of the last SETTIMG
        pending = []            # textures loaded, waiting for their render tile's format and size
        tile_fmt = {}
        last_tlut = None
        p = off
        while p + 8 <= len(a.data):
            w0, w1 = struct.unpack_from(">II", a.data, p)
            op = w0 >> 24
            p += 8
            if op == G_VTX:
                n = (w0 >> 12) & 0xFF
                va, vo = self.target(w1)
                if va is not None:
                    va.vtx.append((vo, vo + 16 * n))
            elif op == G_DL:
                self.walk(w1)
                if (w0 >> 16) & 0xFF:   # a branch: this list ends here
                    break
            elif op == G_ENDDL:
                break
            elif op == G_RDPHALF_1:
                if (w1 >> 24) in self.files:
                    self.walk(w1)
            elif op == G_MTX:
                ma, mo = self.target(w1)
                if ma is not None:
                    ma.mtx.add(mo)
            elif op == G_SETTIMG:
                ta, to = self.target(w1)
                timg = (ta, to) if ta is not None else None
            elif op == G_LOADTLUT:
                if timg:
                    count = ((w1 >> 14) & 0x3FF) + 1
                    timg[0].tlut[timg[1]] = count
                    last_tlut = timg
                    for t in pending:
                        if t[2] is None:
                            t[2] = last_tlut
            elif op in (G_LOADBLOCK, G_LOADTILE):
                if timg:
                    pending.append([timg[0], timg[1], None, None])
            elif op == G_SETTILE:
                tile = (w1 >> 24) & 7
                if tile != 7:
                    tile_fmt[tile] = ((w0 >> 21) & 7, (w0 >> 19) & 3)
            elif op == G_SETTILESIZE:
                tile = (w1 >> 24) & 7
                if tile != 7 and pending and tile in tile_fmt:
                    fmt, siz = tile_fmt[tile]
                    w = ((w1 >> 12) & 0xFFF) // 4 - ((w0 >> 12) & 0xFFF) // 4 + 1
                    h = (w1 & 0xFFF) // 4 - (w0 & 0xFFF) // 4 + 1
                    ta, to, tl, _ = pending.pop(0)
                    if FMT.get(fmt) == "CI" and tl is None:
                        tl = last_tlut
                    tlut = tl[1] if (tl and tl[0] is ta) else None
                    if fmt in FMT:
                        ta.tex.setdefault(to, (f"{FMT[fmt]}{SIZ[siz]}", w, h, tlut))


def commands(data, off):
    p = off
    while p + 8 <= len(data):
        w0, w1 = struct.unpack_from(">II", data, p)
        yield w0 >> 24, (w0 >> 16) & 0xFF, w1
        if w0 >> 24 == 0x14:
            return
        p += 8


def room_headers(data):
    """The room's command lists: the main one and its alternates (0x18)."""
    lists = [0]
    for cmd, _, w1 in commands(data, 0):
        if cmd == 0x18:
            p = w1 & 0xFFFFFF
            # Pointers until one that isn't null or into this file (0x03......); a null
            # one means "same as the main header".
            while p + 4 <= len(data):
                ptr = struct.unpack_from(">I", data, p)[0]
                if ptr != 0 and (ptr >> 24 != 3 or (ptr & 0xFFFFFF) >= len(data)):
                    break
                if ptr:
                    lists.append(ptr & 0xFFFFFF)
                p += 4
    return lists


def mesh_dls(data, off):
    """Display lists of a room mesh header (0x0A): types 0 (plain) and 2 (cullable)."""
    kind, count = data[off], data[off + 1]
    start = struct.unpack_from(">I", data, off + 4)[0] & 0xFFFFFF
    out = []
    if kind == 0:
        for i in range(count):
            out += struct.unpack_from(">II", data, start + 8 * i)
    elif kind == 2:
        for i in range(count):
            out += struct.unpack_from(">II", data, start + 16 * i + 8)
    else:
        raise ValueError(f"room mesh type {kind} not handled yet")
    return [d for d in out if d]


def mm_scene_collision(sdata):
    for cmd, _, w1 in commands(sdata, 0):
        if cmd == 0x03:
            return w1 & 0xFFFFFF
    raise ValueError("scene has no collision")


def yml_assets(a, prefix, header, names=None):
    """Torch declarations for one file's assets. `names` gives some display lists their own
    symbols (an object's entry points, by the decomp's names), the rest are named by offset."""
    names = names or {}
    lines = [header]
    for off in sorted(a.dls):
        sym = names.get(off, f"{prefix}DL_{off:06X}")
        lines += [f"{sym}:", "  type: GFX", f"  offset: 0x{off:06X}", f"  symbol: {sym}", ""]
    for off in sorted(a.mtx):
        lines += [f"{prefix}Mtx_{off:06X}:", "  type: OOT:MTX", f"  offset: 0x{off:06X}",
                  f"  symbol: {prefix}Mtx_{off:06X}", ""]
    for s, e in a.vtx_arrays():
        lines += [f"{prefix}Vtx_{s:06X}:", "  type: OOT:ARRAY", f"  offset: 0x{s:06X}",
                  f"  symbol: {prefix}Vtx_{s:06X}", f"  count: {(e - s) // 16}",
                  "  array_type: VTX", "  zero_flag: true", "  null_cross_file: true", ""]
    for off, n in sorted(a.tlut.items()):
        w, h = (4, 4) if n == 16 else (16, 16) if n == 256 else (n, 1)
        lines += [f"{prefix}TLUT_{off:06X}:", "  type: TEXTURE", f"  offset: 0x{off:06X}",
                  f"  symbol: {prefix}TLUT_{off:06X}", "  format: RGBA16",
                  f"  width: {w}", f"  height: {h}", ""]
    for off, (fmt, w, h, tlut) in sorted(a.tex.items()):
        if off in a.tlut:
            continue
        lines += [f"{prefix}Tex_{off:06X}:", "  type: TEXTURE", f"  offset: 0x{off:06X}",
                  f"  symbol: {prefix}Tex_{off:06X}", f"  format: {fmt}",
                  f"  width: {w}", f"  height: {h}"]
        if tlut is not None:
            lines.append(f"  tlut: 0x{tlut:X}")
        lines.append("")
    return "\n".join(lines) + "\n"


def convert_scene(rom, scene, rooms, out):
    def load(name):
        vs, ve, ps, pe = rom.entry(name)
        return yaz0.decompress(rom.data[ps:pe]), ps

    sdata, sprom = load(scene)
    directory = f"mm/scenes/{scene}_scene"
    scene_assets = Assets(2, sdata)
    room_files = []
    others = set()
    for r in range(rooms):
        name = f"{scene}_room_{r:02d}"
        rdata, rprom = load(name)
        ra = Assets(3, rdata)
        w = Walker({2: scene_assets, 3: ra})
        for hdr in room_headers(rdata):
            for cmd, _, w1 in commands(rdata, hdr):
                if cmd == 0x0A:
                    for dl in mesh_dls(rdata, w1 & 0xFFFFFF):
                        w.walk(dl)
        others |= w.other_segments
        room_files.append((name, rprom, ra))

    os.makedirs(out, exist_ok=True)
    col = mm_scene_collision(sdata)
    header = (f":config:\n  segments:\n    - [ 2, 0x{sprom:X} ]\n  directory: {directory}\n\n"
              f"{scene}_sceneCollisionHeader_{col:06X}:\n  type: OOT:COLLISION\n  offset: 0x{col:06X}\n"
              f"  symbol: {scene}_sceneCollisionHeader_{col:06X}\n")
    open(os.path.join(out, f"{scene}.yml"), "w").write(yml_assets(scene_assets, f"{scene}_scene", header))
    for name, rprom, ra in room_files:
        header = (f":config:\n  segments:\n    - [ 3, 0x{rprom:X} ]\n    - [ 2, 0x{sprom:X} ]\n"
                  f"  directory: {directory}\n  external_files:\n    - mm_us/scenes/{scene}.yml\n\n")
        # (no OOT:ROOM: the room header is ours, mm_scene.py; Torch exports what it draws)
        open(os.path.join(out, f"{name}.yml"), "w").write(yml_assets(ra, name, header))
        print(f"{name}: {len(ra.dls)} display lists, {len(ra.vtx_arrays())} vertex arrays, "
              f"{len(ra.tex)} textures")
    print(f"{scene}: {len(scene_assets.dls)} display lists, {len(scene_assets.tex)} textures in the scene file"
          + (f"; segments not handled: {sorted(others)}" if others else ""))


if __name__ == "__main__":
    rom = mm_rom.MMRom(sys.argv[1])
    out = sys.argv[2]
    scenes = sys.argv[3:] or list(mm_rom.CLOCK_TOWN)
    for s in scenes:
        convert_scene(rom, s, mm_rom.CLOCK_TOWN[s], out)
