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

### The 3-day clock (2026-11-19, `soh/src/code/z_nv_mm_clock.c`)

Running in Termina's scenes only (`NvMm_IsScene`). Hyrule's time is put aside on the way in
and given back on the way out: each place's days work their own way (the user, 2026-11-19).
The user would consider Hyrule taking on longer days one day too, "carefully weighed". The
user's take (2026-11-19): MM's days are shorter than expected (about 18 minutes, against OoT's
4 or so in Hyrule Field), so not much trouble conceptually; "probably going to be the solution",
not yet, with a few base-game tweaks where needed. The places to check are what waits
for a time that isn't a clean noon or dusk, and the Weird Egg, which hatches at the next dawn
(early in the game, before the Sun's Song). It
could also be a base for other games' time systems (Monster Rancher's weeks).

**MM's rules, from its decomp (our own code):**
- Time moves 3 units a frame (`R_TIME_SPEED`), about 18 real minutes a day, and stands still
  while paused, in a message, during a scene change or game over.
- The day counts up at 6:00. Dawn of the fourth day is the moon's fall.
- The Termina rooms carry no time increment (`mm_scene.py`), so OoT's own clock stands still
  there, and the clock gives OoT its time each frame, sky included.
- Anything else that moves OoT's clock moves Termina's forward by as much:
  - **the Sun's Song works as the Song of Double Time** (to the next 6:00 or 18:00);
  - **RE1's Moonlight Sonata skips to the hour it sets:** midnight today, 19:01 once RE1 changes
    it (below; Termina may get its own target).
- **The Song of Time** (once its message closes) goes back to the dawn of the first day, at
  South Clock Town's clock tower door.

**On screen:** a text line, "Day N hh:mm", and "Dawn of the ... Day" as each day starts.
Placeholders for MM's clock and its day titles.

**Checked headless (`NV_MM_SPEED`):** three full days, then the moon's fall resets to Day 1
at the tower. The light follows the time.

**Not yet:**
- the night sky (the sky's own colour stays daytime; the lighting does follow);
- the Inverted Song of Time and the Song of Double Time's own notes (the shared song listener
  comes first);
