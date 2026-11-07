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

"re1/ui/sheets": whole pictures that z_nv_box.c draws pieces of at RE1's own coordinates (the
decomp's menu tables): STATUS.TIM and ITEMBOXN.TIM, each in its first CLUT: "RSHT", u16 count,
then per sheet u16 width, height and its RGBA5551 pixels (big-endian; colour 0 clear).

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
    ("STATFACE.TIM", 0, 32, 0, 30, 30),  # the face: Link's portrait (re1/assets) over Jill's
]

# the user's own portrait of Link in RE1's portrait style, in the face's place (32 x 32: its
# middle 30 x 30, the size of RE1's faces)
LINK_PORTRAIT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "link_portrait.png")


def _link_face():
    try:
        from PIL import Image
        im = Image.open(LINK_PORTRAIT).convert("RGBA")
    except (OSError, ImportError):
        return None
    w, h = im.size
    im = im.crop(((w - 30) // 2, (h - 30) // 2, (w - 30) // 2 + 30, (h - 30) // 2 + 30))
    return list(im.getdata())


def _rgba5551(rgba):
    r, g, b, a = rgba
    return ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | (1 if a else 0)


def status_blob(re1):
    data = rdt.find_file(re1, "DATA")
    tims = {}
    out = bytearray(b"RSPR" + struct.pack("<H", len(STATUS_SPRITES)))
    for name, clut, u, v, w, h in STATUS_SPRITES:
        if name == "STATFACE.TIM" and _link_face() is not None:
            out += struct.pack("<2H", w, h)
            for rgba in _link_face():
                out += struct.pack(">H", _rgba5551(rgba))
            continue
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


SHEETS = ["STATUS.TIM", "ITEMBOXN.TIM"]  # z_nv_box.c's NV_SHEET_* order


def sheets_blob(re1):
    data = rdt.find_file(re1, "DATA")
    out = bytearray(b"RSHT" + struct.pack("<H", len(SHEETS)))
    for name in SHEETS:
        tim = emd.parse_tim(open(rdt.find_file(data, name), "rb").read(), 0)
        w, h, rgba = emd.tim_to_rgba(tim, {0: 0, 1: 0, 2: 0, 3: 0})
        out += struct.pack("<2H", w, h)
        for k in range(w * h):
            out += struct.pack(">H", _rgba5551(rgba[k * 4:k * 4 + 4]))
    return bytes(out)


def write_sheets(re1, out):
    """Write the whole sheets; returns how many (0 when the files aren't there)."""
    try:
        blob = sheets_blob(re1)
    except (OSError, IndexError):
        return 0
    d = os.path.join(out, "re1", "ui")
    os.makedirs(d, exist_ok=True)
    open(os.path.join(d, "sheets"), "wb").write(
        soh_scene._otr_header(soh_scene.LUS_BLOB) + struct.pack("<I", len(blob)) + blob)
    return len(SHEETS)


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


# The Japanese PC release's file backdrops (Biohazard.exe's reader: the decomp's MainMenu.cpp,
# jpn_file_screen_render): a scanned-paper picture behind each file's pages. Its 14 sheets, and
# which one each of the 16 files shows (0x004af1d8)
FILEI_NAMES = ["filei%02d.tim" % n for n in (3, 4, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17)]
FILEI_FOR_FILE = [1, 0, 2, 3, 8, 12, 3, 5, 7, 0, 4, 11, 6, 9, 10, 13]
# a sheet is 256x120: the picture's top 208x120 on the left, and its bottom strip cut into the
# right-hand column (0x004af478: four 48x24 slices and one 16x24, one under the other); put
# together, a 208x144 picture the reader draws at (48, 48)
BACKDROP_W, BACKDROP_H = 208, 144
_BACKDROP_STRIP = [(208, 0, 48), (208, 24, 48), (208, 48, 48), (208, 72, 48), (208, 96, 16)]


def backdrop_blob(jpn, n):
    """File n's Japanese backdrop: "RFIB", u16 width, height, then RGBA5551 (big-endian)."""
    sheet = open(rdt.find_file(rdt.find_file(jpn, "ITEM_M2"), FILEI_NAMES[FILEI_FOR_FILE[n]]), "rb").read()
    w, h, rgba = emd.tim_to_rgba(emd.parse_tim(sheet, 0))

    def px(x, y):
        k = (y * w + x) * 4
        return rgba[k:k + 4] if (0 <= x < w and 0 <= y < h) else b"\0\0\0\0"

    out = bytearray(b"RFIB" + struct.pack("<2H", BACKDROP_W, BACKDROP_H))
    for y in range(BACKDROP_H):
        for x in range(BACKDROP_W):
            if y < 120:
                c = px(x, y)
            else:
                k, sx = min(x // 48, 4), x % 48
                ux, uy, uw = _BACKDROP_STRIP[k]
                c = px(ux + sx, uy + (y - 120)) if sx < uw else b"\0\0\0\0"
            out += struct.pack(">H", _rgba5551(c))
    return bytes(out)


def write_files(re1, out, jpn=None):
    """Write each file's pages (and, given the Japanese PC release's folder, each file's backdrop);
    returns how many files (0 when the files aren't there)."""
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
        if jpn:
            try:
                blob = backdrop_blob(jpn, n)
            except (OSError, IndexError, ValueError):
                continue
            open(os.path.join(d, "fileb%d" % n), "wb").write(
                soh_scene._otr_header(soh_scene.LUS_BLOB) + struct.pack("<I", len(blob)) + blob)
    return done


# The lab's slide projector (504; the decomp's LabSlides.cpp): Data/slide.tim, 640x256 8-bit,
# its six slides in 192x128 cells, slide i at x (i / 2) * 192, y (i % 2) * 128 drawn with CLUT i
# (lab_slides_update's texture page and texV). RE1 shows 191x127 of each.
SLIDE_W, SLIDE_H = 191, 127


def slides_blob(re1):
    """"RSLD", u16 count, width, height, then each slide as RGBA5551, big-endian."""
    tim = emd.parse_tim(open(rdt.find_file(rdt.find_file(re1, "DATA"), "slide.tim"), "rb").read(), 0)
    out = bytearray(b"RSLD" + struct.pack("<3H", len(tim.cluts), SLIDE_W, SLIDE_H))
    for i in range(len(tim.cluts)):
        w, h, rgba = emd.tim_to_rgba(tim, {p: i for p in range(8)})
        x0, y0 = (i // 2) * 192, (i % 2) * 128
        for y in range(SLIDE_H):
            for x in range(SLIDE_W):
                k = ((y0 + y) * w + x0 + x) * 4
                out += struct.pack(">H", _rgba5551(rgba[k:k + 3] + b"\xff"))
    return bytes(out)


def write_slides(re1, out):
    """Write the projector's slides (re1/ui/slides); how many (0 when the file isn't there)."""
    try:
        blob = slides_blob(re1)
    except (OSError, IndexError, ValueError):
        return 0
    d = os.path.join(out, "re1", "ui")
    os.makedirs(d, exist_ok=True)
    open(os.path.join(d, "slides"), "wb").write(
        soh_scene._otr_header(soh_scene.LUS_BLOB) + struct.pack("<I", len(blob)) + blob)
    return struct.unpack_from("<H", blob, 4)[0]


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
