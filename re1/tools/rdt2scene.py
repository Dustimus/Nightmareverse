"""Convert an RE1 room (RDT + per-camera PAK backgrounds) into an OoT pre-rendered scene.

    python3 tools/oot/rdt2scene.py --stage <RE1>/JPN --room 106:re_foyer [--room 203:re_foyer2f ...] \\
        --out out/scenes [--preview out/foyer_map.png]

Writes <out>/<name>/ with <name>_scene.c/.h, <name>_room_0.c/.h and one JPEG per camera,
laid out like the decomp's own scenes (assets/scenes/...). tools/oot/integrate.py
--scene installs it.

How RE1 maps onto OoT's pre-rendered rooms (RoomShapeImageMulti):
- each RE1 camera -> a BgCamInfo (CAM_SET_PREREND_FIXED: position, pitch/yaw, fov)
  plus a 320x240 JPEG background tied to that bgCamIndex
- RE1 switches cameras when the player enters trigger quads (RVD). OoT picks the camera
  from the floor polygon under Link, so the floor is cut into cells, each assigned the
  camera RE1 would show there, and merged into rectangles
- RE1 collision rectangles/circles (SCA) -> vertical wall polygons
- RE1 projection: eye, target, focal length H on a 320x240 screen -> fovy = 2 atan(120/H)

Coordinates: OoT = (x, -y, -z) * SCALE. SCALE = 0.0217 matches the actors (scale 0.01085)
(an RE1 human, ~2950 units, becomes 59 OoT units, adult Link's height).
"""

import argparse
import dataclasses
import json
import math
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "re1"))
import rdt  # noqa: E402
import story  # noqa: E402

import examine  # noqa: E402  (tools/oot, next to this file)
import re_items  # noqa: E402

SCALE = 0.0217  # Chris's height = adult Link's hat tip (64 units); actors use 0.01085 to match
CELL = 400  # RE1 units per floor cell (camera borders land within ~4 OoT units)
WALL_HEIGHT = 3000  # RE1 units
DOOR_MARGIN = 1100  # RE1 units added around door zones (Link's radius is ~900)
STAIRS_ANIM = 22  # Door_set animation for stairs: the zone sits on open floor, not in a wall
STAIRS_MARGIN = 1000  # Link stops ~900 short of the wall at the top of the 2F flight


def to_oot(p):
    return (p[0] * SCALE, -p[1] * SCALE, -p[2] * SCALE)


def inside_quad(quad, x, z):
    """Point in convex-ish quad (either winding)."""
    sign = 0
    for i in range(4):
        x1, z1 = quad[i]
        x2, z2 = quad[(i + 1) % 4]
        c = (x2 - x1) * (z - z1) - (z2 - z1) * (x - x1)
        if c != 0:
            s = 1 if c > 0 else -1
            if sign == 0:
                sign = s
            elif s != sign:
                return False
    return True


def project(cam, p):
    """RE1 projection of world point p -> screen (x, y) or None."""
    f = [cam.target[i] - cam.pos[i] for i in range(3)]
    fl = math.sqrt(sum(c * c for c in f))
    f = [c / fl for c in f]
    up = (0, -1, 0)
    r = [f[1] * up[2] - f[2] * up[1], f[2] * up[0] - f[0] * up[2], f[0] * up[1] - f[1] * up[0]]
    rl = math.sqrt(sum(c * c for c in r))
    r = [c / rl for c in r]
    u = [r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0]]
    d = [p[i] - cam.pos[i] for i in range(3)]
    x = sum(d[i] * r[i] for i in range(3))
    y = sum(d[i] * u[i] for i in range(3))
    z = sum(d[i] * f[i] for i in range(3))
    if z <= 10:
        return None
    h = cam.unknown[2]
    return (160 + h * x / z, 120 - h * y / z)


def screen_score(cam, x, z):
    """How well a camera frames a standing character at (x, z): 0 = centered, >1 = off screen."""
    best = 9.0
    for y in (0, -900, -1800):  # feet, waist, head
        s = project(cam, (x, y, z))
        if s is None:
            return 9.0
        best = min(best, max(abs(s[0] - 160) / 160, abs(s[1] - 120) / 120))
    return best


