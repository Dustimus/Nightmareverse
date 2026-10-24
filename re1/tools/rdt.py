"""Resident Evil 1 (PC) room data: RDT room files and PAK-compressed backgrounds.

Formats as documented by Patrice Mandin's reevengi project (GPL); this is an
independent implementation from those descriptions.

PAK (STAGEn/RCsrrc.pak, one per camera: stage, room, camera):
    LZW over a big-endian bit stream. Codes start 9 bits wide; 0x100 ends the stream,
    0x101 widens codes by one bit, 0x102 resets the dictionary. New dictionary
    entries start at 0x103. The output is a PSX TIM (the 320x240 background).

RDT (STAGEn/ROOMsrrp.RDT; p = 0 Chris, 1 Jill):
    header (0x94 bytes):
        u8 unknown, u8 num_cameras, u8[4], u16[3],
        3 x {s32 x, y, z; u32[2]}, u32 offsets[19]
    cameras (right after the header), num_cameras x 44 bytes:
        u32 masks_offset, u32 mask_tim_offset,
        s32 from_x, from_y, from_z, s32 to_x, to_y, to_z, u32 unknown[3]
    offsets[0]: camera switch zones ("RVD"), 20 bytes each until to == 0xFFFF:
        u16 to, from; u16 x1, z1, x2, z2, x3, z3, x4, z4 (unsigned: rooms extend past 32767)
        (to == 9 marks a room boundary rather than a camera switch)
    offsets[6] / offsets[7]: init / room script (SCD bytecode, u16 length then
        instructions). Door_set (0x0C, 26 bytes):
            u8 op, id; u16 x, z, w, d; u8 unk0, unk1, anim, unk2, unk3,
            next_stage_and_room (bits 7-5 stage: 0 = this stage, else the stage number
            as in the file names; bits 4-0 room); u16 next_x, next_y, next_z,
            next_dir; u8 unk4, unk5
        Aot_set (0x0D, 18 bytes): a zone: u8 op, id; u16 x, z, w, d; u8 type, flags, data,
            unk, param, unk[3]. Types seen: 0 event (data = event index; with bit 7 set,
            a message: data & 0x7F, param a condition flag or 0xFF for none), 2 examine
            message (data & 0x7F), 7 use an item here (param = the item), 9 item pickup
    offsets[11]: messages: u16 offsets (relative to the table) then the texts; letters
        A-Z 0x1D-0x36, a-z 0x3D-0x56, 0x00 space, 0x3A ', 0x79 ., control bytes 0x01 end,
        0x02 new line, 0x03 next box; 0x74-0x77 draw the S.T.A.R.S. logo. English text is
        in the European PC release (HORR/USA), the JPN folder's files hold Japanese
    offsets[1]: collision ("SCA"): s16 cx, cz; u32 counts[5]; then sum(counts)
        elements of u16 x1, z1, x2, z2, type, floor (coordinates XOR 0x8000 to
        get signed values; type 1 = rectangle, 3 = circle/ellipse)
"""

import os
import struct
from dataclasses import dataclass

import emd


def pak_depack(data):
    out = bytearray()
    pos = 0  # bit position
    nbits_total = len(data) * 8

    def read(n):
        nonlocal pos
        v = 0
        for _ in range(n):
            if pos < nbits_total:
                bit = (data[pos >> 3] >> (7 - (pos & 7))) & 1
            else:
                bit = 0
            v = (v << 1) | bit
            pos += 1
        return v

    while True:
        prefix = {}
        suffix = {}
        nxt = 0x103
        width = 9
        old = read(width)
        if old == 0x100:
            break
        c = old
        out.append(c)
        stop = False
        while True:
            new = read(width)
            if new == 0x100:
                stop = True
                break
            if new == 0x102:
                break
            if new == 0x101:
                width += 1
                continue
            stack = []
            code = old if new >= nxt else new
            while code > 255:
                stack.append(suffix[code])
                code = prefix[code]
            stack.append(code)
            if new >= nxt:
                stack.insert(0, c)
            c = stack[-1]
            out.extend(reversed(stack))
            prefix[nxt] = old
            suffix[nxt] = c
            nxt += 1
            old = new
        if stop:
            break
    return bytes(out)


