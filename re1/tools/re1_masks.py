"""RE1's camera masks ("room sprites"), read the way RE1 reads and sorts them.

Rules from RE1's PC code (ecruells/resident-evil-pc-decomp: Room_LoadCameraSprites 0x004757c0,
DrawRoomSpr 0x00475b80; our own code, checked against the data: 92% of mask texels equal the
background under them, 7% one colour step off):

- The mask block at camera.mask_pointer: s32 group count; per group u16 sprite count, u16
  texture bits, s16 origin x, y; then the sprites: u8 tex u, v, screen dx, dy; u16 posData;
  u16 flags; and u16 width, height only when (flags & 0xF000) == 0, else a square of side
  (flags & 0xF1FF) >> 9. Screen position = origin + delta (origin is relative to the frame's
  centre, delta biased by 160, 120, which cancel).
- The art is the 8-bit TIM at camera.tim_mask_pointer; a texel whose CLUT colour is 0x0000 is
  transparent (the PlayStation's rule).
- Sorting: a sprite's ordering key is max(0, posData - fadeBias) & ~3, where fadeBias comes from
  a per-(room, camera) record in tables in the executable (29 cameras have one); characters
  enter the same ordering at their view-space depth >> 4. So a sprite covers a character
  exactly when its key is nearer than the character's, whole sprite against whole character.
- Groups (index + 1) are what room scripts switch off and on.

The bias tables are Capcom's data: they're read from the player's own executable, found by
their contents (the 32 records start {0,0,0}, {-250,0,0}, {30,-5,0}; the 160x8 record index
sits right before them, 1280 bytes).
"""
import struct

import emd

RECORDS_SIGNATURE = struct.pack("<9i", 0, 0, 0, -250, 0, 0, 30, -5, 0)


def load_bias_tables(exe_path):
    """(record index [160 * 8], records [(depthBias, fadeBias, mode)] * 32) from an RE1 PC
    executable, or None."""
    if not exe_path:
        return None
    d = open(exe_path, "rb").read()
    i = d.find(RECORDS_SIGNATURE)
    if i < 160 * 8:
        return None
    index = d[i - 160 * 8:i]
    records = [struct.unpack_from("<3i", d, i + 12 * k) for k in range(32)]
    return index, records


def fade_bias(tables, stage, room, cam):
    """The camera's fadeBias (0 for the ~130 cameras without a record). stage: 1-based."""
    if tables is None:
        return 0
    index, records = tables
    s = stage - 1
    if s > 4:
        s -= 5
    room_idx = room + s * 0x20
    if room_idx >= 160 or cam >= 8:
        return 0
    return records[index[room_idx * 8 + cam]][1]


def sprites(raw, mask_pointer):
    """[(group, u, v, x, y, w, h, posData)] for one camera (screen x, y from the frame's corner)."""
    out = []
    if mask_pointer in (0, 0xFFFFFFFF):
        return out
    groups = struct.unpack_from("<i", raw, mask_pointer)[0]
    heads = [struct.unpack_from("<2H2h", raw, mask_pointer + 4 + 8 * g) for g in range(groups)]
    p = mask_pointer + 4 + 8 * groups
    for g, (count, _texbits, ox, oy) in enumerate(heads):
        for _ in range(count):
            u, v, dx, dy, pos, flags = struct.unpack_from("<4B2H", raw, p)
            if flags & 0xF000 == 0:
                w, h = struct.unpack_from("<2H", raw, p + 8)
                p += 12
            else:
                w = h = (flags & 0xF1FF) >> 9
                p += 8
            out.append((g + 1, u, v, ox + dx, oy + dy, w, h, pos))
    return out


def camera_masks(raw, cam_index, bias=0):
    """Camera cam_index's mask pieces, clipped to the screen: [(x, y, w, h, key, group, alpha
    rows)], alpha 255 where the art is opaque. key: RE1's ordering key (view depth / 16)."""
    mask_pointer, tim_pointer = struct.unpack_from("<2I", raw, 0x94 + 44 * cam_index)
    sp = sprites(raw, mask_pointer)
    if not sp or tim_pointer in (0, 0xFFFFFFFF):
        return []
    tim = emd.parse_tim(raw, tim_pointer)
    clut = tim.cluts[0]
    out = []
    for group, u, v, x, y, w, h, pos in sp:
        x0, y0, x1, y1 = max(x, 0), max(y, 0), min(x + w, 320), min(y + h, 240)
        if x1 <= x0 or y1 <= y0:
            continue
        rows = []
        for yy in range(y0, y1):
            ty = v + yy - y
            row = []
            for xx in range(x0, x1):
                tx = u + xx - x
                inside = tx < tim.width and ty < tim.height
                row.append(255 if inside and clut[tim.pixels[ty * tim.width + tx]] != 0 else 0)
            rows.append(row)
        if not any(any(r) for r in rows):
            continue
        key = max(0, pos - bias) & ~3
        out.append((x0, y0, x1 - x0, y1 - y0, key, group, rows))
    return out
