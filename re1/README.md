# Resident Evil (1996) in Ship of Harkinian

Part of Nightmareverse: Ship of Harkinian (the PC port of the OoT decomp) with other games'
rooms, systems and stories added alongside Hyrule. Resident Evil is the first. Its N64-ROM
forerunner, `dustimus/mod1`, stays a separate, self-contained demo; this is the PC version.

## Where it stands (2026-11-19)

Jill's whole game converts and plays: all seven stages (the mansion, the courtyard and
guardhouse, the caves, the lab, and the mansion's second visit), from the opening movie to the
endings, walked headless from the opening to ending 7 (Barry kept, Chris freed; the other
endings follow from the same scripts' flags). In it:
- every room's own scripts (RE1's script VM and its eight event slots);
- every enemy type RE1 places, each a port of its own code, and the NPCs;
- doors with RE1's door scenes and locks;
- items, the status screen, the item box, files, combining and healing, and poison;
- the guns, saving at typewriters with ink ribbons, death going back to the save, and the
  results screen;
- RE1's music, sounds, voices and movies;
- the scenes, with Link playing Jill's own animations.

You get in from Hyrule by playing the mansion's song on the ocarina (below). Chris's game isn't
playable: Link plays Jill.

Open, in the sections below: the box's Hyrule side; the Japanese release's text; ceilings; a way in through the Lost Woods.

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
`re1.o2r` in Ship of Harkinian's `mods` folder.

The way in: the mansion's own warp song, played on the ocarina anywhere in free play (C-Down,
C-Right, A, C-Right, C-Left, A, C-Left, C-Up: the opening phrase of the basement theme), warps
into the foyer (`z_nv_script.c`, NvSong). It rides OoT's warp-song sequence in Nocturne of
Shadow's slot, playing a stretch of RE1's opening movie's sound in place of a melody, and the first
arrival plays the whole opening movie (seen once: save event flag 0xD7). For testing, a file
`nv_boot.txt` holding `re1_106` in the SoH folder starts the game in the foyer.

