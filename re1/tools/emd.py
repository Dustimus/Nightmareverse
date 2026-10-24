"""Parser for Resident Evil 1 (PC) EMD model files and the PSX TIM textures inside them.

Layout, as found in the RE1 PC enemy/character files (ENEMY/*.EMD):

    The section directory is the last 5 u32s of the file:
        [0] anim list for the "extra" skeleton at offset 0 (junk if [1] == 0)
        [1] main skeleton (0 => the main skeleton is the one at offset 0)
        [2] anim list for the main skeleton
        [3] mesh
        [4] TIM texture

    Skeleton section:
        u16 armature_offset, u16 frames_offset, u16 bone_count, u16 frame_size
        s16 relpos[bone_count][3]             bone offset from parent, PSX units (~0.6 mm; humans are ~3000 tall)
        armature at armature_offset:
            {u16 child_count, u16 child_list_offset}[bone_count]
            u8 children[...]                  offsets are relative to armature_offset
        frames at frames_offset, frame_size bytes each:
            s16 root_offset[3], s16 root_speed[3], s16 angles[bone_count][3]
            angles: 4096 = 360 degrees

    Anim list section:
        {u16 step_count, u16 steps_offset}[anim_count]    (anim_count = first offset / 4)
        u32 steps: low 16 bits = frame index into the skeleton's frames, high 16 = flags

    Mesh section:
        u32 length, u32 unknown, u32 object_count, then object_count entries of
        u32 vtx_off, vtx_count, nor_off, nor_count, tri_off, tri_count, unused
        (offsets relative to the end of the 12-byte header). Object i belongs to bone i.
        vertex/normal: s16 x, y, z, pad
        triangle (28 bytes):
            u32 id; u8 u0,v0; u16 clut; u8 u1,v1; u16 page; u8 u2,v2; u16 pad;
            u16 n0, v0, n1, v1, n2, v2

    PSX coordinates: Y points down.
"""

import math
import struct
from dataclasses import dataclass, field


@dataclass
class Triangle:
    verts: tuple  # 3 vertex indices
    norms: tuple  # 3 normal indices
    uvs: tuple  # 3 (u, v) pairs, in texture pixels (page offset applied)
    clut: int  # CLUT row index


@dataclass
class MeshObject:
    vertices: list
    normals: list
    triangles: list


@dataclass
class Frame:
    root_offset: tuple
    root_speed: tuple
    angles: list  # per bone (x, y, z), 4096 = 360 degrees


@dataclass
class Skeleton:
    relpos: list
    children: list
    frames: list
    parents: list = field(default_factory=list)


@dataclass
class Animation:
    steps: list  # (frame_index, flags)


@dataclass
class Tim:
    width: int
    height: int
    bpp: int
    pixels: bytes
    cluts: list  # list of palettes, each a list of u16 PSX colors
    x: int = 0  # where the image and the palettes go in VRAM (halfwords, lines)
    y: int = 0
    clut_y: int = 0


@dataclass
class Emd:
    skeleton: Skeleton
    animations: list
    extra_skeleton: Skeleton | None
    extra_animations: list
    objects: list
    tim: Tim


def _u16(d, o):
    return struct.unpack_from("<H", d, o)[0]


def parse_skeleton(d, base, end):
    arm_off, frames_off, count, frame_size = struct.unpack_from("<4H", d, base)
    relpos = [struct.unpack_from("<3h", d, base + 8 + 6 * i) for i in range(count)]
    children = []
    for i in range(count):
        n, off = struct.unpack_from("<2H", d, base + arm_off + 4 * i)
        children.append(list(d[base + arm_off + off : base + arm_off + off + n]))
    parents = [-1] * count
    for p, kids in enumerate(children):
        for c in kids:
            parents[c] = p
    frames = []
    nframes = (end - base - frames_off) // frame_size if frame_size else 0
    for f in range(nframes):
        o = base + frames_off + f * frame_size
        vals = struct.unpack_from("<%dh" % (6 + 3 * count), d, o)
        angles = [vals[6 + 3 * i : 9 + 3 * i] for i in range(count)]
        frames.append(Frame(vals[0:3], vals[3:6], angles))
    return Skeleton(relpos, children, frames, parents)