def assign_cameras(room):
    """Grid of (x0, z0) -> camera index (or None outside the room)."""
    boundaries = {}
    leaves = {}
    for s in room.switches:
        if s.is_boundary:
            boundaries.setdefault(s.frm, []).append(s.quad)
        else:
            leaves.setdefault(s.frm, []).append(s.quad)
    xs = [x for qs in boundaries.values() for q in qs for x, _ in q]
    zs = [z for qs in boundaries.values() for q in qs for _, z in q]
    x_lo, x_hi = max(0, min(xs)), max(xs)
    z_lo, z_hi = max(0, min(zs)), max(zs)
    walls = [c for c in room.collisions if _solid(c)]
    grid = {}
    for gx in range(int(x_lo // CELL), int(x_hi // CELL) + 1):
        for gz in range(int(z_lo // CELL), int(z_hi // CELL) + 1):
            x, z = (gx + 0.5) * CELL, (gz + 0.5) * CELL
            if not any(inside_quad(q, x, z) for qs in boundaries.values() for q in qs):
                continue
            if any(_inside_collision(c, x, z) and _is_outer_wall(c) for c in walls):
                continue
            cands = [
                c
                for c in range(len(room.cameras))
                if any(inside_quad(q, x, z) for q in boundaries.get(c, []))
                and not any(inside_quad(q, x, z) for q in leaves.get(c, []))
            ]
            pool = cands or list(range(len(room.cameras)))
            grid[(gx, gz)] = min(pool, key=lambda c: screen_score(room.cameras[c], x, z))
    return grid


def _solid(c):
    """Whether an RE1 collision shape stops Link. Types 4 and 5 aren't obstacles: in the
    gallery (107) they lie across the curtained doorway into its back room and over the ink
    ribbons' item spot, both places Jill walks (what they are for, we don't know)."""
    return c.type not in (4, 5)


def _bounds(c):
    x1, x2 = sorted((c.x1 ^ 0x8000, c.x2 ^ 0x8000))
    z1, z2 = sorted((c.z1 ^ 0x8000, c.z2 ^ 0x8000))
    return x1, z1, x2, z2


def _inside_collision(c, x, z):
    x1, z1, x2, z2 = _bounds(c)
    if c.type == 3:
        cx, cz, rx, rz = (x1 + x2) / 2, (z1 + z2) / 2, (x2 - x1) / 2, (z2 - z1) / 2
        return ((x - cx) / rx) ** 2 + ((z - cz) / rz) ** 2 <= 1
    return x1 <= x <= x2 and z1 <= z <= z2


def _is_outer_wall(c):
    x1, z1, x2, z2 = _bounds(c)
    return (x2 - x1) > 20000 or (z2 - z1) > 20000


def reachable(grid, open_cells, starts):
    """Keep floor cells connected to the spawns through open floor (drops floor behind walls).
    Every spawn seeds it: a room's middle can be cut off from its door (room 114)."""
    open_set = set(open_cells)
    seen = {k for k in starts if k in open_set} or {starts[0]}
    stack = list(seen)
    while stack:
        x, z = stack.pop()
        for n in ((x + 1, z), (x - 1, z), (x, z + 1), (x, z - 1)):
            if n in open_set and n not in seen:
                seen.add(n)
                stack.append(n)
    # keep cells under obstacles next to reachable floor, so walls stand on solid ground
    keep = set(seen)
    for x, z in seen:
        for dx in (-2, -1, 0, 1, 2):
            for dz in (-2, -1, 0, 1, 2):
                if (x + dx, z + dz) in grid:
                    keep.add((x + dx, z + dz))
    return {k: grid[k] for k in keep}


def merge_rects(grid):
    """Cover same-camera cells with few rectangles: (gx0, gz0, gx1, gz1, cam), inclusive.

    Greedy: take the first unclaimed cell (row-major), grow right as far as the camera
    stays the same, then grow down while the whole span matches. OoT's collision lookup
    for fixed-camera scenes is small (20000 bytes), so the polygon count matters.
    """
    left = dict(grid)
    rects = []
    for key in sorted(grid, key=lambda k: (k[1], k[0])):
        if key not in left:
            continue
        cam = left[key]
        x0, z0 = key
        x1 = x0
        while left.get((x1 + 1, z0)) == cam:
            x1 += 1
        z1 = z0
        while all(left.get((x, z1 + 1)) == cam for x in range(x0, x1 + 1)):
            z1 += 1
        for x in range(x0, x1 + 1):
            for z in range(z0, z1 + 1):
                del left[(x, z)]
        rects.append((x0, z0, x1, z1, cam))
    return rects


class Collision:
    def __init__(self):
        self.verts = []
        self.vindex = {}
        self.polys = []  # (surface, (i0, i1, i2), normal, dist)

    def v(self, p):
        key = tuple(int(round(c)) for c in p)
        if key not in self.vindex:
            self.vindex[key] = len(self.verts)
            self.verts.append(key)
        return self.vindex[key]

    def tri(self, surface, a, b, c, want_up=None):
        ia, ib, ic = self.v(a), self.v(b), self.v(c)
        pa, pb, pc = self.verts[ia], self.verts[ib], self.verts[ic]
        u = [pb[i] - pa[i] for i in range(3)]
        w = [pc[i] - pa[i] for i in range(3)]
        n = [u[1] * w[2] - u[2] * w[1], u[2] * w[0] - u[0] * w[2], u[0] * w[1] - u[1] * w[0]]
        ln = math.sqrt(sum(x * x for x in n))
        if ln == 0:
            return
        n = [x / ln for x in n]
        if want_up is not None and ((n[1] > 0) != want_up):
            ib, ic = ic, ib
            n = [-x for x in n]
        dist = -sum(n[i] * self.verts[ia][i] for i in range(3))
        self.polys.append((surface, (ia, ib, ic), n, int(round(dist))))

    def quad(self, surface, a, b, c, d, want_up=None):
        self.tri(surface, a, b, c, want_up)
        self.tri(surface, a, c, d, want_up)


def build_collision(room, grid, wall_surface, floor):
    col = Collision()
    hgrid = {}  # flat cells merge with same-height neighbours; sloped cells stay single
    for (gx, gz), surf in grid.items():
        hs = {floor.height(x * CELL, z * CELL) for x in (gx, gx + 1) for z in (gz, gz + 1)}
        hgrid[(gx, gz)] = (surf, hs.pop()) if len(hs) == 1 else (surf, "slope", gx, gz)
    for gx0, gz0, gx1, gz1, key in merge_rects(hgrid):
        x0, x1 = gx0 * CELL, (gx1 + 1) * CELL
        z0, z1 = gz0 * CELL, (gz1 + 1) * CELL
        h = floor.height
        col.quad(key[0], to_oot((x0, h(x0, z0), z0)), to_oot((x1, h(x1, z0), z0)), to_oot((x1, h(x1, z1), z1)),
                 to_oot((x0, h(x0, z1), z1)), True)
    seen = set()
    for c in room.collisions:
        if floor.is_open(c) or not _solid(c):
            continue
        x1, z1, x2, z2 = _bounds(c)
        if (x1, z1, x2, z2, c.type) in seen:  # RE1 repeats shapes across its collision groups
            continue
        seen.add((x1, z1, x2, z2, c.type))
        if c.type == 3:
            cx, cz, rx, rz = (x1 + x2) / 2, (z1 + z2) / 2, (x2 - x1) / 2, (z2 - z1) / 2
            ring = [(cx + rx * math.cos(t * math.pi / 4), cz + rz * math.sin(t * math.pi / 4)) for t in range(8)]
        else:
            ring = [(x1, z1), (x2, z1), (x2, z2), (x1, z2)]
        cx = sum(p[0] for p in ring) / len(ring)
        cz = sum(p[1] for p in ring) / len(ring)
        for i in range(len(ring)):
            (ax, az), (bx, bz) = ring[i], ring[(i + 1) % len(ring)]
            # skip faces Link can't reach (e.g. the outside of the outer walls): nothing
            # walkable just beyond them. Keeps OoT's small fixed-camera collision budget.
            mx, mz = (ax + bx) / 2, (az + bz) / 2
            ox, oz = mx - cx, mz - cz
            ol = math.hypot(ox, oz) or 1.0
            px, pz = mx + ox / ol * CELL, mz + oz / ol * CELL
            if (int(px // CELL), int(pz // CELL)) not in grid:
                continue
            a0, b0 = to_oot((ax, floor.depth, az)), to_oot((bx, floor.depth, bz))
            a1, b1 = to_oot((ax, floor.top, az)), to_oot((bx, floor.top, bz))
            col.quad(wall_surface, a0, b0, b1, a1)
            # make the wall face away from the obstacle's center
            mid = to_oot(((ax + bx) / 2, floor.depth, (az + bz) / 2))
            center = to_oot((cx, floor.depth, cz))
            for k in (-2, -1):
                s, idx, n, d = col.polys[k]
                out = (mid[0] - center[0], mid[2] - center[2])
                if n[0] * out[0] + n[2] * out[1] < 0:
                    col.polys[k] = (s, (idx[0], idx[2], idx[1]), [-x for x in n], -d)
    return col


def camera_data(cam):
    eye = to_oot(cam.pos)
    tgt = to_oot(cam.target)
    d = [tgt[i] - eye[i] for i in range(3)]
    yaw = math.atan2(d[0], d[2])
    pitch = math.atan2(d[1], math.hypot(d[0], d[2]))
    binang = lambda a: int(round(a * 0x8000 / math.pi))  # noqa: E731
    fov = int(round(math.degrees(2 * math.atan(120.0 / cam.unknown[2])) * 100))
    return [int(round(c)) for c in eye], (binang(-pitch), binang(yaw), 0), fov


def preview(room, grid, path):
    colors = [(230, 25, 75), (60, 180, 75), (255, 225, 25), (0, 130, 200), (245, 130, 48), (145, 30, 180),
              (70, 240, 240), (240, 50, 230), (210, 245, 60), (250, 190, 212)]
    gx = [k[0] for k in grid]
    gz = [k[1] for k in grid]
    s = 6
    im = Image.new("RGB", ((max(gx) + 2) * s, (max(gz) + 2) * s), (30, 30, 30))
    px = im.load()
    for (x, z), cam in grid.items():
        for i in range(s):
            for j in range(s):
                px[x * s + i, z * s + j] = colors[cam % len(colors)]
    from PIL import ImageDraw

    dr = ImageDraw.Draw(im)
    for c in room.collisions:
        x1, z1, x2, z2 = _bounds(c)
        box = [x1 / CELL * s, z1 / CELL * s, x2 / CELL * s, z2 / CELL * s]
        (dr.ellipse if c.type == 3 else dr.rectangle)(box, outline=(255, 255, 255))
    for i, cam in enumerate(room.cameras):
        x, z = cam.pos[0] / CELL * s, cam.pos[2] / CELL * s
        dr.text((x, z), str(i), fill=colors[i % len(colors)])
    im.save(path)


def jpeg_for(stage, room_id, cam_index, path):
    """Background -> baseline 4:2:0 JPEG (as OoT's decoder wants), written as a C u64 array
    of just the JPEG bytes. Not a .jpg asset: the decomp pads those to a full 150 KB decode
    buffer each, which can't fit 8 cameras in RAM. Rooms decode into one shared canvas
    instead (see integrate.py, Mod1_BgSource)."""
    import io

    w, h, rgba = rdt.load_background(rdt.find_file(stage, "RC%s%d.pak" % (room_id, cam_index)))
    buf = io.BytesIO()
    Image.frombytes("RGBA", (w, h), rgba).convert("RGB").save(buf, "JPEG", quality=90, subsampling=2)
    data = buf.getvalue()
    data += b"\0" * (-len(data) % 8)
    words = ["0x%016X" % int.from_bytes(data[i : i + 8], "big") for i in range(0, len(data), 8)]
    with open(path, "w") as f:
        for i in range(0, len(words), 4):
            f.write("    " + ", ".join(words[i : i + 4]) + ",\n")
    return len(data)


# No sub-keep object (OBJECT_INVALID): the mansion uses none of OoT's dungeon objects (doors,
# chests, pots...), and gameplay_dangeon_keep's 95 KB of object space go to the RE models.
SCENE_CMDS = """SceneCmd %s[] = {
    SCENE_CMD_SOUND_SETTINGS(0, NATURE_ID_NONE, NA_BGM_NO_MUSIC),
    SCENE_CMD_ROOM_LIST(ARRAY_COUNT(sRoomList), sRoomList),
    SCENE_CMD_MISC_SETTINGS(SCENE_CAM_TYPE_FIXED, 0),
    SCENE_CMD_COL_HEADER(&%s_col),
    SCENE_CMD_SPAWN_LIST(sSpawns),
    SCENE_CMD_SPECIAL_FILES(NAVI_QUEST_HINTS_NONE, OBJECT_INVALID),
    SCENE_CMD_PLAYER_ENTRY_LIST(ARRAY_COUNT(sPlayerEntries), sPlayerEntries),
    SCENE_CMD_SKYBOX_SETTINGS(SKYBOX_NONE, 0, LIGHT_MODE_SETTINGS),
    SCENE_CMD_EXIT_LIST(sExits),
    SCENE_CMD_ENV_LIGHT_SETTINGS(ARRAY_COUNT(sLights), sLights),
    SCENE_CMD_END(),
};

"""

ROOM_CMDS = """SceneCmd %s[] = {
    SCENE_CMD_ECHO_SETTINGS(5),
    SCENE_CMD_ROOM_BEHAVIOR(ROOM_TYPE_NORMAL, ROOM_ENV_DEFAULT, LENS_MODE_SHOW_ACTORS, false),
    SCENE_CMD_SKYBOX_DISABLES(true, true),
    SCENE_CMD_TIME_SETTINGS(0xFF, 0xFF, 0),
    SCENE_CMD_ROOM_SHAPE(&sRoomShape),
    SCENE_CMD_END(),
};

"""


# Rooms whose floors aren't flat. RE1 stores no heights for them (it plays its own climb
# animations), so these are measured against the backgrounds. RE1 units, positive = down.
ROOM_FLOORS = {
}


def apply_trims(room, room_id):
    """ROOM_FLOORS "trim": move a collision box's sides ((old x1, z1, x2, z2), (new ...))."""
    for old, new in ROOM_FLOORS.get(room_id, {}).get("trim", []):
        for i, c in enumerate(room.collisions):
            if all(abs(a - b) < 100 for a, b in zip(_bounds(c), old)):
                x1, z1, x2, z2 = new
                room.collisions[i] = dataclasses.replace(c, x1=x1 ^ 0x8000, z1=z1 ^ 0x8000, x2=x2 ^ 0x8000,
                                                         z2=z2 ^ 0x8000)


# Each room's floor level (RE1 units, positive = down), from RE1's own door data: the height
# Jill arrives at through a door that gives one (most give 0). The main hall's upper floor (203)
# is 2885 up, the corridor by the save room's stairs (207) 2885 down, 20E 1402 up; checked
# against the backgrounds (Link's feet on the painted floor). Filled in by main().
ROOM_LEVELS = {}
if __name__ == "__main__":  # soh_scene imports this module: let it see this run's ROOM_LEVELS
    sys.modules["rdt2scene"] = sys.modules[__name__]


class Floor:
    """Floor height (RE1 units, positive = down) and opened-up collision boxes of a room."""

    def __init__(self, room_id):
        cfg = ROOM_FLOORS.get(room_id, {})
        self.base = ROOM_LEVELS.get(room_id.upper(), 0)
        self.open = cfg.get("open", [])
        self.regions = cfg.get("regions", [])
        self.depth = max([max(r[4], r[5]) for r in self.regions] + [self.base])
        self.top = min([min(r[4], r[5]) for r in self.regions] + [self.base]) - WALL_HEIGHT

    def height(self, x, z):
        for x0, z0, x1, z1, h0, h1 in self.regions:
            if x0 <= x <= x1 and z0 <= z <= z1:
                return h0 + (h1 - h0) * (x - x0) / (x1 - x0)
        return self.base

    def is_open(self, c):
        b = _bounds(c)
        return any(all(abs(b[i] - o[i]) < 100 for i in range(4)) for o in self.open)


# RE1 Em_set models -> (OoT actor, object). Characters (Barry, Wesker, Kenneth...) have no
# OoT actor yet and are skipped.
ENEMY_ACTORS = {  # model -> (actor, object, params high byte: the actor's model variant)
    0x00: ("ACTOR_EN_RE_ZOMBIE", "OBJECT_RE_ZOMBIE", 0),
    0x01: ("ACTOR_EN_RE_ZOMBIE", "OBJECT_RE_ZOMBIE", 0),  # TODO: EM1001 (nude zombie) model
    0x11: ("ACTOR_EN_RE_ZOMBIE", "OBJECT_RE_ZOMBIE2", 1),  # EM1011 (the corridor zombie in 104; elsewhere variant 3)
    0x0C: ("ACTOR_EN_RE_TYRANT", "OBJECT_RE_TYRANT", 0),
    0x02: ("ACTOR_EN_RE_DOG", "OBJECT_RE_DOG", 0),  # Cerberus, EM1002
    0x05: ("ACTOR_EN_RE_CROW", "OBJECT_RE_CROW", 0),  # a crow, EM1005 (perched up high)
    0x0D: ("ACTOR_EN_RE_YAWN", "OBJECT_RE_YAWN", 0),  # Yawn, the huge snake, EM100D (its first fight, 210)
}
BODY_MODELS = (0x25, 0x29)  # Kenneth: a zombie next to him is feeding (RE_ZOMBIE_TYPE_FEEDING)
# RE1 characters -> (object, En_ReNpc type)
CHARACTER_ACTORS = {
    0x22: ("OBJECT_RE_BARRY", 0),
    0x25: ("OBJECT_RE_KENNETH", 1),
    0x24: ("OBJECT_RE_WESKER", 2),
    0x27: ("OBJECT_RE_RICHARD", 3),  # poisoned in 20D (dead once the story says so: En_ReNpc)
}


STORY_DOOR_FILES = (3, 4, 31)  # foyer <-> dining room <-> corridor, the hall's east door, the sword door


def fit_door_scenes(name, actors, door_actors, sizes_path, budget):
    """The door scenes (En_ReDoor) that fit in the room's object space with its other objects
    (object_sizes.py's sizes; all of them without): a door file already loaded costs nothing
    more, the others go in while the total stays within budget, the rest keep the fade."""
    if not sizes_path:
        return door_actors
    sizes = json.load(open(sizes_path))
    objs = {act[1] for act in actors if act[1]}
    # the story path's door files first (the ones converted first), when not all fit
    door_actors = sorted(door_actors, key=lambda act: act[3][0] not in STORY_DOOR_FILES)
    used = sum(sizes.get(o, 0) for o in objs)
    kept = []
    for act in door_actors:
        if act[1] not in objs:
            if used + sizes.get(act[1], 0) > budget:
                print("  %s: no room for %s (%d + %d bytes > %d): fade" % (name, act[1], used, sizes.get(act[1], 0),
                                                                           budget))
                continue
            used += sizes.get(act[1], 0)
            objs.add(act[1])
        kept.append(act)
    return kept


def after_spot(af, text_id, listed):
    """An examine spot's text after its switch flag (story "examine_after"): the next message
    (RE1's "then") or, with none, nothing. Returns En_ReCheck's home.rot.y."""
    if "then" in af:
        listed.append({"msg": af["then"], "text": None, "text_id": text_id + 1})
        return (af["switch"] + 1) & 0x3F
    listed.append({"msg": af["msg"], "text": None, "text_id": text_id + 1})  # (unused)
    return ((af["switch"] + 1) & 0x3F) | 0x80


def question_spot(q, rect, params, text_id, listed):
    """En_ReCheck for a story "questions" entry: its question (message text_id) and what it shows
    once answered Yes (text_id + 1), or with no "then", gone once its switch flag is set."""
    x, z, w, d = rect
    listed.append({"msg": q["ask"], "text": None, "text_id": text_id})
    listed.append({"msg": q.get("then", q["ask"]), "text": None, "text_id": text_id + 1})
    flag = (q["switch"] + 1) | (0 if "then" in q else 0x80) | (((q.get("cam", -1) + 1) & 0xFF) << 8)
    return ("ACTOR_EN_RE_CHECK", None, (x + w / 2, 0, z + d / 2),
            (round(w / 2 * SCALE), flag, round(d / 2 * SCALE)), params | examine.QUESTION_BIT)


def room_cast(room, grid, cast):
    """RE1 characters with an OoT model. A room script lists a character once per story
    state (e.g. Barry by the door, then by the fireplace); each cast entry
    {"model", "pick", "flag_bits", "story": (requires, forbids)} places the pick-th of them
    (on the floor), with params bits 8-14 for a room switch flag ("until"/"after") and
    the story flags it appears with / leaves with in the entry's x / z rotations. Without
    a cast entry a character gets its first placement."""
    out = []
    by_model = {}
    for e in room.enemies or []:
        # (a far-off y parks a character off-stage until a script moves it in)
        if e.model in CHARACTER_ACTORS and abs(e.pos[1]) < 4000 and (int(e.pos[0] // CELL), int(e.pos[2] // CELL)) in grid:
            by_model.setdefault(e.model, []).append(e)
    for c in cast:  # "at": [x, z, dir] places one where RE1's event script does (its Em_set is parked)
        if "at" in c and c["model"] in CHARACTER_ACTORS:
            by_model.setdefault(c["model"], [])
    for model, placements in sorted(by_model.items()):
        entries = [c for c in cast if c["model"] == model] or [{"model": model, "pick": 0}]
        for c in entries:
            if "at" in c:
                pos, yaw = (c["at"][0], 0, c["at"][1]), re_dir_to_yaw(c["at"][2])
            elif placements:
                e = placements[min(c.get("pick", 0), len(placements) - 1)]
                pos, yaw = e.pos, re_dir_to_yaw(e.dir)
            else:
                continue
            obj, npc_type = CHARACTER_ACTORS[model]
            req, forb, high = c.get("story", (0, 0, False))
            out.append(("ACTOR_EN_RE_NPC", obj, pos, (req, yaw, forb),
                        npc_type | c.get("flag_bits", 0x3F00) | (0x8000 if high else 0)))
    return out


CORRIDOR_ZOMBIE_ROOM = "104"  # where EM1011 is the story's corridor zombie (En_ReZombie's story logic)
ZOMBIE_DEAD_FLAGS = 40  # switch flags 40.. keep a room's other zombies dead


def room_actors(room, grid, enemy_specs=(), room_id=""):
    """RE1 enemy placements -> [(actor, object, (x, y, z) RE1, yaw, params)].

    Scripts list both branches of their conditions (e.g. before/after an event), so only
    the first placement per (model, killed flag) is used; placeholders at the origin or
    off the floor are dropped. enemy_specs: the story file's "enemies" for this room (a
    Cerberus's window and switch flags)."""
    out, seen, dead_count = [], set(), 0
    for e in room.enemies or []:
        if e.model not in ENEMY_ACTORS:
            continue
        if ENEMY_ACTORS[e.model][0] == "ACTOR_EN_RE_CROW":  # crows share killed flags (212's): one per place
            if any(k[0] == e.model and abs(k[1] - e.pos[0]) + abs(k[2] - e.pos[2]) < 400 for k in seen if len(k) == 3):
                continue
            seen.add((e.model, e.pos[0], e.pos[2]))
        elif (e.model, e.flag) in seen:
            continue
        x, y, z = e.pos
        if ENEMY_ACTORS[e.model][0] == "ACTOR_EN_RE_CROW":  # up by the ceiling, over ledges and frames
            spec = next((d for d in enemy_specs if d.get("model") == "0x05"), {})
            height = -(y if y < 0x8000 else y - 0x10000)
            params = (ZOMBIE_DEAD_FLAGS + dead_count) | ((spec.get("wake_flag", 0x3F) & 0x3F) << 6)
            dead_count += 1
            out.append(("ACTOR_EN_RE_CROW", "OBJECT_RE_CROW", (x, 0, z), (0, re_dir_to_yaw(e.dir), round(height * SCALE)),
                        params))
            continue
        if (int(x // CELL), int(z // CELL)) not in grid or abs(y if y < 0x8000 else y - 0x10000) > 100:
            continue
        if ENEMY_ACTORS[e.model][0] == "ACTOR_EN_RE_YAWN":
            # the first placement (by the moon crest: RE1 parks it there, hidden, for the fight;
            # the second is where it lies after); out when the crest is taken (a switch flag the
            # story file gives the crest pickup), gone for good once back in its hole
            seen.add((e.model, e.flag))
            spec = next((d for d in enemy_specs if d.get("model") == "0x0D"), {})
            params = (ZOMBIE_DEAD_FLAGS + dead_count) | ((spec.get("wake_flag", 0x3F) & 0x3F) << 6)
            dead_count += 1
            hx, hz = spec.get("hole", (x, z))
            out.append(("ACTOR_EN_RE_YAWN", "OBJECT_RE_YAWN", (x, y, z),
                        (round(hx * SCALE), re_dir_to_yaw(e.dir), round(-hz * SCALE)), params))
            continue
        if ENEMY_ACTORS[e.model][0] == "ACTOR_EN_RE_DOG":
            seen.add((e.model, e.flag))
            spec = next((d for d in enemy_specs if d.get("em") == e.flag), {})
            params = (((spec.get("dead_flag", 0x3F) & 0x3F) << 2)
                      | ((1 | ((spec["window_flag"] & 0x3F) << 8)) if "window" in spec else 0))
            wx, wz = spec.get("window", (x, z))
            out.append(("ACTOR_EN_RE_DOG", "OBJECT_RE_DOG", (x, y, z),
                        (round(wx * SCALE), re_dir_to_yaw(e.dir), round(-wz * SCALE)), params))
            continue
        seen.add((e.model, e.flag))
        actor, obj, variant = ENEMY_ACTORS[e.model]
        if e.model == 0x11 and room_id != CORRIDOR_ZOMBIE_ROOM:
            variant = 3  # RE_ZOMBIE_MODEL_EM1011: the same model, none of the corridor zombie's story
        params = 0
        if actor == "ACTOR_EN_RE_ZOMBIE" and any(
            b.model in BODY_MODELS and math.hypot(b.pos[0] - x, b.pos[2] - z) < 1500 for b in room.enemies
        ):
            params = 1
        rot = re_dir_to_yaw(e.dir)
        if actor == "ACTOR_EN_RE_ZOMBIE" and variant not in (1, 2):
            # it stays dead (RE1's Em_set killed flag): a switch flag of the room's, 40 on
            rot = (0, rot, ZOMBIE_DEAD_FLAGS + dead_count + 1)
            dead_count += 1
        out.append((actor, obj, (x, y, z), rot, params | (variant << 8)))
    return out


MASK_MAX_SCALE = 1024  # mask vertices: camera-relative, at most this many steps a unit
MASK_BLOCK = 32  # mask pieces are cut into blocks of at most 32x32 texels (CI8: 1 KB of TMEM)


def psx_to_rgba5551(c):
    if c == 0:
        return 0  # PSX transparent color
    r, g, b = c & 31, (c >> 5) & 31, (c >> 10) & 31
    return (r << 11) | (g << 6) | (b << 1) | 1


def mask_world_quad(cam, x0, y0, x1, y1, depth):
    """Screen rect (pixels) at camera-space depth -> 4 OoT world points (TL, TR, BR, BL)."""
    f = [cam.target[i] - cam.pos[i] for i in range(3)]
    fl = math.sqrt(sum(c * c for c in f))
    f = [c / fl for c in f]
    up = (0, -1, 0)
    r = [f[1] * up[2] - f[2] * up[1], f[2] * up[0] - f[0] * up[2], f[0] * up[1] - f[1] * up[0]]
    rl = math.sqrt(sum(c * c for c in r))
    r = [c / rl for c in r]
    u = [r[1] * f[2] - r[2] * f[1], r[2] * f[0] - r[0] * f[2], r[0] * f[1] - r[1] * f[0]]
    h = cam.unknown[2]
    pts = []
    for sx, sy in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
        xc = (sx - 160) * depth / h
        yc = (120 - sy) * depth / h
        p = [cam.pos[i] + r[i] * xc + u[i] * yc + f[i] * depth for i in range(3)]
        pts.append(to_oot(p))
    return pts


def mask_c(room, ci, prefix, bg, extra=()):
    """C source for camera ci's foreground masks, or (None, "") if it has none.

    Each RE1 mask piece is a cut-out of the background with a camera-space depth; it
    becomes textured quads at exactly that depth, so the depth buffer hides actors behind
    it (and elsewhere it just repaints identical background pixels).

    bg = (w, h, rgba) background: some RE1 mask entries span more of the mask atlas than
    their piece and pick up neighbouring pieces, so only texels that match the background
    underneath are kept.

    extra: [(x0, y0, x1, y1, depth)] pieces RE1 lacks (the story file's "masks" for this camera):
    the background in that screen rect, in the mask palette's nearest colors, at that
    camera-space depth (RE1 units)."""
    masks, tim = rdt.load_masks(room, ci)
    bw_, _bh, bgpx = bg

    def matches(c, x, y):
        o = (y * bw_ + x) * 4
        rgb = ((c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3)
        return sum(abs(bgpx[o + i] - rgb[i]) for i in range(3)) <= 60

    if not masks or tim is None or tim.bpp != 8:
        return None, ""
    cam = room.cameras[ci]
    tlut = [psx_to_rgba5551(c) for c in tim.cluts[0]] + [0] * (256 - len(tim.cluts[0]))
    # index 0 is the holes (texels of index 0 are never kept): transparent, whatever the TIM's
    # palette has there (212's camera 4 has white, 0x7FFF, which drew every hole white)
    tlut[0] = 0
    if os.environ.get("MOD1_MASK_DEBUG"):  # tint masks red to check their placement
        tlut = [(v & 0x07FF) | 0xF800 if v else 0 for v in tlut]
    out = ["static u16 %sTlut[256] ALIGNED(8) = {\n" % prefix]
    for i in range(0, 256, 16):
        out.append("    " + ", ".join("0x%04X" % v for v in tlut[i : i + 16]) + ",\n")
    out.append("};\n\n")
    blocks = []  # (texture name, padded width, height, vertex quad)

    def emit(dst, w, h, grid, depth16):
        for by in range(0, h, MASK_BLOCK):
            for bx in range(0, w, MASK_BLOCK):
                bw, bh = min(MASK_BLOCK, w - bx), min(MASK_BLOCK, h - by)
                pw = 8  # LoadBlock: rows of a power-of-two number of 8-byte words, or later rows drift
                while pw < bw:
                    pw *= 2
                rows = [[grid[by + yy][bx + xx] if xx < bw else 0 for xx in range(pw)] for yy in range(bh)]
                if not any(any(r) for r in rows):
                    continue
                data = bytes(v for row in rows for v in row)
                data += b"\0" * (-len(data) % 8)
                name = "%sTex%d" % (prefix, len(blocks))
                out.append("static u64 %s[] = {\n" % name)
                words = ["0x%016X" % int.from_bytes(data[i : i + 8], "big") for i in range(0, len(data), 8)]
                for i in range(0, len(words), 4):
                    out.append("    " + ", ".join(words[i : i + 4]) + ",\n")
                out.append("};\n\n")
                quad = mask_world_quad(cam, dst[0] + bx, dst[1] + by, dst[0] + bx + bw, dst[1] + by + bh,
                                       depth16)
                blocks.append((name, pw, bh, bw, quad))

    for m in masks:
        if m.depth == 0:
            continue  # no usable depth (a quad at the eye)
        w, h = m.size
        # only filter pieces that picked up other pieces; clean ones keep every texel
        # (palette rounding makes a few texels differ slightly even in good pieces)
        texels = [(m.dst[0] + xx, m.dst[1] + yy, tim.pixels[(m.src[1] + yy) * tim.width + m.src[0] + xx])
                  for yy in range(h) for xx in range(w)
                  if m.dst[0] + xx < 320 and m.dst[1] + yy < 240 and m.src[0] + xx < tim.width and m.src[1] + yy < tim.height]
        opaque = [(x, y, v) for x, y, v in texels if v]
        bad = sum(1 for x, y, v in opaque if not matches(tim.cluts[0][v], x, y))
        dirty = bad > 0.05 * max(1, len(opaque))
        grid = [[0] * w for _ in range(h)]
        for x, y, v in opaque:
            if not (dirty and not matches(tim.cluts[0][v], x, y)):
                grid[y - m.dst[1]][x - m.dst[0]] = v
        # PSX color 0 is transparent, so pure black background pixels are holes in the
        # mask, and filtering can leave single-texel holes: fill holes mostly surrounded
        # by mask texels
        for _ in range(3):
            fill = []
            for yy in range(h):
                for xx in range(w):
                    x, y = m.dst[0] + xx, m.dst[1] + yy
                    if grid[yy][xx] or x >= 320 or y >= 240:
                        continue
                    o = (y * bw_ + x) * 4
                    dark = bgpx[o] + bgpx[o + 1] + bgpx[o + 2] <= 48
                    nb = [grid[j][i] for i, j in ((xx - 1, yy), (xx + 1, yy), (xx, yy - 1), (xx, yy + 1))
                          if 0 <= i < w and 0 <= j < h and grid[j][i]]
                    if len(nb) >= (2 if dark else 3):
                        fill.append((xx, yy, min(nb, key=lambda v: sum(tim.cluts[0][v] >> k & 31 for k in (0, 5, 10)))))
            for xx, yy, v in fill:
                grid[yy][xx] = v
        emit(m.dst, w, h, grid, m.depth * 16)
    pal = tim.cluts[0]
    cache = {}

    def nearest(o):  # a background pixel in the mask palette's nearest color
        key = (bgpx[o] >> 3, bgpx[o + 1] >> 3, bgpx[o + 2] >> 3)
        if key not in cache:
            cache[key] = min(range(1, len(pal)), key=lambda i: sum(
                abs(((pal[i] >> k) & 31) - key[j]) for j, k in enumerate((0, 5, 10))))
        return cache[key]

    for x0, y0, x1, y1, depth in extra:
        x0, y0, x1, y1 = max(0, x0), max(0, y0), min(320, x1), min(240, y1)
        grid = [[nearest(((y0 + yy) * bw_ + x0 + xx) * 4) for xx in range(x1 - x0)] for yy in range(y1 - y0)]
        emit((x0, y0), x1 - x0, y1 - y0, grid, depth)
    if not blocks:
        return None, ""  # every piece was empty or at the eye (room 113's camera 3)
    # Vertex precision: a Vtx holds whole units, and pieces near the camera (104's wall lamp,
    # 35 x 16 RE1 units away) are only a unit or two across, so rounded corners moved and
    # stretched them on screen. Each block's corners are stored relative to the camera,
    # scaled by the largest power of two that fits 16 bits, and a matrix per scale (the
    # camera's translation and 1/scale) puts them back.
    eye = to_oot(cam.pos)
    groups = {}  # scale -> [block index]
    rel = []
    for i, (_, _, _, _, quad) in enumerate(blocks):
        r_ = [[c - e for c, e in zip(pt, eye)] for pt in quad]
        big = max(abs(c) for pt in r_ for c in pt) or 1
        scale = 1
        while scale < MASK_MAX_SCALE and big * scale * 2 < 30000:
            scale *= 2
        rel.append((r_, scale))
        groups.setdefault(scale, []).append(i)

    def mtx(scale, t):
        vals = [1 / scale, 0, 0, 0, 0, 1 / scale, 0, 0, 0, 0, 1 / scale, 0, t[0], t[1], t[2], 1]
        fixed = [round(v * 65536) & 0xFFFFFFFF for v in vals]
        hi = [f >> 16 for f in fixed]
        lo = [f & 0xFFFF for f in fixed]
        words = [(hi[2 * k] << 16) | hi[2 * k + 1] for k in range(8)] + [(lo[2 * k] << 16) | lo[2 * k + 1] for k in range(8)]
        return "{ { " + ", ".join("{ %s }" % ", ".join("0x%08X" % w for w in words[r * 4 : r * 4 + 4]) for r in range(4)) + " } }"

    for scale in sorted(groups):
        out.append("static Mtx %sMtx%d = %s;\n" % (prefix, scale, mtx(scale, eye)))
    out.append("static Mtx %sMtxId = %s;\n\n" % (prefix, mtx(1, (0, 0, 0))))
    out.append("static Vtx %sVtx[] = {\n" % prefix)
    for (_, _, bh, bw, _), (r_, scale) in zip(blocks, rel):
        for (x, y, z), (s_, t_) in zip(r_, ((0, 0), (bw, 0), (bw, bh), (0, bh))):
            out.append("    VTX(%d, %d, %d, %d, %d, 255, 255, 255, 255),\n"
                       % (round(x * scale), round(y * scale), round(z * scale), s_ << 5, t_ << 5))
    out.append("};\n\n")
    dbg = ""
    if os.environ.get("MOD1_MASK_DEBUG"):  # red square in the corner: proves the list runs
        dbg = ("    gsDPPipeSync(),\n    gsDPSetCycleType(G_CYC_FILL),\n    gsDPSetRenderMode(G_RM_NOOP, G_RM_NOOP2),\n"
               "    gsDPSetFillColor(0xF801F801),\n    gsDPFillRectangle(20, 20, 50, 50),\n")
    out.append("""static Gfx %sDL[] = {
""" % prefix + dbg + """    gsDPPipeSync(),
    gsSPLoadGeometryMode(G_ZBUFFER),
    gsSPTexture(0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON),
    gsDPSetCycleType(G_CYC_1CYCLE),
    gsDPSetRenderMode(G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2),
    gsDPSetCombineMode(G_CC_DECALRGBA, G_CC_DECALRGBA),
    gsDPSetAlphaCompare(G_AC_THRESHOLD),
    gsDPSetBlendColor(0, 0, 0, 1),
    gsDPSetTextureFilter(G_TF_POINT),
    gsDPSetTexturePersp(G_TP_PERSP),
    gsDPSetTextureLUT(G_TT_RGBA16),
    gsDPLoadTLUT_pal256(%sTlut),
""" % prefix)
    if os.environ.get("MOD1_MASK_DEBUG") == "2":  # flat red, no depth: shows where the quads land
        out.append("    gsSPLoadGeometryMode(0),\n    gsDPSetRenderMode(G_RM_OPA_SURF, G_RM_OPA_SURF2),\n"
                   "    gsDPSetCombineMode(G_CC_PRIMITIVE, G_CC_PRIMITIVE),\n    gsDPSetPrimColor(0, 0, 255, 0, 0, 255),\n"
                   "    gsDPSetAlphaCompare(G_AC_NONE),\n")
    if os.environ.get("MOD1_MASK_DEBUG") == "3":  # textured, no depth test
        out.append("    gsSPLoadGeometryMode(0),\n    gsDPSetRenderMode(G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2),\n")
    for scale in sorted(groups):
        out.append("    gsSPMatrix(&%sMtx%d, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH),\n" % (prefix, scale))
        for i in groups[scale]:
            name, pw, bh, _, _ = blocks[i]
            out.append("    gsDPLoadTextureBlock(%s, G_IM_FMT_CI, G_IM_SIZ_8b, %d, %d, 0, G_TX_CLAMP, G_TX_CLAMP, "
                       "G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD),\n" % (name, pw, bh))
            out.append("    gsSPVertex(&%sVtx[%d], 4, 0),\n    gsSP2Triangles(0, 1, 2, 0, 0, 2, 3, 0),\n" % (prefix, 4 * i))
    out.append("    gsSPMatrix(&%sMtxId, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH),\n" % prefix)
    out.append("    gsDPPipeSync(),\n    gsDPSetTextureLUT(G_TT_NONE),\n    gsDPSetAlphaCompare(G_AC_NONE),\n"
               "    gsSPEndDisplayList(),\n};\n\n")
    return prefix + "DL", "".join(out)


def write_scene(room, grid, stage, room_id, name, out, spawns, exits=(), cell_exit=None, actors=(), extra_masks=()):
    """spawns: [(x, z, yaw_binang)] in RE1 units (index = OoT spawn/entrance number).
    exits: entrance constants, in exit-list order; cell_exit: floor cell -> exit (1-based)."""
    d = os.path.join(out, name)
    os.makedirs(d, exist_ok=True)
    ncam = len(room.cameras)
    cell_exit = cell_exit or {}
    # one surface type per (camera, exit) pair on the floor, plus one for walls
    keys = sorted({(cam, cell_exit.get(k, 0)) for k, cam in grid.items()})
    surf_index = {key: i for i, key in enumerate(keys)}
    wall_surface = len(keys)
    surf_grid = {k: surf_index[(cam, cell_exit.get(k, 0))] for k, cam in grid.items()}
    floor = Floor(room_id)
    col = build_collision(room, surf_grid, wall_surface, floor)
    sc, rm = name + "_scene", name + "_room_0"

    # backgrounds
    for c in range(ncam):
        jpeg_for(stage, room_id, c, os.path.join(d, "%s_bg%d.jfif.inc.c" % (rm, c)))

    xs = [v[0] for v in col.verts]
    ys = [v[1] for v in col.verts]
    zs = [v[2] for v in col.verts]

    L = []
    L.append('#include "%s.h"\n#include "%s.h"\n' % (sc, rm))
    L.append('#include "actor.h"\n#include "array_count.h"\n#include "camera.h"\n#include "bgcheck.h"\n'
             '#include "object.h"\n#include "segment_symbols.h"\n#include "sequence.h"\n#include "skybox.h"\n'
             '#include "ultra64.h"\n#include "light.h"\n#include "scene.h"\n#include "player.h"\n')
    # OoT reads the command list from the very start of the file, so it must come first:
    # forward-declare the data it points at, emit the commands, then define the data.
    L.append("static ActorEntry sPlayerEntries[%d];\nstatic RomFile sRoomList[1];\nstatic Spawn sSpawns[%d];\n"
             "static s16 sExits[%d];\nstatic EnvLightSettings sLights[1];\nstatic Vec3s sBgCamFuncData[%d];\n"
             "static BgCamInfo sBgCamList[%d];\nstatic SurfaceType sSurfaceTypes[%d];\nstatic CollisionPoly sPolys[%d];\n"
             "static Vec3s sVerts[%d];\n\n" % (len(spawns), len(spawns), max(1, len(exits)), 3 * ncam, ncam,
                                                wall_surface + 1, len(col.polys), len(col.verts)))
    L.append(SCENE_CMDS % (sc, sc))
    L.append("static ActorEntry sPlayerEntries[] = {\n")
    for sx, sz, syaw in spawns:
        p = to_oot((sx, floor.height(sx, sz), sz))
        # Start on the camera that owns the spawn cell: with the default (0xFF) the first
        # frame runs before Link has a floor, and the room looks up a bogus background.
        cam = grid.get((int(sx // CELL), int(sz // CELL)), 0)
        L.append("    { ACTOR_PLAYER, { %d, %d, %d }, { 0, %d, 0 }, PLAYER_PARAMS(PLAYER_START_MODE_IDLE, %d) },\n"
                 % (round(p[0]), round(p[1]), round(p[2]), syaw, cam))
    L.append("};\n\n")
    L.append("static RomFile sRoomList[] = { ROM_FILE(%s) };\n\n" % rm)
    L.append("static Spawn sSpawns[] = { %s };\n\n" % ", ".join("{ %d, 0 }" % i for i in range(len(spawns))))
    L.append("static s16 sExits[] = { %s };\n\n" % (", ".join(exits) if exits else "0"))
    L.append("""static EnvLightSettings sLights[] = {
    { { 60, 55, 50 }, { 73, 73, 73 }, { 180, 160, 130 }, { -73, -73, -73 }, { 50, 45, 40 }, { 10, 10, 10 },
      BLEND_RATE_AND_FOG_NEAR(1, 994), 3200 },
};

""")
    L.append("static Vec3s sBgCamFuncData[] = {\n")
    for c, cam in enumerate(room.cameras):
        pos, rot, fov = camera_data(cam)
        L.append("    { %d, %d, %d }, { %d, %d, %d }, { %d, %d, -1 }, // camera %d\n"
                 % (pos[0], pos[1], pos[2], rot[0], rot[1], rot[2], fov, c, c))
    L.append("};\n\n")
    L.append("static BgCamInfo sBgCamList[] = {\n")
    for c in range(ncam):
        L.append("    { CAM_SET_PREREND_FIXED, 3, &sBgCamFuncData[%d] },\n" % (3 * c))
    L.append("};\n\n")
    L.append("static SurfaceType sSurfaceTypes[] = {\n")
    for (cam, ex) in keys + [(0, 0)]:
        L.append("    { { SURFACETYPE0(%d, %d, FLOOR_TYPE_0, 0, WALL_TYPE_0, FLOOR_PROPERTY_0, false, false),\n"
                 "        SURFACETYPE1(SURFACE_MATERIAL_STONE, FLOOR_EFFECT_0, 0, 1, false, CONVEYOR_SPEED_DISABLED,\n"
                 "                     CONVEYOR_DIRECTION_FROM_BINANG(0x0), false) } },\n" % (cam, ex))
    L.append("};\n\n")
    L.append("static CollisionPoly sPolys[] = {\n")
    for s, (a, b, c), n, dist in col.polys:
        L.append("    { %d, { COLPOLY_VTX(%d, 0), COLPOLY_VTX(%d, 0), COLPOLY_VTX(%d, 0) }, "
                 "{ COLPOLY_SNORMAL(%.5f), COLPOLY_SNORMAL(%.5f), COLPOLY_SNORMAL(%.5f) }, %d },\n"
                 % (s, a, b, c, n[0], n[1], n[2], dist))
    L.append("};\n\n")
    L.append("static Vec3s sVerts[] = {\n")
    for v in col.verts:
        L.append("    { %d, %d, %d },\n" % v)
    L.append("};\n\n")
    L.append("CollisionHeader %s_col = {\n    { %d, %d, %d },\n    { %d, %d, %d },\n"
             "    ARRAY_COUNT(sVerts), sVerts, ARRAY_COUNT(sPolys), sPolys, sSurfaceTypes, sBgCamList, 0, NULL,\n};\n\n"
             % (sc, min(xs), min(ys), min(zs), max(xs), max(ys), max(zs)))
    open(os.path.join(d, sc + ".c"), "w").write("".join(L))
    open(os.path.join(d, sc + ".h"), "w").write(
        "#ifndef %s_H\n#define %s_H\n\n#include \"ultra64.h\"\n#include \"scene.h\"\n#include \"bgcheck.h\"\n\n"
        "extern SceneCmd %s[];\nextern CollisionHeader %s_col;\n\n#endif\n" % (sc.upper(), sc.upper(), sc, sc))

    R = []
    R.append('#include "%s.h"\n#include "%s.h"\n' % (rm, sc))
    R.append('#include "actor.h"\n#include "object.h"\n#include "array_count.h"\n#include "alignment.h"\n#include "gfx.h"\n#include "room.h"\n#include "scene.h"\n'
             '#include "ultra64.h"\n\n')
    R.append("static RoomShapeImageMulti sRoomShape;\n\n")
    objects = sorted({a[1] for a in actors if a[1]})
    cmds = ROOM_CMDS % rm
    if actors:
        if objects:
            R.append("static s16 sObjects[%d];\n" % len(objects))
            cmds = cmds.replace("    SCENE_CMD_END(),", "    SCENE_CMD_OBJECT_LIST(ARRAY_COUNT(sObjects), sObjects),\n    SCENE_CMD_END(),")
        R.append("static ActorEntry sActors[%d];\n\n" % len(actors))
        cmds = cmds.replace("    SCENE_CMD_END(),", "    SCENE_CMD_ACTOR_LIST(ARRAY_COUNT(sActors), sActors),\n    SCENE_CMD_END(),")
    R.append(cmds)
    if actors:
        if objects:
            R.append("static s16 sObjects[%d] = { %s };\n\n" % (len(objects), ", ".join(objects)))
        R.append("static ActorEntry sActors[%d] = {\n" % len(actors))
        for actor, _, (x, y, z), rot, params in actors:
            # (a pickup can sit above the floor: the map on the gallery's statue)
            p = to_oot((x, Floor(room_id).height(x, z) + (y if actor in ("ACTOR_EN_RE_PICKUP", "ACTOR_EN_RE_BLOCK") else 0), z))
            rx, ry, rz = rot if isinstance(rot, tuple) else (0, rot, 0)
            R.append("    { %s, { %d, %d, %d }, { %d, %d, %d }, 0x%04X },\n"
                     % (actor, round(p[0]), round(p[1]), round(p[2]), rx, ry, rz, params))
        R.append("};\n\n")
    for c in range(ncam):
        R.append("static u64 sBg%d[] = {\n"
                 '#include "assets/scenes/mod1/%s/%s_bg%d.jfif.inc.c"\n};\n\n' % (c, name, rm, c))
    R.append("// Shared decode target for the active camera's background, plus 8 bytes of bookkeeping.\n"
             "// Initialized (non-zero) so it lands in .data and ships in the room file.\n"
             "static u64 sCanvas[(SCREEN_WIDTH * SCREEN_HEIGHT * G_IM_SIZ_16b_BYTES + 8) / sizeof(u64)] = { 1 };\n\n")
    mask_dls = []
    for c in range(ncam):
        bg = rdt.load_background(rdt.find_file(stage, "RC%s%d.pak" % (room_id, c)))
        dl, code = mask_c(room, c, "sMask%d" % c, bg, [tuple(m["rect"]) + (m["depth"],) for m in extra_masks
                                                      if m["camera"] == c])
        mask_dls.append(dl)
        R.append(code)
    # tlut is unused for full-color (JPEG) backgrounds: Mod1 keeps the camera's mask DL there
    R.append("static RoomShapeImageMultiBgEntry sBackgrounds[] = {\n")
    for c in range(ncam):
        R.append("    { 130, %d, sBg%d, (u32)sCanvas, %s, 320, 240, 0, 2, 0, 0 },\n" % (c, c, mask_dls[c] or "NULL"))
    R.append("};\n\n")
    R.append("static RoomShapeDListsEntry sDLists = { NULL, NULL };\n\n")
    R.append("static RoomShapeImageMulti sRoomShape = {\n    { { ROOM_SHAPE_TYPE_IMAGE }, ROOM_SHAPE_IMAGE_AMOUNT_MULTI, &sDLists },\n"
             "    ARRAY_COUNT(sBackgrounds), sBackgrounds,\n};\n\n")
    open(os.path.join(d, rm + ".c"), "w").write("".join(R))
    open(os.path.join(d, "spawns.txt"), "w").write("%d\n" % len(spawns))
    open(os.path.join(d, "re1room.txt"), "w").write("%s\n" % room_id)  # pause map floor (integrate.py)
    # what the scene keeps in the save (integrate.py gives it words of it): switch flags (story
    # events, characters), collectible flags (pickups), one word per room object model
    names = [act[0] for act in actors]
    objs = [act[4] & 0x3F for act in actors if act[0] == "ACTOR_EN_RE_BLOCK" and not act[4] >> 6]  # pushed ones
    json.dump({"swch": any(n in ("ACTOR_EN_RE_EVENT", "ACTOR_EN_RE_NPC", "ACTOR_EN_RE_SONG", "ACTOR_EN_RE_USE",
                                 "ACTOR_EN_RE_DOG", "ACTOR_EN_RE_ZOMBIE", "ACTOR_EN_RE_ORDER", "ACTOR_EN_RE_CROW",
                                 "ACTOR_EN_RE_YAWN")
                           for n in names) or any(act[0] == "ACTOR_EN_RE_PICKUP" and act[3][1] >> 8 for act in actors)
                       or any(act[0] == "ACTOR_EN_RE_CHECK" and act[4] & examine.QUESTION_BIT for act in actors)
                       or any(act[0] == "ACTOR_EN_RE_LOCK" and act[4] >> 8 & 0x3F and (act[4] >> 6) & 3 == 2
                              for act in actors),
               "collect": "ACTOR_EN_RE_PICKUP" in names, "objects": max(objs) + 1 if objs else 0},
              open(os.path.join(d, "save.json"), "w"))
    open(os.path.join(d, rm + ".h"), "w").write(
        "#ifndef %s_H\n#define %s_H\n\n#include \"ultra64.h\"\n#include \"scene.h\"\n\nextern SceneCmd %s[];\n\n#endif\n"
        % (rm.upper(), rm.upper(), rm))
    return col


def re_dir_to_yaw(direction):
    """RE1 facing (4096 = 360; models face +X and turn toward -Z) -> OoT yaw (signed binang)."""
    v = (0x4000 - direction * 16) & 0xFFFF
    return v - 0x10000 if v >= 0x8000 else v


def stage_dir(path, room_id):
    """--stage may be JPN/ or any JPN/STAGEn: rooms are found in STAGE<first digit of id>."""
    base = os.path.dirname(path.rstrip("/")) if os.path.basename(path.rstrip("/")).upper().startswith("STAGE") else path
    for fn in os.listdir(base):
        if fn.upper() == "STAGE" + room_id[0]:
            return os.path.join(base, fn)
    raise SystemExit("no STAGE%s directory next to %s" % (room_id[0], path))


# RE1's later mansion stages (6, 7) are the mansion again (1, 2): its doors are the same doors
SAME_STAGE = {"6": "1", "7": "2"}


def lock_bits(path, player):
    """Every locked door in the game, numbered for the save (ReDoor_Unlocked): {(room, room):
    bit}, one per pair of rooms (both sides of a door share it), sorted so a door keeps its bit
    however many rooms are built. RE1 sets each lock's own flag; this keeps 64 for all of them."""
    base = os.path.dirname(stage_dir(path, "1"))
    pairs = set()
    for sd in os.listdir(base):
        if not sd.upper().startswith("STAGE"):
            continue
        for fn in os.listdir(os.path.join(base, sd)):
            u = fn.upper()
            if not (u.startswith("ROOM") and u.endswith("%s.RDT" % player) and len(u) == 12):
                continue
            try:
                room = rdt.load_room(os.path.join(base, sd, fn))
            except Exception:  # a few rooms aren't ones rdt.py reads (unused/cut)
                continue
            for door in room.doors or []:
                if door.raw[24]:
                    pairs.add(lock_pair(u[4:7], door))
    bits = {k: i for i, k in enumerate(sorted(pairs))}
    if len(bits) > 64:
        raise SystemExit("%d locked doors: more than ReInventory.doors holds" % len(bits))
    return bits


def lock_pair(rid, door):
    a = rid.upper()
    b = "%d%02X" % (door.next_stage or int(a[0]), door.next_room)
    a, b = (SAME_STAGE.get(r[0], r[0]) + r[1:] for r in (a, b))
    return tuple(sorted((a, b)))


def load_rdt(stage, room_id, player):
    for fn in os.listdir(stage):
        if fn.upper() == ("ROOM%s%s.RDT" % (room_id, player)).upper():
            return rdt.load_room(os.path.join(stage, fn))
    raise SystemExit("no RDT for room %s" % room_id)


def _pack_soh(folder, re1, anims):
    import soh_doors
    import soh_scene
    import soh_enemy
    print("soh: door scenes %s" % soh_doors.write(re1, folder, anims))
    print("soh: enemies %s" % soh_enemy.write(re1, folder, soh_scene.ENEMY_TYPES))
    soh_scene.pack_o2r(folder, folder.rstrip("/") + ".o2r")
    print("soh: packed %s.o2r" % folder.rstrip("/"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--stage", required=True, help="RE1 JPN directory (or one of its STAGEn directories)")
    ap.add_argument("--exe", default=None, help="RE1's PC executable (Bio.exe / the GOG one): its per-camera "
                    "mask depth corrections (re1_masks.py); without it those 29 cameras sort uncorrected")
    ap.add_argument("--soh", default=None, help="also write each room as Ship of Harkinian resources into this folder "
                    "(soh_scene.py), and pack them into <folder>.o2r")
    ap.add_argument("--room", action="append", required=True,
                    help="room id:scene name, e.g. 106:re_foyer (repeat; doors between listed rooms become exits)")
    ap.add_argument("--out", required=True)
    ap.add_argument("--player", default="0", help="RDT player variant: 0 Chris, 1 Jill")
    ap.add_argument("--spawn", default=None, help="x,z[,yaw] in RE1 units for the first room's spawn 0")
    ap.add_argument("--story", default=None, help="story_*.json: trigger areas for RE1 conversations")
    ap.add_argument("--examine", action="store_true",
                    help="place RE1's examine spots (En_ReCheck); integrate.py --examine-text adds their text")
    ap.add_argument("--door-scenes", default="",
                    help="RE1 door files converted by re_door.py, by Door_set animation index (e.g. 3): "
                    "doors that use them play RE1's door scene (En_ReDoor) instead of the fade")
    ap.add_argument("--object-sizes", default=None,
                    help="object_sizes.py's JSON: door scenes are only added while the room's objects fit "
                    "--object-budget (the rest keep the fade)")
    ap.add_argument("--object-budget", type=int, default=430000,
                    help="bytes of object space for a room's objects (OoT's 1000 KB less gameplay_keep and "
                    "the rest: 438,352 bytes in the corridor, measured in game; some margin kept)")
    ap.add_argument("--outside-exit", action="append", default=[],
                    help="ROOM:x,z,w,d:ENTRANCE: an extra exit to an OoT entrance over that RE1 rectangle (no "
                    "margin), e.g. 106:14600,800,5200,2200:ENTR_GRAVEYARD_2 for the foyer's front door (testing)")
    ap.add_argument("--room-objects", default="",
                    help="rooms whose own object models re_objects.py converted (e.g. 107): their Om_set "
                    "placements become En_ReBlock (blocks to push, steps to climb)")
    ap.add_argument("--preview", default=None, help="camera-region map of the first room")
    a = ap.parse_args()
    import re1_masks
    bias_tables = re1_masks.load_bias_tables(a.exe)
    if a.soh and not bias_tables:
        print("no executable (--exe): masks without RE1's per-camera depth corrections")

    rooms = []
    for item in a.room:
        room_id, name = item.split(":")
        rooms.append((room_id.upper(), name, load_rdt(stage_dir(a.stage, room_id), room_id, a.player)))
        apply_trims(rooms[-1][2], room_id.upper())
    by_id = {rid: (name, room) for rid, name, room in rooms}

    # floor levels: the non-zero heights RE1's doors arrive at (ROOM_LEVELS)
    for rid, name, room in rooms:
        for door in room.doors:
            y = door.next_pos[1] - 0x10000 if door.next_pos[1] >= 0x8000 else door.next_pos[1]
            if y:
                target = "%d%02X" % (door.next_stage or int(rid[0]), door.next_room)
                ROOM_LEVELS[target] = y

    # spawns: 0 = test/default spawn, then one per door arriving from another listed room
    spawns = {rid: [] for rid, _, _ in rooms}
    arrivals = {}  # (from_room, door index) -> (to_room, spawn index)
    for rid, name, room in rooms:
        for di, door in enumerate(room.doors):
            target = "%d%02X" % (door.next_stage or int(rid[0]), door.next_room)
            if target in by_id:
                arrivals[(rid, di)] = (target, None)
    for rid, name, room in rooms:
        grid = assign_cameras(room)
        fl = Floor(rid)
        open_cells = [k for k in grid if not any(_inside_collision(c, (k[0] + 0.5) * CELL, (k[1] + 0.5) * CELL)
                                                 and not fl.is_open(c) and _solid(c) for c in room.collisions)]
        if rid == rooms[0][0] and a.spawn:
            p = [float(v) for v in a.spawn.split(",")]
            spawn0 = (p[0], p[1], int(p[2]) if len(p) > 2 else 0)
        else:
            cx = sum(k[0] for k in open_cells) / len(open_cells)
            cz = sum(k[1] for k in open_cells) / len(open_cells)
            k = min(open_cells, key=lambda k: (k[0] - cx) ** 2 + (k[1] - cz) ** 2)
            spawn0 = ((k[0] + 0.5) * CELL, (k[1] + 0.5) * CELL, 0)
        spawns[rid].append(spawn0)
        by_id[rid] = (name, room, grid, open_cells)
    for (src, di), (target, _) in list(arrivals.items()):
        door = by_id[src][1].doors[di]
        spawns[target].append((door.next_pos[0], door.next_pos[2], re_dir_to_yaw(door.next_dir)))
        arrivals[(src, di)] = (target, len(spawns[target]) - 1)

    # door exits per room. RE1's doors open with the action button only, so they have no floor
    # cells: En_ReLock (A on the door) takes its exit (door_exit: (room, door) -> 0-based exit);
    # floor cells -> 1-based exit index are left for --outside-exit's walk-out
    plans, door_exit, door_zones = {}, {}, {}
    for rid, name, room in rooms:
        _, _, grid, open_cells = by_id[rid]
        s0 = spawns[rid][0]
        grid = reachable(grid, open_cells, [(int(sx // CELL), int(sz // CELL)) for sx, sz, _ in spawns[rid]])
        exits, cell_exit, door_cells = [], {}, set()
        for di, door in enumerate(room.doors):
            if (rid, di) not in arrivals:
                continue
            target, spawn_index = arrivals[(rid, di)]
            exits.append("ENTR_%s_%d" % (by_id[target][0].upper(), spawn_index))
            door_exit[(rid, di)] = len(exits) - 1
            for k in grid:  # the door's zone (floor cells), which arrival points are walked clear of
                x, z = (k[0] + 0.5) * CELL, (k[1] + 0.5) * CELL
                m = DOOR_MARGIN if door.anim != STAIRS_ANIM else STAIRS_MARGIN
                if door.x - m <= x <= door.x + door.w + m and door.z - m <= z <= door.z + door.d + m:
                    door_cells.add(k)
            print("  %s door %d -> %s spawn %d (exit %d)" % (name, di, by_id[target][0], spawn_index, len(exits) - 1))
        for spec in a.outside_exit:
            room_part, rect, entrance = spec.split(":")
            if room_part.upper() != rid:
                continue
            ox, oz, ow, od = (int(v) for v in rect.split(","))
            exits.append(entrance)
            for k in grid:
                x, z = (k[0] + 0.5) * CELL, (k[1] + 0.5) * CELL
                if ox <= x <= ox + ow and oz <= z <= oz + od:
                    cell_exit[k] = len(exits)
            if (int(s0[0] // CELL), int(s0[1] // CELL)) in cell_exit:
                raise SystemExit("--outside-exit %s covers %s's spawn 0" % (spec, name))
            print("  %s outside exit -> %s (%d exit cells)"
                  % (name, entrance, sum(1 for v in cell_exit.values() if v == len(exits))))
        plans[rid] = (grid, exits, cell_exit)
        door_zones[rid] = door_cells

    # SoH: RE1's own arrival points, as they are. Its doors open only when the point in front of
    # the player is in the door's rectangle and the action button is pressed (Bio.exe 0x456880),
    # and Link arrives facing away from the door, so nothing needs walking clear. Spawn 0 (the
    # room's own start) is the first room's --spawn, else the room's first arrival.
    soh_spawns = {}
    for rid, name, room in rooms:
        first = spawns[rid][0] if (rid == rooms[0][0] and a.spawn) or len(spawns[rid]) < 2 else spawns[rid][1]
        soh_spawns[rid] = [first] + list(spawns[rid][1:])

    # arrival points sit close to the door in RE1, against the wall; walk them into the room
    # until they're clear of the door zones (and any exit), as RE1's Jill steps in (N64 scenes)
    for rid, name, room in rooms:
        grid, exits, cell_exit = plans[rid]
        cell_exit = dict(cell_exit)
        cell_exit.update({k: 0 for k in door_zones[rid] if k not in cell_exit})
        for i in range(1, len(spawns[rid])):
            x, z, yaw = spawns[rid][i]
            fx, fz = math.sin(yaw * math.pi / 0x8000), -math.cos(yaw * math.pi / 0x8000)  # OoT yaw -> RE1 x/z
            clear = 0
            for _ in range(60):
                if (int(x // CELL), int(z // CELL)) in cell_exit:
                    clear = 0
                else:
                    clear += 100
                    if clear >= 400:
                        break
                x, z = x + fx * 100, z + fz * 100
            # ...and onto the floor: RE1's arrival point can sit just past it (202's, from 203's
            # door, over the dining room's drop), where Link would fall forever
            open_set = set(by_id[rid][3]) & set(grid)
            k = (int(x // CELL), int(z // CELL))
            if k not in open_set:
                k = min((c for c in open_set if c not in cell_exit),
                        key=lambda c: ((c[0] + 0.5) * CELL - x) ** 2 + ((c[1] + 0.5) * CELL - z) ** 2)
                x, z = (k[0] + 0.5) * CELL, (k[1] + 0.5) * CELL
            spawns[rid][i] = (x, z, yaw)

    examine_count, examine_info = 0, {}  # examine spots: running text id, per scene
    locks = None  # lock_bits(), once a built door is locked
    for rid, name, room in rooms:
        grid, exits, cell_exit = plans[rid]
        if a.preview and rid == rooms[0][0]:
            preview(room, grid, a.preview)
        actors = room_actors(room, grid, [d for d in (story.load_enemies(a.story) if a.story else [])
                                          if d["room"].upper() == rid], rid)
        cast = [c for c in story.load_cast(a.story) if c["room"] == rid] if a.story else []
        actors += room_cast(room, grid, [dict({"model": int(c["model"], 0), "pick": c.get("pick", 0),
                                               "flag_bits": story.cast_flag_bits(a.story, c),
                                               "story": story.cast_story_masks(c)},
                                              **({"at": c["at"]} if "at" in c else {})) for c in cast])
        for idx, ev in story.room_triggers(story.load_story(a.story) if a.story else [], rid):
            x0, z0, x1, z1 = ev["zone"]
            half = (round((x1 - x0) / 2 * SCALE), round((z1 - z0) / 2 * SCALE))
            actors.append(("ACTOR_EN_RE_EVENT", None, ((x0 + x1) / 2, 0, (z0 + z1) / 2), (half[0], 0, half[1]),
                           (ev.get("flag", 0x3F) & 0x3F) | (idx << 6)))
        # the room's own object models (Om_set): blocks Link can push, steps he can climb
        if rid in {r.upper() for r in a.room_objects.split(",") if r}:
            seen_om = set()
            kinds = {o["model"]: o for o in (story.load_objects(a.story) if a.story else []) if o["room"].upper() == rid}
            for om in room.objects or []:
                at = kinds.get(om.id & 0x3F, {}).get("at")
                if om.id in seen_om or (at and (om.pos[0], om.pos[2]) != tuple(at)):
                    continue  # (the scripts place some objects twice, by state: "at" picks one)
                seen_om.add(om.id)
                yaw = (-om.dir * 16) & 0xFFFF  # the model is mirrored into OoT's axes, not turned
                yaw = yaw - 0x10000 if yaw >= 0x8000 else yaw
                spec = kinds.get(om.id & 0x3F, {})
                params, rot = om.id & 0x3F, yaw
                if spec.get("kind") == "skip":  # not built yet (e.g. only there after another puzzle)
                    continue
                om_y = 0
                if spec.get("kind") == "fixed":
                    params |= 1 << 6
                elif spec.get("kind") == "shown":  # drawn once a switch flag is set (11A's crests)
                    params |= (3 << 6) | ((spec["flag"] & 0x3F) << 8)
                    om_y = om.pos[1]  # where RE1 has it (the crests sit up on the door)
                    if "pitch" in spec:  # turned about x instead (11A's wind crest: its model lies flat)
                        rot = ((spec["pitch"] * 16 + 0x8000) % 0x10000 - 0x8000, 0, 0)
                elif spec.get("kind") == "slide":
                    dx, dz = spec["slide"]
                    sx, sz = dx * SCALE, -dz * SCALE  # RE1 -> OoT axes (z mirrored)
                    slide_yaw = round(math.atan2(sx, sz) / math.pi * 0x8000)
                    slide_yaw = slide_yaw - 0x10000 if slide_yaw >= 0x8000 else slide_yaw
                    params |= (2 << 6) | ((spec["flag"] & 0x3F) << 8) | ((1 << 14) if spec.get("visible") else 0)
                    dist = round(math.hypot(sx, sz))
                    assert dist < 256, "slide too far for En_ReBlock (%d)" % dist
                    rot = (slide_yaw, yaw, dist | (((spec.get("shut_flag", -1) + 1) & 0x3F) << 8))
                elif "target" in spec:  # pushed onto its spot it stays, setting a switch flag (205's armor)
                    tx, tz = spec["target"]["at"]
                    params |= (((spec["target"]["switch"] + 1) & 0x3F) << 8) | (1 << 14)
                    rot = (round(tx * SCALE), yaw, round(-tz * SCALE))
                elif "drop" in spec:  # pushed to its edge spot it goes over (202's statue)
                    ex, ez = spec["drop"]["edge"]
                    params |= ((story.STORY_FLAGS[spec["drop"]["story"]] + 1) & 0x3F) << 8
                    rot = (round(ex * SCALE), yaw, round(-ez * SCALE))
                nx, nz = spec.get("nudge", (0, 0))  # (out from a wall: the background's depth hides it there)
                actors.append(("ACTOR_EN_RE_BLOCK", "OBJECT_RE_OM%s" % rid, (om.pos[0] + nx, om_y, om.pos[2] + nz), rot,
                               params))
        # spots where an item is Used (En_ReUse: RE1's Aot type 7)
        for u in (story.load_uses(a.story) if a.story else []):
            if u["room"].upper() != rid:
                continue
            x, z, w, d = u["rect"]
            none = 0x3F
            actors.append(("ACTOR_EN_RE_USE", None, (x + w / 2, 0, z + d / 2),
                           (round(w / 2 * SCALE), (u.get("clears", none) & 0x3F) | ((u.get("requires", none) & 0x3F) << 6)
                            | (((u.get("cam", -1) + 1) & 0xF) << 12),
                            round(d / 2 * SCALE)),
                           (int(u["item"], 0) & 0xFF) | ((u.get("sets", none) & 0x3F) << 8)
                           | (1 << 14 if u.get("keep") else 0)))
        # spots that answer the ocarina's "Moonlight Sonata" (En_ReSong: RE1's piano)
        for sp in (story.load_songs(a.story) if a.story else []):
            if sp["room"].upper() != rid:
                continue
            x0, z0, x1, z1 = sp["zone"]
            actors.append(("ACTOR_EN_RE_SONG", None, ((x0 + x1) / 2, 0, (z0 + z1) / 2),
                           (round((x1 - x0) / 2 * SCALE), 0, round((z1 - z0) / 2 * SCALE)),
                           (sp["flag"] & 0x3F) | ((sp["cam"] & 0x1F) << 6)))
        for o in (story.load_orders(a.story) if a.story else []):  # switches to press in order (En_ReOrder)
            if o["room"].upper() == rid:
                actors.append(("ACTOR_EN_RE_ORDER", None, (0, 0, 0), (o.get("fail", -1) + 1, 0, (1 if o.get("any") else 0) | (2 if o.get("gate") else 0)),
                               (o["first"] & 0x3F) | ((o["count"] & 0xF) << 6) | ((o["done"] & 0x3F) << 10)))
        # pickups (En_RePickup): items that have texts (tools/oot/re_items.py) and a place
        # (scripts also park items at 0,0 to move them in later: story_*.json "pickups" places
        # those by hand); one collectible flag each
        pickups, placed = [], set()
        for it in room.items or []:
            if it.item not in re_items.ITEMS or it.w <= 1 or (it.x, it.z) == (0, 0) or (it.flag, it.item) in placed:
                continue  # (a 1x1 rectangle or the origin: parked until a script hands it out; 203's
                # acid rounds, 16x16 at 0,0, are Barry's to give)
            placed.add((it.flag, it.item))
            pickups.append((it.item, it.count, (it.x, it.z, it.w, it.d), 0, None))
        # RE1's aot type 14: a spot that hands out a parked Item_set when checked (its param is
        # the Item_set's id): 102's and 111's shotgun shells
        by_item_id = {it.id: it for it in room.items or []}
        for aot in room.aots or []:
            it = by_item_id.get(aot.param) if aot.type == 14 else None
            if it is None or it.item not in re_items.ITEMS or (it.flag, it.item) in placed:
                continue
            placed.add((it.flag, it.item))
            pickups.append((it.item, it.count, (aot.x, aot.z, aot.w, aot.d), 0, None))
        for p in (story.load_pickups(a.story) if a.story else []):
            if p["room"].upper() == rid:
                pickups.append((int(p["item"], 0), p.get("count", 1), tuple(p["rect"]), p.get("height", 0),
                                p.get("appears")))
        locked_doors = set()  # doors with an En_ReLock of their own (locks, story door_locks)
        if a.examine:
            tier = examine.tiers(a.story).get(rid, {})
            questions = {q.get("msg", "rect%d" % i): q
                         for i, q in enumerate(story.load_questions(a.story) if a.story else [])
                         if q["room"].upper() == rid}
            afters = {af["msg"] if "rect" not in af else "rect%d" % i: af
                      for i, af in enumerate(story.load_examine_after(a.story) if a.story else [])
                      if af["room"].upper() == rid}
            listed = []
            for aot, msg, own_text, attention in examine.spots(room):
                if (own_text not in (examine.TYPEWRITER_TEXT, examine.BOX_TEXT) and msg not in questions
                        and any((aot.x, aot.z, aot.w, aot.d) == pk[2] for pk in pickups)):
                    continue  # the item's own spot ("There is no time to check it" in other states);
                    # a typewriter keeps its spot beside its ribbons (the foyer's), a question its
                    # spot beside the item it hands out first (20A's shelf and its file)
                text_id = examine.TEXT_BASE + examine_count
                examine_count += 1
                attention = attention or (msg is not None and tier.get(str(msg)) == "attention")
                params = (text_id - examine.TEXT_BASE) | (examine.ATTENTION_BIT if attention else 0)
                half = (round(aot.w / 2 * SCALE), round(aot.d / 2 * SCALE))
                if own_text == examine.BOX_TEXT:
                    params |= examine.BOX_BIT
                if own_text == examine.TYPEWRITER_TEXT:  # its two messages: no ribbon / use one?
                    params |= examine.TYPEWRITER_BIT
                    listed.append({"msg": None, "text": None, "exe": examine.TYPEWRITER_NO_RIBBON, "text_id": text_id})
                    listed.append({"msg": None, "text": None, "exe": examine.TYPEWRITER_USE_RIBBON,
                                   "text_id": text_id + 1})
                    examine_count += 1
                elif msg in questions and "rect" not in questions[msg]:  # Yes/No, then what's left
                    examine_count += 1
                    actors.append(question_spot(questions[msg], (aot.x, aot.z, aot.w, aot.d), params, text_id, listed))
                    continue
                else:
                    listed.append({"msg": msg, "text": own_text, "text_id": text_id})
                switch_rot = 0
                if msg in afters and "rect" not in afters[msg]:  # its text changes with a switch flag
                    switch_rot = after_spot(afters[msg], text_id, listed)
                    examine_count += 1
                actors.append(("ACTOR_EN_RE_CHECK", None, (aot.x + aot.w / 2, 0, aot.z + aot.d / 2),
                               (half[0], switch_rot, half[1]), params))
            for ck in (story.load_checks(a.story) if a.story else []):  # RE1's event checks (201's keypad)
                if ck["room"].upper() != rid:
                    continue
                text_id = examine.TEXT_BASE + examine_count
                examine_count += 1
                listed.append({"msg": ck["msg"], "text": None, "text_id": text_id})
                x, z, w, d = ck["rect"]
                actors.append(("ACTOR_EN_RE_CHECK", None, (x + w / 2, 0, z + d / 2),
                               (round(w / 2 * SCALE), ((ck.get("cam", -1) + 1) & 0xFF) << 8, round(d / 2 * SCALE)),
                               text_id - examine.TEXT_BASE))
            for af in afters.values():  # spots of their own (RE1's script turns an aot into the message)
                if "rect" in af:
                    text_id = examine.TEXT_BASE + examine_count
                    examine_count += 1
                    listed.append({"msg": af["msg"], "text": None, "text_id": text_id})
                    switch_rot = after_spot(af, text_id, listed)
                    examine_count += 1
                    x, z, w, d = af["rect"]
                    actors.append(("ACTOR_EN_RE_CHECK", None, (x + w / 2, 0, z + d / 2),
                                   (round(w / 2 * SCALE), switch_rot, round(d / 2 * SCALE)),
                                   (text_id - examine.TEXT_BASE) | examine.ATTENTION_BIT))
            for q in questions.values():  # question spots of their own (no examine aot there in RE1)
                if "rect" in q:
                    text_id = examine.TEXT_BASE + examine_count
                    examine_count += 2
                    actors.append(question_spot(q, q["rect"], examine.ATTENTION_BIT | (text_id - examine.TEXT_BASE),
                                                text_id, listed))
            built = {di for (src, di) in arrivals if src == rid}
            built |= {i for i, door in enumerate(room.doors) if door.anim == STAIRS_ANIM}
            for (x, z, w, d), offset in examine.door_spots(room, built):
                text_id = examine.TEXT_BASE + examine_count
                examine_count += 1
                actors.append(("ACTOR_EN_RE_CHECK", None, (x + w / 2, 0, z + d / 2),
                               (round(w / 2 * SCALE), 0, round(d / 2 * SCALE)), text_id - examine.TEXT_BASE))
                listed.append({"msg": None, "text": None, "exe": offset, "text_id": text_id})
            # locked doors that do lead somewhere built (En_ReLock): shut until opened
            for di, door in enumerate(room.doors):
                lock = door.raw[24]
                if (rid, di) not in arrivals or not lock:
                    continue
                if locks is None:
                    locks = lock_bits(a.stage, a.player)
                text_id = examine.TEXT_BASE + examine_count
                examine_count += 1
                listed.append({"msg": None, "text": None, "exe": examine.lock_message(lock), "text_id": text_id})
                # RE_LOCK_PICK (the sword key's doors: Jill's lock pick) / HERE / KEY (the key's
                # item id, which is the lock byte: armor 0x34, shield 0x35, helmet 0x36) / SHUT
                kind = 0 if lock == 0x33 else 1 if lock == 0xFE else 3 if lock in examine.LOCK_MESSAGES else 2
                m = DOOR_MARGIN
                half = (round((door.w / 2 + m) * SCALE), round((door.d / 2 + m) * SCALE))
                actors.append(("ACTOR_EN_RE_LOCK", None, (door.x + door.w / 2, 0, door.z + door.d / 2),
                               (half[0], (text_id - examine.TEXT_BASE) | ((door_exit[(rid, di)] + 1) << 8), half[1]),
                               locks[lock_pair(rid, door)] | (kind << 6) | ((lock << 8) if kind == 3 else 0)))
                locked_doors.add(di)
                print("  %s door %d locked (0x%02X): bit %d" % (name, di, lock, locks[lock_pair(rid, door)]))
            # doors RE1's script keeps shut until a switch flag (story "door_locks": 11A's crest door)
            for dl in (story.load_door_locks(a.story) if a.story else []):
                if dl["room"].upper() != rid:
                    continue
                door = room.doors[dl["door"]]
                text_id = examine.TEXT_BASE + examine_count
                examine_count += 1
                listed.append({"msg": dl["msg"], "text": None, "text_id": text_id})
                m = DOOR_MARGIN
                half = (round((door.w / 2 + m) * SCALE), round((door.d / 2 + m) * SCALE))
                exit_bits = ((door_exit[(rid, dl["door"])] + 1) << 8) if (rid, dl["door"]) in door_exit else 0
                actors.append(("ACTOR_EN_RE_LOCK", None, (door.x + door.w / 2, 0, door.z + door.d / 2),
                               (half[0], (text_id - examine.TEXT_BASE) | exit_bits, half[1]),
                               (2 << 6) | (((dl["opens"] + 1) & 0x3F) << 8)))
                locked_doors.add(dl["door"])
            if examine_count > examine.TEXT_COUNT:
                raise SystemExit("%d examine texts: more than fit before the item messages" % examine_count)
            examine_info[name] = {"room": rid, "player": a.player, "spots": listed}
        # every other built door (stairs too): an En_ReLock with no lock, A taking its exit
        for di, door in enumerate(room.doors):
            if (rid, di) not in door_exit or di in locked_doors:
                continue
            m = DOOR_MARGIN if door.anim != STAIRS_ANIM else STAIRS_MARGIN
            half = (round((door.w / 2 + m) * SCALE), round((door.d / 2 + m) * SCALE))
            actors.append(("ACTOR_EN_RE_LOCK", None, (door.x + door.w / 2, 0, door.z + door.d / 2),
                           (half[0], ((door_exit[(rid, di)] + 1) << 8) | 0x2000, half[1]), 0))
        # door scenes (En_ReDoor): one per arrival spawn whose door's scene is converted, while the
        # room's objects still fit in OoT's object space (the others keep the fade)
        door_scenes = {int(x) for x in a.door_scenes.split(",") if x}
        door_actors = []
        for (src, di), (target, spawn_index) in arrivals.items():
            door = by_id[src][1].doors[di]
            if target != rid or door.anim not in door_scenes:
                continue
            sx, sz, _ = spawns[rid][spawn_index]
            params = spawn_index | ((door.raw[10] & 0xF) << 4) | ((door.raw[13] & 0xF) << 8)
            door_actors.append(("ACTOR_EN_RE_DOOR", "OBJECT_RE_DOOR%02d" % door.anim, (sx, 0, sz),
                                (door.anim, 0, 0), params))

        # a zombie that follows Link in (RE1: Em_set at the origin, moved in by the room's code;
        # the dining room's for the corridor zombie): it enters through the door from the room
        # where that zombie lives, only once the story says it followed (En_ReZombie, follower)
        for e in room.enemies or []:
            if e.model != 0x11 or max(abs(e.pos[0]), abs(e.pos[2])) > 2:
                continue
            for (src, di), (target, spawn_index) in arrivals.items():
                if target != rid or not any(
                        o.model == 0x11 and max(abs(o.pos[0]), abs(o.pos[2])) > 2 for o in by_id[src][1].enemies or []):
                    continue
                door = by_id[src][1].doors[di]  # RE1's arrival point: by the door, behind Link's spawn
                _, _, syaw = spawns[rid][spawn_index]
                actors.append(("ACTOR_EN_RE_ZOMBIE", "OBJECT_RE_ZOMBIE2H", (door.next_pos[0], 0, door.next_pos[2]),
                               syaw, 3 | (2 << 8)))
                break
            break
        switches = {int(p["item"], 0): p["switch"] for p in (story.load_pickup_switches(a.story) if a.story else [])
                    if p["room"].upper() == rid and "switch" in p}
        appear = {int(p["item"], 0): p["appears"] for p in (story.load_pickup_switches(a.story) if a.story else [])
                  if p["room"].upper() == rid and "appears" in p}
        appear_story = {int(p["item"], 0): story.STORY_FLAGS[p["appears_story"]]
                        for p in (story.load_pickup_switches(a.story) if a.story else [])
                        if p["room"].upper() == rid and "appears_story" in p}
        sets_story = {int(p["item"], 0): story.STORY_FLAGS[p["sets_story"]]
                      for p in (story.load_pickup_switches(a.story) if a.story else [])
                      if p["room"].upper() == rid and "sets_story" in p}
        for n, (item, count, (x, z, w, d), height, appears) in enumerate(pickups):
            half = (round(w / 2 * SCALE), round(d / 2 * SCALE))
            sw = ((switches[item] + 1) & 0x3F) << 8 if item in switches else 0
            if item in sets_story:  # taking it sets a story flag (En_RePickup params bit 15 alone)
                sw = ((sets_story[item] + 1) & 0x3F) << 8
                item |= 1 << 15
            if appears is None:
                appears = appear.get(item)
            if appears is not None:  # hidden until that switch flag (En_RePickup params bit 14)
                sw = ((appears + 1) & 0x3F) << 8
                item |= 1 << 14
            elif item in appear_story:  # ...or until a story flag set in another room (bits 14 + 15)
                sw = ((appear_story[item] + 1) & 0x3F) << 8
                item |= 3 << 14
            # collectible flags from 1: OoT's Flags_SetCollectible ignores flag 0
            assert n + 1 < 0x20, "more pickups than a room's saved collectible flags"
            actors.append(("ACTOR_EN_RE_PICKUP", None, (x + w / 2, -height, z + d / 2), (half[0], count | sw, half[1]),
                           item | ((n + 1) << 8)))
        for actor, _, pos, yaw, params in actors:
            print("  %s: %s at %s params %d" % (name, actor, pos, params))
        actors.extend(fit_door_scenes(name, actors, door_actors, a.object_sizes, a.object_budget))
        extra_masks = [m for m in (story.load_extra_masks(a.story) if a.story else []) if m["room"].upper() == rid]
        col = write_scene(room, grid, stage_dir(a.stage, rid), rid, name, a.out, spawns[rid], exits, cell_exit, actors,
                          extra_masks)
        if a.soh:
            import soh_scene
            soh_doors = [(door, by_id[arrivals[(rid, di)][0]][0] + "_scene", arrivals[(rid, di)][1])
                         for di, door in enumerate(room.doors) if (rid, di) in arrivals]
            arr = []
            for (src, di), (target, _) in arrivals.items():
                if target == rid:
                    dd = by_id[src][1].doors[di]
                    ny = dd.next_pos[1] - 0x10000 if dd.next_pos[1] >= 0x8000 else dd.next_pos[1]
                    arr.append((dd.next_pos[0], dd.next_pos[2], ny))
            print("  soh: %s" % soh_scene.write_room(room, grid, stage_dir(a.stage, rid), rid, name, a.soh,
                                                     soh_spawns[rid], doors=soh_doors, bias_tables=bias_tables,
                                                     arrivals=arr, aots=soh_scene.room_aots(room.raw)))
        camset = os.path.join(a.out, name, "camset.json")
        for cs in (story.load_camera_sets(a.story) if a.story else []):
            if cs["room"].upper() == rid:
                json.dump({"count": cs["count"], "flag": cs["flag"]}, open(camset, "w"))
                break
        else:
            if os.path.exists(camset):
                os.remove(camset)
        if name in examine_info:
            with open(os.path.join(a.out, name, "examine.json"), "w") as f:
                json.dump(examine_info[name], f, indent=1)
        print("%s: %d cameras, %d spawns, %d exits, %d collision polys, %d verts"
              % (name, len(room.cameras), len(spawns[rid]), len(exits), len(col.polys), len(col.verts)))
    if a.soh:
        base = os.path.dirname(a.stage.rstrip("/")) if os.path.basename(a.stage.rstrip("/")).upper().startswith(
            "STAGE") else a.stage
        _pack_soh(a.soh, base, [d.anim for rid, _, room in rooms for di, d in enumerate(room.doors)
                                if (rid, di) in arrivals])


if __name__ == "__main__":
    main()
