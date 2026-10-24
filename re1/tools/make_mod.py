#!/usr/bin/env python3
"""Make the RE1 mod for Ship of Harkinian from your own RE1 PC files, in one go.

    python re1/tools/make_mod.py <RE1 data folder> <output .o2r>

The RE1 data folder is the one holding Stage1..Stage7 (and Item_m1): "USA" in the GOG release,
"JPN" in the Japanese PC one. Put the .o2r in SoH's "mods" folder. Needs Python 3 and Pillow
(pip install pillow).

Converts pass 1 (the mansion's first visit: every room of stages 1 and 2 with data; the other
room numbers are 4-byte placeholders), Jill's versions of the rooms, with RE1's door scenes.
"""
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import rdt2scene  # noqa: E402

FIRST_ROOM = "106"  # the foyer: spawn 0 is where Jill stands after the opening
FIRST_SPAWN = "17200,3600,-32768"
PLAYER = "1"  # Jill


def rooms(re1):
    out = []
    for stage in ("1", "2"):
        d = rdt2scene.stage_dir(re1, stage + "00")
        for fn in sorted(os.listdir(d)):
            u = fn.upper()
            if u.startswith("ROOM") and u.endswith(PLAYER + ".RDT") and len(u) == len("ROOM1001.RDT"):
                if os.path.getsize(os.path.join(d, fn)) > 1024:
                    out.append(u[4:7])
    return out


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    re1, target = sys.argv[1], sys.argv[2]
    ids = rooms(re1)
    if FIRST_ROOM not in ids and not os.path.exists(rdt2scene.stage_dir(re1, FIRST_ROOM)):
        sys.exit("no RE1 rooms found in %s (point it at the folder holding Stage1..Stage7)" % re1)
    ids = [FIRST_ROOM] + [r for r in ids if r != FIRST_ROOM]
    work = tempfile.mkdtemp(prefix="re1mod_")
    try:
        soh = os.path.join(work, "re1")
        args = [sys.executable, os.path.join(HERE, "rdt2scene.py"), "--stage", re1, "--player", PLAYER,
                "--spawn", FIRST_SPAWN, "--out", os.path.join(work, "n64"), "--soh", soh]
        for r in ids:
            args += ["--room", "%s:re1_%s" % (r, r.lower())]
        print("converting %d rooms (a few minutes)..." % len(ids))
        subprocess.run(args, check=True, stdout=subprocess.DEVNULL)
        shutil.copyfile(soh + ".o2r", target)
        print("wrote %s: put it in Ship of Harkinian's mods folder" % target)
    finally:
        shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    main()
