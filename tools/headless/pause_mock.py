"""Pause-menu mock-up (NV_PAUSE_MENU.md): one side's pages as a flat column that loops.

    python tools/headless/pause_mock.py <clean.png> <with_hud.png> <out.png> [<out.gif>]

Inputs are the same pause page captured twice, 640x480 (pause_shots.sh, cropped): once with the
HUD switched off (a temporary local build: Interface_Draw skipped while capturing) and once as
normal. Their difference is the HUD, laid back over the result exactly.

The pages stand in a column, flat, one above the other, and the column loops. At rest the page in
front is exactly as OoT has it (nothing resized). The bottom of the page above shows over its top
border and the top of the page below under its bottom border, flat, in the ceiling and floor space
already there. Moving the cursor past the top or bottom row slides the column one whole page.
The GIF shows a slide each way and the loop. The other games' pages are OoT's page recoloured,
as stand-ins.
"""
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageEnhance, ImageFont

clean = Image.open(sys.argv[1]).convert("RGB")
with_hud = Image.open(sys.argv[2]).convert("RGB")
W, H = clean.size
L, R, T, B = 73, 562, 75, 388  # the page's outer border (the frame), Items side, measured per row on the clean capture
GAP = 60                        # wood between pages: at rest the neighbours show only a thin
                                # strip at the screen's very top and bottom, clear of the page
board = clean.crop((L, T, R, B))
bw, bh = board.size
STEP = bh + GAP
NAME = (190, 389, 447, 441)     # the name bar under the page (kaleido's own, stays in front)

font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 20)


def stand_in(rgb, label, grey=False):
    base = board.convert("L").convert("RGB") if grey else board
    img = ImageEnhance.Brightness(Image.blend(base, Image.new("RGB", board.size, rgb), 0.45)).enhance(0.85)
    d = ImageDraw.Draw(img)
    title_bg = img.getpixel((bw // 2, 6))
    d.rectangle((110, 6, bw - 110, 34), fill=title_bg)  # its name over "SELECT ITEM"
    for y in (8,):  # as the title (seen once it has slid in)
        w = d.textlength(label, font=font)
        d.text(((bw - w) / 2, y), label, font=font, fill=(240, 235, 255), stroke_width=2, stroke_fill=(0, 0, 0))
    return img


# the column, top to bottom (it loops): Majora's Mask's above OoT's, Resident Evil's below
column = [stand_in((120, 40, 160), "MAJORA'S MASK: ITEMS"), board,
          stand_in((40, 90, 40), "RESIDENT EVIL: INVENTORY", grey=True)]
HOME = 1

diff = np.abs(np.asarray(with_hud).astype(int) - np.asarray(clean).astype(int)).sum(axis=2)
hud_mask = Image.fromarray(np.where(diff > 40, 255, 0).astype("uint8"))
c = np.asarray(clean).astype(int)
woodish = (c[..., 0] >= c[..., 2] + 8)  # the name bar is blue-grey; the floor round it is wood
name_mask = np.zeros((H, W), "uint8")
x0, y0, x1, y1 = NAME
name_mask[y0:y1, x0:x1] = np.where(woodish[y0:y1, x0:x1], 0, 255)
name_mask = Image.fromarray(name_mask)

strip = Image.new("L", (W, H), 0)
ImageDraw.Draw(strip).rectangle((L, 0, R - 1, H), fill=255)
wood = clean.crop((L, 0, R, 60))  # the ceiling's wood, as the column's background


def frame(cur, offset):
    """Page `cur` in front, the column moved down by `offset` pixels (mid-slide)."""
    col = Image.new("RGB", (W, H))
    for y in range(0, H, wood.size[1]):
        col.paste(wood, (L, y))
    for k in (-2, -1, 0, 1, 2):
        col.paste(column[(cur + k) % len(column)], (L, T + k * STEP + int(round(offset))))
    out = clean.copy()
    out.paste(col, (0, 0), strip)
    out.paste(clean, (0, 0), name_mask)
    out.paste(with_hud, (0, 0), hud_mask)
    return out


frame(HOME, 0).save(sys.argv[3])
if len(sys.argv) > 4:
    frames, durs = [], []

    def hold(img, ms):
        frames.append(img)
        durs.append(ms)

    def slide(cur, direction):
        """direction -1: up, to the page above (the column moves down); +1: down."""
        for i in range(1, 13):
            t = i / 12
            ease = 1 - (1 - t) ** 3
            hold(frame(cur, -direction * STEP * ease), 35)
        return (cur + direction) % len(column)

    cur = HOME
    hold(frame(cur, 0), 1400)
    cur = slide(cur, -1); hold(frame(cur, 0), 1200)  # up: Majora's Mask's items
    cur = slide(cur, +1); hold(frame(cur, 0), 900)   # down: back to OoT's
    cur = slide(cur, +1); hold(frame(cur, 0), 1200)  # down: Resident Evil's inventory
    cur = slide(cur, +1); hold(frame(cur, 0), 1400)  # down again: loops round to Majora's Mask
    frames[0].save(sys.argv[4], save_all=True, append_images=frames[1:], duration=durs, loop=0)
