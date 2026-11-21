"""MM's messages, from the player's ROM, in OoT's text format (for OoT's own text box).

MM's message table is in its `code` file (MessageTableEntry: the text id, a byte, a pad byte,
the message's segment 8 address in `message_data_static`). Each message starts with an 11-byte
header: the box's type and position, an icon, the next message's id, and two rupee prices.
After it, ASCII with MM's control codes (the decomp's message_data_fmt_nes.h), ending at 0xBF.
They become OoT's codes (z_message_PAL.c's):
- colours: OoT has no silver or orange, so white and yellow;
- newline, box breaks, shift, the player's name, quick text on and off, events, persistent,
  delayed box break and fade;
- two and three choices; the button and stick icons;
- MM's counters, timers, codes, prices and sound effects (MM's own sound ids) are left out.

Written as one resource, `mm/text/messages` ("MTXT", by MM text id: box type, position, next id,
OoT-coded text), which the game reads by id (NvMmText.cpp). Nothing here is committed: the
resource is the player's game data.
"""

import struct

import yaz0

CODE_FILE, MESSAGE_FILE = 31, 29          # their indices in the US 1.0 file table
MESSAGE_TABLE = 0x1210D8                  # in `code`, US 1.0 (found by its shape: ids rising, 0x08......)

COLOURS = {0: 0x40, 1: 0x41, 2: 0x42, 3: 0x43, 4: 0x46, 5: 0x44, 6: 0x45, 7: 0x40, 8: 0x46}
BUTTONS = {0xB0: 0x9F, 0xB1: 0xA0, 0xB2: 0xA1, 0xB3: 0xA2, 0xB4: 0xA3, 0xB5: 0xA4, 0xB6: 0xA5, 0xB7: 0xA6,
           0xB8: 0xA7, 0xB9: 0xA8, 0xBA: 0xA9, 0xBB: 0xAB}


def _file(rom, index):
    vs, ve, ps, pe = rom.entry_index(index)
    return yaz0.decompress(rom.data[ps:pe]) if rom.data[ps:ps + 4] == b"Yaz0" else rom.data[ps:ps + ve - vs]


def convert(text):
    """One MM message body (after its header) as OoT's codes, ending with OoT's end (0x02)."""
    out = bytearray()
    i = 0
    while i < len(text):
        c = text[i]
        i += 1
        if c == 0xBF:
            break
        if c <= 0x08:
            out += bytes((0x05, COLOURS[c]))
        elif c == 0x0A or c == 0x13 or c == 0x15 or c == 0xC1:  # text speed, carriage return, continue, bg
            pass
        elif c in (0x10, 0x12):
            out.append(0x04)
        elif c == 0x11:
            out.append(0x01)
        elif c == 0x14:
            out += bytes((0x06, text[i]))
            i += 1
        elif c == 0x16:
            out.append(0x0F)
        elif c == 0x17:
            out.append(0x08)
        elif c == 0x18:
            out.append(0x09)
        elif c in (0x19, 0xE0):
            out.append(0x0B)
        elif c == 0x1A:
            out.append(0x0A)
        elif c == 0x1B:
            out += bytes((0x0C, text[i + 1]))
            i += 2
        elif c in (0x1C, 0x1D):
            out += bytes((0x0E, text[i + 1]))
            i += 2
        elif c in (0x1E, 0x1F):  # MM's sound effect (its own ids), a delay
            i += 2
        elif c in BUTTONS:
            out.append(BUTTONS[c])
        elif c == 0xC2:
            out.append(0x1B)
        elif c == 0xC3:
            out.append(0x1C)
        elif 0xC4 <= c <= 0xE4:  # counters, timers, codes, prices: not shown
            pass
        else:
            out.append(c)
    out.append(0x02)
    return bytes(out)


def messages(rom):
    """{MM text id: (box type, position, next id, OoT-coded text)}"""
    code = _file(rom, CODE_FILE)
    data = _file(rom, MESSAGE_FILE)
    out = {}
    p = MESSAGE_TABLE
    while True:
        tid, _b, _pad, seg = struct.unpack_from(">HBBI", code, p)
        if tid == 0xFFFF or (seg >> 24) != 0x08:
            break
        off = seg & 0xFFFFFF
        box, pos = data[off], data[off + 1]
        nxt = struct.unpack_from(">H", data, off + 3)[0]
        out[tid] = (box, pos, nxt, convert(data[off + 11:off + 11 + 0x800]))
        p += 8
    return out


def blob(msgs):
    """"MTXT", a count, then per message: id, box type, position, next id, text offset and length;
    then the texts."""
    ids = sorted(msgs)
    head = bytearray(b"MTXT" + struct.pack("<I", len(ids)))
    texts = bytearray()
    base = 8 + 12 * len(ids)
    for tid in ids:
        box, pos, nxt, t = msgs[tid]
        head += struct.pack("<HBBHHI", tid, box, pos, nxt, len(t), base + len(texts))
        texts += t
    return bytes(head + texts)
