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
# OoT units between the arrival and the start position: RE1 puts Jill right by the door, and
# OoT pushes Link (wider) clear of the wall on arrival, by up to about his radius
POS_TOLERANCE = 16


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


def floor_tris(col_path):
    """The room's floor triangles, as ((x, z) * 3)."""
    s = open(col_path).read()
    verts = [tuple(map(int, m)) for m in re.findall(r'<Vertex X="(-?\d+)" Y="(-?\d+)" Z="(-?\d+)"', s)]
    out = []
    for a, b, c, ny in re.findall(r'<Polygon Type="\d+" VertexA="(\d+)" VertexB="(\d+)" VertexC="(\d+)" '
                                  r'NormalX="-?\d+" NormalY="(-?\d+)"', s):
        if int(ny) >= soh_check.FLOOR_NY:
            out.append(tuple((verts[int(i)][0], verts[int(i)][2]) for i in (a, b, c)))
    return out


def _closest_on_tri(p, tri):
    def side(a, b):
        return (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])

    d = [side(tri[i], tri[(i + 1) % 3]) for i in range(3)]
    if all(v >= 0 for v in d) or all(v <= 0 for v in d):
        return p
    best = None
    for i in range(3):
        a, b = tri[i], tri[(i + 1) % 3]
        dx, dz = b[0] - a[0], b[1] - a[1]
        ll = dx * dx + dz * dz
        u = 0.0 if ll == 0 else max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dz) / ll))
        q = (a[0] + u * dx, a[1] + u * dz)
        if best is None or (q[0] - p[0]) ** 2 + (q[1] - p[1]) ** 2 < (best[0] - p[0]) ** 2 + (best[1] - p[1]) ** 2:
            best = q
    return best


def closest_floor(p, tris):
    return min((_closest_on_tri(p, t) for t in tris), key=lambda q: (q[0] - p[0]) ** 2 + (q[1] - p[1]) ** 2)


LINK_RADIUS = 18  # adult Link's wall radius: kept between his centre and the floor's edge or a wall


def _inside(p, tri):
    def side(a, b):
        return (b[0] - a[0]) * (p[1] - a[1]) - (b[1] - a[1]) * (p[0] - a[0])

    d = [side(tri[i], tri[(i + 1) % 3]) for i in range(3)]
    return all(v >= 0 for v in d) or all(v <= 0 for v in d)


def _seg_dist(p, a, b):
    dx, dz = b[0] - a[0], b[1] - a[1]
    ll = dx * dx + dz * dz
    u = 0.0 if ll == 0 else max(0.0, min(1.0, ((p[0] - a[0]) * dx + (p[1] - a[1]) * dz) / ll))
    return ((a[0] + u * dx - p[0]) ** 2 + (a[1] + u * dz - p[1]) ** 2) ** 0.5


def wall_segs(col_path):
    """The walls' footprints, as ((x, z), (x, z)) segments (each wall quad's horizontal edges)."""
    s = open(col_path).read()
    verts = [tuple(map(int, m)) for m in re.findall(r'<Vertex X="(-?\d+)" Y="(-?\d+)" Z="(-?\d+)"', s)]
    out = set()
    for a, b, c, ny in re.findall(r'<Polygon Type="\d+" VertexA="(\d+)" VertexB="(\d+)" VertexC="(\d+)" '
                                  r'NormalX="-?\d+" NormalY="(-?\d+)"', s):
        if abs(int(ny)) >= soh_check.FLOOR_NY:
            continue
        t = [verts[int(i)] for i in (a, b, c)]
        for i in range(3):
            p, q = t[i], t[(i + 1) % 3]
            if p[1] == q[1] and (p[0], p[2]) != (q[0], q[2]):
                out.add(tuple(sorted(((p[0], p[2]), (q[0], q[2])))))
    return list(out)


def door_stand(rect, tris, walls=()):
    """Where a player stands to open the door, and the point he faces (the door's middle): of
    the floor within 50 units of the door, the spot nearest the door's rectangle that is at
    least Link's radius from the floor's edge and the walls (so OoT neither pushes him off nor
    lets him fall through it)."""
    x0, z0, x1, z1 = rect
    c = ((x0 + x1) / 2, (z0 + z1) / 2)
    edges = {}
    for t in tris:
        for i in range(3):
            e = tuple(sorted((t[i], t[(i + 1) % 3])))
            edges[e] = edges.get(e, 0) + 1
    boundary = [e for e, n in edges.items() if n == 1] + walls
    near_tris = [t for t in tris if any(abs(p[0] - c[0]) < 120 and abs(p[1] - c[1]) < 120 for p in t)] or tris
    cand = []
    for gx in range(int(x0) - 50, int(x1) + 51, 2):
        for gz in range(int(z0) - 50, int(z1) + 51, 2):
            p = (gx, gz)
            if any(_inside(p, t) for t in near_tris):
                edge = min((_seg_dist(p, a, b) for a, b in boundary), default=1e9)
                dr = ((max(x0 - gx, 0, gx - x1)) ** 2 + (max(z0 - gz, 0, gz - z1)) ** 2) ** 0.5
                cand.append((dr, (gx - c[0]) ** 2 + (gz - c[1]) ** 2, edge, p))
    best = None
    # the clearance relaxes where the way to the door is narrower than Link (the foyer's stairs,
    # between the banisters), as long as the spot stays within his reach of the door
    for clearance in (LINK_RADIUS, 9, 5):
        ok = [k for k in cand if k[2] >= clearance]
        if ok:
            k = min(ok)
            best = ((k[0], k[1]), k[3])
            if k[0] <= 12:
                break
    if best is None:
        return closest_floor(c, tris), c
    p = best[1]
    if best[0][0] == 0:  # inside the rectangle (a door on open floor): step back to face it
        dx, dz = p[0] - c[0], p[1] - c[1]
        n = (dx * dx + dz * dz) ** 0.5
        if n < 8:
            return p, (c[0] + (c[0] - p[0]) * 3 + 0.01, c[1] + (c[1] - p[1]) * 3) if n > 0.5 else (c[0], c[1] - 20)
    return p, c


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
            col_path = os.path.join(a.scenes, "scenes", "shared", name + "_scene", name + "_col")
            tris = floor_tris(col_path)
            walls = wall_segs(col_path)
            steps = []
            for i, d in enumerate(ds):
                x0, z0, x1, z1 = d["rect"]
                # as a player would: stand at the door, push into it for a while (aiming past
                # its middle, so the walk never ends with him turning on the spot), then back at
                # the door (pushing can slide him along the wall), face it and press A
                (qx, qz), (cx, cz) = door_stand(d["rect"], tris, walls)
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