The mansion on the way back (RE1's stages 6 and 7: the return trip's own rooms, scripts and
enemies on the first visit's backgrounds) is its own set of rooms, as in RE1. Once the return flag
is set (scenario flag 0, raised as Plant 42 dies), a door to stage 1 or 2 goes to the same room of
stage 6 or 7 (RE1's room_transition_load adds 5; `z_nv_door.c`), and the return rooms' doors to
the other floor stay in them. A test switch for comparing the two: the foyer's front door (shut
for the whole game in RE1) opens onto the other visit's foyer, 106 <-> 606
(`rdt2scene.py` front_door_switch).

RE1's elevator car (700, between 713 and 71C) has no doors to walk to: its script runs the
ride. Here a room whose doors are all 1x1 points rides for a few seconds and then takes the door
that doesn't lead back where Link came from. Its arrival points lie in the car's walls (RE1's
script places Jill), so the converter moves an arrival in a wall to the nearest open floor.

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

Room scripts (2026-10-25, all but the visual extras by 2026-11-17): each room keeps RE1's three
scripts as they are (`<name>_scd`, `tools/soh_scene.py`), and `soh/src/code/z_nv_script.c` runs them
as RE1 does: the init chain once as the room loads, the main chain every tick, and the eight event
slots (RE1's event VM, its characters' animation and movement opcodes included). RE1's ten flag banks
(a bit test passes when the bit *differs* from the byte given) and the state block are saved with
the game (`z_nv_save.c`). Stepped over at their RE1 length (NV_LOG names them): the mirror (0x0F),
model and joint tints (0x34, 0x4D), the light parameters (0x40, 0x46), effect flags (0x4E), the
costume variant (0x4F), the attack-animation override (0x2B), and RE1's two dead slots (0x26, 0x2E),
which hang in RE1 itself.

Enemies (2026-10-25: "port them over as accurately as possible and then reverse engineer some
Zelda logic on to them"): RE1's zombie is a port, kept apart from the OoT side.

The dog (Cerberus, type 2) is the second port, `soh/src/code/z_nv_cerberus.inc`, included at the
end of `z_nv_zombie.c` to share its helpers: its states (behave, hit, death) and behaviours
(patrol, chase with its skid and wall following, the leap and bite, strafe, alert, close snap,
stalk), steering by probing 500 units ahead. Its bite takes 12 of RE1's health (the same share of
Link's hearts) and knocks Link back; RE1's player flinch is a second's gate on the next bite.
The maul (RE1's kill of a badly hurt player) is in.
A dog its room's script still holds (behaviour 0x80, 108's two behind the windows) isn't drawn
or hit until the script lets it go. Dev: `NV_TEST_ENEMY="type,x,z,behaviour,slot"` (slot 15,
the default, has no hit collider; another slot can be fought).
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
and enemy_pos_set; one whose behaviour has bit 7 is held for its script. Every enemy type RE1
places is now a port of its own code (`z_nv_*.inc`: the dog, spider, Black Tiger, crow, hunter,
wasp, the greenhouse vines, Plant 42, chimera, adder, Neptune, Tyrant, Yawn, the spider web), with
its sounds; the characters (Barry, Wesker, Richard, Enrico, ...) are driven by their events
(`z_nv_npc.inc`).
Each casts RE1's ground shadow (2026-11-20, `z_nv_enemy.c` NvEnemy_DrawShadow, FadeSprite.cpp's
entity_add_fade_sprite and DrawFadeSpr): RE1's shadow mask (data/kage.tim, `re1/ui/kage` from
`tools/soh_ui.py`) on a quad flat on the floor under it, turned as it faces, at the half sizes each
enemy's own init gives it (the zombie 700 x 900, the dog 0x550 x 0x200, the Tiger 2500, the
characters Jill's 500 x 700; the chimera and the adder none), darkening the floor as RE1's shadow
page does (to about 61% at its core: a soft shadow, not a black one). Left out: the corpse's
pool and the shadows of Plant 42, its roots and the web. A hit's blood: see Weapons. Dev hook:
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
breaks and highlight colour) and never committed. The executable's own text comes from the player's
English executable (`tools/soh_text.py`, `re1/exe_text`): the shared messages (id bit 0x40: the
prompts, locked doors, "You can't use it here."), the item names inside messages, and the item
descriptions. Yes/no questions and the other handlers (items, keys, events) run. Not yet: the
Japanese release's text (its own font encoding).

Pickups (2026-10-30): the scripts' item_set (0x18) puts each item lying in a room into the room
action table (handler 4 an item, 0x0F a map, 0x0D a document; none once taken). A on one asks RE1's own
"Will you take the <item>?" (its shared message 0) with OoT's yes/no (`soh/src/code/z_nv_items.c`);
a map (handler 0x0F) is RE1's pickup_key_event instead: no slot, it's owned (8:0x7C + its index),
usually through its room's event asking first ("A dormitory sketch. Will you take it?"); yes takes it by RE1's rules (room_event_item_pickup: ammo and ink
ribbons merge into a carried stack up to 250, the rest a new slot; with no room it stays where it
lies) and sets the item's taken flag, so it's gone from the room. Jill's 8 slots, the item box and
the taken flags are saved with the game. The items lying in a room are drawn
with RE1's own item models (RDT+0x54, `<name>_items`, `tools/soh_scene.py` items_blob) at the
item_set's position, by one actor (`Nv_Items`, `z_nv_items.c`), with RE1's glint
(`z_nv_effects.c`), and vanish when taken. Items RE1 hangs on a room object draw on it, and one
hung on the player (scaParent 0xFE, 2026-11-20) at its offset in Link's frame, moving and turning
with him; objects ride the player or an enemy (0x80 + its slot) the same way. Whether an item's
model shows is RE1's byte per item model (g_item_model_table[n][0]: item_set shows it, the
scripts' 0x19 and 0x35 hide or show it), kept by model, not by action entry: before, the two were
mixed up, so those scripts hid the wrong item or none (303's flare, which RE1 hangs on the player
and its script hides, floated in front of Link).

