#!/usr/bin/env python3
"""RE1's status-screen pictures from the player's own files, for soh/src/code/z_nv_status.c.

"re1/ui/icons": the item icons (Data/ITEM_ALL.PIX: 40 x 30, 8 bpp, one after another; RE1 picks
one by its item record's byte 1, GameStart.cpp LoadItemImage: z_nv_status.c keeps that index) in
the status screen's colours (Data/STATUS.TIM, its third CLUT): "RICN", u16 count, u16 width,
height, then each icon as RGBA5551, big-endian.
"""
import os
import struct

import emd
import rdt
import soh_scene

ICON_W, ICON_H = 40, 30


def icons_blob(re1):
    data = rdt.find_file(re1, "DATA")
    pix = open(rdt.find_file(data, "ITEM_ALL.PIX"), "rb").read()
    tim = emd.parse_tim(open(rdt.find_file(data, "STATUS.TIM"), "rb").read(), 0)
    clut = [emd.psx_color(c) for c in tim.cluts[2]]
    count = len(pix) // (ICON_W * ICON_H)
    out = bytearray(b"RICN" + struct.pack("<3H", count, ICON_W, ICON_H))
    for k in range(count):
        icon = pix[k * ICON_W * ICON_H:(k + 1) * ICON_W * ICON_H]
        for b in icon:
            r, g, bl, _a = clut[b]
            out += struct.pack(">H", ((r >> 3) << 11) | ((g >> 3) << 6) | ((bl >> 3) << 1) | 1)
    return bytes(out)


def write_icons(re1, out):
    """Write the icons into the SoH folder; returns how many (0 when the files aren't there)."""
    try:
        blob = icons_blob(re1)
    except (OSError, IndexError):
        return 0
    d = os.path.join(out, "re1", "ui")
    os.makedirs(d, exist_ok=True)
    open(os.path.join(d, "icons"), "wb").write(
        soh_scene._otr_header(soh_scene.LUS_BLOB) + struct.pack("<I", len(blob)) + blob)
    return struct.unpack_from("<H", blob, 4)[0]
