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

Before converting, `make_mod.py` checks the copy (`tools/re1_verify.py`): each room file's
fingerprint against the releases we know (`tools/re1_known.json`: hashes only, no game data) and
the language of its text. It prints which release and language it found, and warns about room
files that match no known release (modded, damaged or another version) or text in more than one
language. `re1_verify.py --learn <name> <folder>` adds another release's fingerprints.

## Pipeline

`tools/rdt2scene.py` drives the conversion (it started as the N64 demo's converter, with its
RE1 readers `rdt.py`, `emd.py`, `dor.py`, `evt.py`; the parts that decide how a room plays are
being replaced one by one with RE1's own rules, read from the PC decompilation). It reads a
room's RDT (cameras, camera zones, collision, doors, scripts).
With `--soh <folder>`, `tools/soh_scene.py` also writes each room as SoH resources and packs
them into `<folder>.o2r`:

- `scenes/shared/<name>_scene/<name>_scene`: the scene, in SoH's XML scene format (root
  `<Room>`; the readers are `soh/soh/resource/importer/SceneFactory.cpp` and
  `scenecommand/*Factory.cpp`)
- `.../<name>_room_0`: the room, with an image room shape: one background per RE1 camera
- `.../<name>_col`: collision, XML `<CollisionHeader>` (cameras are `CAM_SET_PREREND_FIXED`)
- `.../<name>_bg<n>`: camera n's background, a binary Background resource holding RE1's own
  pixels as raw 320x240 RGBA5551 (lossless: RE1's backgrounds are 15-bit, and SoH draws a
  Background directly unless it starts with a JPEG header)
- `.../<name>_bg<n>_mask`: camera n's mask pieces (rectangle, depth, shape), drawn by
  `soh/src/code/z_nv_mask.c` as invisible depth-only rectangles right after the background

- `.../<name>_camzones`, `_doors`, `_climbs`: RE1's camera switch zones, doors and stairs
  (below)

Scale: 0.0217 OoT units per RE1 unit (`rdt2scene.SCALE`). RE1's models: Chris 2941 units
tall, Jill 2808. At this scale Chris comes to adult Link's hat tip (~64) and Jill to his head
(~61), which matches them in play. Measured against a background, Link's full silhouette
(hat included) reads ~5% taller than Jill: that's the hat, not the scale. Settled with the
user, 2026-10-24.

How the room plays, from RE1's own rules (ecruells/resident-evil-pc-decomp, read for the
rules; none of its code is copied):

