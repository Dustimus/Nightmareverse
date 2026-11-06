#!/usr/bin/env python3
"""RE1's sounds for soh/soh/Enhancements/Nightmareverse/NvSound.cpp, from the player's own files.

Every WAV in SOUND (effects and music: RE1 plays them by name, PlayerAnimations / SoundSystem's
tables) and VOICE (the cutscenes' lines), copied unchanged: "re1/sound/<name>" and
"re1/voice/<name>", the name lower case without ".wav".

With the game's executable, also its voice name table (voice_load_and_play's: per stage, the
9-byte names of the lines cmd_voice_play plays by number), as "re1/voice_names": "RVNM", u16
stages, then per stage u16 count and the names. It's Capcom's data, read from the player's own
.exe (found by its shape: the eight stage pointers, the 6th and 7th repeating the 1st and 2nd).
"""
import os
import struct

import rdt
import soh_scene


def _pe_sections(d):
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    opt = struct.unpack_from("<H", d, pe + 20)[0]
    base = struct.unpack_from("<I", d, pe + 24 + 28)[0]
    secs = []
    so = pe + 24 + opt
    for _ in range(nsec):
        vs, va, rs, ra = struct.unpack_from("<4I", d, so + 8)
        secs.append((base + va, max(vs, rs), ra, rs))
        so += 40
    return secs


def voice_names(exe_path):
    """[[name, ...] per stage (0-4)] from an RE1 PC executable, or None."""
    if not exe_path:
        return None
    d = open(exe_path, "rb").read()
    if d[:2] != b"MZ":
        return None
    secs = _pe_sections(d)

    def off(v):
        for va, size, ra, rs in secs:
            if va <= v < va + size and v - va < rs:
                return ra + (v - va)
        return None

    for o in range(0, len(d) - 32, 4):
        p = struct.unpack_from("<8I", d, o)
        if p[7] or p[5] != p[0] or p[6] != p[1] or len(set(p[:5])) != 5:
            continue
        offs = [off(v) for v in p[:5]]
        if None in offs or any(d[x:x + 1] != b"V" for x in offs):
            continue
        table_va = None
        for va, size, ra, rs in secs:
            if ra <= o < ra + rs:
                table_va = va + (o - ra)
        ends = sorted(p[:5]) + [table_va]
        rows = []
        for v in p[:5]:
            end = ends[ends.index(v) + 1]
            n = (end - v) // 9
            x = off(v)
            names = [d[x + 9 * k:x + 9 * k + 9].split(b"\0")[0].decode("latin-1") for k in range(n)]
            rows.append([nm if nm[:1] == "V" else "" for nm in names])  # (an empty record: no line)
        return rows
    return None


def write_voice_names(exe_path, out):
    """re1/voice_names from the executable; the number of names (0: no executable / not found)."""
    rows = voice_names(exe_path)
    if not rows:
        return 0
    blob = bytearray(b"RVNM" + struct.pack("<H", len(rows)))
    for r in rows:
        blob += struct.pack("<H", len(r))
        for name in r:
            blob += name.encode("latin-1")[:8].ljust(9, b"\0")
    os.makedirs(os.path.join(out, "re1"), exist_ok=True)
    open(os.path.join(out, "re1", "voice_names"), "wb").write(
        soh_scene._otr_header(soh_scene.LUS_BLOB) + struct.pack("<I", len(blob)) + bytes(blob))
    return sum(len(r) for r in rows)


def write(re1, out):
    """Copy the WAVs; returns (sounds, voices) written."""
    counts = []
    for folder, sub in (("SOUND", "sound"), ("VOICE", "voice")):
        n = 0
        try:
            src = rdt.find_file(re1, folder)
            names = sorted(os.listdir(src))
        except OSError:
            counts.append(0)
            continue
        d = os.path.join(out, "re1", sub)
        os.makedirs(d, exist_ok=True)
        for fn in names:
            if not fn.lower().endswith(".wav"):
                continue
            data = open(os.path.join(src, fn), "rb").read()
            if data[:4] != b"RIFF":
                continue
            open(os.path.join(d, fn[:-4].lower()), "wb").write(
                soh_scene._otr_header(soh_scene.LUS_BLOB) + struct.pack("<I", len(data)) + data)
            n += 1
        counts.append(n)
    return tuple(counts)