def load_background(path):
    """Decode an RCsrrc.pak background -> (width, height, rgba bytes)."""
    with open(path, "rb") as f:
        tim = emd.parse_tim(pak_depack(f.read()), 0)
    return emd.tim_to_rgba(tim)


@dataclass
class Camera:
    pos: tuple  # eye
    target: tuple
    unknown: tuple
    masks_offset: int


@dataclass
class CamSwitch:
    to: int
    frm: int
    quad: tuple  # 4 (x, z) points

    @property
    def is_boundary(self):
        return self.to == 9


@dataclass
class Collision:
    x1: int
    z1: int
    x2: int
    z2: int
    type: int
    floor: int


# SCD instruction lengths by opcode (fixed-length ones), from reevengi's scd1 notes
SCD_LEN = {
    0x00: 2, 0x01: 2, 0x02: 2, 0x03: 2, 0x04: 4, 0x05: 4, 0x06: 4, 0x07: 6, 0x08: 4, 0x09: 2,
    0x0A: 2, 0x0B: 4, 0x0C: 26, 0x0D: 18, 0x0E: 2, 0x0F: 8, 0x10: 2, 0x11: 2, 0x12: 10,
    0x13: 4, 0x14: 4, 0x15: 2, 0x16: 2, 0x18: 26, 0x19: 4, 0x1A: 2, 0x1B: 22, 0x1C: 6,
    0x1D: 2, 0x1E: 4, 0x1F: 28, 0x20: 14, 0x21: 14, 0x22: 4, 0x23: 2, 0x24: 4, 0x25: 4,
    0x26: 1, 0x27: 2, 0x29: 2, 0x2A: 12, 0x2B: 4, 0x2C: 2, 0x2D: 4, 0x2E: 1, 0x2F: 4,
    0x30: 12, 0x31: 4, 0x32: 4, 0x34: 8, 0x35: 4, 0x36: 4, 0x37: 4, 0x38: 4, 0x39: 2,
    0x3A: 4, 0x3B: 6, 0x3C: 6, 0x3D: 12, 0x3E: 2, 0x3F: 6, 0x40: 16, 0x41: 4, 0x42: 4,
    0x43: 4, 0x44: 2, 0x45: 2, 0x46: 38, 0x47: 14, 0x48: 2, 0x49: 2, 0x4A: 2, 0x4B: 2,
    0x4C: 4, 0x4D: 2, 0x4E: 4, 0x4F: 2, 0x50: 2,
}


def scd_inst_len(code, i):
    op = code[i]
    if op in SCD_LEN:
        return SCD_LEN[op]
    if op == 0x17:
        return 10 if code[i + 4] in (0, 1, 2, 3) else 6
    if op == 0x28:
        return {0: 6, 2: 6, 3: 6, 5: 6, 9: 6, 10: 6, 1: 8, 6: 4, 8: 4}.get(code[i + 2], 0)
    if op == 0x33:
        return {0: 2, 4: 2, 6: 2, 7: 2, 1: 4, 3: 4, 5: 4, 8: 4, 9: 4, 10: 4}.get(code[i + 1], 0)
    return 0


@dataclass
class Door:
    id: int
    x: int
    z: int
    w: int
    d: int
    anim: int
    next_stage: int  # like the file names (ROOMs...), or None for the door's own stage
    next_room: int
    next_pos: tuple  # x, y, z
    next_dir: int  # 4096 = 360 degrees
    raw: bytes


def parse_doors(d, offset):
    """Walk an SCD script and return its Door_set instructions."""
    doors = []
    if not offset:
        return doors
    length = struct.unpack_from("<H", d, offset)[0]
    code = d[offset + 2 : offset + 2 + length]
    i = 0
    while i < len(code):
        n = scd_inst_len(code, i)
        if n <= 0:
            break  # unknown instruction: stop rather than misparse
        if code[i] == 0x0C and i + 26 <= len(code):
            (op, did, x, z, w, dd, u0, u1, anim, u2, u3, sr, nx, ny, nz, ndir, u4, u5) = struct.unpack_from(
                "<BB4H6B4H2B", code, i
            )
            stage = (sr >> 5) or None
            doors.append(Door(did, x, z, w, dd, anim, stage, sr & 31, (nx, ny, nz), ndir, code[i : i + 26]))
        i += n
    return doors


