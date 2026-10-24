"""Parser and player for Resident Evil 1 (PC) door scenes (ITEM_M1/*.DOR): the short 3D
scene RE1 plays between rooms (the door swinging open as the camera moves through), which
also covers the next room's loading.

Which file a door plays is Door_set's animation index (rdt.py; RE1's table: 0-14
door00-door14, 15 mon, 16-21 elevators, 22-25 kai01/03/02/04 (stairs), 26-27 ladders,
28-32 door00k/01k/03k/05k/06k, 33 door15).

Layout (worked out from DOOR03.DOR; not documented anywhere we found):
    u32 script_off (0x0C), u32 mesh_off, u32 tim_off
    script: u32 routine offsets (relative to script_off, 0 ends the list), then the routines
    mesh: EMD mesh section (emd.parse_mesh): the door leaf and its hardware (knob roses,
          spindles, levers, handles), one object per model
    TIM: 8 bpp, one 256-colour CLUT, at VRAM x 320 (texture page 5)

The script is a tiny multitasking bytecode, u8 opcode then fixed-size operands (sizes in
OPS). Routine 0 runs first and starts the others on numbered threads by door type. Each
frame every live thread runs until it yields. Meanings as far as we could tell (the ones
we use are borne out by the motion they produce):
    00        end of thread                01  exit thread
    03        yield one frame              04 n k 0 v16  if var[k] == v run the next n bytes, else skip them
    06 0 n16  loop n times (to 07)         07  loop end (yields a frame)
    08 0 t r  start routine r on thread t
    0e/0f ..  sounds (0e when the door starts opening, 0f when it shuts; ids not decoded)
    10 s m p f c0   slot s draws model m, parented to slot p (0xFF none), flags f
    11 eye[3] target[3] (s16)  camera   12 velocity[6]  13  camera += velocity
    14 0 i d  camera velocity[i] += d (s8)
    15 s 0 x y z  slot position        16 s 0 x y z  position velocity
    17 s      position += velocity     18 s i d  position velocity[i] += d
    19 s 0 x y z  slot rotation (4096 = 360 degrees)
    1b s      rotation += rotation velocity           1c s i d  rotation velocity[i] += d
    21 s f c0 slot flags (draw order against the door, we think)
    20        where the scene can end once the next room has loaded (we think: the single
              door reaches it right after shutting, then loops on for 10 s; we end there)
    02 (6 bytes), 24, 25 (2), 1f (4): not understood, skipped
Variables: var 0 = Door_set byte 13 (door type: 7 the foyer/dining double door, 4 the
single dining/corridor door), var 3 = Door_set byte 10 (which handle, per side).
PSX coordinates: y points down.
"""

import struct
from dataclasses import dataclass, field

import emd

OPS = {0x00: 2, 0x01: 2, 0x02: 6, 0x03: 2, 0x04: 6, 0x06: 4, 0x07: 2, 0x08: 4, 0x0A: 2, 0x0E: 6,
       0x0F: 6, 0x10: 6, 0x11: 14, 0x12: 14, 0x13: 2, 0x14: 4, 0x15: 8, 0x16: 8, 0x17: 2, 0x18: 4,
       0x19: 8, 0x1B: 2, 0x1C: 4, 0x1F: 4, 0x20: 2, 0x21: 4, 0x24: 2, 0x25: 2}
SOUND_OPEN, SOUND_SHUT = 0x0E, 0x0F


@dataclass
class Slot:
    model: int
    parent: int
    flags: int
    pos: list = field(default_factory=lambda: [0, 0, 0])
    rot: list = field(default_factory=lambda: [0, 0, 0])
    pos_vel: list = field(default_factory=lambda: [0, 0, 0])
    rot_vel: list = field(default_factory=lambda: [0, 0, 0])


@dataclass
class FrameState:
    eye: tuple
    target: tuple
    slots: dict  # slot -> (model, parent, flags, pos, rot)
    sounds: list  # opcodes of sounds started this frame


@dataclass
class Dor:
    routines: list  # bytes per routine
    objects: list  # emd MeshObjects
    tim: object


def load(path):
    d = open(path, "rb").read()
    script_off, mesh_off, tim_off = struct.unpack_from("<3I", d, 0)
    offs = []
    o = script_off
    while True:
        v = struct.unpack_from("<I", d, o)[0]
        if v == 0 and offs:
            break
        offs.append(v)
        o += 4
    routines = []
    for i, v in enumerate(offs):
        end = offs[i + 1] if i + 1 < len(offs) else mesh_off - script_off
        routines.append(d[script_off + v : script_off + end])
    objects = emd.parse_mesh(d, mesh_off)
    for obj in objects:  # texture page 5 (VRAM x 320) is the TIM's left edge
        for t in obj.triangles:
            t.uvs = tuple((u - 640, v) for u, v in t.uvs)
    return Dor(routines, objects, emd.parse_tim(d, tim_off))


