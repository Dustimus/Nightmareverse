#!/usr/bin/env python3
"""RE1's guns for Link (soh/src/code/z_nv_weapon.c), from the player's own files.

"re1/weapons/w<id>" per weapon (RE1's item id): "RWPN", u16 item id, then
  - Jill's side of it, as soh_enemy.py's player blobs ("RPLA" bones and bank, z_nv_linkanim.c):
    PLAYERS/W1<x>.EMW, her skeleton and her animations holding that weapon (RE1's PLAYERS/W1x:
    x is the item id). PlayerAnimations.cpp's gun aim plays, per aim direction, a raise, a fire
    and a hold: level 5 / 6 / 7, up 8 / 9 / 10, down 11 / 12 / 13 (checked by her gun arm's
    pitch); the grenade launcher has one aim; the knife raises with 5 and slashes with 7.
  - u32 length, then the gun as RE1's item viewer shows it (ITEM_M2/I<nn>V.IVM: a TIM and a TMD
    after it), packed as soh_scene.py model_bytes: drawn in Link's fist, not Jill's low-poly
    weapon mesh (the EMW's own mesh is her hand and the gun in one).

"re1/hits" (with the executable): RE1's weapon hit records (WeaponDamage.cpp's first-playthrough
table: per enemy type and weapon, 12 bytes: knockback x, y, z, damage, the post-hit kind and data,
the hit state), found by the hash of its first row (the zombie's) and copied as they are: "RHIT",
u16 count, then the records. z_nv_enemy.c looks a shot up by enemy type * 10 + weapon id - 1.

"re1/combine" (with the executable): RE1's item records and combine tables (MenuData.cpp's g_ItemMaxQty
records, 4 bytes per item: the most a slot holds, the picture, the item's combine table, flags; and
g_ItemCombineData's tables, a count then 4-byte records {the other item, what the chosen one becomes,
what the other becomes, the effect}, with g_ItemCombinePtrs' places), the records and the tables found
by the hashes of their first bytes, the pointers by being consecutive places in the tables: "RCMB",
u16 item count, the records, u16 table count, u16 offset per table, u16 data length, the data.
z_nv_items.c's combine runs RE1's menu_item_check_combine on them.

"re1/weaponfx" (with the executable): each weapon's fire effects (PlayerAnimations.cpp's
g_weaponFireBillboard, g_weaponMuzzleFlash and g_weaponFlash2, 14 entries of 10 bytes each: the
frame, the effect type and group, a pad, x, y, z; each table 144 bytes), found by the hash of the
first's start: "RWFX", then the three tables. z_nv_weapon.c fires them as the shot goes.
"""
import hashlib
import os
import struct

import emd
import rdt
import soh_enemy
import soh_scene

# RE1's item id: (Jill's weapon file, the item viewer's model). The model is the item record's
# byte 1 (MenuData.cpp g_ItemMaxQty) into g_ItemModelFileNames.
WEAPONS = {
    0x01: ("W11.EMW", "I05V.IVM"),  # knife
    0x02: ("W12.EMW", "I00V.IVM"),  # Beretta
    0x03: ("W13.EMW", "I02V.IVM"),  # shotgun
    0x04: ("W14.EMW", "I04V.IVM"),  # Colt Python (DumDum)
    0x05: ("W14.EMW", "I04V.IVM"),  # Colt Python (both Pythons are W14; W15 is the flamethrower's)
    0x07: ("W16.EMW", "I08V.IVM"),  # grenade launcher (explosive rounds)
    0x08: ("W16.EMW", "I08V.IVM"),  # (acid rounds)
    0x09: ("W16.EMW", "I08V.IVM"),  # (flame rounds)
    # the rocket launcher: its model is item 10's (I07V), its moves Jill's W17
    0x0A: ("W17.EMW", "I07V.IVM"),
}


def _emw(path):
    d = open(path, "rb").read()
    anim_off, _mesh_off = struct.unpack_from("<2I", d, len(d) - 8)
    return emd.parse_skeleton(d, 0, anim_off), emd.parse_anim_list(d, anim_off)


def _ivm(path):
    d = open(path, "rb").read()
    tim = emd.parse_tim(d, 0)
    clut_len = struct.unpack_from("<I", d, 8)[0]
    img_len = struct.unpack_from("<I", d, 8 + clut_len)[0]
    return rdt.parse_tmd(d, 8 + clut_len + img_len, tim), tim


def weapon_blob(re1, item):
    emw, ivm = WEAPONS[item]
    sk, anims = _emw(rdt.find_file(rdt.find_file(re1, "PLAYERS"), emw))
    out = bytearray(b"RWPN" + struct.pack("<H", item))
    out += b"RPLA" + struct.pack("<H", len(sk.relpos))
    for i in range(len(sk.relpos)):
        out += struct.pack("<4h", *sk.relpos[i], sk.parents[i])
    out += soh_enemy.bank(sk, anims)
    model = soh_scene.model_bytes(*_ivm(rdt.find_file(rdt.find_file(re1, "ITEM_M2"), ivm)))
    out += struct.pack("<I", len(model)) + model
    return bytes(out)


def write(re1, out):
    """Write each weapon RE1's files have; returns the item ids written."""
    d = os.path.join(out, "re1", "weapons")
    done = []
    for item in sorted(WEAPONS):
        try:
            blob = weapon_blob(re1, item)
        except (OSError, IndexError, ValueError):
            continue
        os.makedirs(d, exist_ok=True)
        open(os.path.join(d, "w%d" % item), "wb").write(
            soh_scene._otr_header(soh_scene.LUS_BLOB) + struct.pack("<I", len(blob)) + blob)
        done.append(item)
    return done


