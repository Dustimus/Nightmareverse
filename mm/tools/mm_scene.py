"""MM scenes as SoH scenes: headers in SoH's XML formats, models through Torch.

The scene and room headers are read from the player's MM ROM and written as SoH XML
(soh/soh/resource/importer/scenecommand/*Factory.cpp, the XML readers), keeping what OoT's
engine already understands and leaving out MM's own pieces until something uses them:
- actors and objects (MM's ids), doors, exits, paths, cutscenes;
- the minimap, actor cutscenes and texture animation.

The meshes and collision come from Torch (mm2torch.py declares them), under mm/scenes/.

Layout (SoH loads scenes from scenes/shared/<file>/<file>, z_play_otr.cpp):
    scenes/shared/mm_<scene>_scene/mm_<scene>_scene       scene header (XML)
    scenes/shared/mm_<scene>_scene/mm_<scene>_room_NN     room header (XML)
    mm/scenes/<SCENE>_scene/...                           display lists, vertices, textures,
                                                          collision (Torch)
"""

import os
import struct
from xml.sax.saxutils import quoteattr

import yaz0
from mm2torch import commands, room_headers
from mm_entrances import Layout, soh_name
import mm_actors

SKIPPED = {}  # MM actors left out of the rooms so far: {id: count} (make_mod.py reports it)


def _el(tag, **attrs):
    return "<%s %s/>" % (tag, " ".join("%s=%s" % (k, quoteattr(str(v))) for k, v in attrs.items()))


def mm_rot(r):
    """MM actor entries keep spawn flags in a rotation's low 7 bits; the angle is in degrees
    above them (the decomp's ActorEntry)."""
    return (((r & 0xFFFF) >> 7) & 0x1FF) * 0x10000 // 360 & 0xFFFF


def _s16(v):
    return v - 0x10000 if v & 0x8000 else v


def collision_offset(sdata):
    for cmd, _, w1 in commands(sdata, 0):
        if cmd == 0x03:
            return w1 & 0xFFFFFF
    raise ValueError("scene has no collision")


def exits(scene, sdata, lay, spawns):
    """The scene's exit list (0x13): MM's entrance numbers as ours. An exit into a place that
    isn't built yet goes "back": to this scene's start position nearest that exit's polygons
    (the spot that place's own exit would bring Link to), shown with a note (z_nv_mm.c)."""
    cmds = {cmd: (a1, w1) for cmd, a1, w1 in commands(sdata, 0)}
    if 0x13 not in cmds:
        return []
    col = collision_offset(sdata)
    nverts = struct.unpack_from(">H", sdata, col + 0x0C)[0]
    vtx = struct.unpack_from(">I", sdata, col + 0x10)[0] & 0xFFFFFF
    npolys = struct.unpack_from(">H", sdata, col + 0x14)[0]
    polys = struct.unpack_from(">I", sdata, col + 0x18)[0] & 0xFFFFFF
    stypes = struct.unpack_from(">I", sdata, col + 0x1C)[0] & 0xFFFFFF
    verts = [struct.unpack_from(">3h", sdata, vtx + 6 * i) for i in range(nverts)]
    where = {}  # exit index (1-based) -> the vertices of its polygons
    for i in range(npolys):
        t, a, b, c = struct.unpack_from(">4H", sdata, polys + 16 * i)
        ex = (struct.unpack_from(">I", sdata, stypes + 8 * t)[0] >> 8) & 0x1F
        if ex:
            where.setdefault(ex, []).extend(verts[k & 0x1FFF] for k in (a, b, c))
    count = max(where) if where else 0
    base = cmds[0x13][1] & 0xFFFFFF
    out = []
    for i in range(count):
        v = struct.unpack_from(">H", sdata, base + 2 * i)[0]
        e = lay.mm_entrance(v)
        if e is None:
            pts = where.get(i + 1)
            spawn = 0
            if pts:
                cx = sum(p[0] for p in pts) / len(pts)
                cz = sum(p[2] for p in pts) / len(pts)
                spawn = min(range(len(spawns)),
                            key=lambda k: (spawns[k][1] - cx) ** 2 + (spawns[k][3] - cz) ** 2)
            e = lay.back[(scene, spawn)]
        out.append(e)
    return out


