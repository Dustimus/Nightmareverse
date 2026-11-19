"""Yaz0 decompression (the N64 Zelda games' file compression)."""

import struct


def decompress(src):
    if src[:4] != b"Yaz0":
        raise ValueError("not Yaz0")
    size = struct.unpack_from(">I", src, 4)[0]
    out = bytearray()
    p = 16
    while len(out) < size:
        flags = src[p]
        p += 1
        for bit in range(8):
            if len(out) >= size:
                break
            if flags & (0x80 >> bit):
                out.append(src[p])
                p += 1
            else:
                b1, b2 = src[p], src[p + 1]
                p += 2
                dist = ((b1 & 0xF) << 8 | b2) + 1
                n = b1 >> 4
                if n == 0:
                    n = src[p] + 0x12
                    p += 1
                else:
                    n += 2
                start = len(out) - dist
                for k in range(n):
                    out.append(out[start + k])
    return bytes(out)
