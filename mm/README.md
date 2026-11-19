# Majora's Mask in Nightmareverse

Termina brought into Ocarina of Time's world, played as one game: one Link, one inventory, one
save. The design talk and the decisions made with the user so far are in `../NV_GAMES.md`
("Majora's Mask merged into OoT"). This file is the working record.

Same ground rules as every game here (`../NIGHTMAREVERSE.md`):
- no MM data in the repository;
- the player's own ROM is converted locally into `mm.o2r`;
- MM's decomp (zeldaret/mm) and 2Ship2Harkinian are read to learn the rules, and their code
  isn't copied.

## First target: Clock Town (started 2026-11-19)

Clock Town's six scenes, with the 3-day cycle running in Termina only:
- South (`Z2_CLOCKTOWER`)
- North (`Z2_BACKTOWN`)
- East (`Z2_TOWN`)
- West (`Z2_ICHIBA`)
- the rooftop (`Z2_OKUJOU`)
- inside the tower (`Z2_INSIDETOWER`)

Steps:
1. Scenes loading in SoH, with Link walking an empty town.
2. The clock (`soh/src/code/z_nv_mm_clock.c`).
3. Actors.

### Where it stands (2026-11-19)

**Step 1 works.** All six scenes load in SoH (headless dev boot, `NV_BOOT_ENTRANCE=mm_z2_clocktower`
and so on), with MM's rooms, textures and collision, and Link standing and walking on them.

Not done yet:
- **Characters and objects:** MM's actor and object lists are left out of the rooms.
- **Doors and exits:** nothing leads anywhere yet.
- **Music:** none.
- **Animated textures:** the segments 8 and 9 the game sets while running.
- **The second billboard matrix:** MM's rooms ask for one at `0x01000040`, which OoT's engine
  doesn't set.
- **Spawns:** they start Link idle, without MM's start modes or cameras.

**To raise with the user and the RE1 chat (shared code):** RE1's code tests "an RE1 room" as
`play->sceneNum >= SCENE_RE1_100` in 18 places (`z_nv_*.c`, `NvActors.cpp`, `z_player.c`, `z_actor.c`,
`z_parameter.c`). Every scene after RE1's, MM's included, passes that test. A range check, for
example a helper `NvRe1_IsScene()` up to `SCENE_RE1_71C`, fixes it. It's RE1's code, so the
change belongs on RE1's branch. Without `re1.o2r` loaded nothing visible goes wrong in Clock
Town, but with both games installed RE1's systems would treat Termina as part of the mansion.

### Where RE1 and MM meet (from RE1's README as of re1/foyer 561c55c5, 2026-11-19)

RE1 is close to finished: the whole game plays, from the opening to the endings. These are the
RE1 pieces that MM's plans run into, to raise with the user before building the MM side:
- **Getting there.** RE1's way in is its own warp song (8 notes, in free play), riding OoT's
  warp-song sequence in Nocturne's slot (`z_nv_script.c`, NvSong).
  - MM's songs (Soaring, Double Time, Inverted, Healing, ...) need the same kind of listener.
    One shared "custom song" mechanism, instead of a second listener next to RE1's, should be
    agreed first.
  - None of MM's songs is the mansion song.
- **The Moonlight Sonata Easter egg.** RE1's Sonata (A, C-Down, C-Right, twice) sets OoT's
  clock to midnight in free play outside the mansion.
  - **Decided with the user (2026-11-19): in Termina it's a time skip.** It jumps the 3-day
    clock to that day's midnight. Warping by the Song of Time's variants alone often isn't
    enough, and MM3D has a similar choose-your-hour feature.
  - **A clash, reported to the RE1 chat.** Its listener keeps the last 8 notes across ocarina
    uses and clears them only on its own match. So the Song of Time (R A D R A D), then the
    Song of Time started again (R), completes A D R A D R: midnight. `tools/nv_songs.py`
    finds it.
- **Songs from every game: one list, checked for clashes** (`tools/nv_songs.py`, decided with
  the user that clashes matter, 2026-11-19: probably at least one song per game).
  - A song clashes when its notes appear in a row inside another's, since the ocarina listens
    as the notes are played.
  - Today: OoT's 12, RE1's 2 and MM's 9 new ones, with no clashes, apart from the leftover-notes
    case above.
- **Pause menu.** RE1 replaces the map page with its status and map screens in RE1's rooms
  (z_kaleido_scope_PAL.c, z_kaleido_setup.c). MM's mask screen and Bomber's Notebook would hook
  the same place, gated by Termina's scenes the same way.
- **Saving.** RE1 keeps its state in its own file next to each SoH save
  (`Save/nv_re1_<file>.sav`, NvSaveFile.cpp). MM can follow the same pattern
  (`nv_mm_<file>.sav`). It also uses OoT event flag 0xD7 ("opening seen"); MM's flags should
  live in its own file, not in OoT's spare flags.
- **The scene check** (above): in 18 places on re1/foyer now. The RE1 chat has taken it on.

## Building `mm.o2r`

    cmake --build build-cmake --target soh-torch      # SoH's extractor, built with SoH
    python mm/tools/make_mod.py <MM rom> mm.o2r       # then mm.o2r into SoH's mods folder

`make_mod.py` generates the Torch configs into a temporary folder, runs `soh-torch` on the ROM,
adds our scene and room headers, and packs the archive. `--keep <folder>` keeps the work folder
for inspection. Nothing it writes is committed.

`python mm/tools/soh_tables.py <MM rom>` regenerates `soh/include/tables/nv_mm_*_table.h`:
scenes from 0x10E and entrances from 0xF64, after RE1's. Run it when the scene list changes.

## Tools (`tools/`)

- `make_mod.py`: the whole pipeline (above).
- `mm2torch.py`: walks each room's display lists (display lists, vertices, textures, palettes,
  matrices) and writes Torch's declarations for them.
- `mm_scene.py`: the scene and room headers, as SoH XML.
- `soh_tables.py`: the scene and entrance tables.
- `nv_songs.py`: every game's ocarina songs, checked for clashes (cross-game; add a new song here first).

- `mm_rom.py`: checks the ROM against known releases (US N64 1.0 so far) and finds files in
  its file table. `python mm/tools/mm_rom.py <rom>` lists Clock Town's files.
- `yaz0.py`: decompresses the ROM's files.

## How the rooms come over (plan)

MM's scene format is OoT's with additions. From South Clock Town's headers, MM has:
- **Unchanged from OoT:** mesh (0x0A), collision (0x03), spawns, entrances, room list, paths,
  skybox, echo, time, room behaviour.
- **Same command, different contents:**
  - actors (0x01) and objects (0x0B) use MM's ids;
  - cutscenes (0x17) are a list, not a single pointer;
  - exits (0x13) use MM's entrance numbering;
  - lighting (0x0F) has MM's entries;
  - 0x19 marks a region visited.
- **MM only:** actor cutscenes (0x1B), the minimap (0x1C), minimap chests (0x1E), texture
  animation (0x1A).

So:
- **Meshes and textures:** SoH's own extractor (Torch, `torch/`) does them, the same way it
  does OoT's. A config generated for the MM ROM points it at Clock Town's room files, so
  display lists, vertices and textures come out in the formats SoH already loads.
- **Room and scene headers:** written by our converter, in SoH's formats.
  - OoT-compatible commands are kept.
  - Actor and object lists are remapped to the actors we've ported, or dropped.
  - Exits are remapped to our entrance table.
  - MM-only commands get our own resources once something uses them.
- **Paths:** all of MM's resources go under their own prefix, so nothing collides with OoT's
  (both games have, for example, a `gameplay_keep`).
