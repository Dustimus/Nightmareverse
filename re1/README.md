# Resident Evil (1996) in Ship of Harkinian

Part of Nightmareverse: Ship of Harkinian (the PC port of the OoT decomp) with other games'
rooms, systems and stories added alongside Hyrule. Resident Evil is the first. Its N64-ROM
forerunner, `dustimus/mod1`, stays a separate, self-contained demo; this is the PC version.

## Game data stays out of the repository

Nothing from RE1 or OoT is committed: no backgrounds, models, text, sounds or videos. Each
player supplies their own copies: OoT's ROM for SoH as usual, and their own RE1 PC files,
which the tools here convert locally into a mod archive (`.o2r`) for SoH's `mods` folder.
Only our code, tools and notes are committed.

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

The engine side registers each room in `soh/include/tables/scene_table.h` and
`entrance_table.h` (after OoT's own).

Example (the foyer, room 106):

    python3 re1/tools/rdt2scene.py --stage <RE1>/JPN --player 1 --room 106:re_foyer \
        --spawn 17200,3600,-32768 --out /tmp/c_out --soh /tmp/re1
    cp /tmp/re1.o2r <SoH folder>/mods/

## Dev boot

`NV_BOOT_ENTRANCE=<entrance index>` (e.g. `0x614`, the foyer) makes SoH skip the title and
file select and load straight into that entrance on a debug save (`ovl_title/z_title.c`).

## Plan (agreed with the user, 2026-10-23)

1. Every room's foundation first, with nothing in the rooms: backgrounds, exact depth masks,
   collision, camera switching, doors. Passes: stages 1-2 (the mansion's first visit), then
   3-5 (courtyard, guardhouse, caves, lab), then 6-7 (the mansion revisited).
2. Automated checks: every camera of every room captured headless; floor edges and corners
   walked; every door walked into (must not change rooms) and opened with A.
3. Core systems (text, examine spots, inventory, locks, saving, story flags), generic first.
4. Characters and animation, checked side by side against RE1's own.
5. Content, room by room, each with a short checklist for play-testing.

Doors: never walk-in exits. A door is RE1's door rectangle with an "A to open" spot, checked
for Link facing it, with arrivals placed clear of it.

Later: RE1's FMVs (Cinepak AVIs, from the player's English PC copy), and the mansion's way in
through a Lost Woods wrong turn instead of a replaced OoT door.
