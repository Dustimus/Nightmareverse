"""Mod1 story events: RE1 conversations as OoT text, started by trigger areas.

The repo only says *where* a scene plays and *which voice clips* it uses (story_*.json);
the words come from the user's own game at build time, so no game text is committed:

    voice text: a JSON {"V105_02.WAV": {"text": "..."}} made by transcribing the game's
                English voice clips (RE1's English release has no subtitles for its voiced
                scenes; the room files only point at the clips)
    fixes:      optional JSON {"V105_02": [["Barry", "..."], ["Jill", "..."]]} that splits
                a clip between speakers or corrects the transcript

story_*.json:
    {"events": [{"id": "dining_blood", "room": "105", "flag": 1,
                 "zone": [x0, z0, x1, z1],            # RE1 units (the trigger rectangle)
                 "lines": [["Barry", "V105_00", {"cam": 3, "anim": {"Barry": 9}}], ...]}]}

Each line is its own OoT message (RE_EVENT_TEXT_BASE + running line number). The
optional shot is RE1's staging for that line, read from the room's event scripts
(tools/re1/evt.py): "cam" cuts to that RE1 camera, "anim" plays an RE1 animation (index
into the character's EMD animations) on a character until the next line changes it,
"walk" {"Barry": {"anim": 2, "path": [[x, z], ...]}} walks a character along RE1 points
first (RE1 units, up to 3). "Jill" (the player) walks Link along her route. "shots": n has
Barry fire n times after the line (RE1's aim/fire animations 7/8 and his magnum), the last
killing the room's zombie, which the scene drives meanwhile (it shambles at Link). An event's
"requires" / "forbids" name story flags (STORY_FLAGS) the event waits for / is off with,
"sets" the flags it sets once played. An event with no lines is silent: it just sets its flags
while Link is in its zone. A line's speaker may be "" and its clip "TEXT:<room>:<message hex>"
for one of the room's own messages (read from the game at build time, like the voice text).
"blocks_exit": the zone keeps Link from taking an exit inside it while the event is live (the
door doesn't open), and he steps back out after the lines; "arm_outside": it only arms once
Link has been outside the zone (he may arrive inside it). Without a "flag" an event repeats.
"switch": a room switch flag (e.g. a pickup_switches entry) that starts it wherever Link is,
after "quake" frames of the room shaking (with no "flag" of its own it clears that switch after
playing, so it plays again each time the switch is set); "exit": an entrance (ENTR_...) it fades out to after
its lines (RE1 moving Jill on to another room for the rest of a scene).
Event i becomes an En_ReEvent actor in its room (rdt2scene.py --story); integrate.py
--story writes the messages and the per-line staging table (z_en_re_event_data.inc.c).
"""

import json
import os

TEXT_BASE = 0x7200  # keep in sync with RE_EVENT_TEXT_BASE (z_en_re_event.h)
# speaker name -> En_ReNpc type (z_en_re_npc.h)
NPC_TYPES = {"Barry": 0, "Kenneth": 1, "Wesker": 2, "Richard": 3}
MAX_LINE_ANIMS = 2  # keep in sync with ReEventLine (z_en_re_event.h)
# "requires": story flags that cross rooms (ReStoryFlag, re_inventory.h): the scene only plays while set
STORY_FLAGS = {"zombie_follows": 0, "zombie_dead": 1, "zombie_alt": 2, "zombie_down": 3, "blood_seen": 4,
               "dining_visited": 5, "went_back": 6, "lost_courage": 7, "corridor_visited": 8,
               "hall_return": 9, "hall_talked": 10, "barry_left": 11, "lockpick": 12, "map_1f": 13,
               "song": 14, "trap_sprung": 15, "statue_fallen": 16,
               "richard_met": 17, "richard_dead": 18, "radio": 19, "barry_20a": 20, "barry_212": 21,
               "chemical": 22, "barry_203": 23, "yawn_bitten": 24, "yawn_rescued": 25}
EVENT_FLAGS = {"blocks_exit": 1, "arm_outside": 2}  # RE_EVENT_FLAG_* (z_en_re_event.h)


def _mask(names):
    if isinstance(names, str):
        names = [names]
    m = 0
    for n in names or []:
        m |= 1 << STORY_FLAGS[n]
    return m
PATH_MAX = 3  # RE_EVENT_PATH_MAX
PLAYER_NAMES = ("Jill", "Chris", "Link")
SCALE = 0.0217  # RE1 units -> OoT (rdt2scene.SCALE)


