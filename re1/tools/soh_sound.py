#!/usr/bin/env python3
"""RE1's sounds for soh/soh/Enhancements/Nightmareverse/NvSound.cpp, from the player's own files.

Every WAV in SOUND (effects and music: RE1 plays them by name, PlayerAnimations / SoundSystem's
tables) and VOICE (the cutscenes' lines), copied unchanged: "re1/sound/<name>" and
"re1/voice/<name>", the name lower case without ".wav".
"""
import os
import struct

import rdt
import soh_scene


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
