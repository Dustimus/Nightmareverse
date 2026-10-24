#!/usr/bin/env python3
"""Headless door check: every door of every converted RE1 room, in SoH.

Per door, in one SoH run per room (NV_WALK, soh/src/code/z_nv_dev.c):
  1. warp into the room, put Link on the floor nearest the door and walk him into it for a
     while: the room must NOT change (RE1's doors never open by walking into them)
  2. press A: the game must go to the door's target room, with Link at the target's start
     position for that door (RE1's arrival point), facing RE1's arrival direction

Writes a report (doors.txt) and screenshots of each arrival. Needs Xvfb and ImageMagick, and
a SoH folder with the RE1 mod installed; screenshots show RE1's backgrounds (keep local).

    python3 re1/tools/soh_doorcheck.py --soh-dir <SoH folder> --scenes <rdt2scene --soh folder> \
        --out <folder> [--room re1_106 ...]
"""
import argparse
import os
import re
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import soh_check  # noqa: E402
import soh_tables  # noqa: E402

WALK_IN_FRAMES = 60  # frames spent pushing into the door
ARRIVE_FRAMES = 60  # frames after A for the fade and the next room
POS_TOLERANCE = 6  # OoT units between the arrival and the start position (OoT pushes out of walls)


def doors(scenes, name):
    d = open(os.path.join(scenes, "scenes", "shared", name + "_scene", name + "_doors"), "rb").read()[64 + 4:]
    n = struct.unpack_from("<I", d, 4)[0]
    out = []
    for i in range(n):
        x0, z0, x1, z1, flags, anim, spawn, _, _, target = struct.unpack_from("<4fBBHBB2x24s", d, 8 + 48 * i)
        out.append(dict(rect=(x0, z0, x1, z1), flags=flags, anim=anim, spawn=spawn,
                        target=target.rstrip(b"\0").decode()[:-len("_scene")]))
    return out


def start_positions(scenes, name):
    xml = open(os.path.join(scenes, "scenes", "shared", name + "_scene", name + "_scene")).read()
    # in order: a start position's index is its place in the list
    return [(int(m[0]), int(m[1]), int(m[2]) & 0xFFFF)
            for m in re.findall(r'<StartPositionEntry Id="\d+" PosX="(-?\d+)" PosY="-?\d+" PosZ="(-?\d+)" '
                                r'RotX="-?\d+" RotY="(-?\d+)"', xml)]


def floor_points(col_path):
    """Sample points of the room's floor (as soh_check.camera_spots samples), with their wall distance."""
    s = open(col_path).read()
    verts = [tuple(map(int, m)) for m in re.findall(r'<Vertex X="(-?\d+)" Y="(-?\d+)" Z="(-?\d+)"', s)]
    pts = []
    for a, b, c, ny in re.findall(r'<Polygon Type="\d+" VertexA="(\d+)" VertexB="(\d+)" VertexC="(\d+)" '
                                  r'NormalX="-?\d+" NormalY="(-?\d+)"', s):
        if int(ny) < soh_check.FLOOR_NY:
            continue
        pa, pb, pc = verts[int(a)], verts[int(b)], verts[int(c)]
        for i in range(1, 4):
            for j in range(1, 4 - i):
                k = 4 - i - j
                pts.append(tuple((pa[n] * i + pb[n] * j + pc[n] * k) / 4 for n in (0, 2)))
    return pts


