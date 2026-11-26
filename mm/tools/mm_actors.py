"""MM's placed actors as OoT's: the ones OoT already has a version of (group 1, mm/README.md).

MM's room actor lists and scene door lists hold MM's actor ids and MM's params. Where OoT has the
same thing (pots, crates, torches, collectibles, doors), the converter writes OoT's actor with the
params translated, and the OoT object it needs in the room's object list. Anything else is left
out until it's ported (counted, so it's clear what's missing).

MM's own rules followed here (zeldaret/mm, z_actor.c Actor_SpawnEntry):
- an entry's id carries three flags (0x8000, 0x4000, 0x2000 for Y, X, Z): set, the rotation's
  top 9 bits are used as a raw value; clear, they're degrees;
- its rotations' low bits are the half-days it appears in (rot.x & 7 and rot.z & 0x7F, ten bits,
  day 0 night to day 4 night); 0 means always. MM spawns an entry only in its half-days: each
  entry's mask goes to the room's "_halfdays" resource, which z_nv_mm_halfday.c follows.
"""

import struct

HALFDAYBIT_ALL = 0x3FF

# OoT's ids (soh/include/tables/actor_table.h, object_table.h)
ACTOR_EN_DOOR, ACTOR_EN_ITEM00, ACTOR_OBJ_SYOKUDAI, ACTOR_OBJ_KIBAKO, ACTOR_OBJ_TSUBO = 0x09, 0x15, 0x5E, 0x110, 0x111
OBJECT_GAMEPLAY_DANGEON_KEEP, OBJECT_SYOKUDAI, OBJECT_TSUBO = 0x03, 0xA4, 0x12C
DOOR_SCENEEXIT = 3

# Actors ported from MM (soh/src/code/z_nv_mm_*.c, registered in NvMmActors.cpp): the room
# names them 0x7000 + MM's id, MM's params as they are; z_nv_mm_halfday.c spawns them.
NV_MM_ACTOR_MARKER = 0x7000
PORTED = {
    0x26F,  # En_Dt: Mayor Dotour (z_nv_mm_mayor.c)
    0x26C,  # En_Baisen: Viscen (z_nv_mm_mayor.c)
    0x26D,  # En_Heishi: the soldier at the meeting (z_nv_mm_mayor.c)
    0x1EF,  # En_Kendo_Js: the swordsman (z_nv_mm_kendo.c)
    0x1F8,  # En_Maruta: his practice logs (z_nv_mm_kendo.c; spawned by him)
    0x1C1,  # En_Takaraya: the Treasure Chest Shop gal (z_nv_mm_takaraya.c)
    0x1BB,  # Obj_Takaraya_Wall: her board (z_nv_mm_takaraya.c; spawned by her)
    0x290,  # En_Recepgirl: the Mayor's receptionist (z_nv_mm_recep.c)
    0x0FE,  # En_Mm2: the Postman's letter (z_nv_mm_recep.c)
    0x19C,  # Obj_Tokeidai: the clock tower (z_nv_mm_tokeidai.c)
    0x17C,  # En_Fall: the moon (z_nv_mm_fall.c)
    0x09C,  # En_Daiku: the carpenters (z_nv_mm_daiku.c)
    0x1C7,  # En_Stop_heishi: the gate guards (z_nv_mm_stopheishi.c)
    0x280,  # En_Bombers: the Bombers (z_nv_mm_bombers.c)
    0x14D,  # Obj_Entotu: the chimneys (z_nv_mm_entotu.c)
    0x081,  # Obj_Kibako: the small crates, MM's own (z_nv_mm_kibako.c)
    0x039,  # Obj_Syokudai: the torches, MM's own (z_nv_mm_syokudai.c)
    0x082,  # Obj_Tsubo: the pots, MM's own (z_nv_mm_tsubo.c)
    0x1D5,  # En_Pm: the Postman (z_nv_mm_pm.c)
    0x1F2,  # En_Pst: the mailboxes (z_nv_mm_pst.c)
    0x202,  # En_An: Anju (z_nv_mm_an.c)
    0x253,  # En_Ah: Anju's mother (z_nv_mm_ah.c)
    0x243,  # En_Nb: Anju's grandmother (z_nv_mm_nb.c)
    0x276,  # En_Ig: the Goron at the inn (z_nv_mm_ig.c)
    0x004,  # En_Light: flames and lamps (z_nv_mm_light.c)
    0x294,  # Obj_Yado: the inn's upstairs windows (z_nv_mm_yado.c)
    0x20A,  # Obj_Hariko: the bobblehead cow toy (z_nv_mm_hariko.c)
    0x183,  # Obj_Etcetera: the Deku flowers (z_nv_mm_etcetera.c)
    0x261,  # En_Talk: things to read or check (z_nv_mm_talk.c)
    0x221,  # Obj_Tokei_Turret: the carnival platforms and flags (z_nv_mm_scenery.c)
    0x229,  # Obj_Tree: the branching tree (z_nv_mm_scenery.c)
    0x1CC,  # Bg_Market_Step: West Clock Town's market stalls (z_nv_mm_scenery.c)
    0x23B,  # Obj_Kinoko: the magic mushrooms (z_nv_mm_scenery.c)
    0x090,  # En_Kusa: grass and bushes, MM's own (z_nv_mm_kusa.c)
    0x0A8,  # En_Kanban: the square signposts, MM's own (z_nv_mm_kanban.c)
    0x223,  # Obj_Warpstone: the owl statues (z_nv_mm_warpstone.c)
    0x265,  # En_Hit_Tag: the hidden rupees (z_nv_mm_scenery.c)
    0x1B6,  # Bg_Ctower_Gear: the clock tower's cogs and water wheel (z_nv_mm_ctower.c)
    0x061,  # Bg_Ctower_Rot: the twisting corridor and its stone doors (z_nv_mm_ctower.c)
    0x1A2,  # Obj_Tokei_Tobira: the clock tower's swinging doors (z_nv_mm_ctower.c)
    0x1B8,  # Obj_Tokei_Step: the tower's front, the last night's steps (z_nv_mm_ctower.c)
    0x248,  # En_Guruguru: Guru-Guru (z_nv_mm_guruguru.c)
    0x27B,  # En_Rz: the Rosa sisters (z_nv_mm_rz.c)
    0x244,  # En_Ja: the jugglers (z_nv_mm_ja.c)
    0x270,  # En_Cha: the Laundry Pool's bell (z_nv_mm_townprops.c)
    0x14E,  # Obj_Bell: East Clock Town's big bell (z_nv_mm_townprops.c)
    0x2A1,  # En_Drs: the wedding dress's mannequin (z_nv_mm_townprops.c)
    0x26B,  # En_Muto: Mutoh (z_nv_mm_townprops.c)
    0x281,  # En_Bombers2: the Bombers' hideout guard (z_nv_mm_bombers.c)
    0x177,  # En_Ginko_Man: the bank teller (z_nv_mm_bank.c)
    0x0E2,  # En_Dg: the dogs (z_nv_mm_dg.c)
    0x282,  # En_Bombal: the Majora balloon (z_nv_mm_bomgame.c)
    0x27E,  # En_Bomjima: Jim (z_nv_mm_bomgame.c)
    0x27F,  # En_Bomjimb: the Bombers hiding, the game (z_nv_mm_bomgame.c)
    0x11C,  # En_Bom_Bowl_Man: the Bombers lined up, the notebook (z_nv_mm_bomgame.c)
    0x236,  # En_Baba: the Bomb Shop lady (z_nv_mm_sakon.c)
    0x237,  # En_Suttari: Sakon, the first night (z_nv_mm_sakon.c)
    0x239,  # En_Kujiya: the Lottery Shop (z_nv_mm_kujiya.c)
}