- the final hours (music, the countdown);
- the moon's fall itself (for now it's a Song of Time);
- what resets with the Song of Time (Termina's flags, once there are any);
- headless checks of the songs (they need ocarina steps in `NV_WALK`).

Dev: `NV_MM_TIME="day,hhmm"`, `NV_MM_SPEED=n`, `NV_LOG` ("NVMM ...").

### Where it stands (2026-11-19)

**Step 1 works.** All six scenes load in SoH (headless dev boot, `NV_BOOT_ENTRANCE=mm_z2_clocktower`
and so on), with MM's rooms, textures and collision, and Link standing and walking on them.

Not done yet:
- **Characters and objects:** MM's actor and object lists are left out of the rooms.
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

### Saving (2026-11-20, `soh/soh/Enhancements/Nightmareverse/NvMmSave.cpp`)

Decided with the user (2026-11-19): Termina is the **"termina" section of SoH's own save file**,
not a separate file. SoH saves and loads every section in the same moment (SaveManager, as the
randomizer and its trackers do), so Hyrule and Termina always come from the same save: nothing
can be taken from Termina and kept while loading an older Hyrule, or the reverse. Each game keeps
to its own section (sections are by name; SoH refuses a second register of one).

Two parts, so the Song of Time can never undo an item:
- `cycle`: what the Song of Time resets (day, time, speed; later the town's events).
- `permanent`: what it never resets: items received, randomizer checks taken (empty so far). A
  chest the Song of Time refills gives something else once its check is taken, as MM's
  randomizer does.

**A save made in Termina:** OoT's own time in the file is Termina's then, so the section also
keeps Hyrule's, and loading puts it back (the loaded game starts in Hyrule, at OoT's respawn).

**A save from before Termina** (no section): the first dawn, not whatever the last file left
behind (the OnLoadGame hook).

**Checked headless** (`NV_MM_SAVETEST=save`, then `=load`): Day 2 21:35 saved in Termina, with
Hyrule at noon, came back as Day 2 21:35, with Hyrule's noon back in OoT's time. The file holds
`termina` next to `base`, `sohStats` and the trackers' sections.

Not yet: respawning in Clock Town (owl statues) instead of OoT's respawn.

### Exits (2026-11-20)

Each scene's exit list is MM's own (0x13), with MM's entrance numbers turned into ours
(`mm_entrances.py`, shared by the table generator and the converter so they always agree). The
number of exits is the highest exit index the collision's surfaces use.
- **A way into a place we have** goes there: Clock Town's six scenes reach each other as in MM.
- **A way into a place not built yet** (Termina Field, the Laundry Pool, the shops, ...) goes
  to a "back" entrance: the same scene, at its start position nearest that exit, which is the
  spot that place's own exit would bring Link to. "The rest of Termina isn't built yet. You turn
  back." shows once the fade is done (`z_nv_mm.c`), as RE1's unbuilt doors say so.
  - The back entrances are a second block after the arriving ones
    (`ENTR_MM_BACK_FIRST..LAST`); as a place gets built, its exits stop using them.

**Checked headless:** walking east out of South Clock Town arrives in East Clock Town. Walking
out of the south gate (Termina Field) comes back to the gate, with the note.

Not yet: doors (MM's door actors, 0x0E transition actors, need actors first); the ways in
that need a form or an event (the clock tower's door opens on the final night).

### Actors, group 1: what OoT already has (2026-11-20, `tools/mm_actors.py`)

Clock Town places 67 kinds of actor (233 in all) and 14 doors. In order of work:
1. **What OoT has its own version of:** pots, crates, torches, collectibles, wooden doors.
2. **The clock tower and the moon:** MM's own, tied to the clock.
3. **The townsfolk on their schedules.**

**Group 1:**
- **The converter writes OoT's actor in place of MM's** (`mm_actors.py`, MAP: the actor, its
  params translated, the OoT objects it needs), in the rooms' actor and object lists and the
  scenes' door lists.
- **Rotations follow MM's own rule:** the id's three flags say raw value or degrees.
- **Anything not mapped is left out.** `make_mod.py` lists what's missing by MM id.

**In now:**
- the pots on the rooftop and the crates in East Clock Town;
- the two heart pieces (South Clock Town's on the clock tower deck, North's in the tree), now
  that Termina's pickups are saved (below);
- the torches, in their day/night pairs (lit ones night-only, half-day mask 0x155; unlit ones
  day-only, 0x2AA), by the half-day rule below;
- the 14 wooden doors, as OoT's scene-exit doors. Their way out is the exit behind them, so a
  door into a building not built yet brings Link back with the note.

**Held back:**
- **Chests and signs.** They wait for the item rules and MM's text.

**MM's half-days (built 2026-11-20, `soh/src/code/z_nv_mm_halfday.c`).** Every placed MM actor
has ten half-day bits (day 0 night to day 4 night, in its rotations' low bits; 0 means always).
MM spawns it only in those half-days, and when the half-day turns (6:00, 18:00) it spawns the
newly due and removes the ones whose time is up. Here, to MM's rules:
- **The masks:** the converter writes each room's masks beside it, `<room>_halfdays` ("MHDY",
  a count, one u16 per actor entry). Only rooms with a limited entry get one.
- **Spawning:** OoT's room spawn (`z_actor.c`, one hook) skips the entries not due in the
  clock's half-day (`NvMmClock_HalfDayBit`: the day turns at 6:00, so a night belongs to the
  day before it, as in MM).
- **At each turn**, the room's actors whose entry isn't due any more are removed (found by SoH's
  actor list index), and the entries due now that weren't before are spawned. As in MM, a pot
  broken earlier stays broken.
- **Checked headless:** West Clock Town from 17:45, sped up. At 18:00 the three unlit daytime
  torches went and the three lit night ones came; loaded at 20:00 it starts with the night ones.
- **Not yet:** MM's extra rule for actors flagged 0x800 after a Song of Time (EVENTINF_17), and
  half-days for the doors (MM's door types close shops by day; they're all open here).

**Hearts (decided with the user, 2026-11-20): they stack.** OoT's 20 and MM's 20 give 37 in all
(3 to start), and SoH already draws more rows and moves the magic meter. Balance can come later.
The same goes for Double Defense being in both games.

### Saved pickups (2026-11-20, `soh/src/code/z_nv_mm_scene.c`)

OoT saves per-scene flags (chests, switches, cleared rooms, collectibles, visited rooms) for its
own 124 scenes only. Scenes added after them get throwaway flags (`z_nv_scene.c`,
`NvScene_SavedFlags`). Termina's scenes now get their own, kept in the `termina` save section
(one added line in `NvScene_SavedFlags`; the RE1 chat should know, since the file is shared).
Saved by scene name, so renumbering doesn't break saves.
- **`permanent`:** collectibles (heart pieces and the like), visited rooms and floors. Never
  reset.
- **`cycle`:** chests, switches, cleared rooms. Reset by the Song of Time and the moon's fall,
  as in MM. The scene Link is in drops its live ones too, so leaving it doesn't write them back.
- **OoT's own limits:**
  - collectible flags 0x00-0x1F are saved and 0x20-0x3F last one visit;
  - MM has 7 bits where OoT has 6.

  A heart piece or container with a flag past 0x1F is left out rather than reappear on every
  visit (none in Clock Town).

**Checked headless (`NV_MM_SAVETEST=save`, `load,save`):**
- a fresh game picks up South Clock Town's heart piece on the deck (count 0 to 1), and the save
  holds its flag (`permanent`, flag 10);
- loading that save and walking back onto the deck, it doesn't come back (the count stays 1).

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
  - **Termina may want its own target** (the user, 2026-11-19): a clean midnight, as the user
    remembers MM3D doing, against Hyrule's 19:01. To settle when MM's songs are built. The
    clock can't tell the Sonata from the Sun's Song by the time alone, so a Termina-only
    target needs RE1's Sonata to hand the skip to the MM clock in Termina's scenes (one call)
    instead of setting OoT's time itself.
  - **The user, later the same day: the Sonata should go to 19:01 (passed to the RE1 chat),
    not midnight, in Hyrule.**
    That fixes most of the waiting that longer days would add in Hyrule (Dampé's tour, evening
    setups), and suits the name. RE1's side sets the hour. Termina's clock follows whatever it
    sets, with no change needed here.
  - **Decided with the user (2026-11-19): in Termina it's a time skip.** It jumps the 3-day
    clock to that day's midnight. Warping by the Song of Time's variants alone often isn't
    enough, and MM3D has a similar choose-your-hour feature.
  - **A clash, fixed on RE1's side (re1/foyer 7cc6721b, NvSong_Reset).** Its listener keeps the last 8 notes across ocarina
    uses and clears them only on its own match. So the Song of Time (R A D R A D), then the
    Song of Time started again (R), completes A D R A D R: midnight. `../tools/nv_songs.py`
    finds it.
- **Songs from every game: one list, checked for clashes** (`../tools/nv_songs.py`, decided with
  the user that clashes matter, 2026-11-19: probably at least one song per game).
  - A song clashes when its notes appear in a row inside another's, since the ocarina listens
    as the notes are played.
  - Today: OoT's 12, RE1's 2 and MM's 9 new ones, with no clashes, apart from the leftover-notes
    case above.
- **Pause menu.** RE1 replaces the map page with its status and map screens in RE1's rooms
  (z_kaleido_scope_PAL.c, z_kaleido_setup.c). MM's mask screen and Bomber's Notebook would hook
  the same place, gated by Termina's scenes the same way.
- **Saving.** RE1 keeps its state in its own file next to each SoH save
  (`Save/nv_re1_<file>.sav`, NvSaveFile.cpp), saved at its typewriters. MM doesn't follow
  that (below, "Saving").
  - With items mixed across games in a randomizer one day, a file saved apart from OoT's
    could be reloaded against a newer Hyrule. Worth raising with the RE1 chat when the
    randomizer comes up; not urgent while RE1's items stay in the mansion.
- **The scene check** (above): fixed on RE1's side (a50cbcca): `NvRe1_IsScene()` with both ends.
  MM has the same, `NvMm_IsScene()` (`soh/include/z64scene.h`, its range written by
  `soh_tables.py` to `nv_mm_scene_range.h`). Merged re1/foyer into this branch (2026-11-19,
  with the user's okay) to pick both up.

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
- `mm_entrances.py`: where each scene's entrances sit in the table, and MM's entrance numbers
  turned into ours (shared by `soh_tables.py` and `mm_scene.py`).

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