def approach(rect, pts):
    """Where a player would come from: the floor nearest the door's middle, then 25 units back
    from the door along that line (snapped to the floor), so the walk in is straight."""
    x0, z0, x1, z1 = rect
    cx, cz = (x0 + x1) / 2, (z0 + z1) / 2
    qx, qz = min(pts, key=lambda p: (p[0] - cx) ** 2 + (p[1] - cz) ** 2)
    dx, dz = qx - cx, qz - cz
    n = (dx * dx + dz * dz) ** 0.5 or 1.0
    tx, tz = qx + dx / n * 25, qz + dz / n * 25
    return min(pts, key=lambda p: (p[0] - tx) ** 2 + (p[1] - tz) ** 2)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--soh-dir", required=True)
    ap.add_argument("--scenes", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--room", action="append", default=[])
    ap.add_argument("--display", default=":79")
    a = ap.parse_args()

    rooms = soh_tables.rooms(a.scenes)
    scene_id, entrance = {}, {}
    e = soh_tables.FIRST_ENTRANCE
    for i, (name, nspawn) in enumerate(rooms):
        scene_id[name] = soh_tables.FIRST_SCENE + i
        entrance[name] = e
        e += 4 * nspawn
    os.makedirs(a.out, exist_ok=True)
    xvfb = subprocess.Popen(["Xvfb", a.display, "-screen", "0", "1280x720x24"],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    time.sleep(2)
    report, failures = [], []
    try:
        for name, _ in rooms:
            if a.room and name not in a.room:
                continue
            ds = doors(a.scenes, name)
            pts = floor_points(os.path.join(a.scenes, "scenes", "shared", name + "_scene", name + "_col"))
            steps = []
            for i, d in enumerate(ds):
                x0, z0, x1, z1 = d["rect"]
                px, pz = approach(d["rect"], pts)
                # as a player would: stand on the floor nearest the door, push into it (aiming
                # past its middle, so the walk never ends with him turning on the spot), then
                # back at the door, face it and press A (pushing can slide him along the wall)
                cx, cz = (x0 + x1) / 2, (z0 + z1) / 2
                qx, qz = min(pts, key=lambda p: (p[0] - cx) ** 2 + (p[1] - cz) ** 2)
                tx, tz = cx + (cx - qx) * 3, cz + (cz - qz) * 3
                steps += ["entr:0x%X" % entrance[name], "wait:10", "pos:%d,%d" % (qx, qz), "wait:10",
                          "%d,%d" % (tx, tz), "wait:10", "pos:%d,%d" % (qx, qz), "wait:10",
                          "face:%d,%d" % (cx, cz), "wait:2", "mark:%d" % (2 * i), "a",
                          "wait:%d" % ARRIVE_FRAMES, "mark:%d" % (2 * i + 1)]
            steps.append("mark:9999")
            out = os.path.join(a.out, name)
            os.makedirs(out, exist_ok=True)
            env = dict(os.environ, SHIP_HOME=a.soh_dir, LIBGL_ALWAYS_SOFTWARE="1", DISPLAY=a.display,
                       NV_BOOT_ENTRANCE="0x%X" % entrance[name], NV_WALK=";".join(steps),
                       NV_WALK_TIMEOUT=str(WALK_IN_FRAMES))
            log_path = os.path.join(out, "soh.log")
            proc = subprocess.Popen(["./soh.elf"], cwd=a.soh_dir, env=env, stdout=open(log_path, "w"),
                                    stderr=subprocess.STDOUT)
            marks, shot = {}, set()
            t0 = time.time()
            try:
                while time.time() - t0 < 60 + 25 * len(ds) and proc.poll() is None:
                    time.sleep(0.25)
                    lines = open(log_path, errors="replace").read().splitlines()
                    for line in lines:
                        m = re.match(r"NVMARK (\d+) scene=(\d+) pos=(-?\d+),(-?\d+),(-?\d+) yaw=(\d+)", line)
                        if m:
                            marks[int(m.group(1))] = tuple(int(v) for v in m.groups()[1:])
                            k = int(m.group(1))
                            if k % 2 == 1 and k not in shot and k != 9999:
                                shot.add(k)
                                subprocess.run(["import", "-window", "root", "-crop", "640x480+64+64",
                                                os.path.join(out, "door_%02d.png" % (k // 2))], env=env,
                                               stderr=subprocess.DEVNULL)
                    if 9999 in marks:
                        break
            finally:
                proc.terminate()
                try:
                    proc.wait(5)
                except subprocess.TimeoutExpired:
                    proc.kill()
                    proc.wait()
            log = open(log_path, errors="replace").read()
            for i, d in enumerate(ds):
                before, after = marks.get(2 * i), marks.get(2 * i + 1)
                tgt = d["target"]
                label = "%s door %d -> %s spawn %d" % (name, i, tgt, d["spawn"])
                problems = []
                if not d["flags"] & 0x80:
                    report.append("%s: RE1 flags 0x%02X (never opens), skipped" % (label, d["flags"]))
                    continue
                if before is None or after is None:
                    problems.append("run ended early (crash or timeout)")
                else:
                    if before[0] != scene_id[name]:
                        problems.append("walking into the door changed the room (scene %d)" % before[0])
                    if tgt not in scene_id:
                        problems.append("target room not converted")
                    elif after[0] != scene_id[tgt]:
                        problems.append("A did not reach the target (scene %d)" % after[0])
                    else:
                        sx, sz, syaw = start_positions(a.scenes, tgt)[d["spawn"]]
                        dist = ((after[1] - sx) ** 2 + (after[3] - sz) ** 2) ** 0.5
                        if dist > POS_TOLERANCE:
                            problems.append("arrived %.0f units from the start position" % dist)
                        if abs(((after[4] - syaw + 0x8000) & 0xFFFF) - 0x8000) > 0x400:
                            problems.append("arrived facing %d, not %d" % (after[4], syaw))
                report.append("%s: %s" % (label, "; ".join(problems) if problems else "ok"))
                failures += ["%s: %s" % (label, p) for p in problems]
            print("\n".join(r for r in report if r.startswith(name + " ")), flush=True)
    finally:
        xvfb.terminate()
    open(os.path.join(a.out, "doors.txt"), "w").write("\n".join(report) + "\n")
    print("%d doors checked, %d problems" % (len(report), len(failures)))


if __name__ == "__main__":
    main()