# MM's ids (the decomp's actor table)
MM_EN_DOOR, MM_EN_ITEM00, MM_OBJ_SYOKUDAI, MM_OBJ_KIBAKO, MM_OBJ_TSUBO = 0x005, 0x00E, 0x039, 0x081, 0x082
MM_EN_BOX = 0x006
ACTOR_EN_BOX, OBJECT_BOX = 0x000A, 0x000E  # OoT's

# MM's collectible types (ITEM00_*) as OoT's: the same numbers up to 0x14, then MM's own
ITEM00_OOT = {i: i for i in range(0x10)}
ITEM00_OOT.update({0x11: 0x11, 0x12: 0x12, 0x13: 0x13, 0x14: 0x14, 0x15: 0x03, 0x16: 0x16, 0x17: 0x0C})
ITEM00_HEART_PIECE, ITEM00_HEART_CONTAINER = 0x06, 0x07


def item00(params):
    """MM's En_Item00 as OoT's, or None. Its collectible flag is kept in Termina's save
    (z_nv_mm_scene.c, the permanent part), so a heart piece taken stays taken. OoT saves flags
    0x00-0x1F only (0x20-0x3F are per visit) and has 6 bits where MM has 7: one past 0x1F is
    left out rather than reappear on every visit."""
    t = params & 0xFF
    flag = (params >> 8) & 0x7F
    if t not in ITEM00_OOT or (t in (ITEM00_HEART_PIECE, ITEM00_HEART_CONTAINER) and flag > 0x1F):
        return None
    return ITEM00_OOT[t] | ((flag & 0x3F) << 8)


def _drop(t):
    """A pot's or crate's drop: MM's collectible type as OoT's (none if OoT has no such)."""
    return ITEM00_OOT.get(t & 0x1F, 0x1F) & 0x1F


