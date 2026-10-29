"""RE1 rooms as Ship of Harkinian resources (XML scenes, rooms and collision, plus the
camera backgrounds), written into a folder that pack_o2r() zips into a mod archive.

The room data is rdt2scene.py's (the N64 build's converter): the floor grid, collision,
cameras and spawns are worked out there, and this module only writes them in SoH's XML
resource formats (soh/soh/resource/importer/*Factory.cpp, the XML readers) instead of C.

Layout, per room (SoH loads scenes from scenes/shared/<file>/<file>, z_play_otr.cpp):
    scenes/shared/<name>_scene/<name>_scene      the scene (XML, root <Room>)
    scenes/shared/<name>_scene/<name>_room_0     its one room (XML)
    scenes/shared/<name>_scene/<name>_col        collision (XML <CollisionHeader>)
    scenes/shared/<name>_scene/<name>_bg<n>      camera n's background (binary Background:
                                                 a JPEG, decoded by SoH's stb_image path)

Game data: everything written here comes from the player's own RE1 files; the output is
never committed.
"""

import io
import math
import os
import struct
import zipfile
from xml.sax.saxutils import quoteattr

from PIL import Image

import rdt
import re1_floor
import re1_masks
import rdt2scene as r2s

SOH_BACKGROUND = 0x4F424749  # "OBGI", SohResourceType.h
LUS_BLOB = 0x4F424C42  # "OBLB", libultraship ResourceType.h
MASK_MAGIC = b"RMSK"
DOOR_MAGIC = b"RDOR"
DOOR_ACTION = 0x80  # Door_set byte 0x19: opens with the action button (all of the mansion's doors)
DOOR_AT_PLAYER = 0x40  # ...tested at the player's position instead of the point in front of him
BG_DECODED_SIZE = 320 * 240 * 2  # the decoded background, RGBA16 (what SoH writes back over the JPEG)


def _otr_header(res_type, version=0, res_id=0):
    """libultraship's 64-byte binary resource header (ResourceLoader::ReadResourceInitDataBinary):
    byte order (0: little), custom flag, 2 unused, type, version, id; the rest reserved."""
    return struct.pack("<BB2xIIQ", 0, 1, res_type, version, res_id).ljust(64, b"\0")


def _surface0(cam, exit_index=0):
    """SURFACETYPE0(bgCamIndex, exitIndex, FLOOR_TYPE_0, 0, WALL_TYPE_0, FLOOR_PROPERTY_0, false, false)"""
    return (cam & 0xFF) | ((exit_index & 0x1F) << 8)


# SURFACETYPE1(SURFACE_MATERIAL_STONE, FLOOR_EFFECT_0, 0, 1, false, CONVEYOR_SPEED_DISABLED, 0, false)
SURFACE1_STONE = 2 | (1 << 11)


def _snormal(v):
    """COLPOLY_SNORMAL"""
    return int(round(v * 32767.0))


def _el(tag, **attrs):
    return "<%s %s/>" % (tag, " ".join("%s=%s" % (k, quoteattr(str(v))) for k, v in attrs.items()))


def background_jpeg(stage, room_id, cam_index, quality=95):
    """Camera background as a baseline JPEG for SoH (ResourceMgr_LoadJPEG: stb_image, 320x240)."""
    w, h, rgba = rdt.load_background(rdt.find_file(stage, "RC%s%d.pak" % (room_id, cam_index)))
    buf = io.BytesIO()
    Image.frombytes("RGBA", (w, h), rgba).convert("RGB").save(buf, "JPEG", quality=quality, subsampling=0)
    return buf.getvalue()


def background_raw(stage, room_id, cam_index):
    """Camera background as raw RGBA5551, big-endian, 320x240: RE1's own 15-bit pixels, bit for
    bit (lossless). SoH draws a Background's data directly as RGBA16 unless it starts with a
    JPEG header (z_room.c swapAndConvertJPEG), so no decoding happens at all."""
    with open(rdt.find_file(stage, "RC%s%d.pak" % (room_id, cam_index)), "rb") as f:
        tim = rdt.emd.parse_tim(rdt.pak_depack(f.read()), 0)
    assert tim.bpp == 16 and tim.width == 320 and tim.height == 240, (room_id, cam_index, tim.bpp)
    out = bytearray(320 * 240 * 2)
    for i in range(320 * 240):
        c = tim.pixels[2 * i] | (tim.pixels[2 * i + 1] << 8)  # PSX BGR555 (little-endian)
        r, g, b = c & 31, (c >> 5) & 31, (c >> 10) & 31
        v = (r << 11) | (g << 6) | (b << 1) | 1  # opaque everywhere (backgrounds have no holes)
        out[2 * i] = v >> 8
        out[2 * i + 1] = v & 0xFF
    return bytes(out)


