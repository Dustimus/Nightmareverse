"""The player's own Majora's Mask ROM: check which release it is, and find files in it.

Only the US N64 1.0 release is known so far. Its file table (dmadata) lists every file's
virtual and physical (ROM) range; the files are Yaz0-compressed in the ROM.

The file names below are the MM decomp's (zeldaret/mm, spec), listed with their index in
the file table; only the ones we use are here. Game data never leaves the player's machine:
this module only reads the ROM.
"""

import hashlib
import struct
import sys

KNOWN = {
    "d6133ace5afaa0882cf214cf88daba39e266c078": {
        "name": "N64 US 1.0",
        "dmadata": 0x1A500,
        "files": {
            "Z2_OKUJOU": 1203,          # the clock tower's rooftop
            "Z2_OKUJOU_room_00": 1204,
            "Z2_INSIDETOWER": 1474,     # inside the clock tower
            "Z2_INSIDETOWER_room_00": 1475,
            "Z2_INSIDETOWER_room_01": 1476,
            "Z2_TOWN": 1510,            # East Clock Town
            "Z2_TOWN_room_00": 1511,
            "Z2_ICHIBA": 1512,          # West Clock Town
            "Z2_ICHIBA_room_00": 1513,
            "Z2_BACKTOWN": 1514,        # North Clock Town
            "Z2_BACKTOWN_room_00": 1515,
            "Z2_CLOCKTOWER": 1516,      # South Clock Town
            "Z2_CLOCKTOWER_room_00": 1517,
            "Z2_ALLEY": 1518,           # the Laundry Pool
            "Z2_ALLEY_room_00": 1519,
            "Z2_YADOYA": 1466,          # the Stock Pot Inn
            "Z2_YADOYA_room_00": 1467,
            "Z2_YADOYA_room_01": 1468,
            "Z2_YADOYA_room_02": 1469,
            "Z2_YADOYA_room_03": 1470,
            "Z2_YADOYA_room_04": 1471,
            "Z2_MILK_BAR": 1173,  # the Milk Bar
            "Z2_MILK_BAR_room_00": 1174,
            "Z2_BOMYA": 1502,  # the Bomb Shop
            "Z2_BOMYA_room_00": 1503,
            "Z2_8ITEMSHOP": 1324,  # the Trading Post
            "Z2_8ITEMSHOP_room_00": 1325,
            "Z2_AYASHIISHOP": 1151,  # the Curiosity Shop
            "Z2_AYASHIISHOP_room_00": 1152,
            "Z2_AYASHIISHOP_room_01": 1153,
            "Z2_SONCHONOIE": 1160,  # the Mayor's Residence
            "Z2_SONCHONOIE_room_00": 1161,
            "Z2_SONCHONOIE_room_01": 1162,
            "Z2_SONCHONOIE_room_02": 1163,
            "Z2_SONCHONOIE_room_03": 1164,
            "Z2_POSTHOUSE": 1312,  # the Post Office
            "Z2_POSTHOUSE_room_00": 1313,
            "Z2_TAKARAYA": 1188,  # the Treasure Chest Shop
            "Z2_TAKARAYA_room_00": 1189,
            "Z2_SYATEKI_MIZU": 1239,  # the Town Shooting Gallery
            "Z2_SYATEKI_MIZU_room_00": 1240,
            "Z2_DOUJOU": 1433,  # the Swordsman's School
            "Z2_DOUJOU_room_00": 1434,
            "Z2_BOWLING": 1158,  # Honey & Darling's
            "Z2_BOWLING_room_00": 1159,
            "Z2_TAKARAKUJI": 1334,  # the Lottery Shop
            "Z2_TAKARAKUJI_room_00": 1335,
            "Z2_TENMON_DAI": 1298,  # the Bombers' hideout and the Astral Observatory
            "Z2_TENMON_DAI_room_00": 1299,
            "Z2_TENMON_DAI_room_01": 1300,
            "Z2_YOUSEI_IZUMI": 1278,  # the Great Fairies' fountains
            "Z2_YOUSEI_IZUMI_room_00": 1279,
            "Z2_YOUSEI_IZUMI_room_01": 1280,
            "Z2_YOUSEI_IZUMI_room_02": 1281,
            "Z2_YOUSEI_IZUMI_room_03": 1282,
            "Z2_YOUSEI_IZUMI_room_04": 1283,
            "KAKUSIANA": 1522,  # the grottos
            "KAKUSIANA_room_00": 1523,
            "KAKUSIANA_room_01": 1524,
            "KAKUSIANA_room_02": 1525,
            "KAKUSIANA_room_03": 1526,
            "KAKUSIANA_room_04": 1527,
            "KAKUSIANA_room_05": 1528,
            "KAKUSIANA_room_06": 1529,
            "KAKUSIANA_room_07": 1530,
            "KAKUSIANA_room_08": 1531,
            "KAKUSIANA_room_09": 1532,
            "KAKUSIANA_room_10": 1533,
            "KAKUSIANA_room_11": 1534,
            "KAKUSIANA_room_12": 1535,
            "KAKUSIANA_room_13": 1536,
            "KAKUSIANA_room_14": 1537,
        },
    },
}