MAP = {
    # actor: (OoT actor, params from MM's, OoT objects it needs)
    MM_EN_ITEM00: (ACTOR_EN_ITEM00, item00, ()),
    # MM's chests: OoT's En_Box (Link's chest opening is its), MM's types numbered as OoT's; the
    # get-item field stays MM's get-item (in Termina En_Box gives it as MM's: z_nv_mm_getitem.c)
    MM_EN_BOX: (ACTOR_EN_BOX, lambda q: q, (OBJECT_BOX,)),
}


def _rot(raw, flag):
    v = ((raw & 0xFFFF) >> 7) & 0x1FF
    if flag:
        return v - 360 if v > 180 else v
    return (v * 0x10000 // 360) & 0xFFFF


def actors(data, off, count):
    """A room's actor list (0x01) as OoT entries: [(id, params, x, y, z, rx, ry, rz, halfdays)],
    the OoT objects they need, and what was left out ({MM actor id: count})."""
    out, objects, skipped = [], [], {}
    for i in range(count):
        aid_raw, x, y, z, rx, ry, rz, params = struct.unpack_from(">H3h3hH", data, off + 16 * i)
        aid = aid_raw & 0x1FFF
        halfdays = ((rx & 7) << 7) | (rz & 0x7F)
        if aid in PORTED:
            m = (NV_MM_ACTOR_MARKER + aid, lambda q: q, ())
        else:
            m = MAP.get(aid)
        p = m[1](params) if m else None
        if m is None or p is None:
            skipped[aid] = skipped.get(aid, 0) + 1
            continue
        oy = _rot(ry, aid_raw & 0x8000)
        if aid == MM_EN_BOX and not (aid_raw & 0x8000):
            # MM's En_Box turns its shape half round and opens from the side its data faces; OoT's turns
            # only its world rotation, its shape (model and opening side) facing the data: half round here
            oy = (oy + 0x8000) & 0xFFFF
        out.append((m[0], p, x, y, z, _rot(rx, aid_raw & 0x4000), oy, _rot(rz, aid_raw & 0x2000), halfdays))
        for o in m[2]:
            if o not in objects:
                objects.append(o)
    return out, objects, skipped


# MM's En_Holl (loading halls) as OoT's: MM's type ((params >> 7) & 7) to OoT's ((params >> 6) & 7).
# MM's 0 (a visible hall, 150 wide each side) has no OoT match (OoT's visible hall is 100): OoT's wide
# invisible one (4, 200); MM's 2 (transparent hall) the same; MM's 1 (a hole) OoT's invisible hole (2);
# MM's 3 (a hole covering the room) OoT's (5); MM's 4 (to another scene) none yet.
MM_EN_HOLL, ACTOR_EN_HOLL = 0x018, 0x0023
HOLL_TYPES = {0: 4, 2: 4, 1: 2, 3: 5}

NV_MM_TRANSITION_MARKER = 0x1000  # (a transition actor's id keeps 13 bits: 0x1000 + MM's id)


def doors(data, off, count):
    """The scene's transition actors (0x0E): MM's wooden doors, ported (z_nv_mm_door.c), MM's
    params as they are; NvMmActor_TransitionId (NvMmActors.cpp) spawns them. Their way out is the
    exit polygon behind them (the scene's exit list), as in both games; a door into a place not
    built yet brings Link back with the note (z_nv_mm.c)."""
    out, skipped = [], {}
    for i in range(count):
        fr, fc, br, bc, aid, x, y, z, ry, params = struct.unpack_from(">4bh3hhH", data, off + 16 * i)
        aid &= 0x1FFF
        if aid == MM_EN_HOLL:  # a loading hall between rooms: OoT's En_Holl, the nearest type (holl_type)
            t = HOLL_TYPES.get((params >> 7) & 7)
            if t is None:
                skipped[aid] = skipped.get(aid, 0) + 1
                continue
            out.append(dict(Id=ACTOR_EN_HOLL, Params=(t << 6) | (params & 0x3F), PosX=x, PosY=y, PosZ=z,
                            RotY=_rot(ry, False), FrontSideRoom=fr, FrontSideEffects=fc & 0xFF,
                            BackSideRoom=br, BackSideEffects=bc & 0xFF))
            continue
        if aid != MM_EN_DOOR:
            skipped[aid] = skipped.get(aid, 0) + 1
            continue
        out.append(dict(Id=NV_MM_TRANSITION_MARKER + MM_EN_DOOR, Params=params & 0x3FF, PosX=x, PosY=y, PosZ=z,
                        RotY=_rot(ry, False), FrontSideRoom=fr, FrontSideEffects=fc & 0xFF,
                        BackSideRoom=br, BackSideEffects=bc & 0xFF))
    return out, skipped
