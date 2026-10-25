"""RE1 rooms as Ship of Harkinian resources (XML scenes, rooms and collision, plus the
camera backgrounds), written into a folder that pack_o2r() zips into a mod archive.

The room data is rdt2scene.py's (the N64 build's converter): the floor grid, collision,
cameras and spawns are worked out there, and this module only writes them in SoH's XML
resource formats (soh/soh/resource/importer/*Factory.cpp, the XML readers) instead of C.

Layout, per room (SoH loads scenes from scenes/shared/<file>/<file>, z_play_otr.cpp):
    scenes/shared/<name>_scene/<name>_scene      the scene (XML, root <Room>)
    scenes/shared/<name>_scene/<name>_room_0     its one room (XML)
    scenes/shared/<name>_scene/<name>_col        collision (XML <CollisionHeader>)
    scenes/shared/<name>_scene/<name>_bg<n>      camera n's background (binary Background:
                                                 a JPEG, decoded by SoH's stb_image path)

Game data: everything written here comes from the player's own RE1 files; the output is
never committed.
"""

import io
import math
import os
import struct
import zipfile
from xml.sax.saxutils import quoteattr

from PIL import Image

import rdt
import re1_floor
import re1_masks
import rdt2scene as r2s

SOH_BACKGROUND = 0x4F424749  # "OBGI", SohResourceType.h
LUS_BLOB = 0x4F424C42  # "OBLB", libultraship ResourceType.h
MASK_MAGIC = b"RMSK"
DOOR_MAGIC = b"RDOR"
DOOR_ACTION = 0x80  # Door_set byte 0x19: opens with the action button (all of the mansion's doors)
DOOR_AT_PLAYER = 0x40  # ...tested at the player's position instead of the point in front of him
BG_DECODED_SIZE = 320 * 240 * 2  # the decoded background, RGBA16 (what SoH writes back over the JPEG)


def _otr_header(res_type, version=0, res_id=0):
    """libultraship's 64-byte binary resource header (ResourceLoader::ReadResourceInitDataBinary):
    byte order (0: little), custom flag, 2 unused, type, version, id; the rest reserved."""
    return struct.pack("<BB2xIIQ", 0, 1, res_type, version, res_id).ljust(64, b"\0")


def _surface0(cam, exit_index=0):
    """SURFACETYPE0(bgCamIndex, exitIndex, FLOOR_TYPE_0, 0, WALL_TYPE_0, FLOOR_PROPERTY_0, false, false)"""
    return (cam & 0xFF) | ((exit_index & 0x1F) << 8)


# SURFACETYPE1(SURFACE_MATERIAL_STONE, FLOOR_EFFECT_0, 0, 1, false, CONVEYOR_SPEED_DISABLED, 0, false)
SURFACE1_STONE = 2 | (1 << 11)


def _snormal(v):
    """COLPOLY_SNORMAL"""
    return int(round(v * 32767.0))


def _el(tag, **attrs):
    return "<%s %s/>" % (tag, " ".join("%s=%s" % (k, quoteattr(str(v))) for k, v in attrs.items()))


def background_jpeg(stage, room_id, cam_index, quality=95):
    """Camera background as a baseline JPEG for SoH (ResourceMgr_LoadJPEG: stb_image, 320x240)."""
    w, h, rgba = rdt.load_background(rdt.find_file(stage, "RC%s%d.pak" % (room_id, cam_index)))
    buf = io.BytesIO()
    Image.frombytes("RGBA", (w, h), rgba).convert("RGB").save(buf, "JPEG", quality=quality, subsampling=0)
    return buf.getvalue()