def _path_c(path):
    pts = [(round(x * SCALE), round(-z * SCALE)) for x, z in path[:PATH_MAX]]
    pts += [(0, 0)] * (PATH_MAX - len(pts))
    return "{ %s }" % ", ".join("{ %d, %d }" % p for p in pts)
LINE_CHARS = 32  # OoT's textbox fits ~32 characters of English per line
BOX_LINES = 4
NAME_COLOR = "LIGHTBLUE"


def load_story(path):
    with open(path) as f:
        return json.load(f)["events"]


def clip_text(speaker, clip, voice_text, fixes, room_text=None):
    """[(speaker, text)] for one line (a clip may hold several speakers, see fixes). A clip
    "TEXT:105:0e" is RE1's own text instead: message 0x0E of room 105 (room_text)."""
    if clip.upper().startswith("TEXT:"):
        _, room, num = clip.split(":")
        return [(speaker, room_text(room, int(num, 16)) if room_text else "...")]
    key = clip.upper()
    if key in fixes:
        return [(s, t) for s, t in fixes[key]]
    entry = voice_text.get(key + ".WAV") or voice_text.get(key)
    return [(speaker, entry["text"].strip())] if entry and entry.get("text") else [(speaker, "...")]


def event_lines(event):
    """[(speaker, clip, shot)] with the optional shot dict defaulted."""
    return [(ln[0], ln[1], ln[2] if len(ln) > 2 else {}) for ln in event["lines"]]


def _wrap(words, width):
    lines, cur = [], ""
    for w in words:
        if cur and len(cur) + 1 + len(w) > width:
            lines.append(cur)
            cur = w
        else:
            cur = (cur + " " + w) if cur else w
    if cur:
        lines.append(cur)
    return lines


def _c_str(s):
    s = s.replace("\u2019", "'").replace("\u2018", "'").replace("\u201c", '"').replace("\u201d", '"')
    s = s.encode("ascii", "replace").decode()
    return s.replace("\\", "\\\\").replace('"', '\\"')


def message_body(lines):
    """One OoT message: each speaker's line in its own box(es), the name in color."""
    boxes = []
    for speaker, text in lines:
        name = (speaker + ": ") if speaker else ""
        wrapped = _wrap((name + text).split(), LINE_CHARS)
        for i in range(0, len(wrapped), BOX_LINES):
            chunk = wrapped[i : i + BOX_LINES]
            parts = []
            for j, line in enumerate(chunk):
                if i == 0 and j == 0 and name:  # the line starts with "Name:"
                    head = name.strip()
                    parts.append('COLOR(%s) "%s" COLOR(DEFAULT) "%s'
                                 % (NAME_COLOR, _c_str(head), _c_str(line[len(head) :])))
                else:
                    parts.append('"%s' % _c_str(line))
            boxes.append('\\n"\n'.join(parts) + '"')
    return "\nBOX_BREAK\n".join(boxes)


def messages_c(events, voice_text, fixes, room_text=None):
    out = []
    n = 0
    for ev in events:
        for speaker, clip, _ in event_lines(ev):
            msg = "MSG(\n%s\n)" % message_body(clip_text(speaker, clip, voice_text, fixes, room_text))
            out.append("// %s %s\nDEFINE_MESSAGE(0x%04X, TEXTBOX_TYPE_BLACK, TEXTBOX_POS_BOTTOM,\nMSG(/* MISSING */)\n"
                       ",\n%s\n,\n%s\n,\n%s\n)\n" % (ev["id"], clip, TEXT_BASE + n, msg, msg, msg))
            n += 1
    return "\n".join(out)


