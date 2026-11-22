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
}

# MM's ids (the decomp's actor table)
MM_EN_DOOR, MM_EN_ITEM00, MM_OBJ_SYOKUDAI, MM_OBJ_KIBAKO, MM_OBJ_TSUBO = 0x005, 0x00E, 0x039, 0x081, 0x082

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
        out.append((m[0], p, x, y, z, _rot(rx, aid_raw & 0x4000), _rot(ry, aid_raw & 0x8000),
                    _rot(rz, aid_raw & 0x2000), halfdays))
        for o in m[2]:
            if o not in objects:
                objects.append(o)
    return out, objects, skipped


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
        if aid != MM_EN_DOOR:
            skipped[aid] = skipped.get(aid, 0) + 1
            continue
        out.append(dict(Id=NV_MM_TRANSITION_MARKER + MM_EN_DOOR, Params=params & 0x3FF, PosX=x, PosY=y, PosZ=z,
                        RotY=_rot(ry, False), FrontSideRoom=fr, FrontSideEffects=fc & 0xFF,
                        BackSideRoom=br, BackSideEffects=bc & 0xFF))
    return out, skipped