def background_raw(stage, room_id, cam_index):
    """Camera background as raw RGBA5551, big-endian, 320x240: RE1's own 15-bit pixels, bit for
    bit (lossless). SoH draws a Background's data directly as RGBA16 unless it starts with a
    JPEG header (z_room.c swapAndConvertJPEG), so no decoding happens at all."""
    with open(rdt.find_file(stage, "RC%s%d.pak" % (room_id, cam_index)), "rb") as f:
        tim = rdt.emd.parse_tim(rdt.pak_depack(f.read()), 0)
    assert tim.bpp == 16 and tim.width == 320 and tim.height == 240, (room_id, cam_index, tim.bpp)
    out = bytearray(320 * 240 * 2)
    for i in range(320 * 240):
        c = tim.pixels[2 * i] | (tim.pixels[2 * i + 1] << 8)  # PSX BGR555 (little-endian)
        r, g, b = c & 31, (c >> 5) & 31, (c >> 10) & 31
        v = (r << 11) | (g << 6) | (b << 1) | 1  # opaque everywhere (backgrounds have no holes)
        out[2 * i] = v >> 8
        out[2 * i + 1] = v & 0xFF
    return bytes(out)


def mask_blob(pieces):
    """The mask resource's data (re1_masks.camera_masks pieces): "RMSK", u32 count; per piece
    s16 x, y, u16 w, h, stride (w rounded up to 8), u16 group (what room scripts switch), f32
    depth (RE1's ordering key * 16: view-space depth, in OoT units); then stride * h I8 texels."""
    data = bytearray(MASK_MAGIC + struct.pack("<I", len(pieces)))
    for x, y, w, h, key, group, rows in pieces:
        stride = (w + 7) & ~7
        data += struct.pack("<hhHHHHf", x, y, w, h, stride, group, key * 16 * r2s.SCALE)
        for r in rows:
            data += bytes(r) + b"\0" * (stride - w)
    return bytes(data)


def extend_floor(room, grid, floor, steps=4):
    """The floor grid grown up to `steps` cells (~35 OoT units) into free space beside it: RE1's
    camera zones don't cover all the floor Jill can walk (she keeps the last camera there; in
    203 the strip before the east wall, with its two doors, lies outside them), and the grid
    only has cells in a zone. Cells inside RE1's solid shapes stay out; a grown cell takes its
    neighbour's surface (camera), as RE1 keeps the camera it had."""
    solid = [c for c in room.collisions if r2s._solid(c) and not floor.is_open(c)]
    C = r2s.CELL
    out = dict(grid)
    front = list(grid)
    for _ in range(steps):
        nxt = []
        for (gx, gz) in front:
            for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                n = (gx + dx, gz + dz)
                if n in out:
                    continue
                x, z = (n[0] + 0.5) * C, (n[1] + 0.5) * C
                if any(r2s._inside_collision(c, x, z) for c in solid):
                    continue
                out[n] = out[(gx, gz)]
                nxt.append(n)
        front = nxt
    return out


def grow_into_walls(room, grid, floor):
    """The floor grid plus one ring of cells reaching into RE1's solid shapes. The grid keeps
    only cells whose middle is clear of them, so it stops up to a cell short of RE1's walls;
    RE1's walls still stop Link, and the floor's edge (edge_walls) then sits at or behind them
    instead of in front (where it kept him out of door alcoves)."""
    solid = [c for c in room.collisions if r2s._solid(c) and not floor.is_open(c)]
    C = r2s.CELL
    out = dict(grid)
    for (gx, gz), surf in grid.items():
        for dx in (-1, 0, 1):
            for dz in (-1, 0, 1):
                n = (gx + dx, gz + dz)
                if n in out:
                    continue
                x, z = (n[0] + 0.5) * C, (n[1] + 0.5) * C
                if any(r2s._inside_collision(c, x, z) for c in solid):
                    out[n] = surf
    return out


