"""Read the staging out of RE1 room event scripts (RDT offsets[8]).

    python3 tools/re1/evt.py <RE1>/JPN/STAGE1/ROOM1051.RDT [event,...]

Prints, per event, the camera cuts, messages, waits and per-character animation/move
commands in order, for writing story_*.json staging by hand. Partial decoding (see
docs/design.md, "RE1 event scripts"):

    07 nn <SCD instruction>   inline SCD op; 07 04 09 NN = cut to camera NN,
                              07 06 0B NN .. = show message NN (the scene's NN-th voice clip)
    F8 F9 nn 00               wait nn frames
    04 EE 00 01               following commands act on character EE (0 = the player)
    84 AA                     play animation AA (index into that character's EMD animations)
    83 AA xx xx zz zz         walk to (x, z) with animation AA
"""

import struct
import sys

import rdt


def event_scripts(room):
    d = room.raw
    o = room.offsets[8]
    if not o:
        return []
    end = min([x for x in room.offsets if x > o] + [len(d)])
    n = struct.unpack_from("<I", d, o)[0] // 4
    offs = list(struct.unpack_from("<%dI" % n, d, o)) + [end - o]
    return [d[o + offs[i] : o + offs[i + 1]] for i in range(n)]


def decode(ev):
    """[(op, args)] for the commands we understand; other bytes are skipped."""
    out, i = [], 0
    while i < len(ev) - 3:
        b = ev[i]
        if b == 0x04 and ev[i + 2] == 0 and ev[i + 3] == 1:
            out.append(("char", ev[i + 1]))
            i += 4
        elif b == 0x07 and ev[i + 1] in (4, 6) and ev[i + 2] == 0x09:
            out.append(("cam", ev[i + 3]))
            i += 4
        elif b == 0x07 and ev[i + 1] == 6 and ev[i + 2] == 0x0B:
            out.append(("msg", ev[i + 3]))
            i += 6
        elif b == 0x07 and ev[i + 1] == 6 and ev[i + 2] == 0x1E and ev[i + 3] == 1:
            out.append(("voice", ev[i + 4]))
            i += 8
        elif b == 0x06 and i + 4 <= len(ev) and ev[i + 3] == 0 and ev[i + 2] + 4 <= ev[i + 1] + 2:
            # inline block of SCD ops: 06 <len> <ops len> 00 <ops>
            j, end = i + 4, i + 4 + ev[i + 2]
            while j < end:
                op = ev[j]
                if op == 0x0B:
                    out.append(("msg", ev[j + 1]))
                    j += 4
                elif op == 0x1E and ev[j + 1] == 1:
                    out.append(("voice", ev[j + 2]))
                    j += 4
                elif op == 0x09:
                    out.append(("cam", ev[j + 1]))
                    j += 2
                else:
                    j += 1
            i += 4 + ev[i + 2]
        elif b == 0x07 and ev[i + 1] == 0x0C and ev[i + 2] == 0x17:
            out.append(("shot", ev[i + 4]))  # SCD 0x17: an effect (gun flash / hit) at x, z
            i += 14
        elif b == 0x07 and ev[i + 1] == 0x08 and ev[i + 2] == 0x28:
            out.append(("damage", ev[i + 6]))  # SCD 0x28: hurts the room's enemy by this much
            i += 10
        elif b == 0xF8 and ev[i + 1] == 0xF9:
            out.append(("wait", ev[i + 2]))
            i += 4
        elif b == 0x84:
            out.append(("anim", ev[i + 1]))
            i += 2
        elif b == 0x85:
            out.append(("anim", ev[i + 1]))  # like 84, with two more bytes (speed / flags?)
            i += 4
        elif b == 0x83 and i + 6 <= len(ev):
            x, z = struct.unpack_from("<2H", ev, i + 2)
            out.append(("move", (ev[i + 1], x, z)))
            i += 6
        else:
            i += 1
    return out


def main():
    room = rdt.load_room(sys.argv[1])
    evs = event_scripts(room)
    which = [int(v) for v in sys.argv[2].split(",")] if len(sys.argv) > 2 else range(len(evs))
    for k in which:
        parts = []
        for op, arg in decode(evs[k]):
            if op == "char":
                parts.append("\n    char %d:" % arg)
            elif op == "move":
                parts.append("move(anim %d -> %d,%d)" % arg)
            else:
                parts.append("%s %d" % (op, arg))
        if parts:
            print("event %d:%s" % (k, " ".join(parts)))


if __name__ == "__main__":
    main()