RE1's player animations on Link (2026-10-31): each enemy's EMD carries the player's side of its
grabs (`re1/enemies/em<type>_pl`, `tools/soh_enemy.py`); `soh/src/code/z_nv_linkanim.c` poses
Jill's 15-bone skeleton (0 torso root, 1 head, 2 pelvis, 3-5 / 6-8 legs, 9-11 / 12-14 arms) and turns
Link's waist, thighs, shins, upper arms and forearms to her bone directions over his own pose, at
the end of Player_Update. The zombie's grab plays its three player animations per direction
(zombie_attack_player_anim_tbl) and holds Link until the last ends. Dev: `NV_LOG_LA=1` logs both
skeletons per frame; `tools/linkanim_debug.py soh.log out.png` draws them side by side.

Scenes (2026-11-17 to 11-19): when a room's script takes the player (an event's character 0, its
state-1 opcodes), Link is walked by a stand-in RE1 entity (`NvCast`, `z_nv_enemy.c`) and plays what
RE1 would play on Jill, by player_scd_behavior_01's sets: 0x10-0x3D the room's own set (RDT+0x6C/0x70,
`re1/player/r<room><0|1>`); below 0x10 g_ScdAnimRemap's pairs, 0-4 Jill's body set (EM1021's own
animations, `re1/player/b1`) and 5-9 the set of the weapon in her hand (`re1/player/w1x`, PLAYERS/W1x.EMW
by g_weaponPathTable; unarmed W10). Its timing is the animation's, so a scene waits as long as RE1's.
Behaviours that move her pick OoT's nearest: 2 walks, 3 runs, 4 and 5 step back, the turns on the spot
step. The HUD is hidden while a scene holds her (the ocarina keeps its own). A scan of Jill's 159 room
files (2026-11-19) found 571 such requests in 58 rooms; 10 aren't in their room's set (eight in 408,
602's 0x2F, 619's 0x28) and keep a fixed hold, and 0x3E and up are never asked of Jill.