def edge_walls(col, grid, wall_surface, floor):
    """Walls along every outer edge of the walkable floor (cell edges with no floor beyond),
    merged into straight runs. RE1's own collision shapes don't close the floor everywhere (the
    foot of the foyer's stairs, some door alcoves: RE1 keeps Jill in by its camera zones), and
    Link would walk off the edge into nothing. Facing into the floor."""
    C = r2s.CELL
    runs = []  # (axis, line, start, end, inward sign)
    for (gx, gz) in grid:
        for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            if (gx + dx, gz + dz) in grid:
                continue
            if dx:  # an edge along z at x = gx + 1 (east) or gx (west)
                runs.append(("x", gx + (dx > 0), gz, gz + 1, -dx))
            else:
                runs.append(("z", gz + (dz > 0), gx, gx + 1, -dz))
    runs.sort()
    merged = []
    for r in runs:
        if merged and merged[-1][0] == r[0] and merged[-1][1] == r[1] and merged[-1][4] == r[4] and \
                merged[-1][3] == r[2]:
            merged[-1] = (r[0], r[1], merged[-1][2], r[3], r[4])
        else:
            merged.append(r)
    for axis, line, a, b, inward in merged:
        if axis == "x":
            p0, p1 = (line * C, a * C), (line * C, b * C)
        else:
            p0, p1 = (a * C, line * C), (b * C, line * C)
        lo = r2s.to_oot((p0[0], floor.depth, p0[1]))
        hi = r2s.to_oot((p1[0], floor.depth, p1[1]))
        top0 = r2s.to_oot((p0[0], floor.top, p0[1]))
        top1 = r2s.to_oot((p1[0], floor.top, p1[1]))
        col.quad(wall_surface, lo, hi, top1, top0)
        # face into the floor: the inward direction in RE1 units, then OoT's (x, -y, -z)
        want = (inward, 0) if axis == "x" else (0, -inward)
        for k in (-2, -1):
            surf, idx, n, d = col.polys[k]
            if n[0] * want[0] + n[2] * want[1] < 0:
                col.polys[k] = (surf, (idx[0], idx[2], idx[1]), [-x for x in n], -d)


def door_blob(doors):
    """Doors, for soh/src/code/z_nv_door.c: "RDOR", u32 count; per door f32 x0, z0, x1, z1 (the
    trigger rectangle, OoT units), u8 flags (Door_set byte 0x19), u8 anim (RE1's door scene),
    u16 spawn (the target room's start position), u8 Door_set bytes 10, 11, 13 and 15 (what
    RE1's door scene reads as its variables: Bio.exe's table at 0x4b0de8), char target[24]
    (the target scene's file).
    doors: [(Door, target scene file, spawn index)]."""
    data = bytearray(DOOR_MAGIC + struct.pack("<I", len(doors)))
    for door, target, spawn in doors:
        # OoT = (x, -y, -z) * SCALE: RE1's z range flips
        x0, x1 = door.x * r2s.SCALE, (door.x + door.w) * r2s.SCALE
        z0, z1 = -(door.z + door.d) * r2s.SCALE, -door.z * r2s.SCALE
        data += struct.pack("<4fBBH4B24s", x0, z0, x1, z1, door.raw[0x19], door.anim, spawn, door.raw[10],
                            door.raw[11], door.raw[13], door.raw[15], target.encode())
    return bytes(data)


def room_aots(raw):
    """Aot_set (SCD 0x0D, 18 bytes) zones of the room's two scripts: [(type, x, z, w, d, d0, sat)].
    sat: 0x40 tests the player's own position instead of the point 600 ahead of her, 0x80 only on
    the action button (update_player_position / check_action_object)."""
    offs = struct.unpack_from("<19I", raw, 0x94 - 76)
    out = []
    for o in (offs[6], offs[7]):
        if not o:
            continue
        length = struct.unpack_from("<H", raw, o)[0]
        code = raw[o + 2:o + 2 + length]
        i = 0
        while i < len(code):
            n = rdt.scd_inst_len(code, i)
            if n <= 0:
                break
            if code[i] == 0x0D and i + 18 <= len(code):
                _op, _id, x, z, w, d, t, sat, d0, _d1, _d2 = struct.unpack_from("<BB4H2B3h", code, i)
                out.append((t, x, z, w, d, d0, sat))
            i += n
    return out


ENEMY_TYPES = set()  # enemy types the written rooms place (soh_enemy.py writes their models)


