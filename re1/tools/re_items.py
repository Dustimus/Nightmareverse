"""Mod1's RE item texts: names, what Navi says when you Check one on the status screen, and
the pickup prompt. Ours (written for Mod1, in Navi's neutral voice), so committed; RE1's own
descriptions can replace them later. integrate.py --pause writes them as OoT messages:

    RE_MSG_CHECK_BASE + item   Check (Navi's black box, light blue, like her hints)
    RE_MSG_TAKE_BASE + item    "It's a clip. Take it?" (two choices) when picking one up
    RE_MSG_CANT_USE, RE_MSG_FULL, RE_MSG_NO_TEXT    (re_inventory.h)
    RE_MSG_FILE_BASE + file    a file read out (picked up, or from the status screen's FILE)
    RE_MSG_FILE_MENU_BASE + file    the FILE browser on it: its name, then Read / Next / Close

and include/re_items_text.h lists the items that have texts.
"""

MSG_CANT_USE = 0x7380
MSG_FULL = 0x7381
MSG_NO_TEXT = 0x7382
MSG_SAVED = 0x7384
MSG_DOOR = 0x7385  # a door's A: no box shown (En_ReLock closes it at once and goes through)
MSG_CHECK_BASE = 0x7400
MSG_TAKE_BASE = 0x7480
MSG_FILE_BASE = 0x7500
MSG_FILE_MENU_BASE = 0x7510
FILE_FIRST = 0x5F  # RE1's files: items 0x5F-0x6E (keep in sync with RE_ITEM_FILE_FIRST)

# RE1 item id: (name with its article, Check text)
ITEMS = {
    0x01: ("a knife", "A combat knife.\nFine up close, but you'd\nrather not get that close."),
    0x02: ("a Beretta", "A Beretta, a 9mm handgun.\nIt takes fifteen rounds.\nAim with Z and fire!"),
    0x03: ("a shotgun", "A pump-action shotgun.\nSlow, but it stops anything\nup close."),
    0x07: ("the bazooka", "Forest's bazooka, a grenade\nlauncher. Six rounds in it."),
    0x0B: ("a clip", "A clip of handgun bullets.\nCombine it with the Beretta\nto load it."),
    0x10: ("explosive rounds", "Grenade rounds for the\nbazooka. They go off on\nimpact. Combine to load."),
    0x11: ("acid rounds", "Acid rounds for the bazooka.\nCombine them with it to\nload it."),
    0x12: ("flame rounds", "Flame rounds for the bazooka.\nCombine them with it to\nload it."),
    0x0C: ("shotgun shells", "A box of shotgun shells.\nCombine them with the\nshotgun to load it."),
    0x1C: ("a broken shotgun", "A shotgun that doesn't work.\nIt looks just like the one\non the rack downstairs."),
    0x1D: ("a crank", "A crank with a square end.\nIt must turn something."),
    0x1F: ("a wooden emblem", "A wooden emblem, taken from\nabove the dining room's\nfireplace."),
    0x20: ("a gold emblem", "A gold emblem, the same shape\nas the wooden one. Heavy."),
    0x21: ("a blue jewel", "A blue jewel, cut to fit\nsomewhere. An eye, maybe?"),
    0x22: ("a red jewel", "A red jewel, cut to fit\nsomewhere. An eye, maybe?"),
    0x23: ("the music notes", "Sheet music: the \"Moonlight\nSonata\". On the ocarina:\n"
                              "[A] [C-Down] [C-Right] [A] [C-Down] [C-Right]"),
    0x26: ("a chemical", "A bottle of some chemical.\nIt has a strong smell."),
    0x29: ("the wind crest", "A six-sided crest with a\nswirl of wind on it. It must\nfit a hollow somewhere."),
    0x2C: ("the moon crest", "A six-sided crest with a\nmoon on it. It must fit a\nhollow somewhere."),
    0x2D: ("the star crest", "A six-sided crest with a star\non it. It must fit a hollow\nsomewhere."),
    0x2E: ("the sun crest", "A six-sided crest with a sun\non it. It must fit a hollow\nsomewhere."),
    0x2F: ("an ink ribbon", "A typewriter ribbon. With one\nof these, a typewriter can\nrecord your progress."),
    0x30: ("a lighter", "A lighter. Handy in a house\nthis dark."),
    0x33: ("the sword key", "A key with a sword carved\ninto its bow."),
    0x34: ("the armor key", "A key with a suit of armor\ncarved into its bow."),
    0x35: ("the shield key", "A key with a shield carved\ninto its bow."),
    0x36: ("the helmet key", "A key with a helmet carved\ninto its bow."),
    0x3D: ("a small key", "A small, plain key. Too\nsmall for any door; it\nmust open something else."),
    0x41: ("a first aid spray", "A first aid spray. It heals\nall of your wounds."),
    0x42: ("a serum", "A serum, an antidote to\nsnake venom. Better have it\nbefore you need it."),
    0x43: ("a red herb", "A red herb. On its own it does\nnothing; mixed with a green\none it's much stronger."),
    0x44: ("a green herb", "A green herb. It heals a\nlittle. Mix them to heal more."),
    0x45: ("a blue herb", "A blue herb. It cures poison."),
    0x46: ("a mixed herb", "Green and red herbs, mixed.\nThey heal everything."),
    0x47: ("a mixed herb", "Two green herbs, mixed.\nThey heal half your wounds."),
    0x48: ("a mixed herb", "Green and blue herbs, mixed.\nThey heal a little and cure\npoison."),
    0x49: ("a mixed herb", "Green, red and blue herbs.\nThey heal everything and\ncure poison."),
    0x4A: ("a mixed herb", "Three green herbs, mixed.\nThey heal everything."),
    0x4B: ("a mixed herb", "Two green herbs and a blue,\nmixed. They heal half your\nwounds and cure poison."),
    0x4E: ("the map of the 1st floor", "The mansion's ground floor,\nall of it. No more guessing\nwhere the doors lead."),
}

