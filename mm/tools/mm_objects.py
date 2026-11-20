"""MM's object files (models, textures) through Torch, for the actors ported from MM.

An object's entry points are its display lists, at fixed offsets in the file (named as the MM
decomp names them, which is what the ported actors draw). From them, mm2torch.Walker finds the
vertices, textures and palettes, as for the rooms. Torch exports everything under
mm/objects/<object>/<symbol>. SoH draws a display list straight from its path, so an actor can
draw MM's models without loading the object into a slot.

The table below holds only names and offsets (no data); the files come from the player's ROM.
"""

import os

import yaz0
from mm2torch import Assets, Walker, yml_assets

OBJECTS = {
    # object: (its index in the US 1.0 file table, {display list symbol: offset})
    "object_obj_tokeidai": (886, {  # the clock tower and the wall clocks (Obj_Tokeidai)
        "gClockTowerTerminaFieldWallsDL": 0x9A08,
        "gClockTowerSpotlightDL": 0xB0C0,
        "gClockTowerCounterweightDL": 0xB208,
        "gClockTowerExteriorGearDL": 0xBA78,
        "gClockTowerClockCenterAndHandDL": 0xBEE8,
        "gClockTowerSunAndMoonPanelDL": 0xC368,
        "gClockTowerMinuteRingDL": 0xCF28,
        "gClockTowerUnusedTowerWallDL": 0xD388,
        "gClockTowerEmptyDL": 0xD8E0,
        "gClockTowerStaircaseToRooftopDL": 0xD8E8,
        "gClockTowerClockFaceDL": 0xE818,
        "gWallClockClockFaceDL": 0xF518,
    }),
}


def convert_object(rom, name, out):
    index, dls = OBJECTS[name]
    vs, ve, ps, pe = rom.entry_index(index)
    data = yaz0.decompress(rom.data[ps:pe]) if rom.data[ps:ps + 4] == b"Yaz0" else rom.data[ps:ps + ve - vs]
    a = Assets(6, data)
    w = Walker({6: a})
    for off in dls.values():
        w.walk(0x06000000 | off)
    header = f":config:\n  segments:\n    - [ 6, 0x{ps:X} ]\n  directory: mm/objects/{name}\n"
    names = {off: sym for sym, off in dls.items()}
    os.makedirs(out, exist_ok=True)
    open(os.path.join(out, f"{name}.yml"), "w").write(yml_assets(a, name, header, names))
    print(f"{name}: {len(a.dls)} display lists, {len(a.vtx_arrays())} vertex arrays, {len(a.tex)} textures"
          + (f"; segments not handled: {sorted(w.other_segments)}" if w.other_segments else ""))