class _Thread:
    def __init__(self, code):
        self.code, self.pc, self.loops, self.done = code, 0, [], False


def play(dor, var0, var3, max_frames=1200):
    """Run the scene for a door of type var0 with handle var3; returns a FrameState per frame
    until every thread has finished."""
    var = {0: var0, 3: var3}
    cam = [0] * 6
    cam_vel = [0] * 6
    slots = {}
    threads = {0: _Thread(dor.routines[0])}
    frames = []
    for _ in range(max_frames):
        sounds = []
        t_ids = sorted(threads)
        i = 0
        while i < len(t_ids):
            th = threads[t_ids[i]]
            i += 1
            while not th.done:
                c = th.code
                if th.pc >= len(c):
                    th.done = True
                    break
                op = c[th.pc]
                n = OPS.get(op)
                if n is None:
                    raise ValueError("unknown door op 0x%02X at %d" % (op, th.pc))
                a = c[th.pc : th.pc + n]
                th.pc += n
                if op in (0x00, 0x01):
                    th.done = True
                elif op == 0x03:
                    break
                elif op == 0x04:
                    if var.get(a[2], 0) != struct.unpack_from("<H", a, 4)[0]:
                        th.pc += a[1]
                elif op == 0x06:
                    th.loops.append([th.pc, struct.unpack_from("<H", a, 2)[0]])
                elif op == 0x07:
                    lp = th.loops[-1]
                    lp[1] -= 1
                    if lp[1] > 0:
                        th.pc = lp[0]
                    else:
                        th.loops.pop()
                    break
                elif op == 0x08:
                    threads[a[2]] = _Thread(dor.routines[a[3]])
                    if a[2] not in t_ids:
                        t_ids = sorted(set(t_ids) | {a[2]})
                elif op in (SOUND_OPEN, SOUND_SHUT):
                    sounds.append(op)
                elif op == 0x10:
                    slots[a[1]] = Slot(a[2], a[3], a[4])
                elif op == 0x11:
                    cam = list(struct.unpack_from("<6h", a, 2))
                elif op == 0x12:
                    cam_vel = list(struct.unpack_from("<6h", a, 2))
                elif op == 0x13:
                    cam = [p + v for p, v in zip(cam, cam_vel)]
                elif op == 0x14:
                    cam_vel[a[2]] += struct.unpack_from("<b", a, 3)[0]
                elif op in (0x15, 0x16, 0x19):
                    v = list(struct.unpack_from("<3h", a, 2))
                    s = slots[a[1]]
                    if op == 0x15:
                        s.pos = v
                    elif op == 0x16:
                        s.pos_vel = v
                    else:
                        s.rot = v
                elif op == 0x17:
                    s = slots[a[1]]
                    s.pos = [p + v for p, v in zip(s.pos, s.pos_vel)]
                elif op == 0x18:
                    slots[a[1]].pos_vel[a[2]] += struct.unpack_from("<b", a, 3)[0]
                elif op == 0x1B:
                    s = slots[a[1]]
                    s.rot = [p + v for p, v in zip(s.rot, s.rot_vel)]
                elif op == 0x1C:
                    slots[a[1]].rot_vel[a[2]] += struct.unpack_from("<b", a, 3)[0]
                elif op == 0x21:
                    if a[1] in slots:
                        slots[a[1]].flags = a[2]
                elif op == 0x20:
                    for t in threads.values():  # the scene ends here (see the module doc)
                        t.done = True
        frames.append(FrameState(tuple(cam[:3]), tuple(cam[3:]),
                                 {k: (s.model, s.parent, s.flags, tuple(s.pos), tuple(s.rot)) for k, s in slots.items()},
                                 sounds))
        threads = {k: t for k, t in threads.items() if not t.done}
        if not threads:
            break
    return frames


def slot_world(state, slot):
    """(rotation matrix, translation) of a slot, through its parents."""
    model, parent, flags, pos, rot = state.slots[slot]
    r = emd.rot_matrix(*rot)  # 4096 = 360 degrees, as emd's
    t = pos
    if parent != 0xFF and parent in state.slots:
        pr, pt = slot_world(state, parent)
        return emd.matmul(pr, r), tuple(a + b for a, b in zip(emd.matvec(pr, t), pt))
    return r, tuple(t)