def mask_blob(pieces):
    """The mask resource's data (re1_masks.camera_masks pieces): "RMSK", u32 count; per piece
    s16 x, y, u16 w, h, stride (w rounded up to 8), u16 group (what room scripts switch), f32
    depth (RE1's ordering key * 16: view-space depth, in OoT units); then stride * h I8 texels."""
    data = bytearray(MASK_MAGIC + struct.pack("<I", len(pieces)))
    for x, y, w, h, key, group, rows in pieces:
        stride = (w + 7) & ~7
        data += struct.pack("<hhHHHHf", x, y, w, h, stride, group, key * 16 * r2s.SCALE)
        for r in rows:
            data += bytes(r) + b"\0" * (stride - w)
    return bytes(data)


def extend_floor(room, grid, floor, steps=4):
    """The floor grid grown up to `steps` cells (~35 OoT units) into free space beside it: RE1's
    camera zones don't cover all the floor Jill can walk (she keeps the last camera there; in
    203 the strip before the east wall, with its two doors, lies outside them), and the grid
    only has cells in a zone. Cells inside RE1's solid shapes stay out; a grown cell takes its
    neighbour's surface (camera), as RE1 keeps the camera it had."""
    solid = [c for c in room.collisions if r2s._solid(c) and not floor.is_open(c)]
    C = r2s.CELL
    out = dict(grid)
    front = list(grid)
    for _ in range(steps):
        nxt = []
        for (gx, gz) in front:
            for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                n = (gx + dx, gz + dz)
                if n in out:
                    continue
                x, z = (n[0] + 0.5) * C, (n[1] + 0.5) * C
                if any(r2s._inside_collision(c, x, z) for c in solid):
                    continue
                out[n] = out[(gx, gz)]
                nxt.append(n)
        front = nxt
    return out


def grow_into_walls(room, grid, floor):
    """The floor grid plus one ring of cells reaching into RE1's solid shapes. The grid keeps
    only cells whose middle is clear of them, so it stops up to a cell short of RE1's walls;
    RE1's walls still stop Link, and the floor's edge (edge_walls) then sits at or behind them
    instead of in front (where it kept him out of door alcoves)."""
    solid = [c for c in room.collisions if r2s._solid(c) and not floor.is_open(c)]
    C = r2s.CELL
    out = dict(grid)
    for (gx, gz), surf in grid.items():
        for dx in (-1, 0, 1):
            for dz in (-1, 0, 1):
                n = (gx + dx, gz + dz)
                if n in out:
                    continue
                x, z = (n[0] + 0.5) * C, (n[1] + 0.5) * C
                if any(r2s._inside_collision(c, x, z) for c in solid):
                    out[n] = surf
    return out


def edge_walls(col, grid, wall_surface, floor):
    """Walls along every outer edge of the walkable floor (cell edges with no floor beyond),
    merged into straight runs. RE1's own collision shapes don't close the floor everywhere (the
    foot of the foyer's stairs, some door alcoves: RE1 keeps Jill in by its camera zones), and
    Link would walk off the edge into nothing. Facing into the floor."""
    C = r2s.CELL
    runs = []  # (axis, line, start, end, inward sign)
    for (gx, gz) in grid:
        for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            if (gx + dx, gz + dz) in grid:
                continue
            if dx:  # an edge along z at x = gx + 1 (east) or gx (west)
                runs.append(("x", gx + (dx > 0), gz, gz + 1, -dx))
            else:
                runs.append(("z", gz + (dz > 0), gx, gx + 1, -dz))
    runs.sort()
    merged = []
    for r in runs:
        if merged and merged[-1][0] == r[0] and merged[-1][1] == r[1] and merged[-1][4] == r[4] and \
                merged[-1][3] == r[2]:
            merged[-1] = (r[0], r[1], merged[-1][2], r[3], r[4])
        else:
            merged.append(r)
    for axis, line, a, b, inward in merged:
        if axis == "x":
            p0, p1 = (line * C, a * C), (line * C, b * C)
        else:
            p0, p1 = (a * C, line * C), (b * C, line * C)
        lo = r2s.to_oot((p0[0], floor.depth, p0[1]))
        hi = r2s.to_oot((p1[0], floor.depth, p1[1]))
        top0 = r2s.to_oot((p0[0], floor.top, p0[1]))
        top1 = r2s.to_oot((p1[0], floor.top, p1[1]))
        col.quad(wall_surface, lo, hi, top1, top0)
        # face into the floor: the inward direction in RE1 units, then OoT's (x, -y, -z)
        want = (inward, 0) if axis == "x" else (0, -inward)
        for k in (-2, -1):
            surf, idx, n, d = col.polys[k]
            if n[0] * want[0] + n[2] * want[1] < 0:
                col.polys[k] = (surf, (idx[0], idx[2], idx[1]), [-x for x in n], -d)


