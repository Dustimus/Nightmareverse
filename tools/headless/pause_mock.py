"""Pause-menu mock-up: alternates on the box's ceiling and floor, the page left as it is.

    python pause_mock.py <page.png 640x480> <out.png>

The page's outer border corners are measured from OoT's Items side. The page above is
painted on the ceiling face (its bottom edge on the page's top border, widening to the screen's
top edge), the page below on the floor face, as the side pages are on the walls. HUD and name
bar pixels (anything that isn't the wood) stay on top.
"""
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageEnhance, ImageFont

src = Image.open(sys.argv[1]).convert("RGB")
W, H = src.size
# the page's outer border (the frame), Items side, 640x480
L, R, T, B = 66, 574, 66, 390
board = src.crop((L, T, R, B))
bw, bh = board.size


def tint(img, rgb, amount):
    over = Image.new("RGB", img.size, rgb)
    return Image.blend(img, over, amount)


def coeffs(dst, srcpts):
    """PIL perspective coefficients mapping output (dst) points to input (src) points."""
    A, b = [], []
    for (x, y), (u, v) in zip(dst, srcpts):
        A.append([x, y, 1, 0, 0, 0, -u * x, -u * y]); b.append(u)
        A.append([0, 0, 0, x, y, 1, -v * x, -v * y]); b.append(v)
    return np.linalg.solve(np.array(A, float), np.array(b, float)).tolist()


def face(page, near_y, far_y, src_near_y, src_far_y, spread):
    """The page painted on a face from the page's border (near) to the screen edge (far)."""
    dst = [(L, near_y), (R, near_y), (R + spread, far_y), (L - spread, far_y)]
    s = [(0, src_near_y), (bw, src_near_y), (bw, src_far_y), (0, src_far_y)]
    img = page.transform((W, H), Image.PERSPECTIVE, coeffs(dst, s), Image.BICUBIC)
    mask = Image.new("L", (W, H), 0)
    ImageDraw.Draw(mask).polygon(dst, fill=255)
    return img, mask


# stand-ins: OoT's page recoloured (the real ones are the other games' pages)
mm = ImageEnhance.Brightness(tint(board, (120, 40, 160), 0.45)).enhance(0.75)
re = ImageEnhance.Brightness(tint(board.convert("L").convert("RGB"), (40, 80, 40), 0.35)).enhance(0.7)
font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", 22)
for img, text in ((mm, "MAJORA'S MASK: ITEMS"), (re, "RESIDENT EVIL: INVENTORY")):
    d = ImageDraw.Draw(img)
    w = d.textlength(text, font=font)
    y = bh - 40 if img is mm else 12
    d.text(((bw - w) / 2, y), text, font=font, fill=(235, 225, 255), stroke_width=2, stroke_fill=(0, 0, 0))

# the face shows the near ~35% of the page: the bottom of the page above, the top of the one below
top, tmask = face(mm, T, 0, bh, bh * 0.62, 70)
bot, bmask = face(re, B, H, 0, bh * 0.38, 70)

a = np.asarray(src).astype(int)
r, g, b_ = a[..., 0], a[..., 1], a[..., 2]
lum = (r + g + b_) / 3
wood = (r >= g) & (g >= b_ - 6) & (r - b_ > 18) & (r - g < 70) & (lum > 25) & (lum < 235)
hud = np.zeros(wood.shape, bool)  # where the HUD and the name bar are (640x480)
for x0, y0, x1, y1 in ((44, 38, 252, 84), (44, 84, 160, 110), (252, 26, 612, 122), (186, 388, 450, 442),
                       (52, 410, 140, 442)):
    hud[y0:y1, x0:x1] = True
keep = Image.fromarray(np.where(hud & ~wood, 255, 0).astype("uint8"))  # stays on top

out = src.copy()
out.paste(top, (0, 0), tmask)
out.paste(bot, (0, 0), bmask)
out.paste(src, (0, 0), Image.composite(keep, Image.new("L", (W, H), 0), Image.composite(tmask, bmask, tmask)))
out.save(sys.argv[2])
