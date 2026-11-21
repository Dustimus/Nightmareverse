# Nightmareverse: start here

Nightmareverse is Ship of Harkinian (SoH, the PC port of the Ocarina of Time decomp) with other
games' rooms, systems and stories added alongside Hyrule, played as Link. Resident Evil (1996)
is the first game in, and is far along: the whole mansion, courtyard, guardhouse, caves and lab
convert and play, with RE1's own scripts, enemies, items, saves and endings.

This note is for anyone (a person or a new chat) starting work on the project, and especially
for bringing in another game. It's the general picture; each game's own notes have the detail
(for RE1, `re1/README.md`, `re1/IDEAS.md` and `re1/VERSIONS.md`).

## Ground rules

These hold for every game in the project.

- **No game data in the repository, ever.** That means no ROMs, backgrounds, models, textures,
  text, sounds, music, videos, or tables lifted from an executable. Each player supplies their own
  copies, and our tools convert them locally into a mod archive (`.o2r`) for SoH's `mods`
  folder. Only our code, tools and notes are committed. Fingerprints (hashes) to recognise a
  release are fine; the data isn't.
- **The user's own files stay private.** Their recordings, MIDI and save files don't get
  committed, uploaded or shared anywhere.
- **Reference code is read, not copied.** Where a game has a decompilation (RE1:
  [ecruells/resident-evil-pc-decomp](https://github.com/ecruells/resident-evil-pc-decomp),
  GPL-3), we read it to learn the game's rules and write our own code from them. Its code isn't
  copied here.
- **The original game's rules win.** When a game's behaviour is in question, its own code (the
  decomp, or the executable) decides, not a guess. RE1 doesn't take its rules from the older N64
  demo (`dustimus/mod1`, a separate self-contained project).
- **Check with the user before running ahead.** Design choices are theirs: how something should
  feel, what stays from the original and what gets a Zelda touch. Their decisions are written down
  in the games' notes with dates ("decided with the user, ..."); read those before proposing
  anything, and add new ones as they're made.
- **Branches.** RE1 work is on `re1/foyer`. Start other work on its own branch, and don't push
  until the user says so. No pull requests unless asked.

## How a game comes in (the RE1 pattern)

1. **Converter (Python, `<game>/tools/`).** It reads the player's own copy of the game and writes
   SoH resources: scenes and rooms in SoH's XML formats, collision, backgrounds, and our own
   binary resources for whatever OoT has no format for (RE1: camera zones, doors, masks, room
   scripts, enemy models and animations, sounds, text). These are packed into one `.o2r`.
   RE1's entry point is `re1/tools/make_mod.py`, which checks the copy first
   (`re1_verify.py`, against known releases' hashes) and drives `rdt2scene.py`.
2. **Tables.** `re1/tools/soh_tables.py` registers the new scenes and entrances with OoT's
   tables (`soh/include/tables/nv_re1_*.h`, included after OoT's own entries).
3. **Engine (C, `soh/src/code/z_nv_*.c`).** The game's systems run inside SoH. Some are ports of
   the original's own logic on its own units and tick rate: RE1's script VM in `z_nv_script.c`
   and its enemies in `z_nv_zombie.c` and the `.inc` files. A Zelda layer turns those into actors
   Link can see, fight and talk to (`z_nv_enemy.c`). Everything is inert outside the game's
   scenes.
4. **Hooks into OoT's code.** These are kept small and marked `// Nightmareverse:` (grep for it):
   - `z_play.c` (update and draw order);
   - `z_player.c` and `z_player_lib.c` (Link);
   - `z_camera.c` (fixed cameras);
   - `z_message_PAL.c` (text, the ocarina);
   - `z_parameter.c` (HUD);
   - `z_kaleido_*` (pause menu);
   - `ovl_title/z_title.c` (dev boot).

   C++ glue (actors, messages, sound, saves) is in `soh/soh/Enhancements/Nightmareverse/`.
5. **Link stands in for the player character.** He keeps OoT's controls, and the game's own
   animations play on him in its scenes (`z_nv_linkanim.c`). Scale is matched so the game's rooms
   fit him (RE1: 0.0217 OoT units per RE1 unit).

A second game would follow the same shape: its own folder beside `re1/` (tools and notes), its
own `z_nv_<game>_*.c` engine files, and its scenes after RE1's in the tables. Shared pieces
(fixed cameras, masks, the dev boot, the headless harness, Link's animation player) are already
general, or close to it.

## Building and running

- Build SoH as in `docs/BUILDING.md` (CMake; `build-cmake/` here).
- Make RE1's mod: `python re1/tools/make_mod.py <RE1 data folder> re1.o2r`, then put it in SoH's
  `mods` folder.
- Dev boot: `NV_BOOT_ENTRANCE=<entrance>` or a file `nv_boot.txt` (e.g. `re1_106`) skips the
  title and loads straight into a room.
- **Headless play** (`re1/tools/headless/`): SoH under Xvfb, driven by a scripted step list
  (`NV_WALK`), with logs and screenshots.
  - `walk.sh` runs one segment from a room or a checkpoint save.
  - `regress.sh` loads every room's checkpoint and reports what changed.
  - `flagdump.py` diffs RE1's story flags between saves.

  The steps and settings are listed in `re1/README.md` ("Walking the game headless"). This is how
  changes get checked without the user playing: walk it, read the log, look at the frames.

## Where RE1 stands, and what's open

RE1's notes are the record: the plan (`re1/README.md`, "Plan"), what each system does and why,
and the user's design decisions (`re1/README.md`, "Design notes"; `re1/IDEAS.md` for ideas not
yet planned). Things the user still has to do or decide (playtesting, their recordings, design
calls) are what most open RE1 work waits on.

## Working in parallel

RE1 work goes on in its own chat on `re1/foyer`, at the same time as work on other games. To keep
the two out of each other's way:

- **Branches.** Each line of work has its own branch, and nothing is pushed to another's branch.
  Branches come together only when the user asks for it.
- **Files.** New work goes in new files with its own names: `<game>/` for tools and notes, and
  `soh/src/code/z_nv_<game>_*.c` for engine code. RE1's files (`re1/`, the RE1 `z_nv_*.c`) are
  edited from RE1's branch only.
- **Shared files.** The OoT hook points (`z_play.c`, `z_player.c`, `z_message_PAL.c`,
  `z_parameter.c`, the scene and entrance tables, CMake source lists) are where both lines add
  code. Keep additions there small and self-contained, a call into the game's own file guarded
  by its own scenes, so they merge cleanly. A change that would rework a shared piece (the
  dev boot, the headless harness, Link's animation player, the fixed cameras) gets raised with
  the user first.
- **Save data.** Each game keeps its state in its own section of the Ship save file (SoH's
  SaveManager, each with its own version number): RE1's is `mansion`
  (`soh/soh/Enhancements/Nightmareverse/NvSaveFile.cpp`), the Majora's Mask branch's `termina`. A game
  reads and writes only its own section; what it needs from another game goes through that game's
  code, not into its section.
- **Scene and entrance numbers.** RE1's take OoT's tables from scene 0x6E and entrance 0x614
  (`soh/include/tables/nv_re1_*.h`). Another game's go after them, in its own table files,
  starting past the last number RE1's tables use at the time. Test "is this one of my game's
  scenes" with both ends of its range, never with a lower bound alone: RE1's code uses
  `NvRe1_IsScene()` (`soh/include/z64scene.h`, its range from `nv_re1_scene_range.h`, which
  `soh_tables.py` writes), and another game's tables should give it the same kind of helper.

## Plans from the user

- A 3D layer later (2026-11-21, a heads-up, nothing planned yet): Ocarina of Time 3D's and Majora's
  Mask 3D's look may come in as a layer over SoH's, leaving OoT itself alone. It shouldn't touch
  RE1, and the user doesn't want a more modern Link in RE1's rooms anyway, so RE1 keeps the N64
  Link it has.

## Ideas that cross games

`re1/IDEAS.md` already has some: getting to the mansion from Hyrule (the Lost Woods), RE1 items
with a use outside the mansion, remixed dungeons, costumes on Link, and Jill as Navi. A new
game's ideas start in its own notes; anything that ties games together belongs in both.
