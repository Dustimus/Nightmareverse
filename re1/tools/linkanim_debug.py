#!/usr/bin/env python3
"""Draws the NVLA lines z_nv_linkanim.c logs (NV_LOG_LA=1): Jill's skeleton and Link's retargeted
one, side by side per frame, from the front and from the side, as stick figures (Link's model
space: x across, y up, z forward). For checking the retargeting by eye.

    python3 re1/tools/linkanim_debug.py soh.log out.png [every]
"""
import sys

from PIL import Image, ImageDraw

JILL_PARENTS = [-1, 0, 0, 2, 3, 4, 2, 6, 7, 0, 9, 10, 0, 12, 13]
# colours by role: torso/head white, right leg red, left leg blue, right arm orange, left arm cyan
JILL_COL = {1: "white", 2: "white", 3: "red", 4: "red", 5: "red", 6: "blue", 7: "blue", 8: "blue",
            9: "orange", 10: "orange", 11: "orange", 12: "cyan", 13: "cyan", 14: "cyan"}
# Link: PLAYER_LIMB_* (z64player.h)
LINK_COL = {4: "red", 5: "red", 6: "red", 7: "blue", 8: "blue", 9: "blue", 14: "cyan", 15: "cyan",
            16: "cyan", 17: "orange", 18: "orange", 19: "orange", 11: "white", 10: "white", 3: "white"}


def parse(path):
    out = []
    for line in open(path, errors="replace"):
        if not line.startswith("NVLA anim"):
            continue
        w = line.split()
        anim, step, frame = int(w[2]), int(w[4]), int(w[6])
        j = w.index("jill")
        k = w.index("link")
        jill = [tuple(int(v) for v in s.split(",")) for s in w[j + 1:k]]
        link = {}
        for s in w[k + 1:]:
            i, p, rest = s.split(":")
            link[int(i)] = (int(p), tuple(int(v) for v in rest.split(",")))
        out.append((anim, step, frame, jill, link))
    return out


def draw(d, segs, ox, oy, scale, view):
    for a, b, col in segs:
        if view == "front":
            p, q = (a[0], a[1]), (b[0], b[1])
        else:
            p, q = (a[2], a[1]), (b[2], b[1])
        d.line([(ox + p[0] * scale, oy - p[1] * scale), (ox + q[0] * scale, oy - q[1] * scale)], fill=col, width=2)


def main():
    rows = parse(sys.argv[1])
    every = int(sys.argv[3]) if len(sys.argv) > 3 else 6
    rows = rows[::every]
    W, H = 120, 150
    img = Image.new("RGB", (W * 4, H * len(rows)), (25, 25, 25))
    d = ImageDraw.Draw(img)
    for r, (anim, step, frame, jill, link) in enumerate(rows):
        js = [(jill[JILL_PARENTS[i]], jill[i], JILL_COL.get(i, "gray")) for i in range(1, len(jill))]
        ls = [(link[p][1], pos, LINK_COL.get(i, "gray")) for i, (p, pos) in link.items() if p in link]
        jy = min(v[1] for v in jill)
        ly = min(v[1][1] for v in link.values())
        for c, (segs, base, scale, who) in enumerate(((js, jy, 0.04, "jill"), (ls, ly, 0.025, "link"))):
            for v, view in enumerate(("front", "side")):
                col = c * 2 + v
                draw(d, segs, col * W + W / 2, r * H + H - 10 + base * scale, scale, view)
                d.text((col * W + 2, r * H + 2), "%s %s a%d s%d" % (who, view, anim, step), fill=(180, 180, 180))
    img.save(sys.argv[2])


if __name__ == "__main__":
    main()