def scene_xml(scene, sdata, rooms, lay):
    """The scene header, from MM's main header."""
    base = soh_name(scene) + "_scene"
    cmds = {cmd: (a1, w1) for cmd, a1, w1 in commands(sdata, 0)}
    S = ['<Room Version="0">']

    S.append("    " + _el("SetSoundSettings", Reverb=0, NatureAmbienceId=0x13, SeqId=0x7F))  # music: later
    S.append("    <SetRoomList>")
    for r in range(rooms):
        S.append("        " + _el("RoomEntry", Path=f"scenes/shared/{base}/{soh_name(scene)}_room_{r:02d}",
                                   VromStart=0, VromEnd=0))
    S.append("    </SetRoomList>")
    S.append("    " + _el("SetCameraSettings", CameraMovement=0, WorldMapArea=0))
    col = collision_offset(sdata)
    S.append("    " + _el("SetCollisionHeader", FileName=f"mm/scenes/{scene}_scene/{scene}_sceneCollisionHeader_{col:06X}"))

    # spawns (0x00) and the entrance list (0x06) that picks among them
    n, w1 = cmds[0x00]
    spawns = [struct.unpack_from(">8h", sdata, (w1 & 0xFFFFFF) + 16 * i) for i in range(n)]
    entrances = []
    if 0x06 in cmds:
        p = cmds[0x06][1] & 0xFFFFFF
        while len(entrances) < 32 and p + 2 <= len(sdata):
            spawn, room = sdata[p], sdata[p + 1]
            if spawn >= n or room >= rooms:
                break
            entrances.append((spawn, room))
            p += 2
    S.append("    <SetEntranceList>")
    for spawn, room in entrances or [(0, 0)]:
        S.append("        " + _el("EntranceEntry", Spawn=spawn, Room=room))
    S.append("    </SetEntranceList>")
    S.append("    " + _el("SetSpecialObjects", ElfMessage=0, GlobalObject=0))
    if 0x0D in cmds:
        xml, n, info = paths_xml(sdata, cmds[0x0D][1] & 0xFFFFFF)
        if n:
            PATHS[base] = xml
            PATH_INFO[base] = info
            S.append("    <SetPathways>")
            S.append("        " + _el("Pathway", FilePath=f"scenes/shared/{base}/{base}_paths"))
            S.append("    </SetPathways>")
    if 0x0E in cmds:
        n, w1 = cmds[0x0E]
        ds, skipped = mm_actors.doors(sdata, w1 & 0xFFFFFF, n)
        for k, v in skipped.items():
            SKIPPED[k] = SKIPPED.get(k, 0) + v
        if ds:
            S.append("    <SetTransitionActorList>")
            for d in ds:
                S.append("        " + _el("TransitionActorEntry", **d))
            S.append("    </SetTransitionActorList>")
    ex = exits(scene, sdata, lay, spawns)
    if ex:
        S.append("    <SetExitList>")
        for e in ex:
            S.append("        " + _el("ExitEntry", Id=e))
        S.append("    </SetExitList>")
    S.append("    <SetStartPositionList>")
    for _id, x, y, z, rx, ry, rz, params in spawns:
        # OoT's player (actor 0); MM's params mean the same start modes in the top nibble
        S.append("        " + _el("StartPositionEntry", Id=0, PosX=x, PosY=y, PosZ=z, RotX=0, RotY=mm_rot(ry),
                                   RotZ=0, Params=(13 << 8) | 0xFF))  # PLAYER_START_MODE_IDLE, no bg camera
    S.append("    </SetStartPositionList>")

    if 0x11 in cmds:
        w1 = cmds[0x11][1]
        S.append("    " + _el("SetSkyboxSettings", Unknown=0, SkyboxId=(w1 >> 24) & 0xFF, Weather=0,
                               Indoors=(w1 >> 8) & 0xFF))

    # lighting (0x0F): MM's EnvLightSettings, the same 22 bytes as OoT's
    if 0x0F in cmds:
        n, w1 = cmds[0x0F]
        S.append("    <SetLightingSettings>")
        for i in range(n):
            b = sdata[(w1 & 0xFFFFFF) + 22 * i:(w1 & 0xFFFFFF) + 22 * (i + 1)]
            sb = [_s16(x | 0xFF00) if x & 0x80 else x for x in b]  # signed bytes for directions
            near, far = struct.unpack_from(">hh", b, 18)
            S.append("        " + _el(
                "LightingSetting", AmbientColorR=b[0], AmbientColorG=b[1], AmbientColorB=b[2],
                Light1DirX=sb[3], Light1DirY=sb[4], Light1DirZ=sb[5],
                Light1ColorR=b[6], Light1ColorG=b[7], Light1ColorB=b[8],
                Light2DirX=sb[9], Light2DirY=sb[10], Light2DirZ=sb[11],
                Light2ColorR=b[12], Light2ColorG=b[13], Light2ColorB=b[14],
                FogColorR=b[15], FogColorG=b[16], FogColorB=b[17], FogNear=near & 0xFFFF, FogFar=far))
        S.append("    </SetLightingSettings>")
    S.append("    <EndMarker/>")
    S.append("</Room>")
    return "\n".join(S) + "\n"


