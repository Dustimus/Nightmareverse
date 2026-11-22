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
