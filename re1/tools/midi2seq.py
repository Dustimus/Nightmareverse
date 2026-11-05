#!/usr/bin/env python3
"""MIDI (.mid) -> an Ocarina of Time sequence (the audio sequence script SoH plays), for music the
player brings to the mod. Each MIDI track with notes becomes a channel playing one OoT instrument
(a soundfont and an instrument slot in it); overlapping notes in a track are spread over the
channel's layers (up to 4). Tempo: the MIDI's first; timing in OoT's 48 ticks a beat.

    python3 re1/tools/midi2seq.py song.mid out.seq --font 3 --inst 6

Writes the raw sequence (.seq); soh_resource(seq, font) wraps it as SoH's sequence resource, to
put under "custom/music/..." in an .o2r (SoH gives it a sequence number at startup).

The OoT sequence script, as SoH's audio_seqplayer.c reads it:
  sequence: D7 mask16 (channels on), DB vol, DD tempo(BPM), 90+ch off16 (start a channel),
            FD delay (var), FF (end)
  channel:  C4 (large notes: each note gives its velocity), C1 inst, DF vol, DD pan,
            88+layer off16 (start a layer), FD delay, FF
  layer:    notes 0x40+pitch delay(var) velocity (sounds the whole delay), C0 delay (a rest), FF
  var:      one byte below 0x80, else two: 0x8000 | value
  pitch:    0..63 above the sequence's transposition: MIDI note - 21
"""
import argparse
import struct
import sys

TICKS = 48  # OoT ticks a beat


def read_midi(path):
    """(ticks per beat, tempo in microseconds a beat, [track: [(start, length, note, velocity, channel volume)]])."""
    d = open(path, "rb").read()
    if d[:4] != b"MThd":
        raise SystemExit("not a MIDI file")
    _fmt, ntracks, division = struct.unpack(">HHH", d[8:14])
    p = 8 + struct.unpack(">I", d[4:8])[0]
    tempo = 500000
    tracks = []
    for _ in range(ntracks):
        if d[p:p + 4] != b"MTrk":
            break
        length = struct.unpack(">I", d[p + 4:p + 8])[0]
        q, end = p + 8, p + 8 + length
        t, status, on, notes, volume = 0, 0, {}, [], 127

        def var():
            nonlocal q
            v = 0
            while True:
                b = d[q]
                q += 1
                v = (v << 7) | (b & 0x7F)
                if not b & 0x80:
                    return v

        while q < end:
            t += var()
            b = d[q]
            if b & 0x80:
                status = b
                q += 1
            kind = status & 0xF0
            if status == 0xFF:
                meta = d[q]
                q += 1
                n = var()
                if meta == 0x51 and tempo == 500000:
                    tempo = int.from_bytes(d[q:q + 3], "big")
                q += n
            elif status in (0xF0, 0xF7):
                q += var()
            elif kind in (0x80, 0x90):
                note, vel = d[q], d[q + 1]
                q += 2
                if kind == 0x90 and vel > 0:
                    on.setdefault(note, []).append((t, vel))
                elif on.get(note):
                    start, v = on[note].pop(0)
                    if t > start:
                        notes.append((start, t - start, note, v))
            elif kind in (0xA0, 0xB0, 0xE0):
                if kind == 0xB0 and d[q] == 7:
                    volume = d[q + 1]
                q += 2
            elif kind in (0xC0, 0xD0):
                q += 1
        p = end
        if notes:
            tracks.append((sorted(notes), volume))
    return division, tempo, tracks


def var(v):
    v = max(0, min(v, 0x7FFF))
    return bytes([v]) if v < 0x80 else bytes([0x80 | (v >> 8), v & 0xFF])


def layer_script(notes):
    """One layer's script: its notes (no overlaps) with rests between."""
    out, t = bytearray(), 0
    for start, length, pitch, vel in notes:
        if start > t:
            gap = start - t
            while gap > 0:
                step = min(gap, 0x7FFF)
                out += b"\xC0" + var(step)
                gap -= step
            t = start
        p = max(0, min(63, pitch - 21))
        out += bytes([0x40 + p]) + var(length) + bytes([min(127, vel)])
        t = start + length
    return bytes(out + b"\xFF"), t