PATHS = {}  # scene file -> its paths resource (XML), written beside the scene by write_scene
PATH_INFO = {}  # scene file -> [(additional path index, custom value)] per path (MM's, OoT's Path has neither)


def paths_xml(sdata, off):
    """MM's path list (0x0D): entries of count, an extra index, a custom value and the points'
    address, until one that isn't a path. As SoH's XML Path resource (PathFactory.cpp)."""
    out = ['<Path Version="0">']
    info = []
    p = off
    while p + 8 <= len(sdata):
        count, extra, custom, ptr = struct.unpack_from(">BBhI", sdata, p)
        if count == 0 or (ptr >> 24) != 2 or (ptr & 0xFFFFFF) + 6 * count > len(sdata):
            break
        info.append((extra, custom))
        out.append("    <PathData>")
        for k in range(count):
            x, y, z = struct.unpack_from(">3h", sdata, (ptr & 0xFFFFFF) + 6 * k)
            out.append("        " + _el("PathPoint", X=x, Y=y, Z=z))
        out.append("    </PathData>")
        p += 8
    out.append("</Path>")
    return "\n".join(out) + "\n", (p - off) // 8, info


HALFDAYS = {}  # room file -> its entries' half-day masks (write_scene writes them beside the room)

LUS_BLOB = 0x4F424C42  # "OBLB", libultraship ResourceType.h


def _otr_header(res_type):
    """libultraship's 64-byte binary resource header (as re1/tools/soh_scene.py writes it)."""
    return struct.pack("<BB2xIIQ", 0, 1, res_type, 0, 0).ljust(64, b"\0")