def room_enemies(raw):
    """Enemy_set (SCD 0x1B, 22 bytes) of the room's init script, each with the conditions of the
    if/else blocks around it: [(raw, [(want, [(bank, bit, value)])])]. A term holds when its bit
    tests (SCD 0x04, cmd_bit_test) all match, or don't, as want says: an if's own tests, or not
    them in its else (if: 01 len, the tests, its block to +len; else: 02 len, its block to +len).
    Other conditions (0x06 compares...) are left out of the term: true."""
    offs = struct.unpack_from("<19I", raw, 0x94 - 76)
    out = []
    if not offs[6]:
        return out
    length = struct.unpack_from("<H", raw, offs[6])[0]
    code = raw[offs[6] + 2:offs[6] + 2 + length]
    blocks = []  # (end, term)
    pending = None  # an if's tests, while they're read
    last_if = None  # the term of the if block that just closed (for an else)
    i = 0
    while i < len(code):
        while blocks and i >= blocks[-1][0]:
            last_if = blocks.pop()[1]
        n = rdt.scd_inst_len(code, i)
        if n <= 0:
            break
        op = code[i]
        if pending is not None and op not in (0x04, 0x06):
            blocks.append((pending[0], (True, pending[1])))
            pending = None
        if op == 0x01:
            pending = (i + code[i + 1], [])
        elif op == 0x02:
            term = (not last_if[0], last_if[1]) if last_if else (True, [])
            blocks.append((i + code[i + 1], term))
        elif op == 0x04 and pending is not None:
            pending[1].append((code[i + 1], code[i + 2], code[i + 3]))
        elif op == 0x1B and i + 22 <= len(code):
            out.append((bytes(code[i:i + 22]), [b[1] for b in blocks if b[1][1]]))
        if op != 0x02:
            last_if = None if op not in (0x04, 0x06) else last_if
        i += n
    return out


def enemy_blob(enemies):
    """"RENE", u32 count; per enemy its 22 Enemy_set bytes, u8 number of terms, per term u8
    want, u8 number of tests, the tests as u8 bank, bit, value (z_nv_enemy.c)."""
    data = bytearray(b"RENE" + struct.pack("<I", len(enemies)))
    for raw, terms in enemies:
        data += raw + bytes([len(terms)])
        for want, tests in terms:
            data += bytes([1 if want else 0, len(tests)])
            for t in tests:
                data += bytes(t)
    return bytes(data)


def walk_cameras(floor, zones, starts):
    """{floor cell: camera} as a player who walked there from (x, z) would have it: RE1 has no
    camera regions, only the room's opening camera (entry_camera) and the switch zones of the
    camera on (z_nv_camera.c). Breadth-first from each start, as RE1 switches (one zone of the
    current camera's group per step); a cell keeps the camera of the shortest walk to it. Only
    warps use it (z_nv_camera.c), so a dev warp shows Link."""
    groups = {}
    for rec in zones:
        groups.setdefault(struct.unpack_from("<h", rec, 2)[0], []).append(rec)

    def switch(cam, k):
        px, pz = int((k[0] + 0.5) * re1_floor.CELL), int((k[1] + 0.5) * re1_floor.CELL)
        for rec in groups.get(cam, [])[1:]:  # the group's first record is its header
            if in_switch_zone(rec, px, pz):
                return struct.unpack_from("<h", rec, 0)[0]
        return cam

    out, seen, queue = {}, set(), []
    for x, z in starts:
        k = floor.cell(x, z)
        st = (k, entry_camera(zones, x, z))
        if k in floor.cells and st not in seen:
            seen.add(st)
            queue.append(st)
    i = 0
    while i < len(queue):
        k, cam = queue[i]
        i += 1
        out.setdefault(k, cam)
        for n in ((k[0] + 1, k[1]), (k[0] - 1, k[1]), (k[0], k[1] + 1), (k[0], k[1] - 1)):
            if n in floor.cells:
                st = (n, switch(cam, n))
                if st not in seen:
                    seen.add(st)
                    queue.append(st)
    return out


def camera_zone_records(raw, ncam):
    """RE1's switch-zone table (RDT+0x48) as its raw 20-byte records, grouped by camFrom (each
    group's first record is its header, the rest the quads tested while that camera is on). The
    table ends with a record of camTo = camFrom = -1."""
    zp = struct.unpack_from("<I", raw, 0x48)[0]
    out = []
    for p in range(zp, len(raw) - 19, 20):
        cam_from = struct.unpack_from("<h", raw, p + 2)[0]
        if not 0 <= cam_from < ncam:
            break
        out.append(raw[p:p + 20])
    return out


