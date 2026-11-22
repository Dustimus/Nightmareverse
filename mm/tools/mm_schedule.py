"""MM's schedule scripts (.schl) into MM's schedule bytecode, as C arrays for z_nv_mm_schedule.c.

MM decides where its people are, and whether its doors open, with small scripts: checks of the
day, the time, the scene, week events and a few items, branching to a "return": a value, two
times and a value (where someone walks between them), or nothing. The decomp writes them in a
little language (src/overlays/actors/ovl_*/scheduleScripts.schl) and compiles them with its own
tool to MM's bytecode (include/z64schedule.h); z_schedule.c runs it. This does the compiling
part for us: the scripts are the decomp's, copied into mm/schedules/ with the actors that use
them; the bytecode is MM's (commands 0x00-0x12, its own layout), run by our port of z_schedule.c.

The language, as the scripts use it:
    name { statements }
    [not] if_week_event_reg (FLAG)          if_time_range (h, m, h, m)    if_misc (WHICH)
    [not] if_scene (SCENE_X)                 if_day (n)                    if_before_time (h, m)
    [not] if_since_time (h, m)               ... { statements } [else if_... { } | else { }]
    branch (label)    label:    return_s (v)    return_l (v)    return_none    return_empty
    return_time (h, m, h, m, v)
Branches are always the long forms here (two-byte offsets), which MM runs the same.

A value may be a name (an enum of the actor's, defined in its C file before the .inc is
included). Week events and scenes are resolved from the decomp's headers (its weekEventReg flags, scene table) at compile
time; a scene that isn't brought over yet becomes 0x7FFF (no scene matches). Usage:
    mm_schedule.py <mm decomp checkout> <out folder> <file.schl>...
writes <out folder>/<file>.schl.inc for each (soh/src/code/nv_mm_schedules/; committed, so the
decomp is only needed to make them again).
"""

import os
import re
import sys

CMD = {"wer": 0x01, "time_range": 0x03, "ret_l": 0x04, "ret_none": 0x05, "ret_empty": 0x06,
       "misc": 0x08, "ret_s": 0x09, "not_scene": 0x0B, "not_day": 0x0D, "ret_time": 0x0E,
       "before": 0x10, "branch": 0x12}
MISC = {"SCHEDULE_CHECK_MISC_ROOM_KEY": 0, "SCHEDULE_CHECK_MISC_LETTER_TO_KAFEI": 1,
        "SCHEDULE_CHECK_MISC_MASK_ROMANI": 2}
NO_SCENE = 0x7FFF

TOKEN = re.compile(r"\s*(?:(//[^\n]*|/\*.*?\*/)|([A-Za-z_][A-Za-z0-9_]*)|(0x[0-9A-Fa-f]+|\d+)|(.))", re.S)


def lo(v):
    """A value's low byte: a number, or a C name the actor's file defines (its enum)."""
    return v & 0xFF if isinstance(v, int) else f"(u8)({v})"


def hi(v):
    return (v >> 8) & 0xFF if isinstance(v, int) else f"(u8)(({v}) >> 8)"


def tokens(text):
    out = []
    for m in TOKEN.finditer(text):
        if m.group(1):
            continue
        if m.group(2):
            out.append(("id", m.group(2)))
        elif m.group(3):
            out.append(("num", int(m.group(3), 0)))
        elif m.group(4) and not m.group(4).isspace():
            out.append(("sym", m.group(4)))
    return out


class Names:
    """The decomp's names: weekEventReg flags, and MM's scenes as ours (C expressions)."""

    def __init__(self, decomp):
        self.flags = {}
        for line in open(os.path.join(decomp, "include", "z64save.h")):
            m = re.match(r"#define (WEEKEVENTREG_\w+) PACK_WEEKEVENTREG_FLAG\((\d+), (0x[0-9A-Fa-f]+)\)", line)
            if m:
                self.flags[m.group(1)] = (int(m.group(2)) << 8) | int(m.group(3), 16)
        self.scenes = {}
        for line in open(os.path.join(decomp, "include", "tables", "scene_table.h")):
            m = re.search(r"DEFINE_SCENE\((\w+), (SCENE_\w+),", line)
            if m:
                self.scenes[m.group(2)] = m.group(1)  # SCENE_ICHIBA -> Z2_ICHIBA

    def scene(self, name):
        """MM's scene as ours: the SoH scene enum if it's brought over, else none."""
        sys.path.insert(0, os.path.dirname(__file__))
        import mm_rom
        file = self.scenes[name]
        return f"SCENE_MM_{file}" if file in mm_rom.CLOCK_TOWN else None