def room_xml(scene, r, rdata):
    """A room header: its behaviour, time, skybox, mesh, and the actors OoT has a version of
    (mm_actors.py; MM's own actors are left out until ported)."""
    name = f"{scene}_room_{r:02d}"
    cmds = {cmd: (a1, w1) for cmd, a1, w1 in commands(rdata, 0)}
    R = ['<Room Version="0">']
    R.append("    " + _el("SetRoomBehavior", GameplayFlags1=0, GameplayFlags2=0))
    if 0x16 in cmds:
        R.append("    " + _el("SetEchoSettings", Echo=cmds[0x16][1] & 0xFF))
    if 0x10 in cmds:
        w1 = cmds[0x10][1]
        # no increment: Termina's clock is ours (z_nv_mm_clock.c), and OoT's must stand still here
        R.append("    " + _el("SetTimeSettings", Hour=0xFF, Minute=0xFF, TimeIncrement=0))
    if 0x12 in cmds:
        w1 = cmds[0x12][1]
        R.append("    " + _el("SetSkyboxModifier", SkyboxDisabled=(w1 >> 24) & 0xFF,
                               SunMoonDisabled=(w1 >> 16) & 0xFF))

    if 0x01 in cmds:
        n, w1 = cmds[0x01]
        acts, objs, skipped = mm_actors.actors(rdata, w1 & 0xFFFFFF, n)
        for k, v in skipped.items():
            SKIPPED[k] = SKIPPED.get(k, 0) + v
        if objs:
            R.append("    <SetObjectList>")
            for o in objs:
                R.append("        " + _el("ObjectEntry", Id=o))
            R.append("    </SetObjectList>")
        HALFDAYS[f"{soh_name(scene)}_room_{r:02d}"] = [hd for *_x, hd in acts]
        if acts:
            R.append("    <SetActorList>")
            for aid, p, x, y, z, rx, ry, rz, _hd in acts:
                R.append("        " + _el("ActorEntry", Id=aid, PosX=x, PosY=y, PosZ=z, RotX=rx, RotY=ry, RotZ=rz,
                                           Params=p))
            R.append("    </SetActorList>")

    off = cmds[0x0A][1] & 0xFFFFFF
    kind, count = rdata[off], rdata[off + 1]
    start = struct.unpack_from(">I", rdata, off + 4)[0] & 0xFFFFFF

    def path(dl):
        return f"mm/scenes/{scene}_scene/{name}DL_{dl & 0xFFFFFF:06X}" if dl else ""

    if kind == 0:
        R.append(f'    <SetMesh Data="0" MeshHeaderType="0" PolyNum="{count}">')
        for i in range(count):
            opa, xlu = struct.unpack_from(">II", rdata, start + 8 * i)
            R.append("        " + _el("Polygon", PolyType=0, MeshOpa=path(opa), MeshXlu=path(xlu)))
    elif kind == 2:
        R.append(f'    <SetMesh Data="0" MeshHeaderType="2" PolyNum="{count}">')
        for i in range(count):
            x, y, z, radius, opa, xlu = struct.unpack_from(">4hII", rdata, start + 16 * i)
            R.append("        " + _el("Polygon", PolyType=2, PosX=x, PosY=y, PosZ=z, Unknown=radius,
                                       MeshOpa=path(opa), MeshXlu=path(xlu)))
    else:
        raise ValueError(f"{name}: room mesh type {kind} not handled yet")
    R.append("    </SetMesh>")
    R.append("    <EndMarker/>")
    R.append("</Room>")
    return "\n".join(R) + "\n"


def write_scene(rom, scene, rooms, out):
    """The scene's XML headers into <out>/scenes/shared/..."""
    def load(name):
        _, _, ps, pe = rom.entry(name)
        return yaz0.decompress(rom.data[ps:pe])

    d = os.path.join(out, "scenes", "shared", soh_name(scene) + "_scene")
    os.makedirs(d, exist_ok=True)
    sdata = load(scene)
    open(os.path.join(d, soh_name(scene) + "_scene"), "w").write(scene_xml(scene, sdata, rooms, Layout(rom)))
    base = soh_name(scene) + "_scene"
    if base in PATHS:
        open(os.path.join(d, base + "_paths"), "w").write(PATHS[base])
        # "MPTH", a count, then per path MM's additional path index (u8), a pad byte and its custom
        # value (s16): what MM's people chain their routes by (z_nv_mm_subs.c)
        info = PATH_INFO[base]
        blob = b"MPTH" + struct.pack("<i", len(info)) + b"".join(struct.pack("<BBh", e, 0, c) for e, c in info)
        open(os.path.join(d, base + "_pathinfo"), "wb").write(_otr_header(LUS_BLOB) + struct.pack("<I", len(blob)) + blob)
    for r in range(rooms):
        rdata = load(f"{scene}_room_{r:02d}")
        room = f"{soh_name(scene)}_room_{r:02d}"
        open(os.path.join(d, room), "w").write(room_xml(scene, r, rdata))
        masks = HALFDAYS.get(room, [])
        if any(m not in (0, mm_actors.HALFDAYBIT_ALL) for m in masks):
            # "MHDY", a count, one u16 per actor entry (z_nv_mm_halfday.c)
            blob = b"MHDY" + struct.pack("<i", len(masks)) + struct.pack(f"<{len(masks)}H", *masks)
            open(os.path.join(d, room + "_halfdays"), "wb").write(
                _otr_header(LUS_BLOB) + struct.pack("<I", len(blob)) + blob)