def door_blob(doors):
    """Doors, for soh/src/code/z_nv_door.c: "RDOR", u32 count; per door f32 x0, z0, x1, z1 (the
    trigger rectangle, OoT units), u8 flags (Door_set byte 0x19), u8 anim (RE1's door scene),
    u16 spawn (the target room's start position), u8 Door_set bytes 10, 11, 13 and 15 (what
    RE1's door scene reads as its variables: Bio.exe's table at 0x4b0de8), char target[24]
    (the target scene's file).
    doors: [(Door, target scene file, spawn index)]."""
    data = bytearray(DOOR_MAGIC + struct.pack("<I", len(doors)))
    for door, target, spawn in doors:
        # OoT = (x, -y, -z) * SCALE: RE1's z range flips
        x0, x1 = door.x * r2s.SCALE, (door.x + door.w) * r2s.SCALE
        z0, z1 = -(door.z + door.d) * r2s.SCALE, -door.z * r2s.SCALE
        data += struct.pack("<4fBBH4B24s", x0, z0, x1, z1, door.raw[0x19], door.anim, spawn, door.raw[10],
                            door.raw[11], door.raw[13], door.raw[15], target.encode())
    return bytes(data)


def room_aots(raw):
    """Aot_set (SCD 0x0D, 18 bytes) zones of the room's two scripts: [(type, x, z, w, d, d0, sat)].
    sat: 0x40 tests the player's own position instead of the point 600 ahead of her, 0x80 only on
    the action button (update_player_position / check_action_object)."""
    offs = struct.unpack_from("<19I", raw, 0x94 - 76)
    out = []
    for o in (offs[6], offs[7]):
        if not o:
            continue
        length = struct.unpack_from("<H", raw, o)[0]
        code = raw[o + 2:o + 2 + length]
        i = 0
        while i < len(code):
            n = rdt.scd_inst_len(code, i)
            if n <= 0:
                break
            if code[i] == 0x0D and i + 18 <= len(code):
                _op, _id, x, z, w, d, t, sat, d0, _d1, _d2 = struct.unpack_from("<BB4H2B3h", code, i)
                out.append((t, x, z, w, d, d0, sat))
            i += n
    return out


ENEMY_TYPES = set()  # enemy types the written rooms place (soh_enemy.py writes their models)
MOVIES = set()  # RE1 movie ids the written rooms' scripts can play (cmd_fmv_set)

# RE1's event scripts (room_events_check 0x0041d6a0; the decomp's docs/SCD_SCRIPT_SYSTEM.md):
# operand lengths of the control bytes (any state) and of state 1's and 2's opcodes
_EVT_CTRL = {0xF6: 1, 0xF7: 1, 0xF8: 3, 0xF9: 3, 0xFA: 4, 0xFB: 1, 0xFC: 2, 0xFD: 1, 0xFE: 1, 0xFF: 1}
_EVT_S1 = {0x00: 1, 0x80: 1, 0x82: 1, 0x83: 9, 0x84: 5, 0x85: 5, 0x86: 1, 0x87: 4, 0x88: 3, 0x89: 2, 0x8A: 2,
           0x8B: 1}
_EVT_S2 = {0: 1, 1: 1, 2: 1, 3: 1, 4: 1, 5: 4, 6: 4, 7: 8, 8: 3, 9: 2, 0xA: 4, 0xB: 12}


def _scd_chain(raw, off):
    """An SCD block chain (run_command_functions): u16 block size (with itself), the commands, ...,
    a zero size."""
    p = off
    while struct.unpack_from("<H", raw, p)[0]:
        p += struct.unpack_from("<H", raw, p)[0]
    return raw[off:p + 2]


def room_scd(raw):
    """The room's scripts as RE1 keeps them: the init chain (RDT+0x60, run once), the main chain
    (RDT+0x64, run every frame by game_loop) and the event table with its scripts (RDT+0x68:
    offsets from the table, zero-ended)."""
    offs = struct.unpack_from("<19I", raw, 0x94 - 76)
    init = _scd_chain(raw, offs[6]) if offs[6] else b"\0\0"
    main = _scd_chain(raw, offs[7]) if offs[7] else b"\0\0"
    evt = b"\0\0\0\0"
    if offs[8]:
        later = sorted(o for o in offs if o > offs[8])
        evt = raw[offs[8]:later[0] if later else len(raw)]
    return init, main, evt


