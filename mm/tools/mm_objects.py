"""MM's object files (models, textures) through Torch, for the actors ported from MM.

An object's entry points are its display lists, at fixed offsets in the file (named as the MM
decomp names them, which is what the ported actors draw). From them, mm2torch.Walker finds the
vertices, textures and palettes, as for the rooms. Torch exports everything under
mm/objects/<object>/<symbol>. SoH draws a display list straight from its path, so an actor can
draw MM's models without loading the object into a slot.

The table below holds only names and offsets (no data); the files come from the player's ROM.
"""

import os
import struct

import yaz0
from mm2torch import Assets, Walker, yml_assets

OBJECTS = {
    # object: (its index in the US 1.0 file table, {"dl": {symbol: offset}, "skel": {symbol: offset}
    #          (flex or plain, standard limbs; their limbs are found from them), "anim": {symbol: offset},
    #          "tex": {symbol: (offset, format, width, height)}, "vtx": {symbol: (offset, count)}})
    # (an actor's own data, in its overlay file, comes the same way: segment 6 is just where the file goes)
    "object_obj_tokeidai": (886, {"dl": {  # the clock tower and the wall clocks (Obj_Tokeidai)
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
    }}),
    "object_lodmoon": (1020, {"dl": {  # the far, low-detail moon (En_Fall)
        "gLodmoonEyesDL": 0x10E0,
        "gLodmoonMoonDL": 0x1158,
    }}),
    "object_fall": (882, {"dl": {  # the full moon, and the crash's pieces (En_Fall)
        "gMoonDL": 0x77F0,
    }}),
    "object_cs": (798, {  # the Bombers (En_Bombers): eyes and numbers set by the actor (segments 8, 9)
        "skel": {"gBomberSkel": 0xF82C},
        "anim": {"gBomberIdleAnim": 0x64B8, "gBomberTalkHandsOnHipsAnim": 0x53F4, "gBomberWalkAnim": 0x1007C,
                 "gBomberSurpriseAnim": 0x349C, "gBomberHideUnderBoxAnim": 0x4960, "gBomberDazedAnim": 0x5128,
                 "gBomberSitAnim": 0x4C1C, "gBomberHoldCuccoAnim": 0x1A1C, "gBomberTurnAroundAnim": 0x3EE4,
                 "gBomberShowNumberAnim": 0x478C, "gBomberTalkShowingNumberAnim": 0x433C,
                 "gBomberTauntAnim": 0x60E8, "gBomberJumpAnim": 0x1708, "gBomberArmSwipeAnim": 0x5DC4,
                 "gBomberSaluteAnim": 0x26B0, "gBomberRunAnim": 0x36B0, "gBomberLookAroundAnim": 0x31C4},
        "tex": {"gBomberEyeOpenTex": (0xC520, "RGBA16", 32, 32), "gBomberEyeHalfTex": (0xCD20, "RGBA16", 32, 32),
                "gBomberEyeClosedTex": (0xD520, "RGBA16", 32, 32),
                "gBomberNumber1Tex": (0xE620, "I8", 32, 32), "gBomberNumber2Tex": (0xEA20, "I8", 32, 32),
                "gBomberNumber3Tex": (0xEE20, "I8", 32, 32), "gBomberNumber4Tex": (0xDD20, "I8", 32, 32),
                "gBomberNumber5Tex": (0xF220, "I8", 32, 32)},
    }),
    "object_sdn": (926, {  # the soldiers (En_Stop_heishi, and others)
        "skel": {"gSoldierSkel": 0xD640},
        "anim": {"gSoldierLookDownAnim": 0x4AC0, "gSoldierComeUpHereAnim": 0x5320,
                 "gSoldierStandHandOnHipAnim": 0x6C18, "gSoldierStandAndLookDownAnim": 0xDC7C,
                 "object_sdn_Anim_0057BC": 0x57BC, "object_sdn_Anim_005D28": 0x5D28,
                 "object_sdn_Anim_0064C0": 0x64C0, "gSoldierStandWithHandOnChestAnim": 0xA54},
    }),
    "object_f53_obj": (861, {"dl": {  # Clock Town's chimneys and their smoke (Obj_Entotu)
        "object_f53_obj_DL_000158": 0x158,
        "object_f53_obj_DL_001C00": 0x1C00,  # the smoke: its vertices are the actor's (segment 9)
    }}),
    "ovl_Obj_Entotu": (299, {  # the chimney actor's own data: the smoke's seven vertices
        "vtx": {"ovl_Obj_Entotu_Vtx_000D10": (0xD10, 7)},
    }),
    "object_kibako": (862, {"dl": {  # the small crate, outdoors (Obj_Kibako)
        "gSmallCrateDL": 0x1180,
        "gSmallCrateFragmentDL": 0x1A70,
    }}),
    "gameplay_keep": (649, {"segment": 4, "dl": {  # MM's main keep: what the ported actors share (shadows, ...)
        "gSquareShadowDL": 0x75A40,
    }}),
    "gameplay_dangeon_keep": (651, {"segment": 5, "dl": {  # MM's dungeon keep: the small crate, indoors (Obj_Kibako)
        "gameplay_dangeon_keep_DL_007890": 0x7890,
        "gameplay_dangeon_keep_DL_007980": 0x7980,
    }}),
    "object_daiku": (779, {  # the carpenters (En_Daiku)
        "skel": {"object_daiku_Skel_00A850": 0xA850},
        "dl": {"object_daiku_DL_0070C0": 0x70C0, "object_daiku_DL_006FB0": 0x6FB0,  # their hats (by type)
               "object_daiku_DL_006E80": 0x6E80, "object_daiku_DL_006D70": 0x6D70,
               "object_daiku_DL_008EC8": 0x8EC8},  # the plank one of them carries
        "anim": {f"object_daiku_Anim_{o:06X}": o for o in
                 (0x000600, 0x000C44, 0x001114, 0x001A24, 0x002134, 0x002FA0, 0x00ACD0, 0x00B690,
                  0x00BEAC, 0x00C234, 0x00C92C, 0x00D328)},
    }),
}