@dataclass
class Enemy:
    model: int  # 0x00-0x15 enemies (0x11 = zombie, EM1011), 0x20+ characters (0x25 = Kenneth)
    flag: int  # "killed" flag index
    pos: tuple  # x, y, z
    dir: int  # 4096 = 360 degrees
    raw: bytes


def parse_enemies(d, offset):
    """Em_set (0x1B, 22 bytes) from an SCD script: u8 op, model, unk, killed_flag;
    u16 unk; u16 unk; u16 dir; u16 unk; u16 x, y, z; u16 unk[3]. Both branches of a
    script's conditions are returned (e.g. before/after an event)."""
    out = []
    if not offset:
        return out
    length = struct.unpack_from("<H", d, offset)[0]
    code = d[offset + 2 : offset + 2 + length]
    i = 0
    while i < len(code):
        n = scd_inst_len(code, i)
        if n <= 0:
            break
        if code[i] == 0x1B and i + 22 <= len(code):
            _op, model, _u, flag, _u1, _u2, direction, _u3, x, y, z = struct.unpack_from("<4B7H", code, i)
            out.append(Enemy(model, flag, (x, y, z), direction, code[i : i + 22]))
        i += n
    return out


@dataclass
class Aot:
    id: int
    x: int
    z: int
    w: int
    d: int
    type: int
    flags: int
    data: int
    param: int
    raw: bytes


def parse_aots(d, offset):
    """Aot_set (0x0D) zones from an SCD script."""
    out = []
    if not offset:
        return out
    length = struct.unpack_from("<H", d, offset)[0]
    code = d[offset + 2 : offset + 2 + length]
    i = 0
    while i < len(code):
        n = scd_inst_len(code, i)
        if n <= 0:
            break
        if code[i] == 0x0D and i + 18 <= len(code):
            _op, aid, x, z, w, dd, typ, flags, data, _u, param = struct.unpack_from("<BB4H5B", code, i)
            out.append(Aot(aid, x, z, w, dd, typ, flags, data, param, code[i : i + 18]))
        i += n
    return out


@dataclass
class Item:
    id: int
    x: int
    z: int
    w: int
    d: int
    item: int  # RE1's item id
    count: int
    flag: int  # RE1's "taken" flag for it (byte 22)
    raw: bytes


def parse_items(d, offset):
    """Item_set (0x18) pickups from an SCD script: a rectangle, the item and how many."""
    out = []
    if not offset:
        return out
    length = struct.unpack_from("<H", d, offset)[0]
    code = d[offset + 2 : offset + 2 + length]
    i = 0
    while i < len(code):
        n = scd_inst_len(code, i)
        if n <= 0:
            break
        if code[i] == 0x18 and i + 26 <= len(code):
            _op, iid, x, z, w, dd, item, count = struct.unpack_from("<BB4H2B", code, i)
            out.append(Item(iid, x, z, w, dd, item, count, code[i + 22], code[i : i + 26]))
        i += n
    return out


# RE1's message encoding (European PC release), read off its font (DATA/FONTUS.TIM: the glyphs
# in byte order from 0x00, 'A' = 0x1D); unknown bytes come out as {xx}. 0x02 is a new line;
# 0x03 and 0x04 (wait for a button, then go on) start a new box.
_TEXT = {0x00: " ", 0x02: "\n", 0x03: "\f", 0x04: "\f"}
_TEXT.update({0x0C + i: str(i) for i in range(10)})
_TEXT.update(dict(zip(range(0x16, 0x1D), [":", ";", ",", '"', "!", "?", "!?"])))
_TEXT.update({0x1D + i: chr(ord("A") + i) for i in range(26)})
_TEXT.update(dict(zip(range(0x37, 0x3D), ["(", "/", ")", "'", "-", "\u00b7"])))
_TEXT.update({0x3D + i: chr(ord("a") + i) for i in range(26)})
_TEXT.update({0x78: '"', 0x79: ".", 0x7A: "...", 0x7B: "-", 0x7C: "+", 0x7D: "="})