def staging_c(events):
    """C tables for En_ReEvent: per event its first line and line count; per line the
    camera cut and character animations."""
    lines, infos = [], []
    for ev in events:
        flags = sum(v for k, v in EVENT_FLAGS.items() if ev.get(k))
        infos.append("    { %d, %d, %d, 0x%X, 0x%X, 0x%X, %d, %d, %s }, // %s"
                     % (len(lines), len(ev["lines"]), flags, _mask(ev.get("requires")), _mask(ev.get("forbids")),
                        _mask(ev.get("sets")), ev.get("switch", -1), ev.get("quake", 0), ev.get("exit", "0xFFFF"),
                        ev["id"]))
        for speaker, clip, shot in event_lines(ev):
            walks = shot.get("walk", {})
            who_all = [w for w in list(shot.get("anim", {})) + list(walks) if w in NPC_TYPES]
            acts = []
            for who in dict.fromkeys(who_all):
                w = walks.get(who, {})
                anim = shot.get("anim", {}).get(who, 0)
                acts.append("{ %d, %d, %d, %d, %s }" % (NPC_TYPES[who], anim, w.get("anim", 2),
                                                        min(PATH_MAX, len(w.get("path", []))),
                                                        _path_c(w.get("path", []))))
            acts = (acts + ["{ 0xFF, 0, 0, 0, %s }" % _path_c([])] * MAX_LINE_ANIMS)[:MAX_LINE_ANIMS]
            link = next((walks[n] for n in PLAYER_NAMES if n in walks), {})
            lines.append("    { %d, { %s }, %d, %s, %d }, // %s %s"
                         % (shot.get("cam", -1), ", ".join(acts), min(PATH_MAX, len(link.get("path", []))),
                            _path_c(link.get("path", [])), shot.get("shots", 0), speaker, clip))
    empty = "{ 0xFF, 0, 0, 0, %s }" % _path_c([])
    return ("// Generated by tools/oot/story.py (integrate.py --story); do not edit.\n"
            "static ReEventLine sReEventLines[] = {\n%s\n    { -1, { %s, %s }, 0, %s, 0 },\n};\n\n"
            "static ReEventInfo sReEventInfo[] = {\n%s\n    { 0, 0, 0, 0, 0, 0, -1, 0, 0xFFFF },\n};\n"
            % ("\n".join(lines), empty, empty, _path_c([]), "\n".join(infos)))


def load_text_sources(voice_text_path, fixes_path=None):
    voice_text = json.load(open(voice_text_path)) if voice_text_path and os.path.exists(voice_text_path) else {}
    fixes = json.load(open(fixes_path)) if fixes_path and os.path.exists(fixes_path) else {}
    return voice_text, {k.upper(): v for k, v in fixes.items()}


def load_cast(path):
    """Which RE1 placement of each character to use per room: [{"room", "model", "pick"}]."""
    with open(path) as f:
        return json.load(f).get("cast", [])


def cast_flag_bits(path, cast_entry):
    """En_ReNpc params bits 8-14 for a cast entry's "until"/"after" event (same room)."""
    events = {ev["id"]: ev for ev in load_story(path)}
    for key, after in (("until", 0), ("after", 1)):
        if key in cast_entry:
            ev = events[cast_entry[key]]
            if ev["room"] != cast_entry["room"]:
                raise SystemExit("cast %s: event %s is in another room (switch flags are per room)"
                                 % (cast_entry, ev["id"]))
            return ((ev["flag"] & 0x3F) << 8) | (after << 14)
    return 0x3F00


def load_pickups(path):
    """Items a room script hands out from a spot it sets up apart (parked at 0,0 in Item_set):
    [{"room", "item", "count", "rect": [x, z, w, d]}]."""
    with open(path) as f:
        return json.load(f).get("pickups", [])


def load_objects(path):
    """How a room's own objects behave, where it isn't Link pushing them (rdt2scene.py
    --room-objects): [{"room", "model", "kind": "fixed" | "slide" | "shown" | "skip", "flag", "slide": [dx, dz],
    "shut_flag", "visible"}] ("shown": drawn, where RE1 has it, only once the switch flag "flag"
    is set, with no collision ("pitch": a turn about x, RE1 units; "nudge": [dx, dz] RE1 units out
    from where RE1 has it, off the wall the background's depth would hide it in): 11A's crests; "slide": opens by that much, RE1 units, while the room's switch flag
    "flag" is set and "shut_flag", if given, isn't; drawn open too if "visible"; "skip": not
    placed; "at": [x, z] which of the script's placements, where it places one twice; "drop":
    {"edge": [x, z], "story": flag} a pushed one that goes over an edge there, setting a story
    flag: 202's statue; "target": {"at": [x, z], "switch": n} one that stays once pushed onto a spot,
    setting a switch flag: 205's armor)."""
    with open(path) as f:
        return json.load(f).get("objects", [])


def load_uses(path):
    """Spots where an item is Used (RE1's Aot type 7, En_ReUse): [{"room", "rect": [x, z, w, d],
    "item", "sets", "clears", "requires", "cam", "keep"}] (switch flags; any may be left out; "cam":
    RE1's camera to show the result from, Link halted meanwhile; "keep": the item isn't used up,
    e.g. the lighter)."""
    with open(path) as f:
        return json.load(f).get("uses", [])