def _scd_commands(chain):
    """Every command of a block chain, as (offset, bytes)."""
    out = []
    p = 0
    while p + 2 <= len(chain) and struct.unpack_from("<H", chain, p)[0]:
        size = struct.unpack_from("<H", chain, p)[0]
        i = p + 2
        while i < p + size and chain[i] != 0:
            n = rdt.scd_inst_len(chain, i)
            if n <= 0:
                break
            out.append((i, chain[i:i + n]))
            i += n
        p += size
    return out


def _event_commands(evt):
    """The SCD commands the event scripts run (state 0's 0x06 blocks and 0x07 single commands),
    read from each script's start through the states it reaches."""
    out = []
    starts = []
    p = 0
    while p + 4 <= len(evt) and struct.unpack_from("<I", evt, p)[0]:
        starts.append(struct.unpack_from("<I", evt, p)[0])
        p += 4
    for a in starts:
        i, st = a, 0
        while i < len(evt):
            op = evt[i]
            if op in _EVT_CTRL:
                n = _EVT_CTRL[op]
                if op == 0xFF:
                    break
            elif st == 0:
                if op in (6, 7):
                    n = evt[i + 1] if op == 6 else evt[i + 1]
                    if op == 6:
                        out += [c for _o, c in _scd_commands(evt[i + 2:i + n])]
                    else:
                        k = rdt.scd_inst_len(evt, i + 2)
                        if k > 0:
                            out.append(evt[i + 2:i + 2 + k])
                elif op in (0, 1, 2, 3):
                    n = 1
                    st = {0: 0, 1: 1, 2: 2, 3: 2}[op]
                elif op in (4, 5):
                    n = 3
                elif op in (8, 9):
                    n = 2
                else:
                    break
            elif st == 1:
                n = (12 if evt[i + 1] & 0x0F else 2) if op == 0x81 else _EVT_S1.get(op, 0)
                if op in (0x80, 0x8B):
                    st = 0
            else:
                n = _EVT_S2.get(op, 0)
                if op == 1:
                    st = 0
            if n <= 0:
                break
            i += n
    return out


# RE1's movie table (the PC exe's, 0x004c39d8): cmd_fmv_set's id -> MOVIE file
MOVIE_FILES = ["OU", "PU", "DMF", "DM3", "DM4", "DM1", "DM6", "DM7", "DM8", "DM2", None, "DMB", "DMC", "DMD", "DME",
               "ED1", "ED2", "ED3", "EU4", "EU5", "ED6", "ED7", "ED8", "capcom", "stfc_r", "stfj_r", "stfz_r",
               "staf_r", "vlogo"]


def write_movies(re1, out, ids):
    """Copy RE1's movies (Cinepak AVIs) for these ids, byte for byte, as "re1/movies/<id>"
    (z_nv_movie.c plays them). Returns the ids written; a missing file is left out."""
    done = []
    d = os.path.join(out, "re1", "movies")
    os.makedirs(d, exist_ok=True)
    try:
        mdir = rdt.find_file(re1, "MOVIE")
    except Exception:
        mdir = os.path.join(re1, "MOVIE")
    for i in sorted(set(ids)):
        name = MOVIE_FILES[i] if i < len(MOVIE_FILES) else None
        path = rdt.find_file(mdir, name + ".avi") if (name and os.path.isdir(mdir)) else None
        if not path or not os.path.exists(path):
            print("movie %d (%s): not found, left out" % (i, name))
            continue
        data = open(path, "rb").read()
        open(os.path.join(d, str(i)), "wb").write(_otr_header(LUS_BLOB) + struct.pack("<I", len(data)) + data)
        done.append(i)
    return done


def re1col_blob(raw):
    """"RCOL", u32 lengths of the boundary block and the walk zones, then both as RE1 has them:
    the boundaries (RDT+0x4C: s16 cellX, cellZ, u32 count per quadrant x5, then 12-byte records
    u16 xMax, zMax, xMin, zMin, type, flags; check_room_collision) and the walk zones (RDT+0x58:
    count byte, pad, then 12-byte zones; zone_path_find). z_nv_zombie.c moves enemies by them."""
    offs = struct.unpack_from("<19I", raw, 0x94 - 76)
    bnd = b""
    if offs[1]:
        counts = struct.unpack_from("<5I", raw, offs[1] + 4)
        bnd = raw[offs[1]:offs[1] + 0x18 + 12 * sum(counts)]
    zones = b""
    if offs[4]:
        zones = raw[offs[4]:offs[4] + 2 + 12 * raw[offs[4]]]
    return b"RCOL" + struct.pack("<2I", len(bnd), len(zones)) + bnd + zones


