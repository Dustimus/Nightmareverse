"""Mod1 examine spots: RE1's examine rectangles as OoT "Check" spots that Navi reads.

RE1's room scripts mark examine spots with Aot_set zones (tools/re1/rdt.py): type 2, or
type 0 with a message (data bit 7) and no condition. Their text is the room's message
table, in English only in the European PC release (HORR/USA), so it is read from the
user's disc at build time and never committed:

    rdt2scene.py --examine   places an En_ReCheck per spot and writes examine.json (each
                             spot's room, message number and OoT text id) in the scene
    integrate.py --examine-text <PAL>/HORR/USA
                             writes those messages, Navi-style (black box, light blue
                             text, like her hints)

Typewriters (Aot_set type 0x10, where RE1 saves) are spots too, with two of RE1's shared
messages (from the executable: "If I had an INK RIBBON..." and "Will you use the INK RIBBON?",
En_ReCheck picks by whether Jill has one; ours without it) and always Navi's attention. Other spots draw
Navi's attention (targetable, green) when the story file says so:
    "examine": {"105": {"23": "attention"}}   (room -> RE1 message number -> tier)
everything else is a quiet "Check". Conditional spots (e.g. "Barry is investigating...")
are left out for now.
"""

import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "re1"))
import rdt  # noqa: E402

TEXT_BASE = 0x7300  # keep in sync with RE_CHECK_TEXT_BASE (z_en_re_check.h)
TEXT_COUNT = 0x80  # spots before the item messages (re_items.py MSG_CANT_USE)
MSG_UNLOCKED = 0x7383  # keep in sync with RE_MSG_UNLOCKED (re_inventory.h): En_ReLock's
ATTENTION_BIT = 1 << 12  # En_ReCheck params
TYPEWRITER_TEXT = "It's a typewriter."  # ours, not RE1's (so it can be committed)

# RE1's shared messages live in its executable (the European PC release's RESIDENTEVIL.EXE,
# 993792 bytes), not in the rooms: where its locked-door ones start, by Door_set lock (byte
# 24: the key it takes; 0x33 the sword key, which Jill's lock pick stands in for, 0x34 armor,
# 0x35 shield, 0x36 helmet). Other locks get the plain "locked" one.
EXE_SIZE = 993792
LOCK_MESSAGES = {0x33: 0xBE6B1, 0x34: 0xBE6D8, 0x35: 0xBE6FD, 0x36: 0xBE725}
LOCKED_PLAIN = 0xBE85C
LOCKED_INSIDE = 0xBE86B  # "It's locked from inside.": 0xFF, doors opened from the other side
UNLOCKED = 0xBE886  # "You unlocked it."
TYPEWRITER_NO_RIBBON = 0xBEA6D  # "It's an old typewriter. If I had an INK RIBBON, I could save..."
TYPEWRITER_USE_RIBBON = 0xBEAC0  # "You can save your progress with this. Will you use the INK RIBBON?"
TYPEWRITER_USE_TEXT = "You can save your progress\nwith this.\fWill you use an ink ribbon?{08}"  # ours, likewise
BOX_BIT = 1 << 15  # En_ReCheck params: an item box (RE1's Aot type 8): checking it opens the box screen
BOX_TEXT = "An item box."  # ours (shown only for a moment: the box opens)
QUESTION_BIT = 1 << 14  # En_ReCheck params: a Yes/No question (story "questions"), then its next message
TYPEWRITER_BIT = 1 << 13  # En_ReCheck params: a typewriter (its message and the next: no ribbon / use one?)
LOCKED_TEXT = "It's locked."  # ours, if the executable isn't given
UNLOCKED_TEXT = "You unlocked it."  # ours, likewise


def lock_message(lock):
    """The exe offset of what RE1 says at a door with this Door_set lock."""
    return LOCKED_INSIDE if lock == 0xFF else LOCK_MESSAGES.get(lock, LOCKED_PLAIN)


EXE_FALLBACKS = {UNLOCKED: UNLOCKED_TEXT, TYPEWRITER_NO_RIBBON: TYPEWRITER_TEXT, TYPEWRITER_USE_RIBBON: TYPEWRITER_USE_TEXT}


def door_spots(room, exits):
    """Examine spots for a room's locked doors that don't lead anywhere yet (not in `exits`,
    the Door_set indices that became exits): [(rect, exe offset)]."""
    out = []
    for di, door in enumerate(room.doors):
        lock = door.raw[24]
        if di in exits or not lock or lock >= 0xFE:  # 0xFE/0xFF: doors the scripts open
            continue
        out.append(((door.x, door.z, door.w, door.d), lock_message(lock)))
    return out