- Collision (`tools/re1_floor.py`): RE1's collision records (RDT+0x4C). Shapes 1 (box) and 3
  (ellipse) keep the player out; shape 4 is a soft zone; shape 5 boxes stop enemies only (the
  player's spawn raises the status bit that skips them, and nothing clears it), which is how
  the player stands in stairwells. Floor is wherever the player can get to from the room's
  arrival points; RE1 has no floor heights, so each part the shapes keep apart takes the height
  of the doors arriving in it, carried across the room's stairs.
- Cameras (`soh/src/code/z_nv_camera.c`): RE1's switch zones (RDT+0x48), grouped by camera;
  the first record of a group is its header, the others are quads that cut to their camera
  while that camera is on. The table ends at a record with camera -1. A room opens on camera
  0 with one pass of camera 0's zones (room_set), wherever the player arrives; Door_set byte
  13 is only a door-scene input. After a warp (dev tools), the camera is the one a player
  walking there would have (`soh_scene.walk_cameras`).
- Stairs (`soh/src/code/z_nv_climb.c`): Aot_set zones of type 5 (along x) and 6 (along z)
  over a solid box. A with the zone in front of Link and him facing along it carries him 4350
  units along the axis and 2885 up or down (player_door_open_sequence); 2114 / 1402 in rooms
  0x05 and 0x0E. Checked in 207: up to the corridor and back down to the stairwell.
- Start positions: RE1's arrival points as they are; start position 0 is the room's first
  arrival (the first room's is `--spawn`).

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

Known quirk, left alone on purpose: RE1's masks only cover what Jill can reach. Link's centre
stops where hers does (the wall inset), but his body is wider, so pressed against furniture his
arm or shield can show through holes in RE1's own mask art (102's wardrobe has a transparent
column inside the wood). Masks and spacing stay as RE1 has them; single spots can be patched
if one bothers in play (agreed with the user, 2026-10-25).

Room scripts (2026-10-25): each room keeps RE1's three scripts as they are (`<name>_scd`,
`tools/soh_scene.py`), and `soh/src/code/z_nv_script.c` runs them as RE1 does: the init chain once
as the room loads, the main chain every tick, and the eight event slots (RE1's event VM). Run so
far: if/else, RE1's ten flag banks (a bit test passes when the bit *differs* from the byte given),
the stage/room/camera test, Enemy_set, enemy_prop_set's behaviour, events, movies. Other commands
are stepped over at their RE1 length and other tests are false. Flags last for the session.

Enemies (2026-10-25: "port them over as accurately as possible and then reverse engineer some
Zelda logic on to them"): RE1's zombie is a port, kept apart from the OoT side.
`soh/src/code/z_nv_zombie.c` is RE1's own code on an RE1 entity (`z_nv_re1.h`, the decomp's
Entity fields) in RE1's units and 30 Hz ticks: `zombie_update` and all its states, the action,
move and damage tables, the attack (grab, bite, push back), the knock-down and getting back up,
death, and the script actions (roar, head shot, vomit); with them RE1's room collision and walk
zones (`<name>_re1col`, `tools/soh_scene.py`: RDT+0x4C and +0x58), sight test, path finding,
wandering and `Joint_move`. `soh/src/code/z_nv_enemy.c` is the Zelda layer: one actor per entity
(registered with the ActorDB in `soh/soh/Enhancements/Nightmareverse/NvActors.cpp`) that draws
RE1's model and animation (`re1/enemies/em<type>`, `tools/soh_enemy.py`), feeds Link to the port
as RE1's player (position, facing, Jill's size, button mashing), turns Link's weapon hits into
RE1 hits (blades as the knife, shots as the handgun, explosions as the grenade launcher; OoT
damage x 9 on RE1's health), holds Link in a grab with OoT's grab (`play->grabPlayer`) at the spot
RE1 puts the player, and turns RE1's bite damage into hearts (10 = half a heart). Z-targeting
reaches 350 units. Masks hide enemies as they hide Link. Room scripts place enemies with Enemy_set
and enemy_pos_set; one whose behaviour has bit 7 is held for its script. Not yet: RE1's sounds and
effects, other enemy types' code (they stand), events animating enemies. Dev hook:
`NV_TEST_ENEMY="type,x,z"` (OoT units) adds a held, non-solid enemy. The headless checks run with
`NV_NO_ENEMIES=1` (no scripts).

Examine spots (2026-10-29): RE1's own, as its scripts build them. The room scripts' door_set,
room_action_set, room_action_reset and room_action_arm (0x0C, 0x0D, 0x12, 0x13) fill RE1's room
action table in `soh/src/code/z_nv_script.c`; A probes it as check_action_object does (the point
600 units in front of Link, or Link's own position for entries flagged 0x40; the first entry
flagged 0x01 and 0x80 that holds it), after doors and stairs. The A button reads Check there.
Message spots (handler 2) show the room's message in OoT's text box, in Navi's blue
(`soh/soh/Enhancements/Nightmareverse/NvMessages.cpp`). The text is decoded from the player's own
room files when the mod is made (`tools/soh_scene.py` decode_message: RE1's font order, its page
breaks and highlight colour) and never committed. Not yet: the executable's shared messages (id
bit 0x40), item names inside messages, yes/no prompts and the other handlers (items, keys,
events), and the Japanese release's text (its own font encoding).