Status screen (2026-10-31): in RE1's rooms the pause menu's map page is RE1's status screen
(`soh/src/code/z_nv_status.c`, hooked into z_kaleido_scope_PAL.c where the world map is drawn) and
the pause menu opens on it there (z_kaleido_setup.c). It is drawn in RE1's own pictures, cut at
mod-making time out of the player's STATUS.TIM, BLUE.TIM and STATFACE.TIM (`re1/ui/status`,
`tools/soh_ui.py` STATUS_SPRITES, at the places MainMenu.cpp draws them from): the Map/File/Exit
tabs, the face (the user's Link portrait in Jill's place) and her condition
(Fine/Caution/Danger by Link's hearts in RE1's quarters, Poison!), the equipped weapon, and the 2 x 4 slots on RE1's blue ground with RE1's icons
(`re1/ui/icons`: ITEM_ALL.PIX in STATUS.TIM's third CLUT, by the item record's icon byte, so the
mixed herbs have theirs), RE1's digits for counts and RE1's cursors. A on an item opens RE1's action
box: Equip (weapons only; RE1's g_EquippedItemId, which follows its slot as slots close up; see
Weapons), Use (heals by RE1's g_ItemHealTable: a third, two thirds or
all of Link's hearts; an item that would do nothing isn't used up), Check, Combine (then the second
item; below). Up from the top slots reaches the tabs: Exit closes the pause menu, File
lists the files found, Map opens RE1's map screen (below), and Radio (2026-11-20, menu_tab_radio)
is RE1's blank plate until Jill has the radio (bank 0 flag 0x7F); with it, where the room lets her
call (an action entry raising item-use flag 0x3F: the courtyard's 300 and 304, at their moment) it's
used on the room as item 0x4D and the menu closes, and elsewhere it shows RE1's line for the radio
(RE1 shows the radio in its item viewer). The four tabs are RE1's, narrowed to fit left of the
slots.

Combine (2026-11-20, `z_nv_items.c` NvItems_Combine, MainMenu.cpp's menu_item_check_combine and
menu_item_apply_combine): RE1's own combine tables, read from the player's executable
(`re1/combine`, `tools/soh_weapons.py` combine_blob: the 35 tables and the item records, found by
their hashes; nothing of them in the repository). The first item's table names what it combines
with, what each becomes and the effect: a gun loads from its clip or shells up to the gun's
capacity (g_ItemMaxQty; the rest stays a smaller stack), two stacks of ammo merge (to 250), the
grenade launcher takes a kind of round only when it's empty, herbs mix after RE1's question
("Will you mix the herbs?", Yes / No), herbs that don't mix say so, and the chemicals mix only in
the guardhouse's drug storehouse (409; elsewhere "Too dangerous to mix here."): a wrong mix there
fails ("It disappeared.") or makes the poison gas (bank 0 flag 0x16, "The poison gas enveloped.",
the menu closes and the room's own script takes over). The messages are RE1's, from its
executable. Slots close up after a combine, the equipped weapon moving with its slot. Without the
executable's tables (an older re1.o2r) only the herbs mix, as before.

Map screen (2026-11-19, `soh/src/code/z_nv_map.c`, MainMenu.cpp's menu_init_map_screen,
menu_map_state_machine, map_tab_* and map_display_*): RE1's two views under the tabs. The browser
shows the section overview (item_m2/map0d, 0c, 00 or 0e: the highest section known), the known
sections' markers, the section's name and the floor's label, and blinking arrows to the floors
beside it; up/down change the floor, left/right the section, A opens the floor's map, B leaves. The
floor's map (map01..0b by RE1's area, over Map_blue.tim) opens and closes with RE1's stretch and has
a scan line at rest; B closes it. What it shows is RE1's: each room owns six colours of the floor
picture's palette (from entry 13), cleared when it hasn't been visited (wholly without the area's
map, the fill only with it), and the room she's in drawn again in red, blinking. Visited is bank 8's
g_StageRoomFlagOffset[stage % 5] + room, raised as each room loads (room_set_visited_flag); owned is
8:0x7C + the map's index. Jill's serum objective blinks on the 1F map with RE1's marker while it's
outstanding. The pictures are the player's own, kept with their palettes (`re1/ui/map`,
`tools/soh_ui.py` map_blob) so the colours can be edited as RE1 does. Saves from before this have
no visited rooms yet: the map fills in from here on.

Files (2026-11-03): a document lying in a room (item 0x5F + its file number, handler 0x0D) is
taken with A and, as in RE1, read at once on its own screen: the room holds and the pages show over
it (`soh/src/code/z_nv_files.c`, NvFilesUi_*, from z_play.c as the box screen), not the pause
menu. The pages are the player's own (`re1/ui/file<n>`, from item_m2/filem_*.pix: a table of offsets
to PAK-packed 256 x 256 4 bpp TIMs, in RE1's g_filemPixNames order), shown whole screen; left/right
turn pages, A on the last page or B closes. The status screen's File tab lists them and reads them
again there. Their list names are our own plain "File n" for now. Dev: `NV_FILE_AT="n,frame"`
takes file n that many frames into the room (as if found there).

Room objects (2026-10-31): each room's own object models (furniture, statues, push blocks, the
item box's lid; RDT object models, `<name>_objs`, `tools/soh_scene.py` items_blob with which=2)
are drawn as RE1 places them: the scripts' omodel_set (0x1F: position, y turn, flags, parent) and
obj_flag_set (0x35: shown or hidden), drawn when bit 0 is set (render_room_objects), turned by
RotMatrix's x, y, z, and carried by their parent object (InitScaMatrix). Items lying on a room
object now draw on it too, and objects on the player or an enemy ride with them (see Pickups).
`NV_NO_OBJS` hides them for comparisons.

