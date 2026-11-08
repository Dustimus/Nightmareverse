#!/usr/bin/env python3
"""RE1's 2D effects (fire, smoke, sparks, glass, blood, water...) for soh/src/code/z_nv_effects.c.

RE1 (the decomp's EffectSprites.cpp / EffectSystem.cpp) has effect TYPES (0-0x31): the eight
weapon / shared ones from Data/core00.esp (their art in core00.etm), and up to eight more a room
declares in its RDT (header +0x7C / +0x80 / +0x84: its type list, the data offsets read backwards
from +0x80, its 4bpp sprite TIMs read backwards from +0x84). A type's data: u16 UV-record count,
u16 frame count, 4 bytes RE1 fills in, the frames (4 bytes: UV record, delay, width, height), the
UV records (u, v, pivot x, pivot y), then its animation (per spawn group, chains of 24-byte phase
blocks the behaviours walk).

Every type's art sits on one of the room's four 256x256 texture pages, stacked down them in
order (setup_effect_sprite_textures: the weapon types first, from page 0, then the room's,
a page further each time 256 rows run out, the next starting at row 3); page p is the Effspr
file g_RoomEffectSpriteTable names for the room's page p, with the room's own sprites blitted at
their rows. The UVs here are made page-absolute (RE1 does the same to the room's: its V bytes
plus the sprite's row).

Written into the mod:
  re1/effects/core                 the core types: "RFXC", then the block below
  scenes/shared/<n>/<n>_effects    the room's: "RFXR", u8 pages[4] (Effspr file per page, 0xFF
                                   none), then the block below, then its sprites: u8 count, per
                                   sprite u8 page, u8 pad, u16 row, u16 w, u16 h, w*h RGBA5551
  re1/effspr/<i>                   Effspr file i as 256x256 RGBA5551 ("RFXP")
  re1/effects/tables               "RFXT", the room pages (896 bytes), the 32 pages' blend bands
                                   (u8 count, rows of start V, length, blend, colour) and the
                                   colour records (u8 count, then u8 count, RGB triplets each)
the block: u8 count, per type u8 type, u8 page, u16 pad, u32 offset of its data in the bytes
that follow; u32 byte count, the bytes.
"""
import os
import struct

import emd
import rdt
import soh_scene

# RE1's own tables, read from the player's English executable when the mod is made (none of it
# is in the repo; checked by the Effspr name table that follows the first): g_RoomEffectSpriteTable
# (0x004c48b8: per stage (7) and room (32), the Effspr file of each of the room's four pages),
# the per-page blend bands (0x004c4f50: 32 x {count, rows of start V, length, blend, colour}) and
# the colour records (0x004c5288: {count, RGB triplets}).
VA_ROOM_PAGES, VA_NAMES, VA_BANDS, VA_COLOURS = 0x004C48B8, 0x004C4C38, 0x004C4F50, 0x004C5288
N_COLOURS = 60


def exe_tables(exe_path):
    """(room pages 896 bytes, bands [[rows]], colours [[rgb]]) from the executable, or None."""
    import soh_sound
    if not exe_path:
        return None
    d = open(exe_path, "rb").read()
    if d[:2] != b"MZ":
        return None
    secs = soh_sound._pe_sections(d)

    def off(v):
        for va, size, ra, rs in secs:
            if va <= v < va + size and v - va < rs:
                return ra + (v - va)
        return None

    o = off(VA_NAMES)
    if o is None or d[o:o + 6] != b"esp000":
        return None
    pages = d[off(VA_ROOM_PAGES):off(VA_ROOM_PAGES) + 7 * 32 * 4]
    bands = []
    for i in range(32):
        n, p = struct.unpack_from("<iI", d, off(VA_BANDS) + i * 8)
        q = off(p)
        bands.append([d[q + k * 4:q + k * 4 + 4] for k in range(n)] if (q and 0 < n < 16) else [])
    colours = []
    for i in range(N_COLOURS):
        n, p = struct.unpack_from("<iI", d, off(VA_COLOURS) + i * 8)
        q = off(p)
        colours.append([d[q + k * 3:q + k * 3 + 3] for k in range(n)] if (q and 0 < n < 16) else [])
    return pages, bands, colours


NAMES = ["esp000", "esp001", "esp200", "esp201", "esp202", "esp203", "esp204", "esp205", "esp206",
         "esp207", "esp208", "esp209", "esp210", "esp211", "esp212", "esp213", "esp214", "esp215",
         "esp216", "esp217", "esp218", "esp219", "esp220", "esp221", "esp222", "esp223", "esp225",
         "esp226", "esp227", "esp228", "esp229", "esp230"]


def _rgba5551(c):
    r, g, b, a = c
    return ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | (1 if a else 0)


