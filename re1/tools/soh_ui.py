#!/usr/bin/env python3
"""RE1's status-screen pictures from the player's own files, for soh/src/code/z_nv_status.c.

"re1/ui/icons": the item icons (Data/ITEM_ALL.PIX: 40 x 30, 8 bpp, one after another; RE1 picks
one by its item record's byte 1, GameStart.cpp LoadItemImage: z_nv_status.c keeps that index) in
the status screen's colours (Data/STATUS.TIM, its third CLUT): "RICN", u16 count, u16 width,
height, then each icon as RGBA5551, big-endian.

"re1/ui/status": the status screen's own pictures, cut out of the same files at the places RE1
draws them from (MainMenu.cpp menu_draw_inventory, menu_draw_cursor, display_item_qty, the item
action box): "RSPR", u16 count, then per sprite u16 width, height and its RGBA5551 pixels
(big-endian; RE1's colour 0 is clear). z_nv_status.c knows them by their order in STATUS_SPRITES.

"re1/ui/file<n>": file n's pages (z_nv_files.c), from the player's item_m2/filem_*.pix (RE1's
g_filemPixNames order: l to z, then 00; each a table of u32 offsets to PAK-packed 4 bpp TIMs):
"RFIL", u16 pages, width, height, then each page as RGBA5551, big-endian.
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


# (file, CLUT, u, v, width, height), in z_nv_status.c's NvStSprite order
STATUS_SPRITES = [
    ("STATUS.TIM", 0, 0, 0, 48, 64),     # the tabs: Map, Radio, File, Exit (16 rows each)
    ("STATUS.TIM", 0, 0, 80, 48, 16),    # the tab cursor
    ("STATUS.TIM", 0, 48, 0, 48, 96),    # the action box: Equip, Use, Check, Combn (24 rows each)
    ("STATUS.TIM", 0, 48, 96, 48, 24),   # the action cursor
    ("STATUS.TIM", 0, 96, 0, 32, 88),    # the condition words (8 rows each)
    ("STATUS.TIM", 0, 128, 0, 8, 80),    # the green digits (8 rows each)
    ("STATUS.TIM", 0, 128, 224, 40, 30), # the slot cursor
    ("STATUS.TIM", 0, 168, 224, 40, 30), # the slot cursor, second item (Combine)
    ("STATUS.TIM", 0, 152, 112, 10, 8),  # the infinity mark
    ("BLUE.TIM", 0, 0, 0, 40, 30),       # an item slot's background
    ("STATFACE.TIM", 0, 32, 0, 30, 30),  # Jill's face
]


def _rgba5551(rgba):
    r, g, b, a = rgba
    return ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | (1 if a else 0)


def status_blob(re1):
    data = rdt.find_file(re1, "DATA")
    tims = {}
    out = bytearray(b"RSPR" + struct.pack("<H", len(STATUS_SPRITES)))
    for name, clut, u, v, w, h in STATUS_SPRITES:
        if name not in tims:
            tims[name] = emd.parse_tim(open(rdt.find_file(data, name), "rb").read(), 0)
        tim = tims[name]
        pal = [emd.psx_color(c) for c in tim.cluts[clut]]
        out += struct.pack("<2H", w, h)
        for y in range(v, v + h):
            for x in range(u, u + w):
                out += struct.pack(">H", _rgba5551(pal[tim.pixels[y * tim.width + x]]))
    return bytes(out)


def write_status(re1, out):
    """Write the status screen's pictures; returns how many (0 when the files aren't there)."""
    try:
        blob = status_blob(re1)
    except (OSError, IndexError):
        return 0
    d = os.path.join(out, "re1", "ui")
    os.makedirs(d, exist_ok=True)
    open(os.path.join(d, "status"), "wb").write(
        soh_scene._otr_header(soh_scene.LUS_BLOB) + struct.pack("<I", len(blob)) + blob)
    return len(STATUS_SPRITES)


FILE_PIX = ["filem_%s0.pix" % c for c in "lmnopqrstuvwxyz"] + ["filem_00.pix"]


def file_blob(re1, n):
    pix = open(rdt.find_file(rdt.find_file(re1, "ITEM_M2"), FILE_PIX[n]), "rb").read()
    pages = []
    for i in range(16):
        o = struct.unpack_from("<I", pix, 4 * i)[0]
        if o == 0 or o >= len(pix):
            break
        pages.append(emd.parse_tim(bytes(rdt.pak_depack(pix[o:])), 0))
    w, h = pages[0].width, pages[0].height
    out = bytearray(b"RFIL" + struct.pack("<3H", len(pages), w, h))
    for tim in pages:
        rgba = emd.tim_to_rgba(tim)[2]
        for k in range(w * h):
            out += struct.pack(">H", _rgba5551(rgba[k * 4:k * 4 + 4]))
    return bytes(out)


def write_files(re1, out):
    """Write each file's pages; returns how many files (0 when the files aren't there)."""
    d = os.path.join(out, "re1", "ui")
    done = 0
    for n in range(len(FILE_PIX)):
        try:
            blob = file_blob(re1, n)
        except (OSError, IndexError, ValueError):
            continue
        os.makedirs(d, exist_ok=True)
        open(os.path.join(d, "file%d" % n), "wb").write(
            soh_scene._otr_header(soh_scene.LUS_BLOB) + struct.pack("<I", len(blob)) + blob)
        done += 1
    return done


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