class Compiler:
    def __init__(self, toks, names):
        self.t, self.i, self.names = toks, 0, names

    def peek(self, k=0):
        return self.t[self.i + k] if self.i + k < len(self.t) else (None, None)

    def take(self, kind=None, val=None):
        tok = self.t[self.i]
        if (kind and tok[0] != kind) or (val is not None and tok[1] != val):
            raise SyntaxError(f"expected {kind} {val!r}, got {tok}")
        self.i += 1
        return tok[1]

    def args(self):
        self.take("sym", "(")
        out = []
        while self.peek() != ("sym", ")"):
            out.append(self.t[self.i][1])
            self.i += 1
            if self.peek() == ("sym", ","):
                self.i += 1
        self.take("sym", ")")
        return out

    def scripts(self):
        out = []
        while self.i < len(self.t):
            name = self.take("id")
            self.take("sym", "{")
            self.code, self.labels, self.n = [], {}, 0
            self.block()
            out.append((name, self.code, self.labels))
        return out

    def new_label(self):
        self.n += 1
        return f".L{self.n}"

    def emit(self, *items):
        self.code.extend(items)

    def block(self):
        """Statements up to the closing brace."""
        while self.peek() != ("sym", "}"):
            self.statement()
        self.take("sym", "}")

    def statement(self):
        kind, word = self.peek()
        if kind == "id" and self.peek(1) == ("sym", ":"):
            self.i += 2
            self.emit(("label", word))
            return
        neg = False
        if word == "not":
            self.i += 1
            neg = True
            word = self.peek()[1]
        if word.startswith("if_"):
            self.conditional(neg)
            return
        self.i += 1
        if word == "return_none":
            self.emit(("bytes", [CMD["ret_none"]]))
        elif word == "return_empty":
            self.emit(("bytes", [CMD["ret_empty"]]))
        elif word == "return_s":
            (v,) = self.args()
            self.emit(("bytes", [CMD["ret_s"], lo(v)]))
        elif word == "return_l":
            (v,) = self.args()
            self.emit(("bytes", [CMD["ret_l"], hi(v), lo(v)]))
        elif word == "return_time":
            h0, m0, h1, m1, v = self.args()
            self.emit(("bytes", [CMD["ret_time"], h0, m0, h1, m1, lo(v)]))
        elif word == "branch":
            (label,) = self.args()
            self.emit(("jump", CMD["branch"], [], label))
        else:
            raise SyntaxError(f"unknown statement {word}")

    def condition(self, word, args):
        """(command bytes before the offset, whether it branches when the condition holds)."""
        if word == "if_week_event_reg":
            f = self.names.flags[args[0]]
            return [CMD["wer"], f >> 8, f & 0xFF], True
        if word == "if_time_range":
            return [CMD["time_range"]] + list(args), True
        if word == "if_before_time":
            return [CMD["before"]] + list(args), True
        if word == "if_since_time":
            return [CMD["before"]] + list(args), False
        if word == "if_misc":
            return [CMD["misc"], MISC[args[0]]], True
        if word == "if_scene":
            s = self.names.scene(args[0])
            hi, lo = (f"(u8)({s} >> 8)", f"(u8)({s})") if s else (NO_SCENE >> 8, NO_SCENE & 0xFF)
            return [CMD["not_scene"], hi, lo], False
        if word == "if_day":
            return [CMD["not_day"], 0, args[0]], False
        raise SyntaxError(f"unknown condition {word}")

    def conditional(self, neg):
        word = self.take("id")
        pre, when_true = self.condition(word, self.args())
        if neg:
            when_true = not when_true
        self.take("sym", "{")
        then_start = len(self.code)
        self.block()
        then_code = self.code[then_start:]
        del self.code[then_start:]
        else_code = []
        if self.peek() == ("id", "else"):
            self.i += 1
            start = len(self.code)
            if self.peek() == ("sym", "{"):
                self.i += 1
                self.block()
            else:
                neg2 = False
                if self.peek() == ("id", "not"):
                    self.i += 1
                    neg2 = True
                self.conditional(neg2)
            else_code = self.code[start:]
            del self.code[start:]
        # the check's offset is a long one (its last two bytes)
        target, end = self.new_label(), self.new_label()
        if pre[0] == CMD["misc"]:  # (MM has only the short form: a one-byte offset)
            self.emit(("jump_s", pre[0], pre[1:], target))
        else:
            self.emit(("jump", pre[0], pre[1:], target))
        first, second = (else_code, then_code) if when_true else (then_code, else_code)
        self.code.extend(first)
        self.emit(("jump", CMD["branch"], [], end))
        self.emit(("label", target))
        self.code.extend(second)
        self.emit(("label", end))


def assemble(code):
    """Positions, then bytes (C expressions where a scene enum goes)."""
    size = lambda it: (len(it[1]) if it[0] == "bytes" else 0 if it[0] == "label"
                       else 1 + len(it[2]) + (1 if it[0] == "jump_s" else 2))
    pos, labels = 0, {}
    for it in code:
        if it[0] == "label":
            labels[it[1]] = pos
        pos += size(it)
    out, pos = [], 0
    for it in code:
        pos += size(it)
        if it[0] == "bytes":
            out += it[1]
        elif it[0] in ("jump", "jump_s"):
            off = labels[it[3]] - pos
            if it[0] == "jump_s":
                if not -128 <= off < 128:
                    raise ValueError("short branch out of range")
                out += [it[1]] + it[2] + [off & 0xFF]
            else:
                out += [it[1]] + it[2] + [(off >> 8) & 0xFF, off & 0xFF]
    return out


def main():
    decomp, dest, *paths = sys.argv[1:]
    names = Names(decomp)
    os.makedirs(dest, exist_ok=True)
    for p in paths:
        lines = [f"/* Generated by mm/tools/mm_schedule.py from mm/schedules/{os.path.basename(p)} (the MM decomp's",
                 " * schedule scripts): MM's schedule bytecode, run by z_nv_mm_schedule.c. Do not edit. */", ""]
        for name, code, _ in Compiler(tokens(open(p).read()), names).scripts():
            body = ", ".join(x if isinstance(x, str) else f"0x{x:02X}" for x in assemble(code))
            lines.append(f"static u8 {name}[] = {{ {body} }};")
        out = os.path.join(dest, os.path.basename(p) + ".inc")
        open(out, "w").write("\n".join(lines) + "\n")
        print(out)


if __name__ == "__main__":
    main()