def exe_text(exe, offset):
    """One of RE1's shared messages, from its executable (None if it isn't the one we know)."""
    if not exe or not os.path.exists(exe) or os.path.getsize(exe) != EXE_SIZE:
        return None
    d = open(exe, "rb").read()
    end = offset
    while d[end] != 0x01:  # the end, past colour changes (0x05 and a colour, which may be 0x01)
        end += 2 if d[end] == 0x05 else 1
    if d[offset] == 0x40:  # the typewriter's starts with 0x40 (not text: shown as "d")
        offset += 1
    return rdt.decode_text(d[offset:end])


def spots(room):
    """[(aot, RE1 message number or None, our own text or None, attention)] for a room's
    examine spots."""
    out = []
    for a in room.aots or []:
        if a.type == 2 or (a.type == 0 and a.data & 0x80 and a.param == 0xFF):
            out.append((a, a.data & 0x7F, None, False))
        elif a.type == 0x10:  # a typewriter
            out.append((a, None, TYPEWRITER_TEXT, True))
        elif a.type == 8:  # an item box
            out.append((a, None, BOX_TEXT, True))
    return out


def tiers(story_path):
    if not story_path:
        return {}
    with open(story_path) as f:
        return json.load(f).get("examine", {})


def find_rdt(stage_dir, room_id, player):
    for fn in os.listdir(stage_dir):
        if fn.upper() == ("ROOM%s%s.RDT" % (room_id, player)).upper():
            return os.path.join(stage_dir, fn)
    raise SystemExit("no RDT for room %s in %s" % (room_id, stage_dir))


def message_body(text):
    """RE1's text (\\n new line, \\f next box, {08} ending in Yes/No) as an OoT message in Navi's
    style."""
    choice = "{08}" in text
    text = text.replace("{08}", "").rstrip()
    boxes = []
    for box in text.split("\f"):
        lines = [ln.strip() for ln in box.split("\n")]
        while lines and not lines[0]:  # RE1 starts one-line messages with an empty line
            lines.pop(0)
        if lines:
            esc = [ln.replace("\\", "\\\\").replace('"', '\\"') for ln in lines]
            boxes.append('COLOR(LIGHTBLUE) "' + '\\n"\n"'.join(esc) + '" COLOR(DEFAULT)')
    body = "\nBOX_BREAK\n".join(boxes) or '"..."'
    if choice:
        body += '\n"\\n"\nTWO_CHOICE COLOR(ADJUSTABLE) "Yes\\n"\n"No" COLOR(DEFAULT)'
    return body


def messages_c(scene_dirs, pal_dir, exe=None):
    """DEFINE_MESSAGE entries for every spot listed in the scenes' examine.json."""
    out = []
    for d in scene_dirs:
        path = os.path.join(d, "examine.json")
        if not os.path.exists(path):
            continue
        with open(path) as f:
            info = json.load(f)
        room = rdt.load_room(find_rdt(os.path.join(pal_dir, "STAGE" + info["room"][0]), info["room"], info["player"]))
        texts = rdt.messages(room)
        for spot in info["spots"]:
            if spot.get("exe") is not None:
                text = exe_text(exe, spot["exe"]) or EXE_FALLBACKS.get(spot["exe"], LOCKED_TEXT)
            else:
                msgs = spot["msg"] if isinstance(spot["msg"], list) else [spot["msg"]]  # several: one after another
                text = spot["text"] if spot.get("text") else "\f".join(rdt.decode_text(texts[m]).strip("\n")
                                                                       for m in msgs)
            msg = "MSG(\n%s\n)" % message_body(text)
            out.append("// room %s %s\nDEFINE_MESSAGE(0x%04X, TEXTBOX_TYPE_BLACK, TEXTBOX_POS_BOTTOM,\n"
                       "MSG(/* MISSING */)\n,\n%s\n,\n%s\n,\n%s\n)\n"
                       % (info["room"], "message %s" % spot["msg"] if spot.get("msg") is not None else
                          ("exe 0x%X" % spot["exe"] if spot.get("exe") is not None else "(ours)"),
                          spot["text_id"], msg, msg, msg))
    msg = "MSG(\n%s\n)" % message_body(exe_text(exe, UNLOCKED) or UNLOCKED_TEXT)
    out.append("// En_ReLock: exe 0x%X\nDEFINE_MESSAGE(0x%04X, TEXTBOX_TYPE_BLACK, TEXTBOX_POS_BOTTOM,\n"
               "MSG(/* MISSING */)\n,\n%s\n,\n%s\n,\n%s\n)\n" % (UNLOCKED, MSG_UNLOCKED, msg, msg, msg))
    return "\n".join(out)
