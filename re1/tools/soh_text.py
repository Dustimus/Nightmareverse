#!/usr/bin/env python3
"""RE1's text kept in the executable, for soh/src/code/z_nv_script.c and z_nv_items.c.

Read from the player's own English PC executable (ResidentEvil.exe / the GOG one) when the mod is
made; nothing of it is in the repo. Three of its tables, found by their contents (not by
address, so another build of the same release works too):

    the shared messages (global_messages, 64: what a message id with bit 0x40 shows: the
        pickup and use prompts, the locked doors, "You can't use it here.", ...)
    the item names (g_ItemNamePointers, by item id - 1: each ends at 07)
    the item descriptions (g_ItemDescriptions, by item id - 1: the status screen's Check)

They become "re1/exe_text": "RXTX", u16 count of each (messages, names, descriptions), then u32
offsets of every string into the strings that follow (NUL-terminated, decoded as the room
messages are: soh_scene.decode_message). The Japanese release keeps its own tables in its own
glyph order, which isn't read: with its executable, no text is written.
"""
import os
import struct

import rdt
import soh_scene
import soh_sound


def _reader(d):
    secs = soh_sound._pe_sections(d)

    def off(v):
        for va, size, ra, rs in secs:
            if va <= v < va + size and v - va < rs:
                return ra + (v - va)
        return None

    return off


def _name(d, x):
    """An item name: RE1's text up to its 07."""
    end = x
    while end < len(d) and d[end] not in (0x07, 0x01) and end - x < 40:
        end += 1
    return rdt.decode_text(d[x:end]).strip()


def tables(exe_path):
    """(messages, names, descriptions) from an English RE1 PC executable, or None."""
    if not exe_path:
        return None
    d = open(exe_path, "rb").read()
    if d[:2] != b"MZ":
        return None
    off = _reader(d)
    messages = names = descs = None
    for o in range(0, len(d) - 512, 4):
        p0 = struct.unpack_from("<I", d, o)[0]
        x0 = off(p0)
        if x0 is None:
            continue
        # the shared messages: 64 pointers, 56 and 58-61 the same line ("You can't use it here.")
        if messages is None:
            p = struct.unpack_from("<63I", d, o)
            if (p[56] == p[58] == p[59] == p[60] == p[61]) and all(off(v) for v in p) and len(set(p)) > 50:
                if soh_scene.decode_message(d, off(p[2])).startswith("You can't carry"):
                    messages = [soh_scene.decode_message(d, off(v)) for v in p] + [""]
                    continue
        # the item names: the knife first
        if names is None and _name(d, x0) == "COMBAT KNIFE":
            p = struct.unpack_from("<127I", d, o)
            if all(off(v) for v in p):
                names = [_name(d, off(v)) for v in p]
                continue
        # the descriptions: the Beretta's second
        if descs is None:
            p1 = struct.unpack_from("<I", d, o + 4)[0]
            if off(p1) and soh_scene.decode_message(d, off(p1)).startswith("Beretta M92FS"):
                p = struct.unpack_from("<79I", d, o)
                if all(off(v) for v in p):
                    descs = [soh_scene.decode_message(d, off(v)) for v in p]
                    continue
        if messages and names and descs:
            break
    if not (messages and names and descs):
        return None
    return messages, names, descs


def write(exe_path, out):
    """re1/exe_text from the executable; (messages, names, descriptions) counts, or None."""
    t = tables(exe_path)
    if t is None:
        return None
    strings = [s for lst in t for s in lst]
    blob = bytearray(b"RXTX" + struct.pack("<3H", *(len(x) for x in t)))
    pos = 0
    offs = []
    body = bytearray()
    for s in strings:
        offs.append(pos)
        b = s.encode("latin-1", "replace") + b"\0"
        body += b
        pos += len(b)
    blob += struct.pack("<%dI" % len(offs), *offs) + body
    os.makedirs(os.path.join(out, "re1"), exist_ok=True)
    open(os.path.join(out, "re1", "exe_text"), "wb").write(
        soh_scene._otr_header(soh_scene.LUS_BLOB) + struct.pack("<I", len(blob)) + bytes(blob))
    return tuple(len(x) for x in t)


def item_names(exe_path):
    """The item names by item id - 1 (for the room messages' 06 n), or None."""
    t = tables(exe_path)
    return t[1] if t else None