def _skeleton_limbs(data, off):
    """A skeleton's limbs (segment 6 offsets), from its limb table (Standard limbs)."""
    table = struct.unpack_from(">I", data, off)[0] & 0xFFFFFF
    count = data[off + 4]
    return [struct.unpack_from(">I", data, table + 4 * i)[0] & 0xFFFFFF for i in range(count)]


def convert_object(rom, name, out):
    index, parts = OBJECTS[name]
    vs, ve, ps, pe = rom.entry_index(index)
    data = yaz0.decompress(rom.data[ps:pe]) if rom.data[ps:ps + 4] == b"Yaz0" else rom.data[ps:ps + ve - vs]
    seg = parts.get("segment", 6)  # (MM's keeps are loaded in their own segments: 4, 5)
    a = Assets(seg, data)
    w = Walker({seg: a})
    dls = dict(parts.get("dl", {}))
    extra = []
    for sym, off in parts.get("skel", {}).items():
        flex = True  # (MM's skeletons here are flex; a plain one would read the same limbs)
        limbs = _skeleton_limbs(data, off)
        for lo in limbs:
            dl = struct.unpack_from(">I", data, lo + 8)[0]
            if dl:
                w.walk(dl)
            extra += [f"{name}_Limb_{lo:06X}:", "  type: OOT:LIMB", f"  offset: 0x{lo:06X}",
                      f"  symbol: {name}_Limb_{lo:06X}", "  limb_type: Standard", ""]
        extra += [f"{sym}:", "  type: OOT:SKELETON", f"  offset: 0x{off:06X}", f"  symbol: {sym}",
                  "  limb_type: Standard", f"  skel_type: {'Flex' if flex else 'Normal'}", ""]
    for sym, (off, fmt, tw, th) in parts.get("tex", {}).items():  # named: an actor sets them in a segment
        extra += [f"{sym}:", "  type: TEXTURE", f"  offset: 0x{off:06X}", f"  symbol: {sym}", f"  format: {fmt}",
                  f"  width: {tw}", f"  height: {th}", ""]
        a.tex.pop(off, None)
    for sym, (off, count) in parts.get("vtx", {}).items():  # named: an actor copies or sets them
        extra += [f"{sym}:", "  type: OOT:ARRAY", f"  offset: 0x{off:06X}", f"  symbol: {sym}", f"  count: {count}",
                  "  array_type: VTX", ""]
    for sym, off in parts.get("anim", {}).items():
        extra += [f"{sym}:", "  type: OOT:ANIMATION", f"  offset: 0x{off:06X}", f"  symbol: {sym}", ""]
    for off in dls.values():
        w.walk((seg << 24) | off)
    header = f":config:\n  segments:\n    - [ {seg}, 0x{ps:X} ]\n  directory: mm/objects/{name}\n"
    names = {off: sym for sym, off in dls.items()}
    os.makedirs(out, exist_ok=True)
    open(os.path.join(out, f"{name}.yml"), "w").write(yml_assets(a, name, header, names) + "\n".join(extra) + "\n")
    print(f"{name}: {len(a.dls)} display lists, {len(a.vtx_arrays())} vertex arrays, {len(a.tex)} textures"
          + (f", {len(parts.get('anim', {}))} animations" if parts.get("anim") else "")
          + (f"; segments not handled: {sorted(w.other_segments)}" if w.other_segments else ""))