# the hit records' first row (the zombie's, 10 weapons x 12 bytes): its hash, so the table is found in
# any build of the executable without its numbers written here; type 1's row is the same, so the first
HIT_ROW0_SHA1 = "218de2eda5a8c4d45471aada4d3a860574f0af3b"
HIT_RECORDS = 200  # 20 enemy types x 10 weapons


def hits_blob(exe_path):
    """RE1's hit records from its executable, or None when they aren't found."""
    if not exe_path:
        return None
    d = open(exe_path, "rb").read()
    for o in range(0, len(d) - HIT_RECORDS * 12, 2):
        if d[o + 11] == 0 and d[o + 23] == 0 and hashlib.sha1(d[o:o + 120]).hexdigest() == HIT_ROW0_SHA1:
            return b"RHIT" + struct.pack("<H", HIT_RECORDS) + d[o:o + HIT_RECORDS * 12]
    return None


def write_hits(exe_path, out):
    """Write the hit records; how many (0 without the executable or when they aren't in it)."""
    blob = hits_blob(exe_path)
    if blob is None:
        return 0
    d = os.path.join(out, "re1")
    os.makedirs(d, exist_ok=True)
    open(os.path.join(d, "hits"), "wb").write(
        soh_scene._otr_header(soh_scene.LUS_BLOB) + struct.pack("<I", len(blob)) + blob)
    return HIT_RECORDS


# the combine data's and the item records' first 64 bytes: their hashes (no numbers here)
COMBINE_SHA1 = "907c98e8209d5ae97d0e394176bb78daa6e1ec2e"
ITEMS_SHA1 = "0c83f3029d2eadda40a44d2999d8b7673087ffe4"
COMBINE_BYTES = 440   # g_ItemCombineData's length in the PC build
COMBINE_TABLES = 35   # its pointers (g_ItemCombinePtrs: 35 in the PC build)
ITEM_RECORDS = 112    # g_ItemMaxQty: 448 bytes, 4 per item


def _find_hash(d, sha1, n=64):
    for o in range(0, len(d) - n, 2):
        if hashlib.sha1(d[o:o + n]).hexdigest() == sha1:
            return o
    return -1


# the weapons' fire effects (PlayerAnimations.cpp g_weaponFireBillboard, g_weaponMuzzleFlash,
# g_weaponFlash2: three tables of 14 entries of 10 bytes, each padded to 144 bytes, one after
# another), found by their start's hash
WEAPON_FX_SHA1 = "5c12e0c4691f2d79e6aa89a49cc189da28053139"
WEAPON_FX_BYTES = 3 * 144


def weapon_fx_blob(exe_path):
    """RE1's fire billboard, muzzle flash and second flash per weapon: "RWFX" + the three tables."""
    if not exe_path:
        return None
    d = open(exe_path, "rb").read()
    if d[:2] != b"MZ":
        return None
    o = _find_hash(d, WEAPON_FX_SHA1)
    if o < 0:
        return None
    return b"RWFX" + d[o:o + WEAPON_FX_BYTES]


def write_weapon_fx(exe_path, out):
    """Write re1/weaponfx; how many weapons' entries (0 without the executable)."""
    blob = weapon_fx_blob(exe_path)
    if blob is None:
        return 0
    d = os.path.join(out, "re1")
    os.makedirs(d, exist_ok=True)
    open(os.path.join(d, "weaponfx"), "wb").write(
        soh_scene._otr_header(soh_scene.LUS_BLOB) + struct.pack("<I", len(blob)) + blob)
    return 14


def combine_blob(exe_path):
    """RE1's item records and combine tables from its executable, or None."""
    if not exe_path:
        return None
    import soh_sound
    d = open(exe_path, "rb").read()
    if d[:2] != b"MZ":
        return None
    cd = _find_hash(d, COMBINE_SHA1)
    it = _find_hash(d, ITEMS_SHA1)
    if cd < 0 or it < 0:
        return None
    va = None
    for sva, size, ra, rs in soh_sound._pe_sections(d):
        if ra <= cd < ra + rs:
            va = sva + (cd - ra)
    if va is None:
        return None
    # the pointer table: 35 places in the data, the first four of them its start + 0, 5, 10, 15
    want = struct.pack("<4I", va, va + 5, va + 10, va + 15)
    pt = d.find(want)
    if pt < 0:
        return None
    ptrs = struct.unpack_from("<%dI" % COMBINE_TABLES, d, pt)
    offs = [p - va for p in ptrs]
    if any(o < 0 or o >= COMBINE_BYTES for o in offs):
        return None
    out = bytearray(b"RCMB" + struct.pack("<H", ITEM_RECORDS) + d[it:it + ITEM_RECORDS * 4])
    out += struct.pack("<H", COMBINE_TABLES) + struct.pack("<%dH" % COMBINE_TABLES, *offs)
    out += struct.pack("<H", COMBINE_BYTES) + d[cd:cd + COMBINE_BYTES]
    return bytes(out)


def write_combine(exe_path, out):
    """Write the item records and combine tables; how many tables (0 without the executable)."""
    blob = combine_blob(exe_path)
    if blob is None:
        return 0
    d = os.path.join(out, "re1")
    os.makedirs(d, exist_ok=True)
    open(os.path.join(d, "combine"), "wb").write(
        soh_scene._otr_header(soh_scene.LUS_BLOB) + struct.pack("<I", len(blob)) + blob)
    return COMBINE_TABLES