def decode_text(b):
    """One message's bytes to text: \n new line, \f next box (0x1A as "!" is a guess); 0x05 and the
    byte after it (a colour) are dropped; {08} is RE1's Yes/No. 0x0A, found only at a message's
    end (e.g. 20D's "Richard is dead.", the examine texts that end on a box of their own), shows
    nothing: dropped, with the empty box it would leave."""
    out = []
    i = 0
    while i < len(b):
        v = b[i]
        if v == 0x01:
            break
        if v == 0x0A:
            i += 1
            continue
        if v == 0x05:  # a colour change and its colour (the item names in red); not kept
            i += 2
            continue
        if b[i : i + 5] == bytes([0x74, 0x75, 0x76, 0x77, 0x74]):
            out.append("S.T.A.R.S.")
            i += 5
            continue
        out.append(_TEXT.get(v, "{%02x}" % v))
        i += 1
    return "".join(out).rstrip(" \f")


def messages(room):
    """The room's messages (offsets[11]) as raw bytes, by number."""
    d, o = room.raw, room.offsets[11]
    if not o:
        return []
    n = struct.unpack_from("<H", d, o)[0] // 2
    offs = list(struct.unpack_from("<%dH" % n, d, o))
    end = min([x for x in room.offsets if x > o] + [len(d)])
    return [d[o + offs[k] : (o + offs[k + 1]) if k + 1 < n else end] for k in range(n)]


@dataclass
class Room:
    cameras: list
    switches: list
    collision_origin: tuple
    collisions: list
    offsets: list
    raw: bytes
    doors: list = None
    enemies: list = None
    aots: list = None
    items: list = None
    objects: list = None  # Om_set placements (RoomObject)


@dataclass
class Mask:
    src: tuple  # x, y in the camera's mask TIM
    size: tuple  # w, h
    dst: tuple  # x, y on the 320x240 screen
    depth: int  # raw value; camera-space depth is 16x this


def load_masks(room, cam_index):
    """Foreground masks for a camera: (list of Mask, mask TIM or None).

    PRI data at camera.masks_offset: u16 num_groups, u16 num_masks; groups of
    {u16 count, u16 unk, s16 dst_x, dst_y}; then masks, either 8-byte squares
    {u8 src_x, src_y, dst_x, dst_y; u16 depth; u8 unk, size} or, when size == 0,
    12-byte rectangles {u8 src_x, src_y, dst_x, dst_y; u16 depth, zero, width, height}.
    The mask image is a TIM at the camera's second offset."""
    d = room.raw
    o = 0x94 + 44 * cam_index
    pri, tim_off = struct.unpack_from("<2I", d, o)
    masks = []
    if pri not in (0, 0xFFFFFFFF):
        ngroups, _nmasks = struct.unpack_from("<2H", d, pri)
        groups = [struct.unpack_from("<2H2h", d, pri + 4 + 8 * g) for g in range(ngroups)]
        p = pri + 4 + 8 * ngroups
        for count, _unk, gx, gy in groups:
            for _ in range(count):
                sx, sy, dx, dy, depth, _u, size = struct.unpack_from("<4BH2B", d, p)
                if size == 0:
                    sx, sy, dx, dy, depth, _z, w, h = struct.unpack_from("<4B4H", d, p)
                    p += 12
                else:
                    w = h = size
                    p += 8
                masks.append(Mask((sx, sy), (w, h), (gx + dx, gy + dy), depth))
    tim = None
    if tim_off not in (0, 0xFFFFFFFF):
        tim = emd.parse_tim(d, tim_off)
    return masks, tim


@dataclass
class RoomObject:
    """Om_set (0x1F): one of the room's own object models (a block to push, steps...)."""
    id: int  # low 6 bits: the model (RDT offsets[2])
    pos: tuple  # x, y, z
    dir: int  # 4096 = 360 degrees
    raw: bytes


def parse_objects(d, offset):
    """Om_set instructions from an SCD script (28 bytes: op, id, 2 bytes, then s16 x, y,
    z, direction and fields we don't use)."""
    out = []
    if not offset:
        return out
    length = struct.unpack_from("<H", d, offset)[0]
    code = d[offset + 2 : offset + 2 + length]
    i = 0
    while i < len(code):
        n = scd_inst_len(code, i)
        if n <= 0:
            break
        if code[i] == 0x1F and i + 28 <= len(code):
            x, y, z, ry = struct.unpack_from("<4h", code, i + 4)
            out.append(RoomObject(code[i + 1], (x, y, z), ry & 0xFFFF, code[i : i + 28]))
        i += n
    return out


