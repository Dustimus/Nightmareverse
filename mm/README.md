# Majora's Mask in Nightmareverse

Termina brought into Ocarina of Time's world, played as one game: one Link, one inventory, one
save. The design talk and the decisions made with the user so far are in `../NV_GAMES.md`
("Majora's Mask merged into OoT"). This file is the working record.

Same ground rules as every game here (`../NIGHTMAREVERSE.md`):
- no MM data in the repository;
- the player's own ROM is converted locally into `mm.o2r`.

**MM's code (decided with the user, 2026-11-20):** MM runs on the same engine family as OoT, so
its actors are ported from MM's decomp (zeldaret/mm, read through 2Ship2Harkinian's copy)
closely: the same logic, constants and order, adapted only where OoT's engine calls differ, and
noted in each file's header. That's fewer bugs than reimplementing from behaviour. It isn't the
RE1 approach, where the game had to be heavily adapted either way. Each ported file names the
MM actor it comes from.

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

**Models, checked against the two ROMs (2026-11-20):**
- **Pots:** MM's `object_tsubo` is byte for byte OoT's, so the same pot.
- **Torches:** `object_syokudai` has 2 bytes in 15,504 different, effectively the same.
- **Crates:** Clock Town's use MM's own `object_kibako`, which OoT doesn't have. Stood in by
  OoT's dungeon crate until MM's objects come over.
- **Doors:** MM gives each building its own door object (`object_wdor01`-`05`: the Inn and
  school, the Post Office and Trading Post, the Milk Bar, ...), picked per door. Stood in by
  OoT's generic door until MM's objects come over.

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

### What the Song of Time takes (decided with the user, 2026-11-20)

**Nothing from Link's inventory by default.** Rupees, ammo, bottle contents, items and heart
pieces all carry over, OoT's and MM's alike. The Song of Time only turns Termina back: the clock,
the per-cycle scene flags (chests, switches, cleared rooms), and later the town's events. The
user never liked MM taking rupees and ammo (keeping the bow but losing its arrows), and dropping
it makes MM's bank pointless but breaks nothing.

**A short list of event items that do reset with the cycle**, decided one by one as they're
ported. These are only items tied to an event the cycle undoes, where keeping them would leave
the world contradicting the player:
- Zora Eggs and the Deku Princess in bottles;
- small keys, since the dungeon's doors relock;
- the trade items (Room Key, Title Deeds, letters).

Rewards can't be duplicated either way, since they'll be permanent checks. Some are balance
questions for later, such as Powder Kegs. None of these items exist yet.

**Chests and other checks across cycles (reference, 2026-11-20).** MM's standalone randomizer
(MMR) keeps a location's flag through the Song of Time whenever it holds a real item (MM's own
table of scene flags that survive the cycle), so such a chest stays open. Only rupees, ammo and
a few bottle contents refill, and some spots give rupees on later cycles. It has an option to
return quest and trade items after the Song of Time. Here, the user leans towards chests not
resetting at all (OoT's rupees are plentiful and ammo isn't taken), to be weighed when chests
are ported.

**To check when MM's item models come over (the user, 2026-11-20): the Deku Stick.** As the user
remembers it, OoT's stick draws with the wrong texture: one that sits in Link's own object (part
of his hair?), so it looks different again with adult Link's object loaded. MM's stick has its
own. Compare the two games' stick display lists and where their textures point, and use MM's (or
fix OoT's) if that's right.

### MM's own actors, the first: the clock tower (2026-11-20)

**How a ported actor comes in:**
- **Its model:** MM's object file goes through Torch (`tools/mm_objects.py`). The object's
  display lists are named and placed as in the decomp, and walked for their vertices and
  textures. It comes out as `mm/objects/<object>/<symbol>`, and the actor draws those paths
  directly, with no object slot.
- **Its code:** our own C, to MM's rules (`soh/src/code/z_nv_mm_<actor>.c`), registered with
  SoH's ActorDB on first use (`NvMmActors.cpp`, by MM's id).
