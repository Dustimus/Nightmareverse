"""Checks a player's RE1 files before the mod is made: which release each room file is from
(fingerprints of the releases we know, re1_known.json) and what language its text is in, so a
copy that's modded, damaged or mixed is reported plainly instead of making a mod that's quietly
wrong. Fingerprints only: no game data is kept here.

    python re1/tools/re1_verify.py <RE1 data folder>            report
    python re1/tools/re1_verify.py --learn <name> <RE1 folder>  add a release's fingerprints
"""
import hashlib
import json
import os
import struct
import sys

import rdt

HERE = os.path.dirname(os.path.abspath(__file__))
KNOWN = os.path.join(HERE, "re1_known.json")
PLAYER = "1"


def room_files(re1):
    """{room id: path} for Jill's rooms of stages 1 and 2."""
    out = {}
    for stage in ("1", "2"):
        d = None
        for fn in os.listdir(re1):
            if fn.upper() == "STAGE" + stage:
                d = os.path.join(re1, fn)
        if d is None:
            continue
        for fn in sorted(os.listdir(d)):
            u = fn.upper()
            if u.startswith("ROOM") and u.endswith(PLAYER + ".RDT") and len(u) == len("ROOM1001.RDT"):
                p = os.path.join(d, fn)
                if os.path.getsize(p) > 1024:
                    out[u[4:7]] = p
    return out


def fingerprint(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()[:16]


def language(path):
    """'english', 'japanese' or None (no text) for a room file (text_language)."""
    try:
        return text_language(rdt.load_room(path).raw)
    except Exception:
        return None


def text_language(raw):
    """'english', 'japanese' or None (no text) from a room's messages, read to each one's end
    (01): the English font's text glyphs are 12-86 (digits, letters, punctuation) and 120-123;
    the Japanese text is mostly kana and kanji, glyphs past those (and 0xF8-0xFA wide ones)."""
    mp = struct.unpack_from("<I", raw, 0x74)[0]
    if not mp or mp + 2 > len(raw):
        return None
    first = struct.unpack_from("<H", raw, mp)[0]
    latin = other = 0
    for i in range(first // 2):
        p = mp + struct.unpack_from("<H", raw, mp + 2 * i)[0]
        end = min(len(raw), p + 1024)
        while p < end and raw[p] != 1:
            b = raw[p]
            if b in (3, 4, 5, 6, 0xF8, 0xF9, 0xFA):
                other += b >= 0xF8
                p += 2
                continue
            if 12 <= b <= 86 or 120 <= b <= 123:
                latin += 1
            elif 87 <= b < 0xF8:
                other += 1
            p += 1
    if latin + other < 8:
        return None
    return "japanese" if other > latin else "english"


def check(re1):
    """(release name or None, language or None, problems [str]) for an RE1 data folder."""
    files = room_files(re1)
    known = json.load(open(KNOWN)) if os.path.exists(KNOWN) else {}
    if not files:
        return None, None, ["no RE1 rooms found in %s (point it at the folder holding Stage1..Stage7)" % re1]
    votes = {}
    unknown = []
    langs = {}
    for rid, p in files.items():
        fp = fingerprint(p)
        hit = [name for name, rel in known.items() if rel.get(rid) == fp]
        for name in hit:
            votes[name] = votes.get(name, 0) + 1
        if not hit:
            unknown.append(rid)
        lang = language(p)
        if lang:
            langs.setdefault(lang, []).append(rid)
    release = max(votes, key=votes.get) if votes else None
    problems = []
    if unknown:
        problems.append("%d room file(s) don't match any release we know (modded, damaged or another version): %s"
                        % (len(unknown), " ".join(sorted(unknown))))
    lang = max(langs, key=lambda k: len(langs[k])) if langs else None
    if len(langs) > 1:
        odd = [r for k, rs in langs.items() if k != lang for r in rs]
        problems.append("the rooms' text is in more than one language: %s in %s, %s otherwise"
                        % (" ".join(sorted(odd)), [k for k in langs if k != lang][0], lang))
    return release, lang, problems


def main():
    if len(sys.argv) == 4 and sys.argv[1] == "--learn":
        name, re1 = sys.argv[2], sys.argv[3]
        known = json.load(open(KNOWN)) if os.path.exists(KNOWN) else {}
        known[name] = {rid: fingerprint(p) for rid, p in sorted(room_files(re1).items())}
        json.dump(known, open(KNOWN, "w"), indent=1, sort_keys=True)
        print("%s: %d rooms" % (name, len(known[name])))
        return
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    release, lang, problems = check(sys.argv[1])
    print("release: %s, text: %s" % (release or "unknown", lang or "none"))
    for p in problems:
        print("warning: " + p)


if __name__ == "__main__":
    main()