# RE1's message font (fontus.tim): a message byte b is the glyph in column b % 18, row b / 18 + 2
# (message_render_chars 0x00456020), the cell PrintText8x14 draws for ASCII b + 36. The cells
# follow ASCII from there except where the font has its own glyphs.
MSG_GLYPHS = {24: ",", 25: '"', 26: "!", 27: "?", 55: "(", 56: "/", 57: ")", 58: "'", 59: "-", 60: ".",
              120: '"', 121: ".", 122: "-", 123: "-"}


def _msg_char(b):
    if b in MSG_GLYPHS:
        return MSG_GLYPHS[b]
    if 12 <= b <= 23 or 29 <= b <= 54 or 61 <= b <= 86:
        return chr(b + 36)  # digits, ':', ';', A-Z, a-z
    return ""  # RE1's own symbols, and a handful of accented capitals


def decode_message(raw, p):
    """One RE1 message at raw[p] (message state machine, Rendering.cpp 0x00455a10) as an SoH
    CustomMessage string: '^' a new box (RE1's 03 page break), '%r'/'%w' RE1's highlight colour
    (05 n), '%r?%w' for an item name (06 n, from the executable's tables: not yet). A line break
    (02) is a space: SoH wraps lines to its own box. Stops at the end (01) or at a yes/no prompt
    (08: its choices drive item actions, which come with the items)."""
    out = []
    while p < len(raw):
        b = raw[p]
        if b == 0 or b == 2:
            out.append(" ")
            p += 1
        elif b == 1 or b == 8:
            break
        elif b == 3:
            out.append("^")
            p += 2
        elif b == 4:  # text speed (and, as 04 00, a skipped block up to the next 04)
            if p + 1 < len(raw) and raw[p + 1] == 0:
                q = p + 2
                while q < len(raw) and raw[q] != 4:
                    q += 2 if raw[q] in (5, 6, 0xF8, 0xF9, 0xFA) else 1
                p = q + 2
            else:
                p += 2
        elif b == 5:
            out.append("%r" if raw[p + 1] else "%w")
            p += 2
        elif b == 6:
            out.append("%r?%w")
            p += 2
        elif b == 7:
            p += 1
        elif b in (0xF8, 0xF9, 0xFA):
            p += 2
        else:
            out.append(_msg_char(b))
            p += 1
    text = " ".join("".join(out).split())  # RE1 pads lines with spaces
    return text.replace(" ^", "^").replace("^ ", "^").strip("^ ")