def parse_anim_list(d, base):
    first = _u16(d, base + 2)
    anims = []
    for a in range(first // 4):
        n, off = struct.unpack_from("<2H", d, base + 4 * a)
        steps = struct.unpack_from("<%dI" % n, d, base + off)
        anims.append(Animation([(s & 0xFFFF, s >> 16) for s in steps]))
    return anims


def parse_mesh(d, base):
    _length, _unk, count = struct.unpack_from("<3I", d, base)
    hdr = base + 12
    objects = []
    for i in range(count):
        vo, vc, no, nc, to, tc, _ = struct.unpack_from("<7I", d, hdr + 28 * i)
        verts = [struct.unpack_from("<3h", d, hdr + vo + 8 * k) for k in range(vc)]
        norms = [struct.unpack_from("<3h", d, hdr + no + 8 * k) for k in range(nc)]
        tris = []
        for k in range(tc):
            q = hdr + to + 28 * k
            u0, v0, clut, u1, v1, page, u2, v2, _pad = struct.unpack_from("<BBHBBHBBH", d, q + 4)
            n0, i0, n1, i1, n2, i2 = struct.unpack_from("<6H", d, q + 16)
            px = (page & 0xF) * 128  # 8bpp: one PSX texture page = 64 halfwords = 128 px
            uvs = ((u0 + px, v0), (u1 + px, v1), (u2 + px, v2))
            tris.append(Triangle((i0, i1, i2), (n0, n1, n2), uvs, (clut >> 6) - 480))
        objects.append(MeshObject(verts, norms, tris))
    return objects


def parse_tim(d, base):
    _magic, flags = struct.unpack_from("<2I", d, base)
    p = base + 8
    pmode = flags & 3
    bpp = (4, 8, 16, 24)[pmode]
    cluts = []
    if flags & 8:
        clen, _cx, clut_y, cw, ch = struct.unpack_from("<I4H", d, p)
        for row in range(ch):
            cluts.append(list(struct.unpack_from("<%dH" % cw, d, p + 12 + row * cw * 2)))
        p += clen
    _ilen, ix, iy, iw, ih = struct.unpack_from("<I4H", d, p)
    width = iw * 16 // bpp
    pixels = d[p + 12 : p + 12 + iw * 2 * ih]
    return Tim(width, ih, bpp, pixels, cluts, ix, iy, clut_y if cluts else 0)


def load(path):
    with open(path, "rb") as f:
        d = f.read()
    a0, s1, a1, mesh_off, tim_off = struct.unpack_from("<5I", d, len(d) - 20)
    main_off = s1 if s1 else 0
    main_end = a1
    skeleton = parse_skeleton(d, main_off, main_end)
    animations = parse_anim_list(d, a1)
    extra_skel, extra_anims = None, []
    if s1:
        extra_skel = parse_skeleton(d, 0, a0)
        extra_anims = parse_anim_list(d, a0)
    objects = parse_mesh(d, mesh_off)
    tim = parse_tim(d, tim_off)
    return Emd(skeleton, animations, extra_skel, extra_anims, objects, tim)


def psx_color(c):
    """PSX 15-bit BGR color -> (r, g, b, a). 0x0000 is the transparent color."""
    if c == 0:
        return (0, 0, 0, 0)
    r = (c & 31) * 255 // 31
    g = ((c >> 5) & 31) * 255 // 31
    b = ((c >> 10) & 31) * 255 // 31
    return (r, g, b, 255)


def tim_to_rgba(tim, page_cluts=None):
    """Decode a TIM to (width, height, rgba bytes).

    RE1 enemy textures are 8bpp, 256 px wide, split into two 128 px texture pages, each
    drawn with its own CLUT. page_cluts maps page index -> CLUT row (default: page i uses
    CLUT i, clamped to the CLUTs present).
    """
    w, h = tim.width, tim.height
    out = bytearray(w * h * 4)
    if tim.bpp == 16:
        for i in range(w * h):
            out[i * 4 : i * 4 + 4] = bytes(psx_color(_u16(tim.pixels, i * 2)))
        return w, h, bytes(out)
    page_w = 128 if tim.bpp == 8 else 256
    pals = [[psx_color(c) for c in clut] for clut in tim.cluts]
    for y in range(h):
        for x in range(w):
            page = x // page_w
            ci = (page_cluts or {}).get(page, min(page, len(pals) - 1))
            if tim.bpp == 8:
                idx = tim.pixels[y * w + x]
            else:
                b = tim.pixels[(y * w + x) // 2]
                idx = (b >> 4) if x & 1 else (b & 15)
            o = (y * w + x) * 4
            out[o : o + 4] = bytes(pals[ci][idx])
    return w, h, bytes(out)


def page_cluts(emd):
    """Which CLUT each texture page is drawn with, from the triangles that use it."""
    pc = {}
    for obj in emd.objects:
        for t in obj.triangles:
            pc.setdefault(t.uvs[0][0] // 128, t.clut)
    return pc


# --- pose math --------------------------------------------------------------

ANGLE_SCALE = 2 * math.pi / 4096


def rot_matrix(ax, ay, az):
    """PSX GTE RotMatrix: rotate about X, then Y, then Z (M = Rz * Ry * Rx)."""
    a, b, c = ax * ANGLE_SCALE, ay * ANGLE_SCALE, az * ANGLE_SCALE
    sx, cx = math.sin(a), math.cos(a)
    sy, cy = math.sin(b), math.cos(b)
    sz, cz = math.sin(c), math.cos(c)
    rx = [[1, 0, 0], [0, cx, -sx], [0, sx, cx]]
    ry = [[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]]
    rz = [[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]]
    return matmul(rz, matmul(ry, rx))


def matmul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]


def matvec(m, v):
    return tuple(sum(m[i][k] * v[k] for k in range(3)) for i in range(3))


def pose(skeleton, frame=None):
    """World (rotation, translation) per bone for a frame (None = rest pose)."""
    n = len(skeleton.relpos)
    world = [None] * n
    order = []
    stack = [i for i in range(n) if skeleton.parents[i] == -1]
    while stack:
        i = stack.pop()
        order.append(i)
        stack.extend(skeleton.children[i])
    ident = [[1, 0, 0], [0, 1, 0], [0, 0, 1]]
    for i in order:
        local_r = rot_matrix(*frame.angles[i]) if frame else ident
        p = skeleton.parents[i]
        if p == -1:
            t = tuple(frame.root_offset) if frame else tuple(skeleton.relpos[i])
            world[i] = (local_r, t)
        else:
            pr, pt = world[p]
            off = matvec(pr, skeleton.relpos[i])
            world[i] = (matmul(pr, local_r), tuple(pt[k] + off[k] for k in range(3)))
    return world
