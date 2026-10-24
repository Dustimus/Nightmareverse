#!/usr/bin/env python3
"""Headless camera check: every camera of every converted RE1 room, screenshotted in SoH.

For each room, picks one floor spot per camera from the room's collision (the largest floor
triangle tagged with that camera), boots SoH straight into the room (NV_BOOT_ENTRANCE), tours
Link through the spots (NV_TOUR, soh/src/code/z_nv_dev.c) and screenshots each one. Reports
spots where Link found no floor or the active camera is not the expected one, and writes one
contact sheet per room plus an overview of all rooms.

Needs Xvfb, ImageMagick (import, convert, montage) and a SoH folder with the RE1 mod
installed. Screenshots hold RE1's backgrounds: keep them local, never commit them.

    python3 re1/tools/soh_check.py --soh-dir <SoH run folder> --scenes <rdt2scene --soh folder> \
        --out <folder> [--room re1_106 ...]
"""
import argparse
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import soh_tables  # noqa: E402

FLOOR_NY = int(0.7 * 32767)


def camera_spots(col_path):
    """{camera: (x, y, z)}: the centroid of the camera's largest floor triangle."""
    s = open(col_path).read()
    verts = [tuple(map(int, m)) for m in re.findall(r'<Vertex X="(-?\d+)" Y="(-?\d+)" Z="(-?\d+)"', s)]
    types = [int(m) for m in re.findall(r'<PolygonType Data1="(-?\d+)"', s)]
    best = {}
    for t, a, b, c, ny in re.findall(r'<Polygon Type="(\d+)" VertexA="(\d+)" VertexB="(\d+)" VertexC="(\d+)" '
                                     r'NormalX="-?\d+" NormalY="(-?\d+)"', s):
        if int(ny) < FLOOR_NY:
            continue
        cam = types[int(t)] & 0xFF
        pa, pb, pc = verts[int(a)], verts[int(b)], verts[int(c)]
        area = abs((pb[0] - pa[0]) * (pc[2] - pa[2]) - (pc[0] - pa[0]) * (pb[2] - pa[2]))
        if area > best.get(cam, (0,))[0]:
            best[cam] = (area, tuple((pa[i] + pb[i] + pc[i]) // 3 for i in range(3)))
    return {cam: spot for cam, (_, spot) in sorted(best.items())}


def run_room(soh_dir, entrance, spots, out, display, timeout=120):
    """Boot into the room, tour the spots, screenshot each. Returns {index: log line}."""
    env = dict(os.environ, SHIP_HOME=soh_dir, LIBGL_ALWAYS_SOFTWARE="1", DISPLAY=display,
               NV_BOOT_ENTRANCE="0x%X" % entrance,
               NV_TOUR=";".join("%d,%d,%d" % p for p in spots))
    log_path = os.path.join(out, "soh.log")
    log = open(log_path, "w")
    proc = subprocess.Popen(["./soh.elf"], cwd=soh_dir, env=env, stdout=log, stderr=subprocess.STDOUT)
    seen = {}
    t0 = time.time()
    try:
        while time.time() - t0 < timeout and proc.poll() is None:
            time.sleep(0.25)
            for line in open(log_path, errors="replace"):
                m = re.match(r"NVTOUR (\d+) (.*)", line)
                if m and int(m.group(1)) not in seen:
                    i = int(m.group(1))
                    seen[i] = m.group(2).strip()
                    subprocess.run(["import", "-window", "root", "-crop", "640x480+64+64",
                                    os.path.join(out, "cam_%02d.png" % i)], env=env, stderr=subprocess.DEVNULL)
                if line.startswith("NVTOUR done"):
                    return seen
        return seen
    finally:
        proc.terminate()
        try:
            proc.wait(5)
        except subprocess.TimeoutExpired:  # a hung game can ignore SIGTERM
            proc.kill()
            proc.wait()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--soh-dir", required=True, help="SoH folder (soh.elf, o2r files, mods/re1.o2r)")
    ap.add_argument("--scenes", required=True, help="rdt2scene.py --soh folder (collision is read from it)")
    ap.add_argument("--out", required=True)
    ap.add_argument("--room", action="append", default=[], help="only these rooms (e.g. re1_106)")
    ap.add_argument("--display", default=":79")
    ap.add_argument("--resume", action="store_true", help="skip rooms already checked into --out")
    a = ap.parse_args()

    xvfb = subprocess.Popen(["Xvfb", a.display, "-screen", "0", "1280x720x24"],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(2)
    problems = []
    sheets = []
    try:
        entr = 0
        entrances = {}
        for name, nspawn in soh_tables.rooms(a.scenes):
            entrances[name] = soh_tables.FIRST_ENTRANCE + entr
            entr += 4 * nspawn
        for name, entrance in entrances.items():
            if a.room and name not in a.room:
                continue
            col = os.path.join(a.scenes, "scenes", "shared", name + "_scene", name + "_col")
            spots = camera_spots(col)
            cams = list(spots)
            out = os.path.join(a.out, name)
            room_problems = os.path.join(out, "problems.txt")
            if a.resume and os.path.exists(room_problems):
                problems += [l for l in open(room_problems).read().splitlines() if l]
                print("%s: already checked" % name, flush=True)
                continue
            os.makedirs(out, exist_ok=True)
            first = len(problems)
            seen = run_room(a.soh_dir, entrance, [spots[c] for c in cams], out, a.display)
            labels = []
            for i, cam in enumerate(cams):
                line = seen.get(i)
                if line is None:
                    problems.append("%s cam %d: no screenshot (crash or timeout)" % (name, cam))
                    continue
                got = re.search(r"floor=(\d) cam=(-?\d+)", line)
                if got.group(1) != "1":
                    problems.append("%s cam %d: no floor at %s" % (name, cam, spots[cam]))
                if int(got.group(2)) != cam:
                    problems.append("%s cam %d: active camera is %s" % (name, cam, got.group(2)))
                labels.append(("cam_%02d.png" % i, "%s  camera %d" % (name, cam)))
            open(room_problems, "w").write("".join(p + "\n" for p in problems[first:]))
            print("%s: %d/%d cameras captured" % (name, len(labels), len(cams)), flush=True)
            if labels:
                sheet = os.path.join(a.out, name + ".png")
                cmd = ["montage"]
                for f, label in labels:
                    cmd += ["-label", label, os.path.join(out, f)]
                cmd += ["-tile", "4x", "-geometry", "320x240+4+4", "-background", "#111", "-fill", "#ddd",
                        "-pointsize", "14", sheet]
                subprocess.run(cmd, check=True)
                sheets.append(sheet)
    finally:
        xvfb.terminate()
    open(os.path.join(a.out, "problems.txt"), "w").write("\n".join(problems) + "\n")
    print("%d problems%s" % (len(problems), "".join("\n  " + p for p in problems)))


if __name__ == "__main__":
    main()