def split_layers(notes):
    """Spread a track's notes over layers so none overlaps within one (at most 4: the rest dropped)."""
    layers = []
    for n in notes:
        for lay in layers:
            if lay[-1][0] + lay[-1][1] <= n[0]:
                lay.append(n)
                break
        else:
            if len(layers) < 4:
                layers.append([n])
    return layers


def wait(total):
    out = bytearray()
    while total > 0:
        step = min(total, 0x7FFF)
        out += b"\xFD" + var(step)
        total -= step
    return bytes(out)


def build(path, inst):
    division, tempo, tracks = read_midi(path)
    bpm = max(1, min(255, round(60_000_000 / tempo)))
    scale = TICKS / division
    chans = []
    total = 0
    for k, (notes, volume) in enumerate(tracks[:16]):
        q = [(round(s * scale), max(1, round(n * scale)), note, vel) for s, n, note, vel in notes]
        lays = split_layers(q)
        scripts = [layer_script(lay) for lay in lays]
        total = max([total] + [end for _, end in scripts])
        chans.append((volume, scripts))
    seq = bytearray()
    header_len = 3 + 2 + 2 + 3 * len(chans) + len(wait(total + TICKS)) + 1
    seq += b"\xD7" + struct.pack(">H", (1 << len(chans)) - 1) + b"\xDB\x7F" + b"\xDD" + bytes([bpm])
    chan_off = []
    body = bytearray()
    for k, (volume, scripts) in enumerate(chans):
        chan_off.append(header_len + len(body))
        head = bytearray(b"\xC4" + bytes([0xC1, inst]) + bytes([0xDF, min(127, volume)]) + b"\xDD\x40")
        lay_head_len = len(head) + 3 * len(scripts) + len(wait(total + TICKS)) + 1
        lay_off = []
        lay_body = bytearray()
        for script, _ in scripts:
            lay_off.append(chan_off[-1] + lay_head_len + len(lay_body))
            lay_body += script
        for j, off in enumerate(lay_off):
            head += bytes([0x88 + j]) + struct.pack(">H", off)
        head += wait(total + TICKS) + b"\xFF"
        body += head + lay_body
    for k, off in enumerate(chan_off):
        seq += bytes([0x90 + k]) + struct.pack(">H", off)
    seq += wait(total + TICKS) + b"\xFF"
    assert len(seq) == header_len, (len(seq), header_len)
    seq += body
    return bytes(seq), bpm, total


def soh_resource(seq, font):
    """SoH's sequence resource around a raw sequence (as its custom music packs are built:
    OotrsArchive.cpp BuildSequenceResource): the 0x40-byte resource header, u32 size, the data,
    u8 0, u8 2, u8 2, u32 font count 1, u8 font."""
    head = bytearray(0x40)
    head[0] = 0  # little-endian
    head[1] = 1
    struct.pack_into("<I", head, 4, 0x4F534551)  # ResourceType::SOH_AudioSequence ('OSEQ')
    struct.pack_into("<I", head, 8, 2)
    struct.pack_into("<Q", head, 12, 0xDEADBEEFDEADBEEF)
    return bytes(head) + struct.pack("<I", len(seq)) + seq + bytes([0, 2, 2]) + struct.pack("<I", 1) + bytes([font])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("midi")
    ap.add_argument("out")
    ap.add_argument("--font", type=int, default=3, help="OoT soundfont (3: the orchestra)")
    ap.add_argument("--inst", type=int, default=6, help="instrument slot in it (6: the orchestra's trombone)")
    a = ap.parse_args()
    seq, bpm, total = build(a.midi, a.inst)
    open(a.out, "wb").write(seq)
    print("%s: %d bytes, %d BPM, %.1f beats" % (a.out, len(seq), bpm, total / TICKS))


if __name__ == "__main__":
    sys.exit(main())