Item box (2026-11-01): A at a box (room action type 8, open_itembox) swings its lid (the object
named by the entry's +4) open as RE1's check_itembox_state does, faster then easing to a stop.
Then, as in RE1, the room holds and the box screen takes over (`soh/src/code/z_nv_box.c`, its own
screen, not the pause menu): RE1's status layout drawn piece by piece at the decomp's coordinates
(MenuData.cpp's frame tables, menu_draw_inventory) from the player's STATUS.TIM, with the box's
panel from ITEMBOXN.TIM over its upper left (draw_itembox_menu: the list of three names, the
preview slot, the scroll strip and its marker, the shadows and masks). Controls are RE1's
(menu_itembox_interaction): choose one of Jill's slots (the box dimmed), A, then turn the box's
list with up/down (48 places, wrapping) and A to exchange (Jill's slots stay packed; a weapon put
away is unequipped); B steps back, and B on Jill's side (or START) closes it and the lid shuts.
One box, shared by every box in the mansion. Names are RE1's own (from its executable), drawn in
OoT's small font for now (RE1's 8x14 font, which the results screen already uses, is a later step).
The list slides as RE1's does (2026-11-20, menu_itembox_interaction's states 2 and 3,
draw_itembox_menu): on (down, RE1's R1) the names rise a row over 15 steps and the preview's icon
slides up with the next coming in under it, the place moving on as it ends; back (up, L1) the place
steps back first and the list slides down to it; held, it goes on, three steps a frame after half a
second; the names and the preview are clipped to their windows. Not yet: Hyrule's side (see the
design notes).