# RE1's files (documents), items 0x5F-0x6E: RE1 keeps their pages as pictures of Japanese text
# (ITEM_M2's TEXTM_L0-Z0, 00-02, one letter a file; the titles read in that order), and its
# English pages aren't in the data we have, so these are our own short notes on what each
# says. A file takes no slot (ReInventory_Add keeps a bit for it, gReFiles). By RE1's room
# scripts: 10E's 0x61, 206's 0x6E, 20A's 0x5F (Jill's; 0x60 is Chris's, flag 1:62 picks).
# Files without a note yet read "(not written yet)".
# file item: (name with its article, title, boxes of text)
FILES = {
    0x5F: ("the researcher's will", "Researcher's Will",
           ["A letter. The first half has\nbeen torn away.",
            "In what's left, a researcher\nwrites to his daughter, Alma.\nSomething they made here has\ngot loose, and it's changing\nhim.",
            "He knows he hasn't long.\nHe tells her he's sorry, and\nto forget this place."]),
    0x60: ("the researcher's will", "Researcher's Will",
           ["A letter to a daughter, Alma,\ndated June 3, 1998.",
            "A researcher writes that\nsomething they made here has\ngot loose, and it's changing\nhim.",
            "He knows he hasn't long.\nHe tells her he's sorry, and\nto forget this place."]),
    0x61: ("the keeper's diary", "Keeper's Diary",
           ["A diary kept by one of the\nmansion's animal keepers,\nfrom May 9, 1998.",
            "It starts with card games\nand feeding the creatures in\nhis care.",
            "Then an accident, and the\nentries change. His skin\nitches and swells. He's\nhungry all the time.",
            "By the last pages the hand\nis hardly his own: a few\nwords about the itch, and\nabout food."]),
    0x62: ("the orders", "Orders", None),
    0x63: ("the pass number", "Pass Number", None),
    0x64: ("the Plant 42 report", "Plant 42 Report", None),
    0x65: ("the fax", "Fax", None),
    0x66: ("the scrapbook", "Scrapbook", None),
    0x67: ("the security system notes", "Security System", None),
    0x68: ("the researcher's letter", "Researcher's Letter", None),
    0x69: ("the V-JOLT report", "V-JOLT Report", None),  # (no quotes: the message tool can't take them)
    0x6A: ("Barry's picture", "Barry's Picture", None),
    0x6B: ("pass code 01", "Pass Code 01", None),
    0x6C: ("pass code 02", "Pass Code 02", None),
    0x6D: ("pass code 03", "Pass Code 03", None),
    0x6E: ("the botany book", "Botany Book",
           ["A book on the medicinal\nplants that grow around\nhere.",
            "Green herbs restore your\nstrength. Blue herbs draw out\npoison.",
            "A red herb does nothing on\nits own, but mixed with a\ngreen one it makes the green\nfar stronger."]),
}
for _item, (_name, _title, _boxes) in FILES.items():  # pickups and take prompts as other items
    ITEMS[_item] = (_name, "A file: %s." % _title)


