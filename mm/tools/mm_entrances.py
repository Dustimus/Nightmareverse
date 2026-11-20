"""Where MM's scenes sit in SoH's entrance table, and what MM's own entrance numbers mean.

Shared by soh_tables.py (which writes the table) and mm_scene.py (whose exit lists point into it),
so both always agree. Per scene and start position, four entrances (OoT's child/adult x day/night
setups, all the same spot): first every scene's "arrive" block, from FIRST_ENTRANCE, then the
same again as "back" blocks, used by exits into places that aren't built yet (Link is put back
at that spot, and z_nv_mm.c says the way isn't built). The ranges' ends go to
nv_mm_scene_range.h.
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mm_rom  # noqa: E402
import yaz0  # noqa: E402
from mm2torch import commands  # noqa: E402

FIRST_SCENE = 0x10E  # after RE1's SCENE_RE1_71C
FIRST_ENTRANCE = 0xF64  # after RE1's ENTR_RE1_71C_3_3

# MM's entrance numbers: (scene << 9) | (spawn << 4) | layer, the scene being MM's entrance-scene
# index (the decomp's ENTR_SCENE_*), not its scene file. The ones we have:
MM_ENTRANCE_SCENES = {
    0x6C: "Z2_CLOCKTOWER",   # ENTR_SCENE_SOUTH_CLOCK_TOWN
    0x6B: "Z2_BACKTOWN",     # ENTR_SCENE_NORTH_CLOCK_TOWN
    0x69: "Z2_TOWN",         # ENTR_SCENE_EAST_CLOCK_TOWN
    0x6A: "Z2_ICHIBA",       # ENTR_SCENE_WEST_CLOCK_TOWN
    0x16: "Z2_OKUJOU",       # ENTR_SCENE_CLOCK_TOWER_ROOFTOP
    0x60: "Z2_INSIDETOWER",  # ENTR_SCENE_CLOCK_TOWER_INTERIOR
}


def soh_name(scene):
    """Z2_CLOCKTOWER -> mm_z2_clocktower (our scene file names)."""
    return "mm_" + scene.lower()


def spawn_count(rom, scene):
    _, _, ps, pe = rom.entry(scene)
    data = yaz0.decompress(rom.data[ps:pe])
    for cmd, n, _ in commands(data, 0):
        if cmd == 0x00:
            return n
    return 1


class Layout:
    def __init__(self, rom):
        self.scenes = list(mm_rom.CLOCK_TOWN)
        self.spawns = {s: spawn_count(rom, s) for s in self.scenes}
        self.arrive = {}
        self.back = {}
        e = FIRST_ENTRANCE
        for s in self.scenes:
            for sp in range(self.spawns[s]):
                self.arrive[(s, sp)] = e
                e += 4
        self.back_first = e
        for s in self.scenes:
            for sp in range(self.spawns[s]):
                self.back[(s, sp)] = e
                e += 4
        self.end = e  # one past the last

    def mm_entrance(self, v):
        """Our entrance for one of MM's entrance numbers, or None if its place isn't built."""
        scene = MM_ENTRANCE_SCENES.get(v >> 9)
        spawn = (v >> 4) & 0x1F
        if scene is None or spawn >= self.spawns[scene]:
            return None
        return self.arrive[(scene, spawn)]
