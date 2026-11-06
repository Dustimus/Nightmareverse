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
"""
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
    0x05: ("W15.EMW", "I04V.IVM"),  # Colt Python
    0x07: ("W17.EMW", "I08V.IVM"),  # grenade launcher (explosive rounds)
    0x08: ("W17.EMW", "I08V.IVM"),  # (acid rounds)
    0x09: ("W17.EMW", "I08V.IVM"),  # (flame rounds)
    # the rocket launcher: its model is item 10's (I07V); Jill's W1B holds only her moves with it,
    # no aim or fire, so she aims and fires it as the grenade launcher
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
