# Resident Evil (1996) in Ship of Harkinian

Part of Nightmareverse: Ship of Harkinian (the PC port of the OoT decomp) with other games'
rooms, systems and stories added alongside Hyrule. Resident Evil is the first. Its N64-ROM
forerunner, `dustimus/mod1`, stays a separate, self-contained demo; this is the PC version.

## Reference: RE1's own code

RE1's rules come from RE1, never from the N64 demo (`dustimus/mod1`, whose converter guessed
much of this). The reference is the decompilation of the 1997 PC release,
[ecruells/resident-evil-pc-decomp](https://github.com/ecruells/resident-evil-pc-decomp)
(functional-complete; its `docs/` cover the RDT format, SCD opcodes and the engine), checked
against `Bio.exe` where needed. It is GPL-3: we read it to learn the rules and write our own
code; none of its code is copied here.

## Game data stays out of the repository

Nothing from RE1 or OoT is committed: no backgrounds, models, text, sounds or videos. Each
player supplies their own copies: OoT's ROM for SoH as usual, and their own RE1 PC files,
which the tools here convert locally into a mod archive (`.o2r`) for SoH's `mods` folder.
Only our code, tools and notes are committed.

## Making the mod (players)

With Python 3 and Pillow (`pip install pillow`):

    python re1/tools/make_mod.py <RE1 data folder> re1.o2r

where the data folder is the one holding Stage1..Stage7 ("USA" in the GOG release). Put
`re1.o2r` in Ship of Harkinian's `mods` folder. Until the mansion has its way in (the Lost
Woods), a file `nv_boot.txt` holding `re1_106` in the SoH folder starts the game in the foyer.

## Pipeline

`tools/rdt2scene.py` is the N64 demo's room converter (copied, with its RE1 readers:
`rdt.py`, `emd.py`, `dor.py`, `evt.py`). It reads a room's RDT (cameras, camera zones,
collision, doors, scripts) and works out a walkable floor, OoT collision, cameras and spawns.
With `--soh <folder>`, `tools/soh_scene.py` also writes each room as SoH resources and packs
them into `<folder>.o2r`:

- `scenes/shared/<name>_scene/<name>_scene`: the scene, in SoH's XML scene format (root
  `<Room>`; the readers are `soh/soh/resource/importer/SceneFactory.cpp` and
  `scenecommand/*Factory.cpp`)
- `.../<name>_room_0`: the room, with an image room shape: one background per RE1 camera,
  picked by the camera the floor under Link belongs to
- `.../<name>_col`: collision, XML `<CollisionHeader>` (cameras are `CAM_SET_PREREND_FIXED`)
- `.../<name>_bg<n>`: camera n's background, a binary Background resource holding RE1's own
  pixels as raw 320x240 RGBA5551 (lossless: RE1's backgrounds are 15-bit, and SoH draws a
  Background directly unless it starts with a JPEG header)
- `.../<name>_bg<n>_mask`: camera n's mask pieces (rectangle, depth, shape), drawn by
  `soh/src/code/z_nv_mask.c` as invisible depth-only rectangles right after the background

`tools/soh_tables.py <folder>` then registers the rooms with the engine: it writes
`soh/include/tables/nv_re1_scene_table.h` and `nv_re1_entrance_table.h`, which OoT's scene and
entrance tables include after their own entries (scenes from 0x6E, entrances from 0x614; four
entrances per start position, one per OoT setup). Scenes past OoT's 124 save slots keep their
per-scene flags in a scratch set (`soh/src/code/z_nv_scene.c`); RE1's own state comes with the
core systems.

Pass 1 (stages 1-2, the 44 rooms with data; the other room numbers are 4-byte placeholders):

    python3 re1/tools/rdt2scene.py --stage <RE1>/JPN --player 1 --spawn 17200,3600,-32768 \
        --room 106:re1_106 --room 100:re1_100 ... --out /tmp/c_out --soh /tmp/re1
    python3 re1/tools/soh_tables.py /tmp/re1
    cp /tmp/re1.o2r <SoH folder>/mods/

`tools/soh_check.py` boots each room headless, puts Link on a floor spot of every camera,
screenshots it and flags spots with no floor or the wrong camera; it writes a contact sheet per
room (local only: the screenshots show RE1's backgrounds).

First full run (2026-10-24): all 44 rooms load, every camera with floor was captured, Link
found floor at every spot. One camera still comes up wrong: room 20A camera 3 owns only two
cells of floor about Link's width (likely a camera-assignment leftover in the converter; to
check against RE1's own camera zones).

## Dev boot

`NV_BOOT_ENTRANCE=<entrance index>` (e.g. `0x670`, the foyer; `soh_tables.py` prints each room's) makes SoH skip the title and
file select and load straight into that entrance on a debug save (`ovl_title/z_title.c`).
Easier on Windows: a file `nv_boot.txt` in the SoH folder holding a room name (`re1_106`: its
start position 0) or an entrance index does the same.

Scripted play for headless checks (`soh/src/code/z_nv_dev.c`, inert without the variables):
`NV_POS="x,z[,yaw]"` places Link, `NV_WALK="x,z;x,z;wait:N"` steers him through waypoints with
the control stick, `NV_LOG=1` logs position, floor and camera every 20 frames. Checked on the
foyer (2026-10-24): camera switching follows the floor, and the masks cut Link cleanly behind
the gallery columns, their bases and the banister post.

## Plan (agreed with the user, 2026-10-23)

1. Every room's foundation first, with nothing in the rooms: backgrounds, exact depth masks,
   collision, camera switching, doors. Passes: stages 1-2 (the mansion's first visit), then
   3-5 (courtyard, guardhouse, caves, lab), then 6-7 (the mansion revisited).
2. Automated checks: every camera of every room captured headless; floor edges and corners
   walked; every door walked into (must not change rooms) and opened with A.
3. Core systems (text, examine spots, inventory, locks, saving, story flags), generic first.
4. Characters and animation, checked side by side against RE1's own.
5. Content, room by room, each with a short checklist for play-testing.

Doors: never walk-in exits. RE1's own rule (Bio.exe 0x456880): pressing the action button opens
a door when the point 600 units in front of the player is inside the door's rectangle (all of the
mansion's doors are flagged for this). `soh/src/code/z_nv_door.c` does the same with A: the door
opens if any point from RE1's reach (13 OoT units) to Link's (his wall radius, 18 adult / 14
child, plus 4: RE1's reach past Jill's body, her radius taken as ~450 units until it's read from
Bio.exe) in front of him is inside the rectangle. Link stops his radius short of a wall where
Jill stops hers, and some rectangles start at the wall's surface; others lie on open floor and
are smaller than Link's reach. Link arrives at RE1's own
arrival point, facing into the room. Between rooms, RE1's door scene plays over black
(`z_nv_doorscene.c`, data from `tools/soh_doors.py`); a door without one cuts with a fade.
`tools/soh_doorcheck.py` checks every door headless: walking into it must not change rooms, and
A from where a player would stand must reach the target at its arrival point.

Marking spots by hand: hold L and press D-pad down, and SoH appends the room, Link's position,
facing and camera to `nv_spots.txt` in its folder.

Later: RE1's FMVs (Cinepak AVIs, from the player's English PC copy), and the mansion's way in
through a Lost Woods wrong turn instead of a replaced OoT door.