Weapons (2026-11-03, first pass): in RE1's rooms the weapon equipped on the status screen takes
C-Left (RE1's icon on the button; the OoT item there is back outside the mansion), in
`soh/src/code/z_nv_weapon.c`. Holding C-Left squares Link up as RE1's aim does: he stops, the stick
turns him, up/down aims high or low, and Jill's raise and hold for that weapon and direction play
on him (`re1/weapons/w<id>`, `tools/soh_weapons.py`: her PLAYERS/W1x.EMW by RE1's g_weaponPathTable,
mirrored, as Link is
left-handed; through z_nv_linkanim.c). Releasing C-Left fires (B fires too and keeps the aim): a
round from the slot (the knife none; an empty gun loads itself from her biggest stack of its ammo,
up to its capacity, fire_consume_ammo_stack), Jill's fire animation, and RE1's hit: for the handgun,
shotgun and Pythons (weapon_hit_detect_gun) an enemy in a band straight ahead, from 650 out to any
distance, as wide as the weapon's range (her half of weapons_ranges: the shotgun's 2600 is its
spread) plus the enemy's radius; for the knife (weapon_hit_detect_knife) the enemy's place within
its radius + the reach of the blade, 600 ahead of her at the thrust; and RE1's damage and hit state
from its hit records per enemy type and weapon (`re1/hits`, read from the player's executable by
`tools/soh_weapons.py`: found by the hash of the zombies' row; without the executable every enemy
takes the zombies'). apply_weapon_damage's other rules (2026-11-19): a shot hits an enemy only at
the heights its status flags allow (the aim: up, level, down), and the hit state is raised by a level
or low aim; the per-enemy reach changes (the Black Tiger's -1000, Plant 42's +2000; the knife's own)
and skips (no zombie aimed down at but with the shotgun; the knife misses a dog on the ground or a
spider at the level, and what's on the ceiling); and the knife and the guns don't hit through walls
(check_weapon_line_of_sight, the room's sight-blocking boxes). With the stick level and the aim on
an enemy, the aim takes its height, as RE1's auto-aim points her arm at it (auto_aim_pitch_update):
down at a snake, up at a chimera on the ceiling. The aim follows the
Z-target when it's one of RE1's enemies, else RE1's auto-aim: the nearest enemy ahead in reach.
The gun is RE1's item-viewer model (ITEM_M2/I<nn>V.IVM by the item record's model byte) in Link's
left fist, which closes round it, placed as Mod1 placed its Beretta in his right hand (that looked
right in play), mirrored across the hand; it's drawn only while the gun is up, so another item (the
ocarina) replaces it, and another C button puts it down. Each weapon fires with RE1's own sound
(see Sound). Ammo is loaded by Combine (see the status screen) or, on an empty gun, by the
trigger. The grenade launcher's round (2026-11-20) flies as RE1's does: its fire billboard (type 8,
group 7, `z_nv_effects.c` NvFx_FireRound) leaves Link's hand along his facing on RE1's arc
(behaviour 42 freezes it off the gun and trails it, 43 flies it), its height above Link's feet
giving the aim it hits at; it hits the nearest enemy whose radius plus the weapon's 900 takes it in
(weapon_hit_detect_projectile, from the round: `z_nv_weapon.c` NvGun_RoundHit, RE1's damage and hit
state for the round), and where it lands, on the floor or in a wall (effect_probe_ground on RE1's
collision), it bursts as RE1's effect_shot_impact has it: fire for the explosive round, a splash for
acid, flames for the flame round, each with its own sound. (RE1 also runs an instant hit for the
explosive round, measured from a point near the room's origin, not the round: left out.)

Each shot fires RE1's own effects from its tables in the executable (`re1/weaponfx`,
`tools/soh_weapons.py`: g_weaponFireBillboard, g_weaponMuzzleFlash, g_weaponFlash2): the spark at
the gun, the case flying out, the smoke, at Link's hand along his aim; they come as the shot goes
rather than on their frames of Jill's fire. A hit runs RE1's post-hit for the weapon and the enemy
(2026-11-20, `z_nv_enemy.c` NvEnemy_GunHitFrom, WeaponDamage.cpp's PTR_post_hit_callbacks and
g_enemy_hit_reactions): its rules (a zombie's head blown off by the shotgun inside 3000 aimed level
or down, or by the Python at the level; Jill's handgun 3 more, 7 more on a dog; the shotgun from
past 9000 10 less and a hit state lower; Plant 42 shrugging off half the shots from past 7000), and
its billboards on the enemy from the hit record (blood, the bursts and sparks of the launcher's
rounds, the head's). Left out: the joints' red tint and the spurts from the joints. Dev: `NV_EQUIP=n` equips slot n
of NV_GIVE's items; NV_WALK `cl:N` holds C-Left N frames; `NV_GUN_FIT="x,y,z,rx,ry,rz,size%"` and
`NV_GUN_MIRROR=x|y|z|none` move the gun in the fist.

Sound (2026-11-03, first pass): RE1's WAVs from the player's SOUND and VOICE folders, copied
unchanged into the mod (`re1/sound/<name>`, `re1/voice/<name>`, `tools/soh_sound.py`), mixed on an
SDL device of our own (`soh/soh/Enhancements/Nightmareverse/NvSound.cpp`: a pool of effect voices
and RE1's three music channels). Room music as RE1's (`soh/src/code/z_nv_sound.c`, tables in
`z_nv_sound_tables.c` from the decomp's SoundTables.cpp): each room's music state (all clear at a
new game; set by the scripts' opcode 0x37) picks its group and which channels start
(update_room_bgm), and the scripts start and stop channels (0x15, 0x16); most rooms stay quiet
until their script starts the music, as RE1's mansion does. Leaving the mansion stops it. Effects: the guns' shots (each weapon's fire sound: gun01, shot01, magnum01, grbomb02 ...); the
status, box and file screens' cursor, decide and cancel; the room's sound bank (RE1's per-room 48
names, `z_nv_sound_rooms.c` from the decomp's g_RoomSndData): the zombies' Snd_em (moans, steps,
the dragged leg, roars, bites, eating, vomit, deaths, by the enemy record's sound group), Link's
footsteps (the room's footstep zones, RDT+0x5C, `<name>_ftzones`: the floor's two steps, alternating,
in place of OoT's), the box lid (slot 0x20); and the doors (load_room_sfx: the door record's byte 9
picks the pair, opening as the door scene starts and closing as it ends). How loud each is
(2026-11-20, `z_nv_sound.c` NvSnd_Volume) is RE1's own law, from the room's camera, not from Link
(Play3DSnd: Calc3DSndPan's two pans, lowered by the distance from the camera's eye and on one side
by how far off its line of sight the sound is, then CalcPanVolume's volume from their average):
the shots, the room bank's sounds and the launcher rounds' bursts.
OoT's battle music never starts in RE1's rooms (and their scenes set no sequence). The cutscenes'
voice lines play as the scripts call them (0x1E), and the scripts wait on them as RE1's do; every
ported enemy has its sounds. RE1's PC version doesn't pan its effects left or right (it only
sets their volume from the two pans; it pans only its bank 4 sounds), so neither does this. Real
stereo, as the PlayStation's, would be a choice to make.

Dev: NV_WALK takes `start` and `left`/`right`/`up`/`down` (the D-pad); NV_GIVE="id,qty;..." gives
items on the first room; NV_FILE=n takes file n there.

Movies: RE1's own AVIs (Cinepak + PCM), copied into the mod unchanged for the movies the rooms'
scripts can play (`re1/movies/<id>`), and played by `soh/src/code/z_nv_movie.c` (its own AVI
reader and Cinepak decoder; the sound through SDL, `NvMovieAudio.cpp`). Any button skips. 104's
scene is RE1's: entering camera 4 makes the main script start event 2, which plays movie 3
(DM3.avi), releases the zombie (behaviour 0x85 to 5: it gets up) and sets bank 1 bit 2, so the
next visit has the other zombie.

Ramps: the stairs inside rooms (203's grand staircase, 207's, 20E's) are walked; the stairs RE1
treats as doors (the foyer's to 203, 10B to 207) stay doors.

The bomb layer (2026-11-20, the Discord's "BombWall" idea in IDEAS.md; `soh/src/code/z_nv_bombwall.c`,
`tools/re1_floor.py` bombwall_blob): the walls stay door height, and bombs, bomb flowers and arrows
(and Deku nuts, which fly as arrows) can't leave a room over them. Each room has its floor plan as a
grid (`<name>_bombwall`: the floor, both levels and the stairs, its holes filled so things fly over
furniture, grown out to where the walls stand); one about to leave it, at any height, stops at its
edge: a bomb gets OoT's own wall bounce, an arrow sticks. Link, the enemies and the camera never see
it, so it can't box anyone in the way taller walls or ceilings did (a first try at ceilings over
every floor pushed the collision past OoT's polygon budget). One line each in En_Bom, En_Bombf and
En_Arrow. Checked headless in 103: bombs and nuts sent over the walls stop at them and fall back
into the room. Dev: NV_WALK `bomb:yaw,speed,vy` (a lit bomb from above Link's head, tenths) and
`nut:yaw,pitch` (degrees).

Later: a way in through a Lost Woods wrong turn, alongside the warp song.

## Saving (2026-11-20)

The run (RE1's state as a typewriter writes it: the flags, the items and box, the files, the music,
the clock, where she stood; `soh/src/code/z_nv_save.c`) lives in the Ship save file it belongs to, in
its own section, `mansion` (`soh/soh/Enhancements/Nightmareverse/NvSaveFile.cpp`, through SoH's
SaveManager: each game reads and writes only its own section, each with its own version number).
A typewriter writes that section alone, so OoT's own progress isn't saved by it, as RE1's save
screen is the mansion's only; OoT's saves write it again as it stands, so a new file never inherits
another's run. A run from before (a file of its own, `Save/nv_re1_<file>.sav`) moves into the
section the first time its file loads, and is kept as `.sav.migrated`; erasing or copying over a
file removes such a leftover. The debug file (slot 255, which SoH never writes) still keeps its run
in that file of its own: the walkthrough's checkpoints are such files.

## Walking the game headless (2026-11-14 to 11-17)

The scripts are in `tools/headless/` (`walk.sh` one segment, `regress.sh` every room's
checkpoint, `flagdump.py` the story flags between saves; each says how it's laid out). The
walkthrough runs from a typewriter-style checkpoint (`NV_BOOT_ENTRANCE=warp` with a slot copied
in) and an `NV_WALK` step list, `;`-separated: `x,z` walks, `pos:`, `face:`, `wait:N`, `a`, `b`,
`doorto:XXX` / `door:N` / `aot:N` (walk into a door or action entry and press A), `use:XX`,
`check:XX`, `drop:XX`, `equip:XX`, `faceenemy` (turn to the nearest of RE1's enemies), `bomb:` and
`nut:` (see the bomb layer), `cl:N`
(aim and fire), `list` (the room's doors and live action entries), `mark:N`, `save`. Settings:
`NV_GODMODE`, `NV_GIVE` / `NV_EQUIP`, `NV_FLAGS="bank:bit,-bank:bit"` (set or clear, on a new game
or a loaded save), `NV_WORD="i:v"` (a state word, also on a loaded save: 4 restarts the countdown),
`NV_ANSWERS="yes,no,..."` (the yes/no questions in turn), `NV_BOOT_FILE=1..3` (boot on that Ship save
file instead of the debug one, made if it isn't there, so saving goes through its sections), the step
`ootsave` (OoT's own save of that file), `NV_FLAG_TRACE` (the system flags as the
scripts move them), `NV_EVT_TRACE`, `NV_AOT_TRACE` (action entries as the scripts change them),
`NV_NOSCENEANIM` (scenes without Link's RE1 animations), `NV_LOG`.

What the run showed about RE1's story (Jill), all from the rooms' own scripts:

- Barry: his talk in 308 decides his route. Yes, then no (event 4) keeps him: bank 1 0x5B up,
  0x5C down. Walking past him, or yes twice, leaves 0x5C up, and coming back through 307 then sets
  1:0xC0. On his route he is with her in 30A, on the lab's lift and in 513; 514 (event 6, after the
  Tyrant) arms the self-destruct and unlocks 500->501 and Chris's cell (0:0x2B, 2:0x1C, 2:0x2C),
  and Lab Key A isn't placed at all. Off it, Lab Key A is in 513 for 501 and the cell.
- Chris: freed in 512 (through 50B, once 508's panel has the three pass codes) sets 1:0x4B.
- The countdown (3 minutes) only starts when Barry is there for it (501's event 4 or 6), and 303's
  rooftop Tyrant fight only happens with it (303's event 17 tests 0:0x2B). With Barry and Chris it
  ends 7, the launcher epilogue; Chris alone 3; neither 2 (ending_select_id's numbering).
- The MO disk is spent at each machine: RE1 places three (30F, 503, 717) for 509, 507 and 510.
- Yawn: the first (210) comes in when she walks into its camera 2 and flees under 2800 of its
  3050; the second (70C) is let go by the press spot in the corner (event 1) and can die.
- The rooftop Tyrant wakes once she walks off the helipad zones that keep it asleep (303's 4:0x21,
  0x22), is pinned at 200 by tyrant_update until the launcher's rocket, then dies under the script.

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
- Ink ribbons (2026-10-31): Jill needs them, the console (and PC second-run) rule, not the PC's
  first-run Jill who saves without; the ribbons lie in the rooms for her as for Chris.
- Health (2026-10-31): RE1's heals and damage both work as shares of Link's maximum hearts, as of
  Jill's 96 in RE1 (a herb heals a third of his hearts; a zombie bite of 10 takes 10/96 of them),
  so the mansion is as hard whatever his heart containers.
- Not on the grid: keys (RE1 slots only), the radio (a story flag in RE1; maybe a Quest screen
  icon), the pick axe (unused in RE1), the serum (a story item).

- Sound (2026-11-20): RE1's effects stay mono, as RE1's PC version plays them (volume by its own
  camera-relative rule, no left/right panning). Stereo (the PlayStation's) stays possible later.
- The Hyrule side of the items (2026-11-20): the tab above may be reworked. With other games coming
  in, the guns might go in with something else's, or on a weapons screen of their own; the user is
  looking at Majora's Mask's menus (in the other chat) before deciding. Until then the guns are
  equipped from RE1's status screen, which is enough to test both ways.

- Link takes Jill's place; Jill stays only where the FMVs need her.
- Guns (when they come back): Link is left-handed, as in OoT, so the Beretta and the rest
  are held and fired with his left hand (mod1 had them in the right).
