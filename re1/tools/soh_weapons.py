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