def msg_blob(raw):
    """The room's messages (RDT+0x74: u16 offsets, then the text) for z_nv_script.c: "RMSG", u32
    count, u32 offsets into the strings, then the strings (NUL-terminated, decode_message)."""
    mp = struct.unpack_from("<I", raw, 0x74)[0]
    texts = []
    if mp:
        first = struct.unpack_from("<H", raw, mp)[0]
        for i in range(first // 2):
            texts.append(decode_message(raw, mp + struct.unpack_from("<H", raw, mp + 2 * i)[0]).encode("ascii"))
    head = b"RMSG" + struct.pack("<I", len(texts))
    offs, pos = [], 0
    for t in texts:
        offs.append(pos)
        pos += len(t) + 1
    return head + b"".join(struct.pack("<I", o) for o in offs) + b"".join(t + b"\0" for t in texts)


def scd_blob(init, main, evt):
    """"RSCD", u32 lengths of the init chain, main chain and event table + scripts, then the three
    as RE1 has them (z_nv_script.c runs them)."""
    return b"RSCD" + struct.pack("<3I", len(init), len(main), len(evt)) + init + main + evt


def walk_cameras(floor, zones, starts):
    """{floor cell: camera} as a player who walked there from (x, z) would have it: RE1 has no
    camera regions, only the room's opening camera (entry_camera) and the switch zones of the
    camera on (z_nv_camera.c). Breadth-first from each start, as RE1 switches (one zone of the
    current camera's group per step); a cell keeps the camera of the shortest walk to it. Only
    warps use it (z_nv_camera.c), so a dev warp shows Link."""
    groups = {}
    for rec in zones:
        groups.setdefault(struct.unpack_from("<h", rec, 2)[0], []).append(rec)

    def switch(cam, k):
        px, pz = int((k[0] + 0.5) * re1_floor.CELL), int((k[1] + 0.5) * re1_floor.CELL)
        for rec in groups.get(cam, [])[1:]:  # the group's first record is its header
            if in_switch_zone(rec, px, pz):
                return struct.unpack_from("<h", rec, 0)[0]
        return cam

    out, seen, queue = {}, set(), []
    for x, z in starts:
        k = floor.cell(x, z)
        st = (k, entry_camera(zones, x, z))
        if k in floor.cells and st not in seen:
            seen.add(st)
            queue.append(st)
    i = 0
    while i < len(queue):
        k, cam = queue[i]
        i += 1
        out.setdefault(k, cam)
        for n in ((k[0] + 1, k[1]), (k[0] - 1, k[1]), (k[0], k[1] + 1), (k[0], k[1] - 1)):
            if n in floor.cells:
                st = (n, switch(cam, n))
                if st not in seen:
                    seen.add(st)
                    queue.append(st)
    return out


def camera_zone_records(raw, ncam):
    """RE1's switch-zone table (RDT+0x48) as its raw 20-byte records, grouped by camFrom (each
    group's first record is its header, the rest the quads tested while that camera is on). The
    table ends with a record of camTo = camFrom = -1."""
    zp = struct.unpack_from("<I", raw, 0x48)[0]
    out = []
    for p in range(zp, len(raw) - 19, 20):
        cam_from = struct.unpack_from("<h", raw, p + 2)[0]
        if not 0 <= cam_from < ncam:
            break
        out.append(raw[p:p + 20])
    return out


def in_switch_zone(rec, px, pz):
    """RE1's is_entity_in_switch_zone (0x00462d90) on a raw 20-byte record, as z_nv_camera.c."""
    c = struct.unpack_from("<8H", rec, 4)
    x0, z0, x2, z2 = c[0], c[1], c[4], c[5]
    if (c[2] - x0) * (pz - z0) > (c[3] - z0) * (px - x0):
        return False
    if (c[6] - x0) * (pz - z0) < (c[7] - z0) * (px - x0):
        return False
    if (c[2] - x2) * (pz - z2) < (c[3] - z2) * (px - x2):
        return False
    if (c[6] - x2) * (pz - z2) > (c[7] - z2) * (px - x2):
        return False
    return True


def entry_camera(zones, x, z):
    """The camera RE1 opens a room on with the player at (x, z): camera 0, then one pass of
    camera 0's switch zones (room_set: g_roomCameraId = 0, check_camera_switch(1))."""
    group = [rec for rec in zones if struct.unpack_from("<h", rec, 2)[0] == 0]
    for rec in group[1:]:  # the group's first record is its header
        if in_switch_zone(rec, int(x), int(z)):
            return struct.unpack_from("<h", rec, 0)[0]
    return 0


def write_room(room, grid, stage, room_id, name, out, spawns, cell_exit=None, doors=(), bias_tables=None,
               arrivals=(), aots=()):
    """One RE1 room as an SoH scene. spawns: [(x, z, yaw_binang)] in RE1 units. Returns the
    scene's resource path (what the scene table's file name resolves to)."""
    cell_exit = cell_exit or {}
    sc, rm, colname = name + "_scene", name + "_room_0", name + "_col"
    base = "scenes/shared/%s" % sc
    d = os.path.join(out, base)
    os.makedirs(d, exist_ok=True)
    ncam = len(room.cameras)

    # floor and walls from RE1's rules (re1_floor.py)
    floor = re1_floor.Floor(room.raw, int(room_id[1:], 16),
                            list(arrivals) + [(sx, sz, 0) for sx, sz, _ in spawns], aots)
    # each floor cell's surface carries the camera a player walking there would have (only warps
    # use it: z_nv_camera.c opens rooms and switches by RE1's own rules)
    zones = camera_zone_records(room.raw, ncam)
    cell_cam = walk_cameras(floor, zones, [(a[0], a[1]) for a in arrivals] + [(sx, sz) for sx, sz, _ in spawns])

    def cam_at(x, z):
        return cell_cam.get(floor.cell(x, z), entry_camera(zones, x, z))

    cams = sorted({cam_at((k[0] + 0.5) * re1_floor.CELL, (k[1] + 0.5) * re1_floor.CELL) for k in floor.cells} | {0})
    surf_of_cam = {c: i for i, c in enumerate(cams)}
    keys = [(c, 0) for c in cams]
    wall_surface = len(keys)
    verts, polys = floor.mesh(
        lambda k: surf_of_cam[cam_at((k[0] + 0.5) * re1_floor.CELL, (k[1] + 0.5) * re1_floor.CELL)],
        wall_surface, r2s.to_oot)

    class _Col:
        pass

    col = _Col()
    col.verts, col.polys = verts, polys
    xs, ys, zs = ([v[i] for v in col.verts] for i in range(3))

    C = ['<CollisionHeader Version="0" MinBoundsX="%d" MinBoundsY="%d" MinBoundsZ="%d" '
         'MaxBoundsX="%d" MaxBoundsY="%d" MaxBoundsZ="%d">' % (min(xs), min(ys), min(zs), max(xs), max(ys), max(zs))]
    for v in col.verts:
        C.append("    " + _el("Vertex", X=v[0], Y=v[1], Z=v[2]))
    for s, (a, b, c), n, dist in col.polys:
        C.append("    " + _el("Polygon", Type=s, VertexA=a, VertexB=b, VertexC=c, NormalX=_snormal(n[0]),
                              NormalY=_snormal(n[1]), NormalZ=_snormal(n[2]), Dist=dist))
    for cam, ex in keys + [(0, 0)]:
        C.append("    " + _el("PolygonType", Data1=_surface0(cam, ex), Data2=SURFACE1_STONE))
    for c in range(ncam):  # CAM_SET_PREREND_FIXED (= 0x19 in SoH's camera settings), 3 Vec3s each
        C.append("    " + _el("CameraData", SType=CAM_SET_PREREND_FIXED, NumData=3, CameraPosDataSeg=3 * c))
    for c, cam in enumerate(room.cameras):
        pos, rot, fov = r2s.camera_data(cam)
        C.append("    " + _el("CameraPositionData", PosX=pos[0], PosY=pos[1], PosZ=pos[2], RotX=rot[0], RotY=rot[1],
                              RotZ=rot[2], FOV=fov, JfifID=c, Unknown=-1))
    C.append("</CollisionHeader>")
    open(os.path.join(d, colname), "w").write("\n".join(C) + "\n")

    # scene: sound, room list, camera mode, collision, spawns, special files, player entries,
    # skybox, lighting (rdt2scene.SCENE_CMDS)
    S = ['<Room Version="0">',
         "    " + _el("SetSoundSettings", Reverb=0, NatureAmbienceId=0x13, SeqId=0x7F),
         '    <SetRoomList>', "        " + _el("RoomEntry", Path="%s/%s" % (base, rm), VromStart=0, VromEnd=0),
         '    </SetRoomList>',
         "    " + _el("SetCameraSettings", CameraMovement=1, WorldMapArea=0),  # SCENE_CAM_TYPE_FIXED
         "    " + _el("SetCollisionHeader", FileName="%s/%s" % (base, colname)),
         '    <SetEntranceList>']
    for i in range(len(spawns)):
        S.append("        " + _el("EntranceEntry", Spawn=i, Room=0))
    S.append('    </SetEntranceList>')
    S.append("    " + _el("SetSpecialObjects", ElfMessage=0, GlobalObject=0))
    S.append('    <SetStartPositionList>')
    for sx, sz, syaw in spawns:
        p = r2s.to_oot((sx, floor.height(sx, sz), sz))
        cam = cam_at(sx, sz)
        # ACTOR_PLAYER, PLAYER_PARAMS(PLAYER_START_MODE_IDLE, cam): start on the camera owning the cell
        S.append("        " + _el("StartPositionEntry", Id=0, PosX=round(p[0]), PosY=round(p[1]), PosZ=round(p[2]),
                                  RotX=0, RotY=syaw, RotZ=0, Params=(PLAYER_START_MODE_IDLE << 8) | (cam & 0xFF)))
    S.append('    </SetStartPositionList>')
    S.append("    " + _el("SetSkyboxSettings", Unknown=0, SkyboxId=0, Weather=0, Indoors=1))
    S.append('    <SetLightingSettings>')
    S.append("        " + _el("LightingSetting", AmbientColorR=60, AmbientColorG=55, AmbientColorB=50,
                              Light1DirX=73, Light1DirY=73, Light1DirZ=73, Light1ColorR=180, Light1ColorG=160,
                              Light1ColorB=130, Light2DirX=-73, Light2DirY=-73, Light2DirZ=-73, Light2ColorR=50,
                              Light2ColorG=45, Light2ColorB=40, FogColorR=10, FogColorG=10, FogColorB=10,
                              FogNear=(1 << 10) | 994, FogFar=3200))
    S.append('    </SetLightingSettings>')
    S.append("    <EndMarker/>")
    S.append("</Room>")
    open(os.path.join(d, sc), "w").write("\n".join(S) + "\n")

    # room: the backgrounds (room shape IMAGE, MULTI: one per camera, picked by bgCamIndex)
    R = ['<Room Version="0">',
         "    " + _el("SetRoomBehavior", GameplayFlags1=0, GameplayFlags2=0),
         '    <SetMesh Data="0" MeshHeaderType="1">',
         '        <Polygon PolyType="1" Format="2" BgImageCount="%d" MeshOpa="" MeshXlu="">' % ncam]
    for c in range(ncam):
        # RE1's pixels as they are (lossless; background_jpeg is the old lossy path)
        px = background_raw(stage, room_id, c)
        bgname = "%s_bg%d" % (name, c)
        open(os.path.join(d, bgname), "wb").write(_otr_header(SOH_BACKGROUND) + struct.pack("<I", len(px)) + px)
        # its mask: <bg>_mask (ReMask_Draw finds it by the background's path)
        bias = re1_masks.fade_bias(bias_tables, int(room_id[0]), int(room_id[1:], 16), c)
        blob = mask_blob(re1_masks.camera_masks(room.raw, c, bias))
        open(os.path.join(d, bgname + "_mask"), "wb").write(_otr_header(LUS_BLOB) + struct.pack("<I", len(blob)) + blob)
        R.append("            " + _el("BgImage", Unknown_00=130, Id=c, ImagePath="%s/%s" % (base, bgname),
                                      Unknown_0C=0, TLUT=0, Width=320, Height=240, Fmt=0, Siz=2, Mode0=0,
                                      TLUTCount=0))
    # RE1's camera switch zones, as they are: "<name>_camzones" (z_nv_camera.c)
    zrecs = camera_zone_records(room.raw, ncam)
    blob = b"RCZN" + struct.pack("<I", len(zrecs)) + b"".join(zrecs)
    open(os.path.join(d, name + "_camzones"), "wb").write(_otr_header(LUS_BLOB) + struct.pack("<I", len(blob)) + blob)

    # stairs: "<name>_climbs" (z_nv_climb.c), RE1's Aot_set zones of type 5 and 6 with the move
    # player_door_open_sequence makes (shallower in rooms 0x05 and 0x0E)
    along, up = re1_floor.CLIMB_SHALLOW if int(room_id[1:], 16) in (0x05, 0x0E) else re1_floor.CLIMB
    # (a staircase that became a ramp is walked, not climbed: no A there)
    climbs = [a for a in aots if a[0] in (5, 6) and tuple(a[:5]) not in floor.ramp_zones]
    blob = b"RCLB" + struct.pack("<I", len(climbs)) + b"".join(
        struct.pack("<3Bx4H2h", t, sat, d0 & 0xFF, x, z, w, dd, along, up) for t, x, z, w, dd, d0, sat in climbs)
    open(os.path.join(d, name + "_climbs"), "wb").write(_otr_header(LUS_BLOB) + struct.pack("<I", len(blob)) + blob)

    # RE1's scripts: "<name>_scd" (z_nv_script.c), with the enemies they place and movies they play
    init, main, evt = room_scd(room.raw)
    cmds = [c for _o, c in _scd_commands(init) + _scd_commands(main)] + _event_commands(evt)
    ENEMY_TYPES.update(c[1] for c in cmds if c[0] == 0x1B)
    MOVIES.update(c[1] for c in cmds if c[0] == 0x29)
    blob = scd_blob(init, main, evt)
    open(os.path.join(d, name + "_scd"), "wb").write(_otr_header(LUS_BLOB) + struct.pack("<I", len(blob)) + blob)
    # RE1's own collision and walk zones: "<name>_re1col" (z_nv_zombie.c)
    blob = re1col_blob(room.raw)
    open(os.path.join(d, name + "_re1col"), "wb").write(_otr_header(LUS_BLOB) + struct.pack("<I", len(blob)) + blob)
    # RE1's messages: "<name>_msg" (z_nv_script.c's examine spots)
    blob = msg_blob(room.raw)
    open(os.path.join(d, name + "_msg"), "wb").write(_otr_header(LUS_BLOB) + struct.pack("<I", len(blob)) + blob)

    # doors: "<name>_doors" (z_nv_door.c finds it by the scene's name)
    blob = door_blob(doors)
    open(os.path.join(d, name + "_doors"), "wb").write(_otr_header(LUS_BLOB) + struct.pack("<I", len(blob)) + blob)
    R.append('        </Polygon>')
    R.append('    </SetMesh>')
    R.append("    <EndMarker/>")
    R.append("</Room>")
    open(os.path.join(d, rm), "w").write("\n".join(R) + "\n")
    return "%s/%s" % (base, sc)


CAM_SET_PREREND_FIXED = 0x19  # z64camera.h
PLAYER_START_MODE_IDLE = 13  # z64player.h


def pack_o2r(folder, archive):
    """Zip a resource folder into an .o2r mod archive (a zip of resource paths)."""
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
        for root, _dirs, files in os.walk(folder):
            for f in sorted(files):
                p = os.path.join(root, f)
                z.write(p, os.path.relpath(p, folder).replace(os.sep, "/"))