def _msg(text_id, body, box="TEXTBOX_TYPE_BLACK"):
    return ("DEFINE_MESSAGE(0x%04X, %s, TEXTBOX_POS_BOTTOM,\nMSG(/* MISSING */)\n,\n%s\n,\n%s\n,\n%s\n)\n"
            % (text_id, box, body, body, body))


def _lines(text):
    esc = [ln.replace("\\", "\\\\").replace('"', '\\"') for ln in text.split("\n")]
    return '"' + '\\n"\n"'.join(esc) + '"'


def messages_c():
    out = [_msg(MSG_CANT_USE, 'MSG(\nCOLOR(LIGHTBLUE) "There\'s no use for that here." COLOR(DEFAULT)\n)'),
           _msg(MSG_FULL, 'MSG(\n"You can\'t carry any more."\n)'),
           _msg(MSG_NO_TEXT, 'MSG(\nCOLOR(LIGHTBLUE) "Hmm... I don\'t know what\\n"\n"that is." COLOR(DEFAULT)\n)'),
           _msg(MSG_SAVED, 'MSG(\n"Your progress has been saved."\n)'),
           _msg(MSG_DOOR, 'MSG(\n" "\n)', box="TEXTBOX_TYPE_NONE_NO_SHADOW")]
    for item, (name, check) in sorted(ITEMS.items()):
        out.append(_msg(MSG_CHECK_BASE + item, "MSG(\nCOLOR(LIGHTBLUE) %s COLOR(DEFAULT)\n)" % _lines(check)))
        # plural names (the music notes, acid rounds): "They're ... Take them?"
        plural = name.endswith("s") and not name.startswith(("a ", "an "))
        ask = ("They're %s. Take them?" if plural else "It's %s. Take it?") % name
        out.append(_msg(MSG_TAKE_BASE + item, 'MSG(\n"%s\\n"\n"\\n"\nTWO_CHOICE\n    COLOR(ADJUSTABLE) "Yes\\n"\n'
                                             '    "No" COLOR(DEFAULT)\n)' % ask))
    for item, (name, title, boxes) in sorted(FILES.items()):
        idx = item - FILE_FIRST
        body = "\nBOX_BREAK\n".join(_lines(b) for b in (boxes or ["(not written yet)"]))
        out.append(_msg(MSG_FILE_BASE + idx, 'MSG(\nCOLOR(RED) "%s" COLOR(DEFAULT) "\\n"\nBOX_BREAK\n%s\n)'
                        % (title.replace('"', '\\"'), body)))
        out.append(_msg(MSG_FILE_MENU_BASE + idx, 'MSG(\nCOLOR(RED) "%s" COLOR(DEFAULT) "\\n"\nTHREE_CHOICE\n'
                        '    COLOR(ADJUSTABLE) "Read\\n"\n    "Next\\n"\n    "Close" COLOR(DEFAULT)\n)'
                        % title.replace('"', '\\"')))
    return "\n".join(out)


def header_c():
    return ("#ifndef RE_ITEMS_TEXT_H\n#define RE_ITEMS_TEXT_H\n\n// Generated by tools/oot/re_items.py; do not edit.\n"
            "// The RE items with a Check text and a pickup prompt (message ids in re_inventory.h).\n"
            "#define RE_ITEM_TEXT_ITEMS { %s, 0 }\n\n#endif\n" % ", ".join("0x%02X" % i for i in sorted(ITEMS)))