def room_models(room):
    """The room's own object models (header byte 2 of them; offsets[2]: per model a TMD and a
    TIM offset): [(emd.MeshObject list, emd TIM), or None]. TMD textured triangles only (mode 0x34),
    which is all these use; texture coordinates are relative to the TIM's left edge."""
    d = room.raw
    out = []
    for k in range(d[2]):
        tmd, tim_off = struct.unpack_from("<2I", d, room.offsets[2] + 8 * k)
        if not tmd:  # an object with no model of its own (e.g. room 10F's third: collision only)
            out.append(None)
            continue
        tim = emd.parse_tim(d, tim_off)
        _magic, _flags, nobj = struct.unpack_from("<3I", d, tmd)
        objects = []
        for oi in range(nobj):
            base = tmd + 12
            vt, nv, nt, nn, pt, npr, _scale = struct.unpack_from("<7I", d, base + 28 * oi)
            verts = [struct.unpack_from("<3h", d, base + vt + 8 * i) for i in range(nv)]
            norms = [struct.unpack_from("<3h", d, base + nt + 8 * i) for i in range(nn)]
            tris = []
            q = base + pt
            for _ in range(npr):
                _olen, ilen, _flag, mode = struct.unpack_from("<4B", d, q)
                if mode == 0x34:
                    u0, v0, clut, u1, v1, page, u2, v2 = struct.unpack_from("<BBHBBHBB", d, q + 4)
                    n0, i0, n1, i1, n2, i2 = struct.unpack_from("<6H", d, q + 16)
                    px = (page & 0xF) * 128 - tim.x * 2  # 8 bpp: a texture page is 128 px
                    tris.append(emd.Triangle((i0, i1, i2), (n0, n1, n2),
                                             ((u0 + px, v0), (u1 + px, v1), (u2 + px, v2)), (clut >> 6) - tim.clut_y))
                q += 4 + ilen * 4
            objects.append(emd.MeshObject(verts, norms, tris))
        out.append((objects, tim))
    return out


def find_file(directory, name):
    """directory/name, matching the name in any case (RE1's releases differ: RC1060.pak in the
    Japanese PC files, RC1060.PAK in the English ones)."""
    want = name.upper()
    for fn in os.listdir(directory):
        if fn.upper() == want:
            return os.path.join(directory, fn)
    return os.path.join(directory, name)


def load_room(path):
    with open(path, "rb") as f:
        d = f.read()
    num_cameras = d[1]
    offsets = list(struct.unpack_from("<19I", d, 0x94 - 76))
    cameras = []
    for i in range(num_cameras):
        o = 0x94 + 44 * i
        mo, _to, fx, fy, fz, tx, ty, tz, u0, u1, u2 = struct.unpack_from("<2I6i3I", d, o)
        cameras.append(Camera((fx, fy, fz), (tx, ty, tz), (u0, u1, u2), mo))
    switches = []
    o = offsets[0]
    while o and o + 20 <= len(d):
        to, frm = struct.unpack_from("<2H", d, o)
        if to == 0xFFFF:
            break
        q = struct.unpack_from("<8H", d, o + 4)  # unsigned, like SCA (rooms extend past 32767)
        switches.append(CamSwitch(to, frm, tuple(zip(q[0::2], q[1::2]))))
        o += 20
    collisions = []
    origin = (0, 0)
    o = offsets[1]
    if o:
        cx, cz = struct.unpack_from("<2h", d, o)
        counts = struct.unpack_from("<5I", d, o + 4)
        origin = (cx, cz)
        p = o + 24
        for _ in range(sum(counts)):
            x1, z1, x2, z2, t, fl = struct.unpack_from("<6H", d, p)
            collisions.append(Collision(x1 ^ 0x8000, z1 ^ 0x8000, x2 ^ 0x8000, z2 ^ 0x8000, t, fl))
            p += 12
    doors = parse_doors(d, offsets[6]) + parse_doors(d, offsets[7])
    enemies = parse_enemies(d, offsets[6]) + parse_enemies(d, offsets[7])
    aots = parse_aots(d, offsets[6]) + parse_aots(d, offsets[7])
    items = parse_items(d, offsets[6]) + parse_items(d, offsets[7])
    objects = parse_objects(d, offsets[6]) + parse_objects(d, offsets[7])
    return Room(cameras, switches, origin, collisions, offsets, d, doors, enemies, aots, items, objects)