def in_switch_zone(rec, px, pz):
    """RE1's is_entity_in_switch_zone (0x00462d90) on a raw 20-byte record, as z_nv_camera.c."""
    c = struct.unpack_from("<8H", rec, 4)
    x0, z0, x2, z2 = c[0], c[1], c[4], c[5]
    if (c[2] - x0) * (pz - z0) > (c[3] - z0) * (px - x0):
        return False
    if (c[6] - x0) * (pz - z0) < (c[7] - z0) * (px - x0):
        return False
    if (c[2] - x2) * (pz - z2) < (c[3] - z2) * (px - x2):
        return False
    if (c[6] - x2) * (pz - z2) > (c[7] - z2) * (px - x2):
        return False
    return True


def entry_camera(zones, x, z):
    """The camera RE1 opens a room on with the player at (x, z): camera 0, then one pass of
    camera 0's switch zones (room_set: g_roomCameraId = 0, check_camera_switch(1))."""
    group = [rec for rec in zones if struct.unpack_from("<h", rec, 2)[0] == 0]
    for rec in group[1:]:  # the group's first record is its header
        if in_switch_zone(rec, int(x), int(z)):
            return struct.unpack_from("<h", rec, 0)[0]
    return 0


def write_room(room, grid, stage, room_id, name, out, spawns, cell_exit=None, doors=(), bias_tables=None,
               arrivals=(), aots=()):
    """One RE1 room as an SoH scene. spawns: [(x, z, yaw_binang)] in RE1 units. Returns the
    scene's resource path (what the scene table's file name resolves to)."""
    cell_exit = cell_exit or {}
    sc, rm, colname = name + "_scene", name + "_room_0", name + "_col"
    base = "scenes/shared/%s" % sc
    d = os.path.join(out, base)
    os.makedirs(d, exist_ok=True)
    ncam = len(room.cameras)

    # floor and walls from RE1's rules (re1_floor.py)
    floor = re1_floor.Floor(room.raw, int(room_id[1:], 16),
                            list(arrivals) + [(sx, sz, 0) for sx, sz, _ in spawns], aots)
    # each floor cell's surface carries the camera a player walking there would have (only warps
    # use it: z_nv_camera.c opens rooms and switches by RE1's own rules)
    zones = camera_zone_records(room.raw, ncam)
    cell_cam = walk_cameras(floor, zones, [(a[0], a[1]) for a in arrivals] + [(sx, sz) for sx, sz, _ in spawns])

    def cam_at(x, z):
        return cell_cam.get(floor.cell(x, z), entry_camera(zones, x, z))

    cams = sorted({cam_at((k[0] + 0.5) * re1_floor.CELL, (k[1] + 0.5) * re1_floor.CELL) for k in floor.cells} | {0})
    surf_of_cam = {c: i for i, c in enumerate(cams)}
    keys = [(c, 0) for c in cams]
    wall_surface = len(keys)
    verts, polys = floor.mesh(
        lambda k: surf_of_cam[cam_at((k[0] + 0.5) * re1_floor.CELL, (k[1] + 0.5) * re1_floor.CELL)],
        wall_surface, r2s.to_oot)

    class _Col:
        pass

    col = _Col()
    col.verts, col.polys = verts, polys
    xs, ys, zs = ([v[i] for v in col.verts] for i in range(3))

    C = ['<CollisionHeader Version="0" MinBoundsX="%d" MinBoundsY="%d" MinBoundsZ="%d" '
         'MaxBoundsX="%d" MaxBoundsY="%d" MaxBoundsZ="%d">' % (min(xs), min(ys), min(zs), max(xs), max(ys), max(zs))]
    for v in col.verts:
        C.append("    " + _el("Vertex", X=v[0], Y=v[1], Z=v[2]))
    for s, (a, b, c), n, dist in col.polys:
        C.append("    " + _el("Polygon", Type=s, VertexA=a, VertexB=b, VertexC=c, NormalX=_snormal(n[0]),
                              NormalY=_snormal(n[1]), NormalZ=_snormal(n[2]), Dist=dist))
    for cam, ex in keys + [(0, 0)]:
        C.append("    " + _el("PolygonType", Data1=_surface0(cam, ex), Data2=SURFACE1_STONE))
    for c in range(ncam):  # CAM_SET_PREREND_FIXED (= 0x19 in SoH's camera settings), 3 Vec3s each
        C.append("    " + _el("CameraData", SType=CAM_SET_PREREND_FIXED, NumData=3, CameraPosDataSeg=3 * c))
    for c, cam in enumerate(room.cameras):
        pos, rot, fov = r2s.camera_data(cam)
        C.append("    " + _el("CameraPositionData", PosX=pos[0], PosY=pos[1], PosZ=pos[2], RotX=rot[0], RotY=rot[1],
                              RotZ=rot[2], FOV=fov, JfifID=c, Unknown=-1))
    C.append("</CollisionHeader>")
    open(os.path.join(d, colname), "w").write("\n".join(C) + "\n")

    # scene: sound, room list, camera mode, collision, spawns, special files, player entries,
    # skybox, lighting (rdt2scene.SCENE_CMDS)
    S = ['<Room Version="0">',
         "    " + _el("SetSoundSettings", Reverb=0, NatureAmbienceId=0x13, SeqId=0x7F),
         '    <SetRoomList>', "        " + _el("RoomEntry", Path="%s/%s" % (base, rm), VromStart=0, VromEnd=0),
         '    </SetRoomList>',
         "    " + _el("SetCameraSettings", CameraMovement=1, WorldMapArea=0),  # SCENE_CAM_TYPE_FIXED
         "    " + _el("SetCollisionHeader", FileName="%s/%s" % (base, colname)),
         '    <SetEntranceList>']
    for i in range(len(spawns)):
        S.append("        " + _el("EntranceEntry", Spawn=i, Room=0))
    S.append('    </SetEntranceList>')
    S.append("    " + _el("SetSpecialObjects", ElfMessage=0, GlobalObject=0))
    S.append('    <SetStartPositionList>')
    for sx, sz, syaw in spawns:
        p = r2s.to_oot((sx, floor.height(sx, sz), sz))
        cam = cam_at(sx, sz)
        # ACTOR_PLAYER, PLAYER_PARAMS(PLAYER_START_MODE_IDLE, cam): start on the camera owning the cell
        S.append("        " + _el("StartPositionEntry", Id=0, PosX=round(p[0]), PosY=round(p[1]), PosZ=round(p[2]),
                                  RotX=0, RotY=syaw, RotZ=0, Params=(PLAYER_START_MODE_IDLE << 8) | (cam & 0xFF)))
    S.append('    </SetStartPositionList>')
    S.append("    " + _el("SetSkyboxSettings", Unknown=0, SkyboxId=0, Weather=0, Indoors=1))
    S.append('    <SetLightingSettings>')
    S.append("        " + _el("LightingSetting", AmbientColorR=60, AmbientColorG=55, AmbientColorB=50,
                              Light1DirX=73, Light1DirY=73, Light1DirZ=73, Light1ColorR=180, Light1ColorG=160,
                              Light1ColorB=130, Light2DirX=-73, Light2DirY=-73, Light2DirZ=-73, Light2ColorR=50,
                              Light2ColorG=45, Light2ColorB=40, FogColorR=10, FogColorG=10, FogColorB=10,
                              FogNear=(1 << 10) | 994, FogFar=3200))
    S.append('    </SetLightingSettings>')
    S.append("    <EndMarker/>")
    S.append("</Room>")
    open(os.path.join(d, sc), "w").write("\n".join(S) + "\n")

    # room: the backgrounds (room shape IMAGE, MULTI: one per camera, picked by bgCamIndex)
    R = ['<Room Version="0">',
         "    " + _el("SetRoomBehavior", GameplayFlags1=0, GameplayFlags2=0),
         '    <SetMesh Data="0" MeshHeaderType="1">',
         '        <Polygon PolyType="1" Format="2" BgImageCount="%d" MeshOpa="" MeshXlu="">' % ncam]
    for c in range(ncam):
        # RE1's pixels as they are (lossless; background_jpeg is the old lossy path)
        px = background_raw(stage, room_id, c)
        bgname = "%s_bg%d" % (name, c)
        open(os.path.join(d, bgname), "wb").write(_otr_header(SOH_BACKGROUND) + struct.pack("<I", len(px)) + px)
        # its mask: <bg>_mask (ReMask_Draw finds it by the background's path)
        bias = re1_masks.fade_bias(bias_tables, int(room_id[0]), int(room_id[1:], 16), c)
        blob = mask_blob(re1_masks.camera_masks(room.raw, c, bias))
        open(os.path.join(d, bgname + "_mask"), "wb").write(_otr_header(LUS_BLOB) + struct.pack("<I", len(blob)) + blob)
        R.append("            " + _el("BgImage", Unknown_00=130, Id=c, ImagePath="%s/%s" % (base, bgname),
                                      Unknown_0C=0, TLUT=0, Width=320, Height=240, Fmt=0, Siz=2, Mode0=0,
                                      TLUTCount=0))
    # RE1's camera switch zones, as they are: "<name>_camzones" (z_nv_camera.c)
    zrecs = camera_zone_records(room.raw, ncam)
    blob = b"RCZN" + struct.pack("<I", len(zrecs)) + b"".join(zrecs)
    open(os.path.join(d, name + "_camzones"), "wb").write(_otr_header(LUS_BLOB) + struct.pack("<I", len(blob)) + blob)

    # stairs: "<name>_climbs" (z_nv_climb.c), RE1's Aot_set zones of type 5 and 6 with the move
    # player_door_open_sequence makes (shallower in rooms 0x05 and 0x0E)
    along, up = re1_floor.CLIMB_SHALLOW if int(room_id[1:], 16) in (0x05, 0x0E) else re1_floor.CLIMB
    # (a staircase that became a ramp is walked, not climbed: no A there)
    climbs = [a for a in aots if a[0] in (5, 6) and tuple(a[:5]) not in floor.ramp_zones]
    blob = b"RCLB" + struct.pack("<I", len(climbs)) + b"".join(
        struct.pack("<3Bx4H2h", t, sat, d0 & 0xFF, x, z, w, dd, along, up) for t, x, z, w, dd, d0, sat in climbs)
    open(os.path.join(d, name + "_climbs"), "wb").write(_otr_header(LUS_BLOB) + struct.pack("<I", len(blob)) + blob)

    # enemies: "<name>_enemies" (z_nv_enemy.c)
    enemies = room_enemies(room.raw)
    ENEMY_TYPES.update(e[0][1] for e in enemies)
    blob = enemy_blob(enemies)
    open(os.path.join(d, name + "_enemies"), "wb").write(_otr_header(LUS_BLOB) + struct.pack("<I", len(blob)) + blob)

    # doors: "<name>_doors" (z_nv_door.c finds it by the scene's name)
    blob = door_blob(doors)
    open(os.path.join(d, name + "_doors"), "wb").write(_otr_header(LUS_BLOB) + struct.pack("<I", len(blob)) + blob)
    R.append('        </Polygon>')
    R.append('    </SetMesh>')
    R.append("    <EndMarker/>")
    R.append("</Room>")
    open(os.path.join(d, rm), "w").write("\n".join(R) + "\n")
    return "%s/%s" % (base, sc)


CAM_SET_PREREND_FIXED = 0x19  # z64camera.h
PLAYER_START_MODE_IDLE = 13  # z64player.h


def pack_o2r(folder, archive):
    """Zip a resource folder into an .o2r mod archive (a zip of resource paths)."""
    with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
        for root, _dirs, files in os.walk(folder):
            for f in sorted(files):
                p = os.path.join(root, f)
                z.write(p, os.path.relpath(p, folder).replace(os.sep, "/"))