- **In the rooms:** the converter writes it as a marker id, 0x7000 + MM's id, with MM's params
  as they are (`mm_actors.py`, PORTED). The half-day hook spawns the registered actor in its
  place, so there's no new hook in OoT's code.

**The clock tower (`z_nv_mm_tokeidai.c`, MM's Obj_Tokeidai)**, all 16 placements in Clock
Town, following Termina's clock:
- the tower clock: the minute ring wiggles, then turns on to the next of its 30 places an
  hour; the clock face steps each hour; the sun and moon panel turns over at 6:00 and 18:00;
- the two exterior gears turn with the minutes;
- the counterweight on top turns slowly, its spotlight fading in at night and out by day;
- from midnight on the final night the tower stands open, as MM places it when the time is
  already past that (gear and counterweight moved, the clock face pushed in and tilted up, the
  staircase to the rooftop there).

Its other types are in too: Termina Field's small distant tower (scale 0.15), which is the one
for the idea of the Clock Tower seen from Hyrule (`NV_GAMES.md`), and the wall clocks indoors.

**Checked headless:** South Clock Town at 8:10 and 21:12. The clock face is on the tower, and at
night the lit torches stand beside it (half-days).

**Not yet:**
- the tower opening itself (its cutscene, the fireworks);
- the clock pieces falling as the moon crashes;
- the minute ring's tick (MM's own sound, not in OoT's banks);
- a close look at each piece in motion (the headless camera doesn't reach the tower's face).

### The moon (2026-11-20, `soh/src/code/z_nv_mm_fall.c`, MM's En_Fall)

Ported closely, with MM's models (`object_lodmoon`, `object_fall`):
- **Over Clock Town** (type 6, the far, low-detail moon, in all four town scenes): it grows
  each day (2.4, 3.6 times its 0.16 base), and on the final day it sinks towards the town as
  the hours pass, by MM's own formula. Past 9000 units from the camera it's drawn pulled in to
  9000 along the same line, so the far plane never clips it.
- **On the clock tower's rooftop:** the moon over the tower (type 9) stands still for now (its
  cutscene cues aren't in). The stopped moon (type 7) waits for its event, after the giants.
- **Checked headless:** its numbers on Day 1 (scale 0.384) and at 05:40 on Day 3 (scale 0.576,
  down from 11,000 to about 4,390 high), both MM's. Its model renders: shown once by a temporary
  test that drew it in front of the camera, not committed. The headless camera can't look up at
  it where it really hangs (some 80 degrees overhead in South Clock Town), so its look in the
  sky wants a check in play.
- **Not yet:** the crash (its cutscene, fire balls, debris, fire ring), the Moon's Tear, the
  open-mouthed moon, and the eyes' glow (only cutscenes change it).

### The first townsfolk: the carpenters (2026-11-21, `soh/src/code/z_nv_mm_daiku.c`, MM's En_Daiku)

- **Characters' models:** the object extraction now takes skeletons (flex, standard limbs:
  walked from the skeleton's limb table), limbs and animations, as well as display lists
  (`mm_objects.py`). `object_daiku` gives the skeleton, 21 display lists, 13 textures and 12
  animations. Drawn and animated by path with OoT's own SkelAnime.
- **Scene paths:** MM's path lists (0x0D) now come over as SoH's Path resource beside each
  scene (`<scene>_paths`, `mm_scene.py`), so actors that walk paths can.
- **The carpenters:** ported closely.
  - In South Clock Town by their half-days: one standing, three hammering (two swings in
    turn), three walking paths 5 and 6 carrying planks, each with its own hat and colours.
  - **Checked headless** on Day 1 at 10:00: the hammerer and the walker with his plank, the
    walker moving along his path between shots.
  - **Not yet:** talking (MM's text comes next; MM offers it for types 1-3), Kafei's mask's
    line, and the mayor's meeting flag for type 0.
- **A slip, caught:** at first the carpenter was registered under 0x14D, which is MM's
  `Obj_Entotu` (the rooftop chimneys). So "carpenters" stood on the chimneys' spots at y 553,
  out in the void. The right id is 0x09C. Check ids against the decomp's actor table, not
  against counted lists.

### MM's text (2026-11-21, `tools/mm_text.py`, `soh/soh/Enhancements/Nightmareverse/NvMmText.cpp`)

- **Where it comes from:** MM's message table, in its `code` file (US 1.0: 0x1210D8, 4,589
  messages), pointing into `message_data_static`. Each message has an 11-byte header (box type,
  position, icon, next id, two prices), then ASCII with MM's control codes.
- **Converted to OoT's codes** when the mod is made:
  - colours (silver as white, orange as yellow, OoT having neither);
  - newline, box breaks, shift, the player's name, quick text, events, persistent, delayed box
    break, fade;
  - two and three choices; the button and stick icons.
  - Left out: MM's counters, timers, codes, prices, and sound effects (MM's own ids).

  One blob resource, `mm/text/messages` ("MTXT", sorted by id).