# The scenes brought over (Clock Town and its buildings), and how many rooms each has.
CLOCK_TOWN = {
    "Z2_CLOCKTOWER": 1,
    "Z2_BACKTOWN": 1,
    "Z2_TOWN": 1,
    "Z2_ICHIBA": 1,
    "Z2_OKUJOU": 1,
    "Z2_INSIDETOWER": 2,
    "Z2_ALLEY": 1,       # the Laundry Pool
    "Z2_YADOYA": 5,      # the Stock Pot Inn
    "Z2_MILK_BAR": 1,  # the Milk Bar
    "Z2_BOMYA": 1,  # the Bomb Shop
    "Z2_8ITEMSHOP": 1,  # the Trading Post
    "Z2_AYASHIISHOP": 2,  # the Curiosity Shop
    "Z2_SONCHONOIE": 4,  # the Mayor's Residence
    "Z2_POSTHOUSE": 1,  # the Post Office
    "Z2_TAKARAYA": 1,  # the Treasure Chest Shop
    "Z2_SYATEKI_MIZU": 1,  # the Town Shooting Gallery
    "Z2_DOUJOU": 1,  # the Swordsman's School
    "Z2_BOWLING": 1,  # Honey & Darling's
    "Z2_TAKARAKUJI": 1,  # the Lottery Shop
    "Z2_TENMON_DAI": 2,  # the Bombers' hideout and the Astral Observatory
    "Z2_YOUSEI_IZUMI": 5,  # the Great Fairies' fountains
    "KAKUSIANA": 15,  # the grottos
}


class MMRom:
    def __init__(self, path):
        self.data = open(path, "rb").read()
        self.sha1 = hashlib.sha1(self.data).hexdigest()
        self.release = KNOWN.get(self.sha1)
        if self.release is None:
            raise ValueError(f"unknown Majora's Mask ROM (sha1 {self.sha1}); "
                             "only the US N64 1.0 release is supported so far")

    def entry(self, name):
        """(vrom start, vrom end, rom start, rom end) of a file, by its decomp name."""
        i = self.release["files"][name]
        return struct.unpack_from(">4I", self.data, self.release["dmadata"] + 16 * i)

    def entry_index(self, i):
        """The same, by the file's index in the table."""
        return struct.unpack_from(">4I", self.data, self.release["dmadata"] + 16 * i)

    def rom_offset(self, name):
        return self.entry(name)[2]

    def check(self):
        """Every file we name is Yaz0 in the ROM (a wrong index would land elsewhere)."""
        for name in self.release["files"]:
            _, _, ps, _ = self.entry(name)
            if self.data[ps:ps + 4] != b"Yaz0":
                raise ValueError(f"{name}: no Yaz0 header at ROM offset {ps:#x}")


if __name__ == "__main__":
    rom = MMRom(sys.argv[1])
    rom.check()
    print(rom.release["name"])
    for name in rom.release["files"]:
        vs, ve, ps, pe = rom.entry(name)
        print(f"{name:26} rom {ps:#09x}-{pe:#09x}  size {ve - vs:#x}")
