"""Make mm.o2r (Termina's mod archive) from the player's own Majora's Mask ROM.

    python mm/tools/make_mod.py <MM rom> <mm.o2r> [--torch <soh-torch>] [scene ...]

1. checks the ROM (mm_rom.py);
2. writes Torch configs for the scenes (mm2torch.py) into a work folder and runs SoH's own
   extractor (soh-torch, built with SoH: build-cmake/soh-torch) on the ROM for the models,
   textures and collision;
3. writes the scene and room headers (mm_scene.py);
4. packs it all into one archive for SoH's mods folder.

Default scenes: Clock Town (mm_rom.CLOCK_TOWN). Nothing here is committed: the archive and
the work folder hold the player's game data.
"""

import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import mm2torch  # noqa: E402
import mm_objects  # noqa: E402
import mm_text  # noqa: E402
import mm_rom  # noqa: E402
import mm_scene  # noqa: E402

REPO = os.path.dirname(os.path.dirname(HERE))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("rom")
    ap.add_argument("out")
    ap.add_argument("--torch", default=os.path.join(REPO, "build-cmake", "soh-torch"))
    ap.add_argument("--keep", help="keep the work folder here (for looking at what was made)")
    ap.add_argument("scenes", nargs="*")
    a = ap.parse_args()

    rom = mm_rom.MMRom(a.rom)
    rom.check()
    scenes = a.scenes or list(mm_rom.CLOCK_TOWN)
    work = a.keep or tempfile.mkdtemp(prefix="mm_mod_")
    shutil.rmtree(work, ignore_errors=True)
    yml = os.path.join(work, "yml")
    os.makedirs(os.path.join(yml, "mm_us", "scenes"))
    shutil.copy(os.path.join(REPO, "mm", "assets", "yml", "config.yml"), yml)
    for s in scenes:
        mm2torch.convert_scene(rom, s, mm_rom.CLOCK_TOWN[s], os.path.join(yml, "mm_us", "scenes"))
    mm2torch.write_area_textures(os.path.join(yml, "mm_us", "scenes"))

    os.makedirs(os.path.join(yml, "mm_us", "objects"))
    mm_objects.convert_objects(rom, os.path.join(yml, "mm_us", "objects"))

    torch_out = os.path.join(work, "torch")
    os.makedirs(torch_out)
    log = os.path.join(work, "torch.log")
    with open(log, "w") as f:
        rc = subprocess.call([a.torch, "--src", yml, "--dest", torch_out, "--version", "0.0.0", a.rom],
                             stdout=f, stderr=subprocess.STDOUT)
    if rc != 0:
        sys.exit(f"soh-torch failed ({rc}); see {log}")

    res = os.path.join(work, "res")
    with zipfile.ZipFile(os.path.join(torch_out, "mm.o2r")) as z:
        z.extractall(res)
    for s in scenes:
        mm_scene.write_scene(rom, s, mm_rom.CLOCK_TOWN[s], res)
    mm_objects.write_matanims(res)
    # MM's messages, in OoT's text codes (mm_text.py), as one blob resource
    msgs = mm_text.messages(rom)
    os.makedirs(os.path.join(res, "mm", "text"), exist_ok=True)
    data = mm_text.blob(msgs)
    open(os.path.join(res, "mm", "text", "messages"), "wb").write(
        mm_scene._otr_header(mm_scene.LUS_BLOB) + struct.pack("<I", len(data)) + data)

    with zipfile.ZipFile(a.out, "w", zipfile.ZIP_DEFLATED) as z:
        for root, _dirs, files in os.walk(res):
            for f in sorted(files):
                if f == "version":  # (Torch's: the archive isn't a ROM archive)
                    continue
                p = os.path.join(root, f)
                z.write(p, os.path.relpath(p, res).replace(os.sep, "/"))
    print(f"{a.out}: {len(scenes)} scenes")
    if mm_scene.SKIPPED:
        print("MM actors not brought over yet (MM id: count): " +
              ", ".join(f"0x{k:03X}: {v}" for k, v in sorted(mm_scene.SKIPPED.items(), key=lambda kv: -kv[1])))
    if not a.keep:
        shutil.rmtree(work)


if __name__ == "__main__":
    main()