def load_pickup_switches(path):
    """Switch flags pickups set when taken: [{"room", "item", "switch"}]; "appears" / "appears_story":
    the switch / story flag it shows up with; "sets_story": a story flag taking it sets."""
    with open(path) as f:
        return json.load(f).get("pickup_switches", [])


def load_questions(path):
    """RE1's Yes/No examines with an effect: [{"room", "msg" (the spot's own message, which picks
    the spot), "ask" (the question's message, or a list shown one after another), "switch" (set
    on Yes), "then" (the message shown from then on; without one the spot goes once the switch is
    set), "rect" ([x, z, w, d]: a spot of its own where RE1 has no examine aot), "cam" (RE1's camera
    shown while it asks)}] (room message
    indices; En_ReCheck params bit 14)."""
    with open(path) as f:
        return json.load(f).get("questions", [])


def load_orders(path):
    """Switches pressed in order (En_ReOrder): [{"room", "first", "count" (switch flags first..),
    "done" (set when all are pressed in turn), "fail" (set when one is pressed out of turn),
    "any" (true: any order, just all of them), "gate" (true: the first is a trigger that checks the
    rest: done, or the fail flag and poison gas)}]."""
    with open(path) as f:
        return json.load(f).get("orders", [])


def load_door_locks(path):
    """Doors RE1's script keeps shut (no lock in the door's data): [{"room", "door" (index),
    "opens" (the switch flag that opens it), "msg" (the room message shown meanwhile)}]."""
    with open(path) as f:
        return json.load(f).get("door_locks", [])


def load_enemies(path):
    """How a room's RE1 enemies behave beyond their Em_set: [{"room", "em" (Em_set's killed-flag
    index), "dead_flag", "window": [x, z] (RE1 units: it comes in through a window there),
    "window_flag"}] (switch flags)."""
    with open(path) as f:
        return json.load(f).get("enemies", [])


def load_extra_masks(path):
    """Foreground pieces RE1 lacks: [{"room", "camera", "rect": [x0, y0, x1, y1] (screen pixels,
    end exclusive), "depth" (camera-space, RE1 units)}]: something a camera sees in front of where
    actors go with no mask of RE1's over it (room 210's post, camera 6). The scene gets a mask
    cut from the background there."""
    with open(path) as f:
        return json.load(f).get("masks", [])


def load_camera_sets(path):
    """Rooms with a second set of backgrounds: [{"room", "count", "flag"}]: once the room's
    switch flag is set, camera i (< count) shows camera i + count instead (20F: dark, then
    candle-lit; ReScene_BgCam)."""
    with open(path) as f:
        return json.load(f).get("camera_sets", [])


def load_examine_after(path):
    """Examine spots whose text changes with a switch flag: [{"room", "msg", "switch", "then"}]:
    RE1's message "msg" until the flag is set, then message "then" (or nothing, with no "then");
    with "rect" [x, z, w, d] a spot of its own (RE1's script turns an aot into that message)."""
    with open(path) as f:
        return json.load(f).get("examine_after", [])


def load_checks(path):
    """Examine spots of RE1's events rather than its examine aots: [{"room", "rect": [x, z, w, d],
    "msg": [room messages, shown one after another], "cam"}]: checking it shows the messages, on
    RE1's camera "cam" if given (201's keypad: event 1 on aot 5, camera 7)."""
    with open(path) as f:
        return json.load(f).get("checks", [])


def load_songs(path):
    """Spots that answer the ocarina's "Moonlight Sonata" (En_ReSong): [{"room", "zone":
    [x0, z0, x1, z1], "flag", "cam"}]: playing it there sets the room's switch flag and shows
    RE1's camera "cam"."""
    with open(path) as f:
        return json.load(f).get("songs", [])


def cast_story_masks(cast_entry):
    """(requires, forbids, high) story-flag masks for a cast entry: En_ReNpc takes them in its
    actor entry's x / z rotations (16 bits each): flags 0-15, or with `high` (params bit 15)
    flags 16-31, shifted down; an entry can't mix the two halves."""
    req, forb = _mask(cast_entry.get("requires")), _mask(cast_entry.get("forbids"))
    if (req | forb) >> 16:
        if (req | forb) & 0xFFFF:
            raise SystemExit("cast %s: story flags from both halves (0-15 and 16-31)" % cast_entry)
        return req >> 16, forb >> 16, True
    return req, forb, False


def room_triggers(events, room_id):
    """[(event index, event)] for a room."""
    return [(i, ev) for i, ev in enumerate(events) if ev["room"].upper() == room_id.upper()]
