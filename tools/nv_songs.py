"""Every ocarina song across Nightmareverse's games (OoT, RE1, MM, and any game after them),
checked for clashes.

The ocarina listens to the notes as they're played, so song X clashes with song Y when Y's
notes appear anywhere inside X's in a row: playing X sets off Y partway through. A game adding
a song adds it here first and runs this.

    python tools/nv_songs.py

Notes: A, D (C-Down), R (C-Right), L (C-Left), U (C-Up). The notes are the games' own (OoT's
and MM's songs as the games teach them; RE1's are ours, from z_nv_script.c).
"""

SONGS = {
    # Ocarina of Time
    "OoT Zelda's Lullaby": "LURLUR",
    "OoT Epona's Song": "ULRULR",
    "OoT Saria's Song": "DRLDRL",
    "OoT Sun's Song": "RDURDU",
    "OoT Song of Time": "RADRAD",
    "OoT Song of Storms": "ADUADU",
    "OoT Minuet of Forest": "AULRLR",
    "OoT Bolero of Fire": "DADARDRD",
    "OoT Serenade of Water": "ADRRL",
    "OoT Nocturne of Shadow": "LRRALRD",
    "OoT Requiem of Spirit": "ADARDA",
    "OoT Prelude of Light": "URURLU",
    # Resident Evil (Nightmareverse's own, z_nv_script.c)
    "RE1 mansion warp song": "DRARLALU",
    "RE1 Moonlight Sonata": "ADRADR",
    # Majora's Mask (the songs OoT doesn't already have)
    "MM Song of Healing": "LRDLRD",
    "MM Song of Soaring": "DLUDLU",
    "MM Sonata of Awakening": "ULULARA",
    "MM Goron Lullaby": "ARLARLRA",
    "MM New Wave Bossa Nova": "LULRDLR",
    "MM Elegy of Emptiness": "RLRDRUL",
    "MM Oath to Order": "RDADRU",
    "MM Inverted Song of Time": "DARDAR",
    "MM Song of Double Time": "RRAADD",
}


def clashes(songs):
    out = []
    names = list(songs)
    for a in names:
        for b in names:
            if a == b:
                continue
            if songs[b] == songs[a]:
                if a < b:
                    out.append(f"{a} and {b} are the same notes")
            elif songs[b] in songs[a]:
                out.append(f"playing {a} sets off {b} partway through")
    return out


def near_misses(songs):
    """The Song of Time is played constantly in Termina; list songs that one wrong or extra
    note away from it could set off (informational, not a clash)."""
    sot = songs["OoT Song of Time"]
    hits = []
    for name, notes in songs.items():
        if name == "OoT Song of Time":
            continue
        for extra in "ADRLU":
            if notes in sot + extra or notes in extra + sot or notes in sot + sot:
                hits.append(name)
                break
    return hits


if __name__ == "__main__":
    c = clashes(SONGS)
    print("\n".join(c) if c else f"{len(SONGS)} songs, no clashes")
    n = near_misses(SONGS)
    if n:
        print("would be set off if notes carried over after the Song of Time (they don't: every ocarina use and "
              "every recognised song starts clean, NvSong_Reset): " + ", ".join(n))
