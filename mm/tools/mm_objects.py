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
    #          (flex, standard limbs; their limbs are found from them), "skel_normal": the same for a
    #          plain (not flex) skeleton, "anim": {symbol: offset},
    #          "tex": {symbol: (offset, format, width, height[, tlut offset])}, "vtx": {symbol: (offset, count)}})
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
        "gCircleShadowDL": 0x76BC0,
        "gEffFire1DL": 0x7D590,  # a torch's flame (Obj_Syokudai)
        "gDoorLeftDL": 0x20BB8,  # the plain wooden door (En_Door)
        "gDoorRightDL": 0x20D00,
    }, "skel_normal": {"gDoorSkel": 0x22B28},
        "anim": {"gDoorHumanOpenLeftAnim": 0x204B4, "gDoorHumanOpenRightAnim": 0x205A0}}),
    "gameplay_field_keep": (650, {"segment": 5, "dl": {  # MM's field keep (segment 5, as its dungeon keep): its doors (En_Door)
        "gFieldWoodDoorLeftDL": 0x4050,
        "gFieldWoodDoorRightDL": 0x4228,
    }}),
    # the shop doors of Clock Town (En_Door, by its schedule)
    "object_wdor01": (1099, {"dl": {"gLotteryCuriosityShopMayorHouseDoorDL": 0x548}}),
    "object_wdor02": (1100, {"dl": {"gPostOfficeTradingPostDoorDL": 0x548}}),
    "object_wdor03": (1101, {"dl": {"gInnSchoolDoorDL": 0x548}}),
    "object_wdor04": (1102, {"dl": {"gMilkBarDoorDL": 0x508}}),
    "object_wdor05": (1103, {"dl": {"gMusicBoxHouseDoorDL": 0x508}}),
    "object_tsubo": (786, {"dl": {  # the pot, outdoors (Obj_Tsubo)
        "gPotDL": 0x17C0,
        "gPotShardDL": 0x1960,
    }}),
    "object_racetsubo": (900, {"dl": {  # the magic pot: the clock tower's rooftop, the Goron racetrack (Obj_Tsubo)
        "gMagicPotDL": 0x278,
        "gMagicPotShardDL": 0x1610,
    }}),
    "object_syokudai": (714, {"deps": ["gameplay_dangeon_keep"], "dl": {  # the torches (Obj_Syokudai), by type
        "gObjectSyokudaiTypeSwitchCausesFlameDL": 0x3A0,
        "gObjectSyokudaiTypeNoSwitchDL": 0x870,
        "gObjectSyokudaiTypeFlameCausesSwitchDL": 0xB90,
    }}),
    "gameplay_dangeon_keep": (651, {"segment": 5, "dl": {  # MM's dungeon keep: the small crate, indoors (Obj_Kibako)
        "gameplay_dangeon_keep_DL_007890": 0x7890,
        "gameplay_dangeon_keep_DL_007980": 0x7980,
        "gameplay_dangeon_keep_DL_017EA0": 0x17EA0,  # the pot, indoors (Obj_Tsubo)
        "gameplay_dangeon_keep_DL_018090": 0x18090,
    }}),
    "object_mm": (796, {  # the Postman (En_Pm): his eyes set by the actor (segment 8)
        "skel": {"object_mm_Skel_0096E8": 0x96E8},
        "dl": {"object_mm_DL_008348": 0x8348, "object_mm_DL_0083E0": 0x83E0, "object_mm_DL_0085C8": 0x85C8},
        "anim": {f"object_mm_Anim_{o:06X}": o for o in
                 (0x000468, 0x00099C, 0x000FC4, 0x001F84, 0x002238, 0x0099B4, 0x00A4E0, 0x00A8D8, 0x00B09C,
                  0x00BA78, 0x00C32C, 0x00C640)},
        "tex": {"object_mm_Tex_002950": (0x2950, "CI8", 32, 16, 0x2250),
                "object_mm_Tex_002750": (0x2750, "CI8", 32, 16, 0x2250)},
    }),
    "object_pst": (947, {  # the mailboxes (En_Pst)
        "skel": {"gPostboxSkel": 0x1A80},
        "anim": {"gPostboxIdleAnim": 0x18},
    }),
    "object_an1": (768, {  # Anju (En_An): her face set by the actor (segments 8, 9)
        "skel": {"gAnju1Skel": 0x12618},
        "dl": {"gAnju1FoodTrayDL": 0x111E8, "gAnju1ChopsticksDL": 0x12478},
        "anim": {"gAnju1IdleAnim": 0x8B6C, "gAnju1WalkAnim": 0x13048, "gAnju1TasteAnim": 0x1090,
                 "gAnju1TasteEndAnim": 0x144C, "gAnju1LookUpRelievedAnim": 0x1E74, "gAnju1SurprisedAnim": 0x26B4,
                 "gAnju1WaitWithTrayAnim": 0x341C, "gAnju1WalkWithTrayAnim": 0x3EA8, "gAnju1BowAnim": 0x544C,
                 "gAnju1CookAnim": 0x65C8, "gAnju1SitAnim": 0x71E8, "gAnju1SittingInDisbeliefAnim": 0x7E08},
        "tex": {"gAnju1EyeOpenTex": (0xE1E0, "CI8", 32, 32, 0xDD20), "gAnju1EyeHalfTex": (0xEFA0, "CI8", 32, 32, 0xDD20),
                "gAnju1EyeClosedTex": (0xF3A0, "CI8", 32, 32, 0xDD20), "gAnju1EyeSadTex": (0xF9A0, "CI8", 32, 32, 0xDD20),
                "gAnju1EyeComfortTex": (0xFDA0, "CI8", 32, 32, 0xDD20),
                "gAnju1EyeRelievedClosedTex": (0x103A0, "CI8", 32, 32, 0xDD20),
                "gAnju1MouthClosedTex": (0xE6E0, "CI8", 32, 16, 0xDF20),
                "gAnju1MouthHappyTex": (0xF7A0, "CI8", 32, 16, 0xDF20),
                "gAnju1MouthOpenTex": (0x101A0, "CI8", 32, 16, 0xDF20)},
    }),
    "object_an2": (1017, {  # Anju's umbrella, and her walk holding it (En_An)
        "dl": {"gAnju2UmbrellaDL": 0x378},
        "anim": {"gAnju2UmbrellaWalkAnim": 0x42CC, "gAnju2UmbrellaIdleAnim": 0x28DC,
                 "gAnju2UmbrellaSitAnim": 0x38A0, "gAnju2UmbrellaCryAnim": 0x1B80},
    }),
    "object_an3": (1018, {  # Anju's broom, and her sweeping (En_An)
        "dl": {"gAnju3BroomDL": 0x308},
        "anim": {"gAnju3BroomSweepAnim": 0x12C0, "gAnju3BroomIdleAnim": 0x201C, "gAnju3BroomWalkAnim": 0x2A4C},
    }),
    "object_ah": (1030, {  # Anju's mother (En_Ah): her eyes and mouth set by the actor (segments 8, 9)
        "skel": {"object_ah_Skel_009E70": 0x9E70},
        "anim": {f"object_ah_Anim_{o:06X}": o for o in (0x968, 0xDDC, 0x1860, 0x2280)},
        "tex": {f"object_ah_Tex_{o:06X}": (o, "RGBA16", 32, 32) for o in
                (0x6D70, 0x7570, 0x7D70, 0x8570, 0x8D70, 0x9570)},
    }),
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