Pickups (2026-10-30): the scripts' item_set (0x18) puts each item lying in a room into the room
action table (handler 4 an item, 0x0F a map, 0x0D a document; none once taken). A on one asks
"Take the <item>?" with OoT's yes/no (our own wording and plain item names,
`soh/src/code/z_nv_items.c`); yes takes it by RE1's rules (room_event_item_pickup: ammo and ink
ribbons merge into a carried stack up to 250, the rest a new slot; with no room it stays where it
lies) and sets the item's taken flag, so it's gone from the room. Jill's 8 slots, the item box and
the taken flags last for the session until the save system. Not yet: the items' 3D models in the
rooms, documents, the item box's use, RE1's status screen.

Movies: RE1's own AVIs (Cinepak + PCM), copied into the mod unchanged for the movies the rooms'
scripts can play (`re1/movies/<id>`), and played by `soh/src/code/z_nv_movie.c` (its own AVI
reader and Cinepak decoder; the sound through SDL, `NvMovieAudio.cpp`). Any button skips. 104's
scene is RE1's: entering camera 4 makes the main script start event 2, which plays movie 3
(DM3.avi), releases the zombie (behaviour 0x85 to 5: it gets up) and sets bank 1 bit 2, so the
next visit has the other zombie.

Ramps: the stairs inside rooms (203's grand staircase, 207's, 20E's) are walked; the stairs RE1
treats as doors (the foyer's to 203, 10B to 207) stay doors.

Later: ceilings, so OoT's thrown things (bombs) stay in the room; RE1 has none. A first try
(a ceiling over every floor) boxed Link in, most likely by pushing the collision past OoT's
polygon budget (the foyer went from 7.4k to 9.4k polygons): it needs merged floor and wall
polygons first. Heights per room: to estimate from each room's cameras, checked against the
backgrounds, or marked by hand.

Later: the mansion's way in through a Lost Woods wrong turn instead of a replaced OoT door.

## Design notes (from the user)

### Items (decided with the user, 2026-10-30)

- One pool. Every RE1 item Link owns is in one pool, the same one RE1's (linked) item boxes hold.
  Nothing picked up is ever lost: what doesn't fit stays where it lies (RE1's "can't carry any
  more") or goes to the box.
- Inside the mansion: RE1's own status screen, exactly: 8 slots, items stacked only by Combine up
  to RE1's per-item caps (g_ItemMaxQty), the box a plain swap with separate stacks, ~48 slots.
- Outside: a Zelda-style tab of the pause menu with only what's useful outside, pulled from the
  same pool. It appears once Link has been to the mansion. RE1's ammo has no cap there (it stays
  scarce because the mansion only has so much); walking back in, it splits into RE1's stacks and
  whatever passes the 8 slots goes to the box.
- The tab's 6 x 4 grid, as OoT's (blank until owned):
  guns: knife, Beretta, shotgun, Colt Python, flamethrower, rocket launcher (the infinite one
  is the same item with RE1's flag); the grenade launcher with its rounds beside it, as OoT's bow
  and magic arrows (acid, flame; explosive the default or its own slot), Ingram, Minimi; healing:
  first aid spray, green, red and blue herbs, and each mix its own slot. Ammo isn't on the grid:
  its count is on the gun's icon, as arrows on the bow. 21-22 slots, 2-3 free.
- Puzzle items stay RE1-only (the 8 slots), except ones with a use outside: tentatively the
  lighter (lights torches, burns webs) and the bottle (maybe as one of OoT's bottles instead,
  a crossover puzzle: later). Other ideas noted in IDEAS.md.
- Not on the grid: keys (RE1 slots only), the radio (a story flag in RE1; maybe a Quest screen
  icon), the pick axe (unused in RE1), the serum (a story item).

- Link takes Jill's place; Jill stays only where the FMVs need her.
- Guns (when they come back): Link is left-handed, as in OoT, so the Beretta and the rest
  are held and fired with his left hand (mod1 had them in the right).
