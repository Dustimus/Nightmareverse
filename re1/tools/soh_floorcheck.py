#!/usr/bin/env python3
"""Headless floor check: Link walks every room's open floor, in SoH.

The door check only walks near doors; this one finds the places Link can't cross that he
should (a stray wall, a gap in the floor). Per room, points on a grid over the floor that keep
Link's radius clear of every wall; for each pair of neighbouring points with nothing between
them, Link is put on one and walked to the other (NV_WALK, soh/src/code/z_nv_dev.c). A walk
that ends short of its point is reported, with where he stopped.

    python3 re1/tools/soh_floorcheck.py --soh-dir <SoH folder> --scenes <rdt2scene --soh folder> \\
        --out <folder> [--room re1_106 ...] [--step 80]
"""
import argparse
import math
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import soh_doorcheck as dc  # noqa: E402
import soh_tables  # noqa: E402

WALK_FRAMES = 40  # frames allowed per walk (a step is ~80 units; Link runs ~6 a frame)
CLEARANCE = dc.LINK_RADIUS + 4
SHORT = 8  # OoT units: a walk ending this far short of its point is a problem (running past
# it is only Link's momentum)
VEER = 20  # or this far off the line


def clear(p, walls, d=CLEARANCE):
    return all(dc._seg_dist(p, a, b) >= d for a, b in walls)


def path_clear(p, q, walls):
    n = max(1, int(math.hypot(q[0] - p[0], q[1] - p[1]) / 5))
    return all(clear((p[0] + (q[0] - p[0]) * i / n, p[1] + (q[1] - p[1]) * i / n), walls, dc.LINK_RADIUS + 2)
               for i in range(n + 1))


def on_floor(p, tris):
    return any(dc._inside(p, t) for t in tris)


def pairs(tris, walls, step):
    xs = [v[0] for t in tris for v in t]
    zs = [v[1] for t in tris for v in t]
    pts = set()
    for x in range(int(min(xs)), int(max(xs)) + 1, step):
        for z in range(int(min(zs)), int(max(zs)) + 1, step):
            if on_floor((x, z), tris) and clear((x, z), walls):
                pts.add((x, z))
    out = []
    for p in sorted(pts):
        for q in ((p[0] + step, p[1]), (p[0], p[1] + step)):
            if q in pts and path_clear(p, q, walls):
                out.append((p, q))
    return out


def run_room(soh_dir, name, walks, out, display):
    steps = []
    for i, (p, q) in enumerate(walks):
        steps += ["pos:%d,%d" % p, "wait:3", "%d,%d" % q, "wait:2", "mark:%d" % i]
    steps.append("mark:9999")
    env = dict(os.environ, SHIP_HOME=soh_dir, LIBGL_ALWAYS_SOFTWARE="1", DISPLAY=display,
               NV_BOOT_ENTRANCE=name, NV_WALK=";".join(steps), NV_WALK_TIMEOUT=str(WALK_FRAMES))
    log_path = os.path.join(out, name + ".log")
    proc = subprocess.Popen(["./soh.elf"], cwd=soh_dir, env=env, stdout=open(log_path, "w"),
                            stderr=subprocess.STDOUT)
    marks = {}
    t0 = time.time()
    try:
        while time.time() - t0 < 60 + 3 * len(walks) and proc.poll() is None:
            time.sleep(1)
            for line in open(log_path, errors="replace"):
                m = re.match(r"NVMARK (\d+) scene=\d+ pos=(-?\d+),(-?\d+),(-?\d+)", line)
                if m:
                    marks[int(m.group(1))] = (int(m.group(2)), int(m.group(4)))
            if 9999 in marks:
                break
    finally:
        proc.terminate()
        try:
            proc.wait(5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
    return marks


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--soh-dir", required=True)
    ap.add_argument("--scenes", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--room", action="append", default=[])
    ap.add_argument("--step", type=int, default=80)
    ap.add_argument("--display", default=":74")
    a = ap.parse_args()
    os.makedirs(a.out, exist_ok=True)
    names = a.room or [n for n, _ in soh_tables.rooms(a.scenes)]
    xvfb = subprocess.Popen(["Xvfb", a.display, "-screen", "0", "1280x720x24"],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(2)
    report = []
    try:
        for name in names:
            col = os.path.join(a.scenes, "scenes", "shared", name + "_scene", name + "_col")
            tris, walls = dc.floor_tris(col), dc.wall_segs(col)
            walks = pairs(tris, walls, a.step)
            marks = run_room(a.soh_dir, name, walks, a.out, a.display)
            bad = []
            for i, (p, q) in enumerate(walks):
                if i not in marks:
                    bad.append("walk %d %s -> %s: run ended early" % (i, p, q))
                    break
                e = marks[i]
                length = math.hypot(q[0] - p[0], q[1] - p[1])
                ux, uz = (q[0] - p[0]) / length, (q[1] - p[1]) / length
                along = (e[0] - p[0]) * ux + (e[1] - p[1]) * uz
                off = abs((e[0] - p[0]) * uz - (e[1] - p[1]) * ux)
                if along < length - SHORT or off > VEER:
                    bad.append("walk %s -> %s stopped at %s" % (p, q, e))
            line = "%s: %d walks, %d problems" % (name, len(walks), len(bad))
            print(line)
            for b in bad:
                print("  " + b)
            sys.stdout.flush()
            report += [line] + ["  " + b for b in bad]
    finally:
        xvfb.terminate()
    open(os.path.join(a.out, "floor.txt"), "w").write("\n".join(report) + "\n")


if __name__ == "__main__":
    main()