def _tim_rgba(data, off, clut_row=0):
    """A 4bpp/8bpp TIM as (w, h, rgba), CLUT row clut_row, index 0 see-through."""
    tim = emd.parse_tim(data, off)
    row = min(clut_row, len(tim.cluts) - 1)
    w, h, rgba = emd.tim_to_rgba(tim, {p: row for p in range(8)})
    out = bytearray(rgba)
    for i in range(w * h):  # (RE1's key: index 0)
        idx = tim.pixels[i] if tim.bpp == 8 else ((tim.pixels[i // 2] >> 4) if i & 1 else (tim.pixels[i // 2] & 15))
        if idx == 0:
            out[i * 4 + 3] = 0
    return w, h, bytes(out)


def _pack(heights, cur_v=0, page=0):
    """setup_effect_sprite_textures' cursor: each sprite's (page, row), and where it ends."""
    out = []
    for h in heights:
        if h + cur_v > 0x100:
            cur_v = 3
            page += 1
        out.append((page, cur_v))
        cur_v += h
    return out, cur_v, page


def _types(base, index_off, data_off):
    """The (type, data offset) pairs an index (0xFF-ended, 8 at most) declares."""
    out = []
    for i in range(8):
        t = base[index_off + i]
        if t == 0xFF:
            break
        out.append((t, struct.unpack_from("<I", base, data_off - i * 4)[0]))
    return out


def _shift_v(buf, info, dv):
    """The type's UV records' V bytes moved down its page by dv (RE1's ADD [EDI+1], BL)."""
    n_uv, n_frames = struct.unpack_from("<2H", buf, info)
    uv = info + 8 + n_frames * 4
    for k in range(n_uv):
        buf[uv + k * 4 + 1] = (buf[uv + k * 4 + 1] + dv) & 0xFF


def _block(types, data):
    out = bytearray(struct.pack("<B", len(types)))
    for t, page, off in types:
        out += struct.pack("<BBHI", t, page, 0, off)
    return bytes(out + struct.pack("<I", len(data)) + data)


def core(re1):
    """(core block, the weapon pass's end (row, page)) from Data/core00.esp / .etm."""
    dd = rdt.find_file(re1, "DATA")
    esp = bytearray(open(rdt.find_file(dd, "core00.esp"), "rb").read())
    etm = open(rdt.find_file(dd, "core00.etm"), "rb").read()
    end = (len(esp) & ~3) + (4 if len(esp) & 3 else 0) - 4
    types = _types(esp, 0, end)
    heights = []
    for i in range(len(types)):
        off = struct.unpack_from("<I", etm, len(etm) - 4 - i * 4)[0]
        heights.append(emd.parse_tim(etm, off).height)
    places, cur_v, page = _pack(heights)
    out = []
    for (t, off), (pg, row) in zip(types, places):
        _shift_v(esp, off, row)  # (their UVs are their own sheet's: made the page's)
        out.append((t, pg, off))
    return _block(out, bytes(esp)), cur_v, page


def room(raw, stage, room_id, cur_v, page, room_pages):
    """A room's effects blob (its pages, and the types it declares, if any)."""
    i = (stage * 32 + room_id) * 4
    pages = room_pages[i:i + 4] if 0 <= stage < 7 and 0 <= room_id < 32 else b"\x00\xff\xff\xff"
    ix, da, sp = struct.unpack_from("<3I", raw, 0x7C) if len(raw) >= 0x88 else (0, 0, 0)
    if not ix or ix >= len(raw) or raw[ix] == 0xFF:  # (none of its own: its pages for the core types)
        return b"RFXR" + bytes(pages) + _block([], b"") + struct.pack("<B", 0)
    types = _types(raw, ix, da)
    lo = min([ix] + [o for _t, o in types])
    hi = da + 4
    data = bytearray(raw[lo:hi])
    sprites = []
    heights = []
    tims = []
    for i in range(len(types)):
        toff = struct.unpack_from("<I", raw, sp - i * 4)[0]
        tims.append(toff)
        heights.append(emd.parse_tim(raw, toff).height)
    places, _v, _p = _pack(heights, cur_v, page)
    out = []
    for (t, off), (pg, row), toff in zip(types, places, tims):
        _shift_v(data, off - lo, row)
        out.append((t, pg, off - lo))
        w, h, rgba = _tim_rgba(raw, toff)
        px = b"".join(struct.pack(">H", _rgba5551(rgba[k * 4:k * 4 + 4])) for k in range(w * h))
        sprites.append(struct.pack("<BBHHH", pg, 0, row, w, h) + px)
    return (b"RFXR" + bytes(pages) + _block(out, bytes(data)) + struct.pack("<B", len(sprites)) +
            b"".join(sprites))


def page_blob(re1, i):
    """Effspr file i as a 256x256 RGBA5551 page ("RFXP"), or None."""
    try:
        path = rdt.find_file(rdt.find_file(re1, "EFFSPR"), NAMES[i] + ".tim")
        d = open(path, "rb").read()
    except (OSError, IndexError):
        return None
    w, h, rgba = _tim_rgba(d, 0)
    if (w, h) != (256, 256):
        return None
    return b"RFXP" + b"".join(struct.pack(">H", _rgba5551(rgba[k * 4:k * 4 + 4])) for k in range(w * h))


def _write(path, blob):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    open(path, "wb").write(soh_scene._otr_header(soh_scene.LUS_BLOB) + struct.pack("<I", len(blob)) + blob)


def write_global(re1, out, exe_path):
    """The core types, RE1's effect tables and the Effspr pages; (core's end row, page, room pages)
    for the rooms, or None (no executable, or its files missing)."""
    t = exe_tables(exe_path)
    if t is None:
        return None
    pages, bands, colours = t
    try:
        blk, cur_v, page = core(re1)
    except (OSError, IndexError, ValueError):
        return None
    _write(os.path.join(out, "re1", "effects", "core"), b"RFXC" + blk)
    tb = bytearray(b"RFXT" + pages)
    for rows in bands:  # u8 count, rows of 4
        tb += struct.pack("<B", len(rows)) + b"".join(rows)
    tb += struct.pack("<B", len(colours))
    for rgb in colours:  # u8 count, triplets
        tb += struct.pack("<B", len(rgb)) + b"".join(rgb)
    _write(os.path.join(out, "re1", "effects", "tables"), bytes(tb))
    for i in range(len(NAMES)):
        b = page_blob(re1, i)
        if b is not None:
            _write(os.path.join(out, "re1", "effspr", str(i)), b)
    return cur_v, page, pages


def write_room(raw, stage, room_id, path, start):
    """The room's effects at path, if it declares any (start: write_global's)."""
    if start is None:
        return False
    b = room(raw, stage, room_id, start[0], start[1], start[2])
    if b is None:
        return False
    _write(path, b)
    return True
