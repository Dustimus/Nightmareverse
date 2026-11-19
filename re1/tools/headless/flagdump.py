"""flagdump.py <bank> <save> [<save> ...]: RE1's flag bank in our save files (nv_re1_*.sav: the
ten banks of 8 words from offset 52, bit 0 the top bit as RE1 numbers them). One save: its set
bits; several: what each sets (+) and clears (-) against the one before."""
import struct
import sys


def bits(path, bank):
    d = open(path, "rb").read()
    w = struct.unpack_from("<80I", d, 52)
    out = set()
    for i in range(8):
        for b in range(32):
            if w[bank * 8 + i] & (0x80000000 >> b):
                out.add(i * 32 + b)
    return out


if __name__ == "__main__":
    bank = int(sys.argv[1])
    prev = None
    for p in sys.argv[2:]:
        s = bits(p, bank)
        n = p.split("/")[-1]
        if prev is None:
            print(n, " ".join("%02X" % x for x in sorted(s)))
        else:
            print(n, "+", " ".join("%02X" % x for x in sorted(s - prev)), " -",
                  " ".join("%02X" % x for x in sorted(prev - s)))
        prev = s