- **In the game:** a Termina actor asks for MM text id N as `0xB000 + N`, a range clear of OoT's
  ids and RE1's 0xA1E0. SoH's OnOpenText hook fills OoT's text box from the resource, as
  NvMessages.cpp does for RE1's. `NvMmText_Next(id)` gives a message's next id, for actors
  that chain.
- **Checked headless:** walked up to South Clock Town's standing carpenter on Day 1 and pressed A.
  His line came up word for word ("This poster's an eyesore! What a sorry collection of
  cowards!! Bluh-yek! Foo!"), and the clock stood still while he talked.
- The carpenters talk now (MM's day-by-type lines, offered as MM offers them). Kafei's mask's
  lines wait for that mask.

### The gate guards (2026-11-21, `soh/src/code/z_nv_mm_stopheishi.c`, MM's En_Stop_heishi)

Ported closely, MM's soldier model (`object_sdn`), one in each of Clock Town's four gates:
- **While Link may not leave,** the guard steps sideways along the gate to stay in his way
  (within his reach of the post; a tall, wide collider), and goes back to his post when Link
  walks off.
- **Talking to him with a sword** runs his whole conversation (the second line follows the
  first), and MM's week event 12_20 lets Link through from then on. After that the guard stands
  with a small collider and his lines change.
- **His lines** are MM's, by gate, day, night and whether Link has a sword.
- **His pose:** hand on hip by day, looking down at night, hand on chest on the final day, when
  he also looks about nervously.

**MM's week event flags** came in for it (`z_nv_mm_scene.c`: MM's 100-byte weekEventReg, flags as
MM numbers them, e.g. 12_20 = byte 12, bit 0x20), in the save's `cycleWeekEvents`, cleared by the
Song of Time as MM clears most.

**Checked headless,** South Clock Town, Day 1:
- walking into the gate without talking, the guard blocks and Link never gets past;
- after the whole conversation (seven presses: "It is dangerous outside the town walls...
  ...A sword? ... My apologies, sir. It was wrong of me to treat you like a child."), Link walks
  round him and out of the south gate, to the "not built yet" note;
- with more presses, the next talk gives his "may leave" line ("The southern swamp at Woodfall
  lies in this direction. Be careful."), as in MM.

**Not yet:** the transformations' lines (no forms yet), Takkuri's stolen sword, the
final-day head turns at each earthquake (no earthquakes yet), and his footstep sound (MM's
own).

### The Bombers (2026-11-22, `soh/src/code/z_nv_mm_bombers.c`, MM's En_Bombers)

The blue-hatted kids in East and West Clock Town, ported from MM's En_Bombers with their model
(MM's `object_cs`: skeleton, 17 animations, eye and number textures, named in `tools/mm_objects.py`).
As in MM it's the same four kids placed three times each, one spot per day (half-days 0C0, 030,
00C): params & 0xF is the kid, plus one, which picks the number on their back (the town's show
2-5; 1 is Jim, red bandana). The bandana's colour is MM's own two one-line lists, rebuilt in the file.

- They wander within 75 of their spot (line checks keep them off walls), look about, and talk:
  MM's lines for a human Link, its yes/no, and with the Notebook (84_80) the other set and the salute.
- They're gone once the hide-and-seek game is played (73_10, 85_02); then the ones caught sit at the
  hideout in North Clock Town (type 1, 76_01-76_10). The game itself, the hideout and Jim wait.
- Left out for now: the Deku/Goron/Zora lines (no forms yet), MM's mask reactions, their footstep
  sound (MM's own), the hideout handover (75_40).
- Tested headless in West Clock Town, Day 1 08:00: the kid walks about, turns to talk, and the
  conversation runs through the yes/no to the notebook lines.

### MM's own props: chimneys and crates (2026-11-22)

The rule from here on: in Termina, MM's own props, not OoT's look-alikes, even where they're near
identical. (Group 1 above put OoT's pots, crates and torches in; they're being swapped for MM's.)

- **Chimneys** (`soh/src/code/z_nv_mm_entotu.c`, MM's Obj_Entotu): the camera-facing chimney
  cards on Clock Town's roofs (East, West, North, South), MM's `object_f53_obj`. Smoke rises at
  mealtimes (6-9, 11-14, 16-20 on Termina's clock), each chimney a random few minutes off. The
  smoke's seven vertices are the actor's own data in MM (its overlay file, ROM file 299), brought
  over by `tools/mm_objects.py` like an object (`mm/objects/ovl_Obj_Entotu`), copied and faded.
- **Small crates** (`soh/src/code/z_nv_mm_kibako.c`, MM's Obj_Kibako): MM's crate (`object_kibako`
  outdoors, MM's `gameplay_dangeon_keep` one indoors) with MM's square shadow (MM's `gameplay_keep`).
  Lift, put down, throw, break, sink, as in MM; the drop is MM's drop table turned into OoT's
  collectibles (MM's compass, spore mushroom and big fairy have no OoT drop; MM's three hearts
  give one). The Bombers' hide-and-seek crates (bit 7) are crates until the game comes.
- **Torches** (`soh/src/code/z_nv_mm_syokudai.c`, MM's Obj_Syokudai): MM's stands (`object_syokudai`,
  three by type) and flame (MM's `gameplay_keep` gEffFire1DL), MM's params as they are. Lit by a
  stick or fire arrow, lighting sticks and arrows, groups on a timer, put out by water, as in MM.
  MM's torch cutscene queue is OoT's look-at (as OoT's torches); the flame takes OoT's weapons'
  damage flags. The stands draw with textures from MM's dungeon keep: `mm_objects.py` walks every
  object first, following an object's `"deps"` into the other file, then writes each yml.
- **Pots** (`soh/src/code/z_nv_mm_tsubo.c`, MM's Obj_Tsubo): MM's pot (`object_tsubo` outdoors,
  MM's dungeon keep's indoors) and the big green magic pot (`object_racetsubo`: the clock tower's
  rooftop has four). Lift, throw, break, sink, MM's shard bursts per kind, MM's circle shadow; the
  drop is MM's table (shared with the crates: `NvMm_Drop`), type 3 OoT's random drops. Not yet:
  the Boe and Gold Skulltula pots (MM's z-rotation 1, 2), the big-fairy drop.
- The crates' and pots' damage flags are OoT's own crates' and pots': what hits them, and what
  they hit when thrown, are OoT's weapons and enemies, whose flags mean other things than MM's.
- MM's keeps sit in their own segments (4 for `gameplay_keep`, 5 for `gameplay_dangeon_keep`):
  `mm_objects.py` takes a `"segment"` for them.
- Tested headless in East Clock Town: the crate shows, lifts (overhead, MM's model), goes down,
  and breaks to the sword, dropping its green rupee; a chimney (moved into view for the test)
  draws with its smoke at 7:20; West Clock Town's torch burns at 20:00 (its night copy); a rooftop magic pot breaks to the
  sword and drops its magic jar.

### MM's schedules, and MM's doors (2026-11-22)

**Schedules** (`soh/src/code/z_nv_mm_schedule.c`, MM's z_schedule.c). MM asks small scripts where its
people should be and whether its doors are open: checks of the day, the time, the scene, week events
and a few items, ending in a value, two times and a value, or nothing. The decomp writes them in a
little language (`.schl`, compiled by a tool of its own). `tools/mm_schedule.py` is our compiler for
it, to MM's own bytecode (commands 0x00-0x12): the scripts are the decomp's, copied into
`mm/schedules/` with the actors that use them (doors, the Postman, Anju so far), and their
compiled form is committed in `soh/src/code/nv_mm_schedules/` (the decomp is only needed to make
them again: `mm_schedule.py <decomp> soh/src/code/nv_mm_schedules mm/schedules/*.schl`). Week
events and scenes are resolved from the decomp's headers; MM's scenes become ours, and one not
brought over yet matches nothing. The item checks (Room Key, Letter to Kafei, Romani Mask) are
false until those items come.

**Doors** (`soh/src/code/z_nv_mm_door.c`, MM's En_Door): MM's door skeleton and opening animation, and
the door face by kind: the shops' own (`object_wdor01-05`) or MM's plain door. MM's door types:
closed by days, dawns or nights with MM's messages, small-key locks, ajar doors, and the shops'
doors by their schedule (`mm/schedules/En_Door.schl`: the Post Office is closed before 9:00, out
delivering 9-12, ...). Fitting Link's door code, which is OoT's:
- OoT's player opens a door through the first fields of OoT's EnDoor (animStyle,
  playerIsOpening): the port keeps those at the same place; it goes through the scene exit when
  the door's params say OoT's DOOR_SCENEEXIT, so MM's params are kept aside and the actor's are
  OoT's (a scene exit where both sides are the same room: every Clock Town door);
- a closed door's message comes from the door offering to talk (OoT has no talking door);
- transition actors (doors) are written 0x1000 + MM's id in Termina's scenes; one small hook in
  `Actor_SpawnTransitionActors` (z_actor.c) asks `NvMmActor_TransitionId` for the SoH id.
Tested headless in West Clock Town: the Post Office door at 8:00 gives MM's "Post Office, 9 a.m.
to 3 p.m."; at 12:30 it opens (MM's door face, MM's animation), and Link goes through to the
"not built yet" note.

### The Postman (2026-11-22, `soh/src/code/z_nv_mm_pm.c`, MM's En_Pm; `z_nv_mm_subs.c`)

MM's Postman on his rounds, by MM's schedule (`mm/schedules/En_Pm.schl`, five scripts for the stages
of his day's story), his model MM's `object_mm`. He's placed in each Clock Town scene, and the
schedule says where he is: walking a path between two times, going through a door (he holds our
ported door open with its openTimer), or standing at a mailbox; otherwise he isn't there.
- MM's people follow paths by the clock (time pathing: a B-spline through the path, the progress
  by the time): ported in `z_nv_mm_subs.c` with MM's path chains (each MM path names the next of the
  same person's routes; OoT's paths don't, so `mm_scene.py` writes them beside the scene's paths,
  `<scene>_pathinfo`, "MPTH"), and `NvMmClock_Speed` for how fast the clock goes.
- Not yet: indoors (the Post Office, the inn, the Milk Bar) and the people there; the mailboxes
  (MM's En_Pst: he stops at their spots, and checking one sets MM's week events, the Letter to
  Kafei's too); his footstep sound (MM's own). He talks through MM's message scripts (below).
- Tested headless in West Clock Town, Day 1 from 8:59: he comes out of the Post Office at 9:00 and
  runs his path toward South Clock Town by 9:09, cap and mail bag on.

### MM's message scripts (2026-11-22, `soh/src/code/z_nv_mm_msgevent.c`, MM's z_msgevent.c)

How MM's people talk: small scripts (MM's MsgScript, its MSCRIPT_CMD_* macros in
`soh/include/z_nv_mm_msgevent.h`, written in each actor's file as in MM) that show lines, wait,
branch on week events, the day and night, Link's form, choices, rupees, items, times, and set
flags. The actor runs its script each frame while talking; it stops where it has to wait and goes
on next frame. All 51 of MM's commands are in. As here so far:
- text ids are MM's, shown as ours (0xB000 + id); Link is MM's human form; MM's items, masks,
  songs and quest items are checked as absent until MM's items come; rupees are the shared wallet;
- MM's eventInf (a session's temporary flags) is kept by the interpreter; switch and collectible
  flags are OoT's;
- the Bombers' Notebook isn't in yet: meeting someone sets MM's week event for it (as MM does),
  so it's there when the notebook comes;
- the conversation camera turning to the other person (MM's Actor_ChangeFocus) is OoT's
  talkActor.
The actor's text id stays 0, so OoT's player starts no textbox of its own: the script does.

**Week events across cycles.** The Song of Time used to clear all of MM's week events; MM keeps
some (`sPersistentCycleWeekEventRegs`: the notebook's, the bosses beaten, the owl statues...).
`z_nv_mm_scene.c` now keeps those (154 flags, from the decomp's table by name); a new game clears all.

Tested headless in South Clock Town: at the mailbox at 9:35 on Day 1, the Postman says MM's "I'm
currently on the job. If I stop to talk, it will disrupt my schedule." through his MM script, and
the notebook's "met the Postman" is set.

### The mailboxes (2026-11-22, `soh/src/code/z_nv_mm_pst.c`, MM's En_Pst)

Clock Town's five red mailboxes: MM's model (`object_pst`; the slot drops while in use), MM's
schedule (`mm/schedules/En_Pst.schl`: each is out of reach the four minutes the Postman empties
it) and MM's message scripts. MM's letters and showing an item mid-talk aren't in yet, so MM's
item prompt (its text 0xFF, "Press START to open the Select Item Screen") is skipped by the
script runner and the box goes on as when it's closed in MM: "Deposit mail here." then "Thank you
for using the postal system." The letter scripts are MM's, ready for the letters. The Postman
again needs the box there to stop at it (MM's rule), and the notebook's events now set all of
MM's notebook week events (its 55), not only "met".
Tested headless in South Clock Town at 9:00: the box offers "Check" and runs MM's lines.

### Anju, outdoors (2026-11-22, `soh/src/code/z_nv_mm_an.c`, MM's En_An)

Anju by MM's schedule (`mm/schedules/En_An.schl`), MM's model (`object_an1`, her eyes and mouth by
MM's face logic, segments 8 and 9) and umbrella (`object_an2`). Most of her three days are inside
the Stock Pot Inn, which isn't built, so only her outdoor part is in: at lunchtime on Day 2 she
comes out of the inn's front door (East Clock Town; our door held open for her) under her
umbrella and walks through East and South Clock Town toward the Laundry Pool, and back in the
afternoon. Walking she doesn't talk, as in MM. After the Couples' Mask ending she's gone from
town (MM's week event 51_40). The rest of En_An (the inn, the tray, the broom, the masks, her
conversations and their message scripts) comes with the inn.
Tested headless in East Clock Town, Day 2 from 11:28: she comes out of the inn door and walks
off with her umbrella at 11:41.

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