def _load(rom, index):
    vs, ve, ps, pe = rom.entry_index(index)
    data = yaz0.decompress(rom.data[ps:pe]) if rom.data[ps:ps + 4] == b"Yaz0" else rom.data[ps:ps + ve - vs]
    return data, ps


def convert_objects(rom, out):
    """Every object in OBJECTS, in two passes: first all their display lists are walked (an object
    may draw with another's textures, as MM's torches do with gameplay_dangeon_keep's: "deps"),
    then each one's yml is written with all that was found in it."""
    files = {}
    for name, (index, parts) in OBJECTS.items():
        data, ps = _load(rom, index)
        files[name] = (Assets(parts.get("segment", 6), data), ps)  # (MM's keeps sit in their own: 4, 5)
    extras, others = {}, {}
    for name, (index, parts) in OBJECTS.items():
        a, ps = files[name]
        seg = a.seg
        segs = {seg: a}
        for dep in parts.get("deps", ()):
            segs[files[dep][0].seg] = files[dep][0]
        w = Walker(segs)
        extra = []
        skels = [(sym, off, True) for sym, off in parts.get("skel", {}).items()]
        skels += [(sym, off, False) for sym, off in parts.get("skel_normal", {}).items()]
        for sym, off, flex in skels:
            for lo in _skeleton_limbs(a.data, off):
                dl = struct.unpack_from(">I", a.data, lo + 8)[0]
                if dl:
                    w.walk(dl)
                extra += [f"{name}_Limb_{lo:06X}:", "  type: OOT:LIMB", f"  offset: 0x{lo:06X}",
                          f"  symbol: {name}_Limb_{lo:06X}", "  limb_type: Standard", ""]
            extra += [f"{sym}:", "  type: OOT:SKELETON", f"  offset: 0x{off:06X}", f"  symbol: {sym}",
                      "  limb_type: Standard", f"  skel_type: {'Flex' if flex else 'Normal'}", ""]
        for sym, (off, fmt, tw, th, *tlut) in parts.get("tex", {}).items():  # named: an actor sets them
            extra += [f"{sym}:", "  type: TEXTURE", f"  offset: 0x{off:06X}", f"  symbol: {sym}", f"  format: {fmt}",
                      f"  width: {tw}", f"  height: {th}"] + ([f"  tlut: 0x{tlut[0]:X}"] if tlut else []) + [""]
        for sym, (off, count) in parts.get("vtx", {}).items():  # named: an actor copies or sets them
            extra += [f"{sym}:", "  type: OOT:ARRAY", f"  offset: 0x{off:06X}", f"  symbol: {sym}",
                      f"  count: {count}", "  array_type: VTX", ""]
        for sym, off in parts.get("anim", {}).items():
            extra += [f"{sym}:", "  type: OOT:ANIMATION", f"  offset: 0x{off:06X}", f"  symbol: {sym}", ""]
        for off in parts.get("dl", {}).values():
            w.walk((seg << 24) | off)
        extras[name], others[name] = extra, w.other_segments
    os.makedirs(out, exist_ok=True)
    for name, (index, parts) in OBJECTS.items():
        a, ps = files[name]
        for off, *_ in parts.get("tex", {}).values():
            a.tex.pop(off, None)
        header = f":config:\n  segments:\n    - [ {a.seg}, 0x{ps:X} ]\n"
        for dep in parts.get("deps", ()):
            header += f"    - [ {files[dep][0].seg}, 0x{files[dep][1]:X} ]\n"
        header += f"  directory: mm/objects/{name}\n"
        if parts.get("deps"):
            header += "  external_files:\n" + "".join(f"    - mm_us/objects/{d}.yml\n" for d in parts["deps"])
        names = {off: sym for sym, off in parts.get("dl", {}).items()}
        open(os.path.join(out, f"{name}.yml"), "w").write(
            yml_assets(a, name, header, names) + "\n".join(extras[name]) + "\n")
        print(f"{name}: {len(a.dls)} display lists, {len(a.vtx_arrays())} vertex arrays, {len(a.tex)} textures"
              + (f", {len(parts.get('anim', {}))} animations" if parts.get("anim") else "")
              + (f"; segments not handled: {sorted(others[name])}" if others[name] else ""))
