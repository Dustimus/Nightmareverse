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

## Decisions (read before starting anything new)

Every decision made with the user for the MM side, with where it's written out. Check here
before building a new system; add a line whenever a new one is made.

**Which Link model is on screen (check before drawing anything on Link; getting this wrong cost real
work, the user, 2026-11-24):**
- **Young Link in Termina is MM's own model** (`soh/src/code/z_nv_mm_link.c`: MM's skeleton, body,
  face, hands, waist swapped in on each Termina scene's start). MM's own display lists for him (his
  hands holding things, the shields on his back with their matrices) go on **as they are**: no
  re-placing, no mixing in OoT's hands. Ask `NvMmLink_IsMm()`.
- **Young Link outside Termina is OoT's model.** MM's items on him need OoT's hands and placements.
- **Adult Link is always OoT's model.**
- **The forms are MM's models** (Deku, Goron, Zora, the Fierce Deity: their skeletons swapped in by
  z_nv_mm_form.c). The Fierce Deity is young Link underneath (OoT's child rules need switching off
  for him, as his height and legs, z_player_lib.c hooks) and wears OoT's adult swords and shields
  cut from OoT's adult lists.

- **One game:** one Link, one inventory, one save; MM's content becomes part of OoT's, no
  switching games (`../NV_GAMES.md` "Majora's Mask merged into OoT", 2026-11-19).
- **Porting:** MM's actors ported closely from the decomp; MM's own props and models in MM's
  areas, even when OoT has the same one (below, and the user, 2026-11-22). Trivial is fine: no
  balancing against the other games (NV_GAMES.md "Movement").
- **Saving:** a "termina" section of SoH's own save file (below, "Saving", 2026-11-19).
- **The clock:** runs only in Termina; the Sun's Song is Double Time there, RE1's Moonlight
  Sonata skips to its hour (below, "The 3-day clock").
- **The Song of Time takes nothing of Link's** but a short list of event items (Zora Eggs, the
  Deku Princess, small keys, trade items). Rupees, ammo and bottles are never lost (below, "What
  the Song of Time takes", 2026-11-20). Chests: leaning towards not resetting (same section).
  The Song of Time only acts in Termina; in Hyrule it's OoT's.
- **Time's speed:** the Inverted Song of Time opens a list: a third (N64), half (3DS), normal,
  double, triple; the clock shows the speed when it isn't normal. The Song of Double Time is
  left alone, for its 3DS behaviour (choosing the hour) later (2026-11-23).
- **Songs:** one shared listener for every game's songs, `z_nv_songs.c`, written by the RE1 side
  on re1/foyer (2026-11-23); MM's songs register there. Songs from every game in one list,
  checked for clashes (below, "Where RE1 and MM meet").
- **Items both games have:** one item, one slot; a check that would give a second copy gives a
  reward instead. Exceptions that add something: MM's Hero's Bow and Hookshot are young Link's
  own (one shared quiver) (NV_GAMES.md "Items both games have", 2026-11-19). The songs both
  games really have are the Song of Time, Epona's Song and the Song of Storms (MM's Saria's and
  Sun's Songs are unused leftovers). **Masks are separate items for now**, Keaton, the Bunny Hood
  and the Mask of Truth too: OoT's are trade items, never all held at once. They'll be combined
  eventually, once the user decides what's given in their place (the user, 2026-11-23).
- **Hearts stack:** OoT's and MM's heart containers and pieces together (below, 2026-11-20).
- **Masks and forms:** forms don't depend on age (leaning yes); OoT's people react to the
  Goron and Zora forms as to the masks; OoT-only masks join MM's mask screen; a fake Majora's
  Mask at the end; the Fierce Deity's sword and size (NV_GAMES.md, 2026-11-19 to 21).
- **The forms' transformation scene:** MM's rule, from the decomp: plays every time, skippable
  once watched (into a form), always back to human (NV_GAMES.md, checked 2026-11-23). The
  proposal stays: skipped by default, a setting to watch it.
- **The forms in z_player.c:** our own action functions in our files, started from one-line hooks
  in z_player.c; the user's go-ahead without asking RE1 first ("it can adjust if it needs to",
  2026-11-23). Deku Link's spin is on A, his bubble on B (as MM).
- **The swords and the second Equipment page (2026-11-24):** see NV_GAMES.md ("The swords, decided", the shields with them).
  Built: `soh/src/code/z_nv_gear.c`, the gear page (a plain 24-slot page under OoT's Equipment, before
  MM's masks; MM's mask frame with the title left off, `gNvPauseBlankTop10Tex` made by mm_pause.py).
  Row 1: Kokiri, Master, Giant's Knife, Biggoron's, Great Fairy's, Double Helix; row 2: Deku, Hero's,
  Hylian, MM's Mirror, OoT's Mirror. A takes one (B for a sword); worn ones outlined as on OoT's page;
  the wrong age's greyed. The Knife and Biggoron's owned apart (save section "gear", NvGearSave.cpp;
  OoT's one slot holds the one in hand); Medigoron sells a Knife whenever there's no whole one
  (z_en_gm.c hook, not in a randomizer). Our pages' cursors now step once a push, repeating when held
  (`NvPause_Steps`; MM's pages had moved every frame). Dev: NV_OOT_SWORDS=both, NV_GEAR_TAKE="3,4".
  **The Fierce Deity's shields** (adult Link's, the decision): the Hylian and OoT's Mirror, from the
  gear page or OoT's (its age check lets him have OoT's Mirror). Drawn from OoT's adult lists with
  the hand's and the sheath's triangles left out by texture (`NvMmForm_FdOotShield`; OoT interleaves
  the shield's and the hand's parts): in his own fist when the sword's out or he guards, on his back
  otherwise (he has no sheath). He's young Link underneath, so OoT's child-with-the-Hylian rules (on
  the back, over the head) are switched off for him (`Player_IsChildWithHylianShield` hook,
  z_player_lib.c). The Deku Shield isn't his (none shown). Changing out of him with OoT's Mirror on,
  young Link gets the Hylian back (or the Deku, or none).
  **He'd stood sunk into the ground** (the user saw it, 2026-11-24): OoT draws young Link's body
  lower (the root's height times 0.64) and fits his feet to the floor with a child's leg lengths,
  and the Fierce Deity is young Link underneath; both now take adult Link's for him (z_player_lib.c:
  the root's scale and func_8008F87C's legs, `NvMmForm_IsFierceDeity`).
  **The Great Fairy's Sword on B** (the decision: a B sword, not MM's C item): young human Link takes it
  from the gear page; OoT's two-handed slot stands in for it (B holds Biggoron's item, the slot set
  unbreakable), so it swings as Biggoron's, two-handed, for Biggoron's damage; drawn as MM's sword in
  OoT's child fist (`NvMmForm_OverrideLimb`, as MM's own hand is its fist and the sword), B shows its
  icon (`NvGear_BIcon`, z_parameter.c's B icon hook; the Double Helix's icon for the Fierce Deity
  too). The slot's own Knife/Biggoron's come back as they were when another sword's taken (here or
  on OoT's page). Changing into the Fierce Deity gives him his Double Helix in its place.
  Adult Link takes it too (the user, 2026-11-24), in his own fist. Not yet for it: its own damage,
  MM's draw-it-from-nowhere (it's on no sheath; nothing shows on his back).
  **MM's Hero's and Mirror shields** on young Link (human; not the Fierce Deity, not adult Link): OoT's
  Hylian slot stands in for the Hero's (metal; OoT's child-with-the-Hylian rules off for it, so it's
  held in hand as MM holds it, `Player_IsChildWithHylianShield` hook), OoT's Mirror slot for MM's
  Mirror (it reflects as OoT's). Drawn as MM's. In Termina young Link is MM's own model (z_nv_mm_link.c), so MM's own lists go on as
  they are: its hand holding the shield, its shield "with matrix" from the sheath (the user: no
  placing of our own needed there; MM's and OoT's animations are the same data, checked, so it sits
  as MM's data has it, as in MM). In Hyrule he's OoT's model:
  OoT's child fist and MM's shield in hand; on his back OoT's sheath, then OoT's own placing of a
  shield there (the matrix OoT uses for the Deku Shield, `NvMmForm_MmShieldOnBack`), with MM's
  shield first turned into that matrix's space ((x, y, z) -> (y, -z, -x - 100), measured on OoT's
  Deku Shield, texture coordinates too), which hangs it as MM's own data hangs it in Termina. Checked
  against MM (the user's screenshot, 2026-11-24): the Hero's Shield's Triforce is under the bird by
  its design (MM's icon shows it so), not above it as on OoT's Hylian; that's the right way up. (I
  had judged it by the Hylian's crest and "fixed" it upside down for a while.) The Great Fairy's Sword the same:
  MM's own hand holding it in Termina, OoT's fist and MM's sword elsewhere and on adult Link.
  Known: taking OoT's Hylian on OoT's page while the Hero's is on keeps showing the Hero's (the
  same slot underneath).
  Tested headless: both on his back walking in Clock Town (MM's own placing); in hand only glimpsed.
  Not yet: the Razor and Gilded, a
  title for the page.
  Tested headless: the page, young Link taking the Kokiri Sword and the Deku Shield (outlines move),
  the Fierce Deity taking the Knife, Biggoron's, the Knife again, the Double Helix (the Kokiri Sword
  refused for him); his Hylian and Mirror shields on his back walking and in his fist guarding.
- **The Fierce Deity:** usable anywhere; adult Link's size except in boss rooms (MM's 1.5 there); his
  beam as a light arrow to start (a sword strike: maybe later) (NV_GAMES.md, the user, 2026-11-24).
- **Adult Link and MM (an idea, not to start yet):** sequenced through OoT's ending
  (NV_GAMES.md "Another way round the adult-Link question", 2026-11-20).
- **The Deku Stick's model** to check when MM's item models come (below, 2026-11-20).
- **Young Link in Termina is MM's own model** (2026-11-23); in Hyrule OoT's for now. Moving MM's
  model (and MM's held items) everywhere is likely once MM's items are used outside Termina.
- **Commits and branches:** this side works on its own branch; re1/foyer is RE1's; merges only
  when the user asks; no game data in the repository.

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
  South Clock Town's clock tower door. Only when played in Termina: in Hyrule it's still OoT's
  (the Door of Time, the blocks). What it resets: "What the Song of Time takes", below.

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

Owl statues: a save made at one resumes there (below, "Clock Town's scenery"). Not yet: dying or voiding out in Termina still goes to OoT's respawn.

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

In code since 2026-11-23 (`z_nv_mm_getitem.c` NvMmItem_EndsWithCycle), with the Moon's Tear and
the Pendant of Memories read as trade items too; everything else MM's Sram_SaveEndOfCycle takes
(the wallet, ammo, bottle contents, the Powder Keg) stays.

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

### Guru-Guru (2026-11-25, `soh/src/code/z_nv_mm_guruguru.c`, MM's En_Guruguru)

The music box man, MM's model (`object_fu`, his two cranking animations, his eyes and mouths). By day
in his room at the inn (params 0), at night in the Laundry Pool (params 1); a third (params 2) is his
music through the inn's wall. At night he tells his story line by line, cranking faster as it goes,
angry on the line MM makes him angry on, and at its end gives the Bremen Mask (MM's get-item, once a
cycle: 38_40), then his last line; Deku Link gets MM's Deku lines. The Notebook's events (met him, the
mask) are set. Not yet: his Song of Storms and its speeding up (MM's audio).
Tested headless in the Laundry Pool at 20:00: the whole story, "You got the Bremen Mask!", his last
line, and talking again after (his line for once the mask's given).

### The Rosa sisters (2026-11-25, `soh/src/code/z_nv_mm_rz.c`, MM's En_Rz)

Judo and Marilla, MM's model (`object_rz`: one model; each sister's trim, red or blue, its animated
materials drawn at step 0 or 1, MM's AnimatedMat_DrawStep, now `NvMmMatAnim_DrawStep`). On the first two
nights they practise their dance in West Clock Town and won't be bothered (MM's lines); a third pair walk
their path stopping to think (MM's lines in turn); after Link's shown them Kamaro's dance they sit at the
inn and thank him. The heart piece: MM's, given when Link dances Kamaro's dance in front of them (the dance: see "The
Bremen march and Kamaro's dance"). Their music waits
for MM's audio; MM's cutscene (kneeling, applauding on cue) is plain talk; once they've learnt his dance
MM draws them with Link's dance animation, kept as their own here.
Fixed with them: animated-material colour entries of MM's simplest kind (a colour per frame, no key
frames) were skipped, leaving their segments unset (a crash drawing the sisters).
Tested headless in West Clock Town at 20:00: both dancing, the red and blue trims, MM's "Don't bother
us!!!", the Notebook's "met".

### The jugglers (2026-11-25, `soh/src/code/z_nv_mm_ja.c`, MM's En_Ja)

The two brothers of Gorman's troupe, MM's model (`object_boj`, red or blue by params & 3 through segments
8 and 9, their eyes on segment 0x0A). MM's schedule (mm/schedules/En_Ja.schl): days 1 and 2, by day
juggling to each other in East Clock Town (four balls thrown between them on MM's arcs), at night at the
inn's table with cards in hand; gone on day 3. Their room entries carry the same half-days (town: day,
inn: night). MM's message scripts: each brother speaks in turn (MM's FOCUS_TO_CHILD to the other).
Left out: their heads turning to Link (MM's SubS_UpdateLimb), as for the inn's Goron.
Tested headless: juggling at 12:00 in East Clock Town, their joke, both at the inn's table at 20:00.

### Clock Town's bells, the mannequin and Mutoh (2026-11-25, `soh/src/code/z_nv_mm_townprops.c`)

- **The Laundry Pool's bell** (MM's En_Cha, `object_cha`): rung by a hit, swinging on MM's damped
  spring. The first ring sets week event 51_04; 60_04 (set by others) rings it too. MM's cutscene on
  that first ring waits for Kafei (En_Test3).
- **East Clock Town's big bell** (MM's Obj_Bell, `object_f52_obj`): swung by hits, harder by the
  Goron's punch and harder still by explosions (MM's damage effects 15 and 14, here OoT's nearest
  kinds: the hammer's and explosives'), knocking Link back when it swings into him. MM's spheres are
  OoT's jointed spheres with one element each, moved with the swinging bell as MM does.
- **The wedding dress** (MM's En_Drs, `object_drs`), Anju's room, the Moon Mask on top (`object_msmo`)
  until the Couple's Mask scene starts (week event 87_02).
- **Mutoh** (MM's En_Muto, `object_toryo`): in South Clock Town on the last night (MM's lines, Deku
  Link's, Kafei's Mask's); in the mayor's room (params 1) for the meeting, which isn't in yet, so
  there he only talks.
- Sounds: OoT has no door bell or big bell; its ship bell stands in until MM's audio.
Tested headless: both bells hit and swinging, the mannequin and mask, Mutoh's "Cowards! All of you!"

### MM's message values and number inputs (2026-11-25, `soh/src/code/z_nv_mm_msginput.c`)

MM's text codes for values and number entry, which the text conversion had dropped:
- **Values**: `tools/mm_text.py` keeps each as a marker (0x7F, then MM's code); `NvMmText.cpp`
  writes it out as the message opens, as MM's decoder does: the time, the time and hours until the
  moon falls or the next dawn, the rupees chosen or in total (" Rupee(s)" after), a held item's
  price, the Bombers' code. Others (minigame scores, stray fairies, the Spider House's code...) show
  nothing yet. (MM's codes 0x0B-0x0F had been passed through as OoT's own controls: fixed with it.)
- **Inputs**: the bank's amount (3 digits, the cursor on the last), the dog race's bet (2), the
  Bombers' code (5 digits 1-5, starting 11111), the lottery's numbers (3). The digits are written
  as text, each behind its own colour code, and the box waits at its end (OoT's event code), as
  MM's input boxes. While it waits the stick or D-pad changes the selected digit (repeating while
  held) and moves between them, as MM's Message_Control*Input; the selected digit is red (MM draws
  a flashing box behind it). Each change rewrites the digit and its glyph slot in place. The actor
  reads the digits when A goes on (`NvMmMsgInput_Digit`, `NvMmMsgInput_Value`).
  Hooked from Message_Update (one line).
- **The Bombers' code**: MM's, five different digits 1-5 made with a new file (Sram_
  GenerateRandomSaveFields), saved ("bomberCode"); a file from before gets one on load. Dev:
  `NV_MM_BOMBERCODE=12345` fixes it.
- MM's "control pad" icon is the stick (OoT's 0xAA), not OoT's D-pad glyph, which OoT's font lacks.

### The Bombers' hideout guard (2026-11-25, `soh/src/code/z_nv_mm_bombers.c`, MM's En_Bombers2)

In front of the hideout's way in (East Clock Town), asking for the code with MM's code box; a
correct digit stays correct between tries while he's talked to, as MM. Right, he steps aside (week
event 73_80), salutes and stays aside (also when Link comes out of the hideout). Goron and Zora Link
get MM's other line. His stepping aside is his actor cutscene, MM's shot (2026-11-27); the masks' reactions wait.
Tested headless: a wrong code ("Wrong!"), the right one, his stepping aside and next lines.

### The bank (2026-11-25, `soh/src/code/z_nv_mm_bank.c`, MM's En_Ginko_Man)

West Clock Town's teller, MM's model (`object_boj`: the troupe's body, his green colour in segments
8 and 9, his own head) and lines. MM's bank: deposits stay through the Song of Time (saved,
"bankRupees"), up to 5000; amounts chosen in MM's bank box (z_nv_mm_msginput.c). The first deposit
stamps Link's hand (MM's account lines). Rewards as MM: 200 banked, the next wallet (MM's Adult's
Wallet: OoT's wallets are shared, so the next of OoT's; with the Giant's already, a blue rupee);
1000, a blue rupee; 5000, a piece of heart. At night a withdrawal costs MM's 4 rupee fee; a
withdrawal must fit the wallet. Rupees aren't lost to the Song of Time in Nightmareverse, so the bank
matters less than in MM, but it's MM's and keeps its rewards.
Left out: his knee-slap and the stamp's sounds (MM's own, not in OoT's banks).
Dev: `NV_MM_BANK=100` starts with a balance.
Tested headless: a first deposit of 100 through MM's lines, the box, the stamp and the balance ("a
total of 100 Rupees"); a returning customer's menu, the stamp check, the withdrawal box.

### The Bremen march and Kamaro's dance (2026-11-26, `soh/src/code/z_nv_mm_march.c`, `z_nv_march_follow.c`)

MM's Player_Action_11 and 12. With the Bremen Mask or Kamaro's Mask on, human Link's B is MM's:
held on the ground he marches (MM's clink_normal_okarina_walk, walkB moving, his pace rising and
falling with the beat) or dances (MM's alink_dance_loop); let go, he stands. Hooked at the top of
OoT's action handler lists (z_player.c, one line), ahead of the sword, as MM's B with these masks.
Others see them as MM's player flags: `NvMmMask_Marching`, `NvMmMask_Dancing`. MM's music for each
(NA_BGM_BREMEN_MARCH, NA_BGM_KAMARO_DANCE) waits for MM's audio.

**Who follows the march.** In MM the following is each follower's own code: a dog (En_Dg) and
Grog's chicks (En_Nwc) watch for the march flag and fall in behind Link; Kafei and Igos's knights
react to it too. Here OoT's animals follow it as well (the user's call): a dog (En_Dog) or a
wandering cucco (En_Niw) near Link (300) when he marches joins a line behind him, each following the
one ahead at its own gap, waiting when close; when the march stops they go back to what they were
doing. The line (up to 8) is `NvMarch_Follow`, asked from each one's update (a few lines in
z_en_dog.c and z_en_niw.c); MM's own followers will use it too.

The dance is what the Rosa sisters wait for: danced in front of them, MM's heart piece.
Dev: `NV_MM_WEAR=46` wears that MM mask (MM's item id, hex) from the start, anywhere.
Tested headless: the march in East Clock Town (the mask on his head, his pace), Kamaro's dance for
the Rosa sisters (the heart piece given), a Kakariko cucco and a Market dog following the march.

### The dogs (2026-11-26, `soh/src/code/z_nv_mm_dg.c`, MM's En_Dg)

South Clock Town's dog, MM's model (`object_dog`) on its path, stopping to bark. As MM, to Link's
forms: it chases and jumps at Deku Link; backs away from, runs from and barks at Goron Link; runs up
to Zora Link and sits by him. Human Link can pick it up and throw it (OoT's carrying); with the Mask
of Truth on it speaks its mind (MM's 0x627). The Bremen march: it joins OoT's animals' line
(`NvMarch_Follow`) in place of MM's one-dog following. Left out: swimming, the dog race's dogs (with
the race), MM's own dog sounds (OoT's bark, steps and evening cry stand in).
Tested headless: running its path, following the march (sitting when Link stops), back to its path.

### The Bombers' game (2026-11-26, `soh/src/code/z_nv_mm_bomgame.c`)

MM's hide-and-seek, four of MM's actors with the Bombers' model (`object_cs`):
- **The balloon** (En_Bombal, `object_fusen`): over North Clock Town, Jim shooting at it with his
  blowgun and missing. Popped by OoT's ranged hits (arrows, seeds, the hookshot, the boomerang, magic
  arrows; Deku Link's bubble hits as a seed). After a talk with it gone, Jim blows another.
- **Jim** (En_Bomjima): his lines by Link's form and what's happened (MM's tables). With the balloon
  popped, a Deku or human Link can play: yes, and the other four run in round him; then the game
  starts (week events 73_10 or 85_02, the caught count cleared) and Link is back at North Clock
  Town's spawn 6, as MM.
- **The five hidden** (En_Bomjimb, while the game's on): wandering, running from Link (turning from
  walls and the town's exits), taunting from afar, dazed when hit, hiding under MM's crates (the crate
  hops, he slips under, it shakes; lifted, he runs), one riding a cucco down from a roof (OoT's
  cucco). Touch one to catch it: MM's line by the count ("Now there are only 4 left"), MM's caught
  order kept (saved with the cycle); a Goron or Zora Link, or the wrong form's game, gets MM's "not
  playing". The fifth takes Link to the hideout (the room's exit; week events 75_40, 83_04).
- **The line-up** (En_Bom_Bowl_Man): at the hideout the sitting Bomber number 1 starts it (the
  handover z_nv_mm_bombers.c had left out). The five stand in a row spelling the Bombers' code; Jim's
  lines; a human Link gets MM's Bombers' Notebook; each turns round and shows his number in turn;
  Jim says the code. Deku Link is told the code (73_20). Then North Clock Town afresh (84_80).
  In East Clock Town, coming out of the hideout with the code entered and no notebook, one runs up,
  gives it and runs off on his path, as MM.
MM's actor cutscenes (2026-11-27): the balloon popping, Jim's look as he notices, the four running in.
Not yet: the line-up's (MM's line-up Bomber never fills its cutscene list, so it plays the scene's
cutscene 0, whose camera is one of MM's global ones, ITEM1: not driven here yet; Link is turned to
face the line-up) and the run-off's. MM's Bombers' sounds aren't in OoT's banks; Deku Link hiding in a flower (which stops
them running in MM) waits for that.
Dev: `NV_MM_POP=300` pops the balloon after that many frames; `NV_MM_WE="85:02,75:40"` sets week
events with a new game.
Tested headless: Jim's lines with the balloon popped, the four running in, the game starting; a
Bomber caught ("Aw! You caught me... Now there are only 4 left."); the line-up's lines and the
notebook given.

### The Bomb Shop lady and Sakon (2026-11-26, `soh/src/code/z_nv_mm_sakon.c`)

The first night in North Clock Town, MM's own (En_Baba `object_bba`, En_Suttari on `object_boj` with
his own head), with MM's schedules (mm/schedules/En_Baba.schl, En_Suttari.schl: the night-one branch):
- **The lady** walks home with the shop's new Big Bomb Bags on MM's time path (0:00-0:30); Sakon waits
  (18:00-0:20), then walks up behind her (0:20-0:30). At 0:30 she's knocked over and he snatches the
  bag: her "Ouch! Watch out!", then "Stop! Thief!!!", then MM's chase along his escape route (his path's
  additional path, reached point by point as MM: past the plane through each).
- **Stopping him**: a sword hit (or a punch, a spin) and he drops the bag (week event 33_08 when he's
  away); an arrow, a seed, the hookshot, the boomerang or an explosion finishes him (79_40, MM's "Sakon's
  dead"). Either way, once he's away or gone, the night goes on at North Clock Town's spawn 7.
- **Afterwards** she speaks first: the bag got back, her thanks and MM's Blast Mask; finished, her "I've
  learned my lesson"; got away, "What a shame...". Then the night carries on (81_02).
- **In the Bomb Shop**, her lines with her son by what's happened (MM's; her son isn't ported, so the
  turns to him are just her lines).
The snatch is MM's two cutscene shots (2026-11-27). Left out: the chase music and their
voices (not in OoT's banks), the dropped bag falling (MM's En_Nimotsu: it's simply gone from his
hands). Sakon on other days (the Curiosity Shop, the Sun's Mask, Ikana) waits for Kafei's story.
Tested headless: the walk and the snatch at 0:30, the chase over his 16 points, the lady's lines after
he got away; a sword hit as he ran past (the bag dropped), her thanks and the Blast Mask.

### Clock Town's buildings (2026-11-26, `tools/mm_rom.py`, `tools/mm_entrances.py`)

Fourteen more of MM's scenes, appended after the first eight (so every entrance number already in
use stays as it was): the Milk Bar, the Bomb Shop, the Trading Post, the Curiosity Shop, the Mayor's
Residence, the Post Office, the Treasure Chest Shop, the Town Shooting Gallery (MM's SYATEKI_MIZU),
the Swordsman's School, Honey & Darling's, the Lottery Shop, the Bombers' hideout with the Astral
Observatory (TENMON_DAI), the Great Fairies' fountains and the grottos (both shared, many rooms).
Their file indices: the decomp's spec order, less four (checked against the known ones). The doors
into them now lead in; who's inside comes person by person (most aren't ported yet).

### MM's camera settings (2026-11-26, `soh/src/code/z_nv_mm_camera.c`)

Each floor's camera setting comes from MM's collision, in OoT's layout but MM's numbering: read as
OoT's it was mostly something unrelated (MM's DUNGEON3, all over Clock Town, was OoT's Bongo Bongo
camera; MM's CIRCLE0 in the Curiosity Shop was OoT's twisted-hallway cutscene camera, and crashed
with no cutscene). Now, in MM's scenes, MM's setting becomes OoT's of the same original name where
OoT has one (NORMAL0-2, DUNGEON0-2, FIXED0, CIRCLE0, CIRCLE3) and OoT's NORMAL0 otherwise (MM's
ROOM0/1, DUNGEON3, FIXED2/3, the minigames' own). Hooked where OoT reads it (z_bgcheck.c, one line).
Tested headless: the Curiosity Shop (no crash), the Mayor's Residence, the inn, East Clock Town.

### The Lottery Shop (2026-11-26, `soh/src/code/z_nv_mm_kujiya.c`)

MM's En_Kujiya in West Clock Town's Lottery Shop (`object_kujiya`: the drum, its scrolling-texture
matanim and ten DLs). By day: 10 Rupees for a ticket, three numbers picked in MM's lottery box
(message 0x2B5F; digits start at 111, up/down changes one, left/right moves; A confirms, read as the
message's event), one ticket a day (week event 33, 0x10/0x20/0x40 by day); the booth turns closed
after. From 18:00 it turns open; talking then gives the announcement (0x2B64, 0x2B65 with Link's
numbers and the day's, a short pause) and the purple rupee (MM GI 0x05) if all three match. Each
day's winning numbers are made with a new file (MM's Sram_GenerateRandomSaveFields), kept with
Link's guess in the save's permanent part (NvMmSave.cpp). Value codes 0xDC/0xDD and input 0xD5 in
`z_nv_mm_msginput.c`. Tested headless: buying (140 Rupees after, guess 230 from the stick), the
evening announcement (a loss: "000" against 850). The booth turning is its actor cutscene, MM's shot of
it, with MM's windmill-level sound (2026-11-27).

### The Mayor's receptionist and the Postman's letter (2026-11-26, `soh/src/code/z_nv_mm_recep.c`)

- **En_Recepgirl**, at the desk in the Mayor's Residence (MM's `object_bg`): MM's lines (welcome,
  the Mayor's office on the left, the drawing room on the right, again if asked; the meeting's
  over once week event 60_08 is set; Kafei's Mask's line), her switch flag (params), animations,
  blinking, and her head turning to Link (OoT's limb override in place of MM's transform draw).
- **En_Mm2**, the Postman's letter to himself, on his bed in the Post Office on the final night only
  (its half-day in the room data). Its model is in the actor's overlay file (ROM file 235): the two
  textures and vertex arrays come over as an object (`mm/objects/ovl_En_Mm2`), and its short display
  list (MM's sEnMm2DL) is rebuilt in C from the overlay's commands. "Check" when close and facing it.

Tested headless: the receptionist's first talk (0x2AD9, 0x2ADA, 0x2ADB) and the letter (day 3 21:00).

### The Treasure Chest Shop (2026-11-26, `soh/src/code/z_nv_mm_takaraya.c`)

MM's En_Takaraya (the gal at the counter, MM's `object_bg`) and Obj_Takaraya_Wall (the board,
`object_takaraya_objects`). Her greeting and price by Link's form (Deku 10, human and the Fierce
Deity 20, Goron 30, Zora 5: MM's message prices, which aren't in our message data, so written in),
MM's "next message" chains followed by hand (`NvMmText_Next`). Paying spawns the board: as MM's, a
path carved at random from the chest's square in the back row to the front; the other squares'
walls rise around Link as he walks and sink behind him, and two OC cylinders hold him back. The
chest is OoT's En_Box (MM's switch-flag big chest, type 11 in both; MM's model, z_nv_mm_box.c)
holding the prize by form, the second time a smaller one (MM's switch flags: params >> 8 plus the
form): purple rupee, Goron's piece of heart, Zora's red rupee, Deku's nuts. 45 seconds on OoT's main
timer; the chest opened, or the time's up, and Link is back at the counter (MM's entrance 1) where
she asks "Did you have fun?" or "Won't you try again?".

The chest appearing is MM's shot of it, pulling back toward Link (her actor cutscene, 2026-11-27:
MM's chest starts it as it appears; OoT's chest has none, so she starts it, and OoT's own chest camera
stays off in Termina, a line in `z_en_box.c`).

Left out or adapted: the masks' reactions (MM's face-reaction tables), the closing-time kick-out,
MM's minigame music; the walls' sound is OoT's rising block.

Tested headless: paying (20 Rupees as human), the board and timer, walls rising in Link's way,
"Time's up!", back at the counter and paying again; winning (the chest's column fixed by the dev
switch `NV_MM_CHEST_COLUMN`): the Purple Rupee, and back at the counter.

**Chests face the other way in OoT** (found here): MM's En_Box turns its *shape* half round at init
and opens from the side its data faces; OoT's turns only its *world* rotation, so its model and
opening side face the data. MM's placed chests came over facing backwards; `mm_actors.py` now turns
them half round (and the shop's spawned chest is turned in the code).

### The Swordsman's School (2026-11-26, `soh/src/code/z_nv_mm_kendo.c`)

MM's En_Kendo_Js (the swordsman, `object_js`) and En_Maruta (the practice logs, `object_maruta`).
His lines by day, form and mask, then the courses (MM's three-way choice):
- **Novice Course** (1 Rupee): Link is walked to his spot, a log rises, and the seven lessons
  (side jump, roll, back flip, horizontal cut, vertical cut, thrust, jump attack) are checked
  against Link's moves, with MM's "Wrong!", retries and hints (put the sword away; don't Z-Target).
- **Expert Course** (10 Rupees): five rounds of two logs rising at random from his path's points;
  1 point a cut, 2 a thrust, 3 a jump attack; all 30 for the piece of heart (week event 63_20), a
  red rupee after.
On the final night (day 3 after 23:00) he cowers in the back room and his sign stands at the counter.

The logs rise through the floor, turn to face Link, and the swing that hits one cuts it by MM's
table into MM's pieces (eight chunks making nine shapes), which fly off, spin, bounce, settle and
flatten away as MM's.

Adapted: Link's moves are read from OoT's player (MM's side jump and back flip are OoT's hop,
`PLAYER_STATE2_HOPPING` and its direction; MM's roll flag is OoT's roll action, asked through
`NvPlayer_IsRolling`, a few lines at the end of `z_player.c`; the swings are OoT's of the same
names). MM walks Link to his spot by setting the control stick; here the stick is set the same way
(`NvMmInput_Apply`, run with the dev walker's input in `z_nv_dev.c`). MM's minigame counter is a
"Points" line over the HUD (`z_nv_mm_clock.c`). The Expert Course is seen through MM's course camera
(his cutscene's camera made the main camera while it runs: `NvMmCs_MainCameraFromCutscene`, 2026-11-27).

Tested headless: the novice course's first lesson (a wrong move: "Wrong!", the hint, retry) and the
expert course (paid, walked to the start, five rounds, cuts scored, "Start over" short of 30). Not yet
tested: the right moves for each novice lesson, a full 30 (the prize).

The dev walker's `choice:N` now also reads on through event boxes on the way to the choice.

### MM's cutscene cameras (2026-11-27, `soh/src/code/z_nv_mm_csmgr.c`)

MM's actor cutscenes, for every MM actor that asks for the camera. In MM an actor queues its cutscene
(its csId from the scene's actor entry, or one of the additional ones chained after it), waits until
it's next (one at a time, by priority), starts it and stops it; the cutscene's entry (the scene's
list, command 0x1B) names one of the scene's cameras (command 0x02): a setting and its data. MM's
manager makes a sub-camera for it and on the stop hands the view back.

- **Data** (`tools/mm_scene.py`): each scene's lists as "<scene>_scene_csmgr" ("MCSM": MM's 16-byte
  ActorCutscene entries, then the cameras: setting, data count, data), each room's actor csIds as
  "<room>_csids" (MM's `rot.y & 0x7F` per actor entry, which the converted rotation loses);
  `z_nv_mm_halfday.c` records each ported actor's csId as it spawns it (`NvMmCs_ActorCsId`).
- **The manager**: MM's `CutsceneManager_*` line for line as `NvMmCsMgr_*` (queue, next by
  priority, start with or without Link held, stop, length, additional ids, the global cutscenes),
  updated each frame in Termina (`z_nv_mm.c`); letterbox and HUD as MM's entry says.
- **The camera**: OoT's sub-camera (set to OoT's FREE0 so OoT's own camera code leaves it be), driven
  each frame with MM's camera math for the setting: `Camera_Fixed1` (FIXED1, FIXED2, RFIXED1: a set
  shot) and `Camera_Fixed2` (CIRCLE2, CIRCLE3, CIRCLE8: a fixed eye turning to follow its target;
  `NvMmCs_SetTargetActor` is MM's `Camera_SetTargetActor`). MM's `Camera_ChangeActorCsCamIndex` on
  the main camera (a cutscene's camera outside any cutscene, as the Swordsman's course) is
  `NvMmCs_MainCameraFromCutscene`, asked each frame it's wanted. In Clock Town's scenes those are all the
  actor cutscene cameras but the entrance views (START0/START2, `Camera_Unique0`) and one in the
  bowling alley. Other settings (FREE0) leave the view for the actor to place, as MM's `Camera_Unique6`.
- MM's render-mode segment (0x0C, `Scene_SetRenderModeXlu`): MM's empty one is four end-of-lists,
  and some models call it at +0x10; ours were one long, so the Goron at the inn read past it and
  crashed as he walked in at 15:45 (2026-11-27). Dev: `NV_MM_NODRAW="276,1D5"` hides those MM
  actors (by MM id), for bisecting a bad draw.
- MM's waiting list is cleared each frame once the next is chosen (MM's `CutsceneManager_ClearWaiting`
  after `_Update`); actors queue again each frame they still want theirs.
- Not yet: MM's scripted cutscenes (their camera splines), the entrance views, MM's global
  cameras (negative camera ids: ITEM0-3, CONNECT0, ...: left where they are), MM's camera collision
  check, roll.
- Ported actors using them so far: the Mayor's meeting, the Treasure Chest Shop, the Swordsman's
  course, the Lottery Shop's booth, the hideout guard, the Bombers' game (balloon, Jim, the four),
  Sakon's snatch, the inn's desk talks (Anju with Granny, the Goron, the Postman). Still to bring
  over: the Rosa sisters, the clock tower's doors, the owl statues' opening, the bells.

Tested headless: the Mayor's meeting, each line in MM's own shot (Viscen before the soldiers, the
mayor at his desk, Mutoh with the carpenters), letterboxed, back to the normal camera after.

### The Mayor's meeting (2026-11-26, `soh/src/code/z_nv_mm_mayor.c`, `z_nv_mm_meeting.h`)

MM's En_Dt (Mayor Dotour, `object_dt`), En_Baisen (Viscen, `object_bai`) and En_Heishi (the
soldier, `object_sdn`); Mutoh (`z_nv_mm_townprops.c`) now takes his part too. Talking to the mayor,
Mutoh or Viscen in the office starts the meeting's talk with the mayor leading: MM's lines in order,
the others turning to whoever speaks (MM's focus table), the mayor's look and animation by the stage,
week event 60_08 once heard; after that the short repeat ("All must take refuge!!!" / "On with the
carnival!!!"). With the Couple's Mask: the resolution with everyone's reactions; Mutoh and Viscen
leave (63_80), Link comes back in through the door (MM's entrance 1), and the mayor thanks him with
the piece of heart (60_10, notebook events). After that Viscen talks with the soldier in the lobby;
on the final night the mayor sits alone (60_40 once heard). Kafei's Mask gets MM's line.

MM's En_Dt reaches into En_Muto and En_Baisen to steer them; here each keeps MM's
`cutsceneState`/`targetActor` in a `NvMmMeetingRole` the mayor reaches through `z_nv_mm_meeting.h`.

Each line is shown by MM's own shot for it (MM's sStringIdCsIndexTable: the mayor's cutscene and its
additional ones, z_nv_mm_csmgr.c below). The office's music and crowd murmur aren't in OoT's banks.

Tested headless: the first meeting (lines 0-8), the repeat (9-10), and the resolution with the
Couple's Mask (11-20), the return through the door and the piece of heart.

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

### Clock Town's scenery (2026-11-24, `soh/src/code/z_nv_mm_scenery.c`)

MM's own, ported as they are, MM's models and collision (`tools/mm_objects.py`):
- **The carnival platforms and flags** (Obj_Tokei_Turret, `object_tokei_turret`): South Clock Town's
  tower going up over the three days (its pieces are placed in MM's half-days: 2 on day 1, 3 on day
  2, 5 on day 3), the base and the platform top stood on, the flag strings over the square.
- **The branching tree** (Obj_Tree, `object_tree`): North Clock Town's; its top stood on, its trunk
  solid, swaying (MM's sway) when hit by the sword, a stick, a spin or a Goron hit.
- **West Clock Town's market stalls** (Bg_Market_Step, `object_market_obj`): the day and the night
  stalls and the bank's adverts, each in its half-days.
- **The magic mushrooms** (Obj_Kinoko): seen only with the Mask of Scents on, glowing (gameplay_keep's
  glow). Not yet: catching one in a bottle.
- The object files: MM's object table (in `code`) gives each object's ROM address, matched to the
  file table (object_tokei_turret 991, object_tree 999, object_market_obj 920). Grass (En_Kusa) and the
  square signposts (En_Kanban) are their own actors, still to come.
- **Grass and bushes** (En_Kusa, `soh/src/code/z_nv_mm_kusa.c`): MM's bush (MM's gameplay_field_keep, near
  and fading far), grass and stump (`object_kusa`), flying leaves, MM's wind sway; cut, lift, carry,
  throw, regrow (type 1) as in MM. Cut by OoT's weapons (OoT's own grass's flags); the random drop is
  OoT's random table at MM's index (MM's tables aren't over); bugs wait for MM's bugs. Clock Town's are
  the Laundry Pool's three bushes. (The Keaton's ring is MM's En_Kusa2, another actor: none in Clock
  Town; it comes with the fields.)
- **Square signposts** (En_Kanban, `soh/src/code/z_nv_mm_kanban.c`): MM's sign is OoT's own, line for line
  but for the forms' hits and the mending song, so it's OoT's code with MM's model (`object_kanban`, its
  wood from MM's gameplay_keep), MM's text (0x300 + params) and no young-Link offset. Read, cut into
  pieces (flying, bouncing, floating), whole again on coming back. Not yet: mending with the Song of
  Healing (MM's songs). North and East Clock Town have one each.
- **Owl statues** (Obj_Warpstone, `soh/src/code/z_nv_mm_warpstone.c`): MM's model (`object_sek`, closed and
  open), MM's text, opened by a sword hit (MM's light from its beak, then open; OoT's chime, OoT has no
  owl sound), kept open for good (the termina section's `permanent`: `owls`, MM's owlActivationFlags).
  Talking to an open one: MM's "save your progress and quit" and its yes/no; yes saves the one file, as
  the pause menu does, marked as made at that owl (`owlSave`); loading that file starts at the owl's
  spawn (MM's sOwlWarpEntrances: Clock Town's is South Clock Town's spawn 9) with Termina's clock as it
  was, instead of OoT's respawn (NvMmSave.cpp's load hook). The mark holds while Link stays by the owl
  (SoH writes the file on a thread); any save after he leaves it is an ordinary one.
  **To settle with the user:** MM's owl save is one-time (gone once loaded) and quits to the title;
  here it's a normal save and the game goes on, and MM's own text (warning that reloading loses the
  progress, the Song of Time restart) describes MM's rules, not these. Also: owl statues in other
  places (the user's idea, 2026-11-25: later, with how the map works). Not yet: the Song of Soaring.
  **Saves: on hold (the user, 2026-11-25):** they want owl saves, but not disconnected from the rest;
  an idea: in Termina the ocarina's save (with whatever else it does there) saves to the owl slot by
  default. To work out later; the owl save above stays as it is meanwhile.
- **Hidden rupees** (En_Hit_Tag, `z_nv_mm_scenery.c`): invisible spots (South Clock Town's tower, three
  on East Clock Town's walls) giving three green rupees the first time they're hit, as MM.
- **The clock tower's pieces** (`soh/src/code/z_nv_mm_ctower.c`): inside, the cogs turning and the water
  wheel turning and splashing (stood on); the twisting corridor (it turns as Link walks along it) and its
  two stone doors (they slide shut behind him); the swinging double doors (pushed open by walking into
  them); in South Clock Town the tower's front, whose steps drop at midnight on the last night (MM's
  cutscene's drop, as it plays out) and are down when he arrives after. MM's cutscene cameras left out;
  the sounds OoT has none of are OoT's nearest. The organ only shows in MM's cutscene (kept hidden).
- **Loading halls** (MM's En_Holl, `tools/mm_actors.py`): the tower's corridor and main room are two
  rooms joined by one; the converter now writes them as OoT's En_Holl (OoT's wide invisible hall for
  MM's visible one, which OoT hasn't at that width). Before, the main room never loaded.
- Tested headless: through the corridor into the main room (cogs, water wheel, doors and stone doors
  spawned, the wheel drawn); the steps' drop at midnight on day 3 by the log only (not yet seen).
- Left for later: the invisible sound emitters (Obj_Sound: MM's ambient sounds and music, with MM's
  audio); the bugs (En_Insect, with bottles).
- Dev: NV_POS takes a height (`x,z,yaw,y`) and NV_POS_AT=frames places Link after a gate's walk-in;
  NV_MM_SPAWN="mmid,params[,dist]" (hex) spawns a ported MM actor in front of Link (NV_MM_SPAWN_AT).
- Tested headless: the carnival tower and flags on day 3 (South), the stalls by day (West), the tree
  (North), each by a shot with it drawn and one with it switched off; a Laundry Pool bush drawn and
  cut by the sword; a signpost read ("Deku Scrub Playground down this hole") and cut in two; South Clock
  Town's owl opened by the sword, talked to, saved at (the file: `owls` 16, `owlSave` 4), and that file
  loaded (dev NV_MM_SAVETEST=load,resume: the file select's load hooks and entrance) starting at the owl,
  Day 1 as saved. Not yet seen: standing on the platforms or the tree, the tree's sway, a mushroom,
  lifting a bush.

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

### Anju (2026-11-22, `soh/src/code/z_nv_mm_an.c`, MM's En_An)

Anju by MM's schedule (`mm/schedules/En_An.schl`), MM's model (`object_an1`, her eyes and mouth by
MM's face logic, segments 8 and 9), umbrella (`object_an2`), broom (`object_an3`), tray and
chopsticks. Now that the inn and the Laundry Pool are built, most of her three days are in: the
front desk, cooking (she tastes when spoken to), sweeping, waiting for closing time, her walks
through the inn's halls and doors (with her tray, her broom), out the front door at lunchtime on
Day 2 under her umbrella to sit at the Laundry Pool (crying there once Link has talked to her), and
the midnight meeting. She talks through MM's message scripts with MM's callbacks.
- **Her scripts** are the decomp's, from its En_An file by `tools/mm_msgscripts.py`
  (`soh/src/code/nv_mm_msgscripts/En_An.msg.inc`): it copies an actor's MsgScript arrays and turns
  the names in them (week events, notebook events, MM's items, get-items, masks) into MM's numbers
  from the decomp's headers. Items stay MM's numbers: the script runner gives what both games share
  as OoT's (rupees, hearts, heart pieces) and skips MM's own items for now (the Room Key, the Letter
  to Kafei), going on as if given.
- **Not yet**: her results with Granny (lunch), her mother (the staff room), the Goron and the
  Postman at the desk (those people aren't ported: those times she isn't there, as in MM without
  them); MM's mask and Deku lines; the midnight meeting's cutscene (she sends Link to the inn's
  entrance 3, as MM does, without it); the faces MM sets line by line and her head turn.
- **Talking across the counter**: as in MM, the counter keeps Link farther than her talk range;
  Z-targeting lifts it (OoT's talk code does the same as MM's). The headless walker can hold Z now
  (`NV_HOLD_Z=frame`, z_nv_dev.c).
Tested headless: Day 1 at 8:00 at the desk (locked on) she runs MM's "Um... Are you... staying the
night?" with its Yes/No, to "I'm terribly sorry! There are no vacancies today..."; Day 2 at 14:20
she sits at the Laundry Pool under her umbrella; her Day 2 walk out of the inn as before.

### Anju's mother (2026-11-22, `soh/src/code/z_nv_mm_ah.c`, MM's En_Ah)

MM's model (`object_ah`, her eyes and mouth by MM's blinking), schedule (`mm/schedules/En_Ah.schl`)
and message scripts (`nv_mm_msgscripts/En_Ah.msg.inc`): at the inn's desk on Days 2 and 3, and with
Anju in the staff room on Day 2's night (Anju's own staff-room results, waiting for Kafei and
hearing her mother out, are in now too). Her bow and sad face on MM's lines 0x28FD and 0x2954.
Her Day 3 night at the ranch house waits for that scene; her head turn (MM's transform draw) is
left out. Tested headless: Day 2 at 9:00, locked on at the desk, MM's "I'm sorry. Our rooms are..."

### Anju's grandmother (2026-11-22, `soh/src/code/z_nv_mm_nb.c`, MM's En_Nb)

MM's model (`object_nb`), schedule (`mm/schedules/En_Nb.schl`) and message script
(`nv_mm_msgscripts/En_Nb.msg.inc`): in her room at the inn, with MM's "Now, which story would you
like to hear?" choice. Either story puts Link to sleep (MM's func_80BC01DC without the All-Night
Mask): the Carnival of Time costs two hours (`NvMmClock_Jump`), the Four Giants the night, to 8:00 the
next day (`NvMmClock_SleepToMorning`). He wakes at her side (MM's inn entrance 2) and she speaks
first ("Oh, are you awake?"). MM's EVENTINF flags (42, 43) are kept by the message-script
interpreter (`NvMmMsgEvent_EventInf`). Her lunch with Anju is in (below, with the Goron). Not yet:
the All-Night Mask (the story's pictures), her head turn. Tested headless: both stories, Day 1 9:00
to 11:31, and to Day 2 at 8:00 with her waking line.

### The Goron at the inn (2026-11-22, `soh/src/code/z_nv_mm_ig.c`, MM's En_Ig)

MM's model (`object_dai`, his eyes by MM's blinking), schedule (`mm/schedules/En_Ig.schl`, written
out from the decomp's bytecode) and message scripts (`nv_mm_msgscripts/En_Ig.msg.inc`). Day 1 only,
as in MM: he walks East Clock Town at 15:25 and goes through the inn's door at 15:45, crosses the
lobby to the desk, and asks Anju for his room (16:10-16:30, MM's lines between the two of them,
Anju's side in `z_nv_mm_an.c`). Then up the stairs to the Knife Chamber (16:50); if Link has the
room key (an MM item, not in yet) he walks back out and sleeps in East Clock Town instead. The
desk talk is in Anju's cutscene, the shot turning to each speaker (2026-11-27); his head turn is left out; MM's step
and snore sounds aren't in OoT's sound bank. Tested headless: the street walk, the door, the walk to
the desk, the talk with Anju ("I should have a reservation-goro. The name is LINK."), and the walk
upstairs to the Knife Chamber's door.

Anju's other desk scenes are in with him: her lunch with Granny (12:00-12:15, MM's lines between
them; tested headless: "Grandmother...I am Anju! Tortus was my dad...") and the Postman's letter
(14:08-14:18: he walks in from the inn's door to the desk and out again, MM's lines between the
two; the Postman's scripts are now all MM's, generated to `nv_mm_msgscripts/En_Pm.msg.inc`). Tested
headless: "Ah! Uh, uh, umm... What is t..." through to the Bombers' Notebook events for meeting
both.

### The inn's props (2026-11-23)

MM's own, ported as they are: the flames and lamps (`z_nv_mm_light.c`, MM's En_Light: candles,
the fireplace, their flickering light), the upstairs windows' light shafts by day and by night
(`z_nv_mm_yado.c`, Obj_Yado, with its animated material), the bobblehead cow toy on the desk
(`z_nv_mm_hariko.c`, Obj_Hariko: it bobs in a quake) and the things Link can read (`z_nv_mm_talk.c`,
En_Talk: Granny's diary). Objects' animated materials now come over too (mm_objects.py
"matanim", drawn by `NvMmMatAnim_Draw` in z_nv_mm_scene.c, the same code as the scenes'). With
NV_LOG, the MM actors a room spawns are logged ("NVMM spawn"). Tested headless: they spawn, the
fireplace and a hall candle burn, the diary reads.

Not yet, for want of other systems: the toilet hand
(En_Bjt), Kafei (En_Test3) and the night guests in the Large Suite (Gorman, the Rosa Sisters,
the jugglers, Guru-Guru) are bigger stories of their own; Obj_Kinoko (the Mask of Scents' smell)
and the bugs wait for MM's masks and bottles.

### MM's get-items (2026-11-23, `soh/src/code/z_nv_mm_getitem.c`, `tools/mm_getitems.py`)

All of MM's get-items at once: `mm_getitems.py` reads MM's get-item table (z_player.c: 185 entries,
each its item, model, text and chest opening) and MM's get-item draw table (z_draw.c: 118 models)
from the decomp and writes them as C (`nv_mm_getitems.inc`), with the 85 get-item objects for the
converter (`mm_getitem_objects.py`, used by mm_objects.py). Committed, so the decomp is only needed
to make them again.

An MM get-item rides OoT's own (Link holds it up, the text, the fanfare) as a SoH GetItemEntry with
a mod index of its own: MM's text, MM's model over Link's head (MM's draw routines, ported as they
are), and the give handed to `NvMmGetItem_Give`. Small hooks in shared files: randomizer.cpp
(Randomizer_Item_Give passes MM's on), z_player.c (MM's fanfare by item) and z_en_box.c (Termina's
chests give MM's get-item and look like MM's chest, `z_nv_mm_box.c`; the chests come over as
OoT's En_Box, MM's types numbered the same).

One inventory, as decided ("Decisions": items both games have): what OoT has too (rupees,
hearts, heart pieces, magic, ammo and its upgrades, the lens, beans, elemental arrows, bottles and
the contents OoT knows, the Kokiri Sword, the Song of Time, Epona's Song and the Song of Storms)
goes into OoT's own, and a
check that would give a second copy gives MM's Purple Rupee for now (a placeholder for the reward
pool); what only MM has (masks, MM's songs, the trade items, the Pictograph Box, the Powder Keg,
MM's bottle contents, MM's swords and shields, and young Link's own Hero's Bow and Hookshot) is kept
by MM's item id in the "termina" save section. The Song of Time
takes from it only the event items decided above ("What the Song of Time takes": Zora Eggs and the
Deku Princess in bottles, small keys, the trade items: the Moon's Tear and the deeds, the Room
Key, the letters, the Pendant of Memories), never OoT's side. MM's message scripts now give,
check and take items through this (OFFER_ITEM, CHECK_ITEM, DELETE_ITEM, the quest items and
songs, the Powder Keg); the worn-mask check (CHECK_WORN_MASK) asks the mask worn ("Wearing MM's
masks"); showing an
item from the pause menu mid-talk waits for a pause screen for MM's items.

Open: where MM's own items show (a pause page); the Moon's Tear and the Pendant counted as trade
items (the decided list named the Room Key, deeds and letters); the Powder Keg kept for now. Tested headless: the
Knife Chamber's chest (MM's chest, MM's Silver Rupee model and text, 100 rupees, the wallet's
limit), Anju's Room Key at the desk (MM's text, kept as MM's).

### Time's speed (2026-11-23, `soh/src/code/z_nv_mm_clock.c`)

The clock's speed is kept in half steps a frame (MM's normal R_TIME_SPEED of 3 is 6), so half
speed works (a step of 1 and 2 by turns). The list (`NvMmClock_OpenSpeedMenu`): a third (N64's
Inverted Song of Time), half (3DS's), normal, double, triple, the current one marked; Link held
still while it's open, the clock stopped. The clock shows "x1/3", "x1/2", "x2" or "x3" when the
speed isn't normal. The Song of Time puts it back to normal, as MM's does; it's saved (the old
"speedOffset" field now holds the half steps; an older save's MM offset is read as one). People
walking by the time follow it (`NvMmClock_Speed`: this frame's step). The Inverted Song of Time opens
it (`z_nv_mm_songs.c`, registered with the shared song listener `z_nv_songs.c` that the RE1 side
wrote, after RE1's songs): in Termina only, OoT's correct chime, the ocarina put away as B puts it
away, then the list. Dev: NV_MM_SPEED_MENU=1 opens it on the first frame. Tested headless: the
song played in the inn (`press:` notes), the list, half chosen, the clock's "x1/2".

### MM's pause pages (2026-11-23, `soh/src/code/z_nv_pause.c`, `z_nv_mm_pause.c`, `tools/mm_pause.py`)

The design is `../NV_PAUSE_MENU.md` (decided with the user, 2026-11-20). The shared part,
`z_nv_pause.c`: each side of OoT's box is a column of pages, OoT's first, then any game's
registered pages, looping. At rest the page in front sits exactly where OoT's does and a sliver of
each neighbour (its frame, its title) peeks at the screen's top and bottom; the cursor past the
top or bottom row slides the column one whole page (8 frames; dev NV_PAUSE_SLIDE=n slows it).
Hooks in the kaleido (small ones): the item and equipment sides are drawn through
`NvPause_DrawColumn` (a side with no alternates is OoT's alone, as before), OoT's item and equipment
cursors call `NvPause_Edge` where they stop at the top and bottom rows, the name bar asks
`NvPause_Name`.

MM's two pages, MM's own throughout: the items page in the Items column and the masks page in the
Equipment column, with MM's tiles and titles ("SELECT ITEM", "MASKS"), MM's flat colour (its tiles
are see-through in places, as MM's are), MM's icons and English names, MM's 6 by 4 grid and slot
order. In Termina they open in front; elsewhere they're a slide away. What a slot shows follows the
one inventory: shared things from OoT's (the ocarina, arrows, bombs, sticks, nuts, beans, the
Lens, OoT's four bottles), MM's own from MM's (the Hero's Bow and Hookshot, the keg, the
Pictograph Box, the Great Fairy's Sword, the trade items, MM's own bottled things after OoT's
bottles, the masks).

MM's pause textures live in "yar" archives Torch can't read; `tools/mm_pause.py` unpacks them and
writes the 330 textures (icons, names, tiles, titles) straight into the mod as libultraship
textures, by a list generated once from the decomp (`mm_pause_textures.py`, committed).

Dev: NV_MM_ITEMS="2d,38,32" gives MM items (hex ids) on arrival in Termina; NV_WALK's "sleft" and
"sright" push the stick sideways (turning the box). Tested headless: the items page in front in the
inn with its items, the slide down to OoT's page and back up, the slivers, the masks page, names.

Not yet: using MM's items from C buttons (the items page still shows OoT's "to Equip" prompt; a C
button does nothing there; the masks page's C buttons equip, "Wearing MM's masks" below), ammo counts on MM's icons, the Quest and Map columns, the
slivers' titles under the HUD (the open question in the design).

### Wearing MM's masks (2026-11-23, `soh/src/code/z_nv_mm_mask.c`)

MM's masks stay separate items (the Decisions above). On MM's masks page a C button puts the mask
under the cursor on that button (off any other); out of the menu, pressing it puts the mask on, and
again takes it off, as OoT's own masks. One mask at a time: an MM mask takes OoT's off, and an OoT
mask takes MM's off.

- On the buttons an MM mask is an item id OoT doesn't use (0xA0 + MM's item id - the Deku Mask's,
  0xA0-0xB7). OoT's icon table (`gItemIcons`, z_inventory.c) grew to 256 entries and those get
  MM's icons; `gItemAgeReqs` lets young Link use them. Pressing one reaches `NvMmMask_UseItem`
  first (a hook at the top of z_player.c's Player_UseItem), not OoT's item actions.
- Worn, the mask is drawn as MM draws it: its model in the head's space right after the head
  (a hook in z_player_lib.c's Player_PostLimbDrawGameplay), from the mask's own object (the 20
  `object_mask_*` objects, now in mm.o2r: `MASK_OBJECTS` in mm_objects.py). The Bunny Hood's ears
  (their rest pose, OoT's and MM's same matrices), the Great Fairy's Mask's three strands of hair
  (MM's swing, ported: func_80127DA4/func_80128388; not the stray fairies' sparkle), the Blast
  Mask's colour list.
- MM's people see it: `NvMmMask_Worn` is MM's Player_GetMask (MM's PLAYER_MASK_*), and the
  message scripts' CHECK_WORN_MASK asks it.
- Not kept in the save (as MM: a loaded game starts with none on); a new game takes it off.

Not yet: the forms (Deku, Goron, Zora, Fierce Deity: their buttons give the error sound), MM's
put-on animation (the mask goes on at once, with OoT's sound), what masks do beyond being seen
(the Bunny Hood's speed, the Stone Mask, the Blast Mask's blast, the Giant's Mask...), the moving
parts of some (the Bunny Hood's ears swinging, the Couple's Mask, the Circus Leader's tears),
adult Link (plain masks on adult: open), OoT's people reacting to MM's masks.

Tested headless (young Link in the inn): the masks page's C buttons, the icons on the HUD, the
Postman's Hat, the Bunny Hood, Keaton, the All-Night, Stone, Blast and Great Fairy's masks worn.

### The forms: Deku Link (2026-11-23, `soh/src/code/z_nv_mm_form.c`)

The Deku Mask on a C button (from MM's masks page) changes young Link into Deku Link, and pressing
it again changes him back. He stays Deku through doors and loads, in Termina and in Hyrule alike
(the forms are carried over), and the form is in the save ("termina" section's "permanent": form,
and the sword and the "no sword" flag it put aside).

Nothing of OoT's player code (z_player.c) is rewritten for this. Deku Link is OoT's young Link with:
- MM's Deku skeleton (object_link_nuts, now converted) under OoT's animations. MM's Deku Link walks,
  runs and stands with the same animations as its human Link (MM's animation table has no form
  column), and the two skeletons have the same 21 joints in the same order. Applied to Link as he
  is (both skeleton pointers), and again to each new Link (a scene's start).
- MM's Deku hands, waist and sheath (a one-line hook in z_player_lib.c's limb override).
- No sword and no shield: B is left empty (OoT's own swordless B, its `infTable[29]` flag; without
  it OoT puts the Deku Stick on B), the sword put back on B when he's human.
- MM's Deku size: MM's sPlayerAgeProperties for PLAYER_FORM_DEKU (ceiling, ledges, water depths,
  wall radius, the root's 0.3 scale) over OoT's young Link's (OoT's voice and animations kept),
  and MM's Deku height (36) for the camera (a hook in z_actor.c's Player_GetHeight).
- MM's people see the form: `NvMmForm_Get` is MM's GET_PLAYER_FORM, and the message scripts'
  CHECK_DEKU / CHECK_HUMAN / CHECK_GORON / CHECK_ZORA ask it.
- Changing takes any worn mask off (as MM). Plain masks are for human Link only (as MM). As Deku,
  only the ocarina and the masks can be used for now.

The change is MM's transformation scene (Player_Action_86 and 87): Link turns to the camera and
puts the mask on (MM's cl_setmask and cl_setmaskend; the Deku Mask drawn on his face from frame 12,
squashing onto it, the fused mask from frame 51: gameplay_keep's gDekuMaskDL and object_mask_nuts,
now converted), or as Deku pulls it off (pn_maskoffstart); the light darkens, flashes with the
lightning and turns blue, a light flares by him (MM's D_8085D848, D_8085D910); the screen goes white
(45 a frame), he changes, and it comes back. MM's rule: it plays every time; into a form it can be
skipped (A, B, a C button) only once that form's scene has been watched to the end (a bit per form in
the save), back to human always. The camera is a close one of our own on his face (MM's is its
cutscene manager's); the sounds are OoT's nearest (MM's transformation voice and flash are MM's own).
The proposal to skip it by default with a setting to watch it (NV_GAMES.md) stays open: it needs a
settings page.

The people's Deku lines (2026-11-23): the message scripts' form checks (everyone on scripts: Granny,
Anju's mother, the Postman...), and MM's own branches in the ported actors: the gate guards (MM's
Deku lines by gate and night, and they keep Deku Link in, as a child), Anju (MM's Deku scripts by
her schedule), the Bombers (Deku Link's talk about where Jim is, and its week events).

Not yet: the bubble, the flower launch (below, "Deku Link's moves"), MM's Deku items, adult
Link (forms don't depend on age, leaning yes: open), the other forms.

Tested headless: equipping the Deku Mask, the change and back (the scene both ways: the mask on his
face, the light, the white-out), Deku Link walking in South Clock Town at MM's size next to a guard,
B empty and the sword back, the form kept through a warp into an OoT scene, the gate guard's Deku
line. Not tried yet: Anju's and the Bombers' Deku lines (their code is MM's, as ported).

### MM's player animations (2026-11-23, `tools/mm_player_anims.py`, `mm_objects.write_player_anims`)

All 695 of MM's player animations are in mm.o2r, at `mm/player_anim/<MM's name>` (e.g.
`gPlayerAnim_pn_attack`), each a SoH Link animation whose frames are at `mm/link_animetion/<offset>`
(MM's file 7, stored uncompressed). MM's frames are OoT's format exactly (22 joints and the face,
a frame; MM's idle is OoT's to the value), so OoT's player code plays them as its own: in C, the
path as a LinkAnimationHeader*, as SoH's own animations are. `mm_player_anims.py` is the names and
header offsets in gameplay_keep, from the decomp's XML (no game data).

### Goron Link, the first stage (2026-11-24, `soh/src/code/z_nv_mm_form.c`)

The form code is now a table of forms (sFormDefs: model, hands open and closed, waist, sheath,
eyes, MM's sizes, height and speeds, the mask's two models in the scene, the mask-off animation);
Goron Link joins Deku Link there. The Goron Mask changes him (MM's scene, the Goron Mask on his face
and fused: gameplay_keep's gGoronMaskDL, object_mask_goron, both converted) into Goron Link: MM's
object_link_goron (his skeleton, hands, waist, sheath, his eyes in OoT's eye table while he's Goron:
open, half, closed, surprised for OoT's "shock"), MM's Goron size (unk_08 0.74), height 80 and speeds
(MM's PLAYER_BOOTS_GORON). In deep water he sinks and is put back on land (MM lets him walk the
bottom: not yet). One form's mask while another's on changes him straight across. The gate guards
already had MM's Goron lines (they let him out).

**His moves** (2026-11-24, `soh/src/code/z_nv_mm_goron.c`): our own actions from the same hook as
Deku Link's (OoT's last free-standing handler), MM's numbers:
- **A, the roll** (MM's Player_Action_96, streamlined): he curls (MM's pg_maru_change, or straight
  from a run), rolls while A is held, steered (the stick times 2.6, the turn quicker with speed),
  bouncing off walls at 12 and up; after rolling hard for MM's 50 frames, with magic, the spikes
  (MM's 18 speed; 2 magic at once, then 1 every 10 frames: MM's own drain has no OoT twin), lost on
  letting go, a wall, or no magic. Curled he's drawn as MM draws him: the ball (gLinkGoronCurledDL)
  turning as he rolls, squashed and stretched, the spikes (MM's lg_spike_model), blue after a
  pound; his body's limbs aren't drawn (the form's limb hook). MM's curled height (34) for the camera.
- **B while rolling, the pound**: MM's hop (14), the hang at the top, the slam (gravity -10), a white
  shockwave and a quake, hitting all round (4, radius 60).
- **B standing, the punches**: MM's pg_punchA, B and C, B again in time chaining the next.
- The hits are OoT's nearest kinds (OoT has no Goron damage): the roll as the Deku Stick, the spikes
  and punches as the hammer's swing, the pound as the hammer's jump (so OoT's hammer targets answer).

Dev: NV_GORON_SPIKES lets any roll charge the spikes; the walker's `hold:a` / `release:a` keep A
held across steps (a roll steered by waypoints, B pressed during it).

Not yet: MM's roll physics on slopes, its dust and tracks, the punch's fist effect and hit spots
(here his body's cylinder reaches out), MM's roll camera (CAM_MODE_GORONDASH), the curl on R (MM's
shielding skeleton), the drums, the heavy switches, MM's bump off doors (func_80840A30: a small hop back), the Zora and Fierce Deity forms.

Tested headless: the Goron Mask equipped, the change and back (the scene both ways), Goron Link
standing by the gate guard in South Clock Town at MM's size; the three punches, curling, rolling
steered round the square, the hop and pound, the spikes (with NV_GORON_SPIKES), uncurling.

### Zora Link, the first stage (2026-11-24, `soh/src/code/z_nv_mm_form.c`)

The Zora Mask changes him (MM's scene, gameplay_keep's gZoraMaskDL and object_mask_zora) into Zora Link:
MM's object_link_zora (skeleton, hands open and closed, waist, sheath), his eyes and his mouths in OoT's
eye and mouth tables (the form table now has mouths too), his fins on his forearms (MM's
func_80126BD0, folded: 0.4, 0.6, 0.7), MM's Zora size, height 68 and speeds (MM's PLAYER_BOOTS_ZORA_LAND).
He swims as OoT's Link does for now. The forms' extras are drawn from one per-limb hook in
z_player_lib.c (`NvMmForm_PostLimb`, which the Deku flowers' hand hook became), and the faces are kept
right each frame (the forms' own textures are never taken for OoT's or MM's human ones).

**His moves** (2026-11-24, `soh/src/code/z_nv_mm_zora.c`), our own actions as the other forms':
- **B on land, the punches and kick**: MM's pz_attackA, B, C, chained (the punch chain is now shared
  with Goron Link's, z_nv_mm_goron.c, each with MM's own hit frames: Goron 6-8, 12-18, 8-14; Zora 2-5,
  3-8, 3-10), hitting as OoT's Kokiri Sword's slash (1, the kick 2).
- **A held swimming, the fast swim** (MM's func_80850734, Player_Action_56): the dash (MM's water roll,
  a burst of 16), then gliding at 9 while A is held, the stick pitching him (up dives, as MM's) and
  turning him, rolling over in hard turns, kept under near the surface; let go, he slows into OoT's
  own swimming. His body is pitched and rolled as he goes; his back fins are out (MM's D_801C0ABC).
  Where OoT would restart his swimming each frame, our water hook leaves him be while he glides.
- **R held, the barrier** (MM's func_8082F1AC), on land or in water, while there's magic: MM's light
  circling him, stunning what touches him (OoT's Deku Nut, its nearest) and keeping hits off him;
  1 magic every 6 frames.

Not yet: the fins thrown (MM's boomerang: its En_Boom isn't ported), walking the bottom (MM's Zora
boots), jumping out of the water, the barrier's colour on the scene, the guitar, the Fierce Deity.

Tested headless: the Zora Mask equipped, the change and back (the scene both ways), Zora Link in South
Clock Town, his face and fins; the punches and the barrier in Laundry Pool, the fast swim in its pool
(started, gliding, back to swimming: by the log, the pool's water hides him from the camera).

### The Fierce Deity, the first stage (2026-11-24, `soh/src/code/z_nv_mm_form.c`)

The Fierce Deity's Mask works anywhere (the decision above). MM's scene (gameplay_keep's
gFierceDeityMaskDL and object_mask_boy) changes him into MM's Fierce Deity (object_link_boy: his
skeleton, hands, waist; no sheath), at adult Link's size: MM's Fierce Deity is adult Link's skeleton
and MM's numbers for him are exactly OoT's adult Link's times 1.5, so at 1.0 he has OoT's adult Link's
sizes (sAgeProperties[0]), speeds (OoT's Kokiri boots, adult) and height (68). In a boss room (OoT's
nine boss scenes and Ganon's; MM's to come) he grows, a step a frame, to MM's 1.5: MM's sizes, speeds
(MM's PLAYER_BOOTS_FIERCE_DEITY) and height (124), and shrinks back outside. He keeps the sword on B
(the other forms have none) and swings it as OoT's Link; drawn out, it's his Double Helix Sword (MM's
gLinkFierceDeityLeftHandHoldingSwordDL).

**The sword beam** (`soh/src/code/z_nv_mm_beam.c`, MM's En_M_Thunder as the beam, MM id 0x035): a
swing begun while Z-targeting (locked on, or Z with nothing to lock on to), not a spin, with magic,
sends MM's beam from his waist along his facing, pitched at what he's locked on to (MM's
func_808332A0); 1 magic, taken straight off the meter (OoT's meter drains 2 a step and would empty
on an odd cost). MM's flight: 80 units a frame, growing to 12, fading over 20 frames, its light with
it; MM's model (gameplay_keep's gSwordBeamDL, scrolling, MM's colours). It hits as OoT's light arrow
(DMG_ARROW_LIGHT) for MM's 3: the decision. The user's thought, for later: it might rather count as a
sword strike.

Also: a form's item block (z_nv_mm_mask.c's Player_UseItem hook) let no form use B's sword; the
Fierce Deity's swords now pass. And NV_MAGIC sets the meter's capacity too (with none, OoT's consume
step empties it).

**His swords from the menu** (NV_GAMES.md, the user 2026-11-20): on the Equipment screen he takes
adult Link's Master Sword and Biggoron's (or the Giant's Knife, whole or broken); the Kokiri Sword's
place is his own Double Helix Sword (young Link's sword makes no sense in his hand). Two one-line
hooks in the kaleido: the equip check (`CHECK_AGE_REQ_EQUIP`, z_kaleido_scope.h) and the icons'
greying (z_kaleido_equipment.c), both asking `NvMmForm_EquipAgeOk` (the sword row only: his tunic,
boots and shield stay as the forms have them). Drawn: OoT bakes each sword into adult Link's hand
(and each shield into the other hand), but each of those lists draws the sword first and the hand
from the gauntlet's texture on, so `NvMmForm_FdHoldingOotSword` copies the sword half (up to the sync
before gLinkAdultDefaultGauntlet1Tex, found by its hash) after his own fist (MM's
gLinkFierceDeityLeftHandDL: MM's own sword hand is that fist and then the Double Helix). Built once,
kept. Changing out of him with an adult sword on, young Link gets the Kokiri Sword back (or none).
Damage follows OoT's sword (the Double Helix's own damage: later). Dev: NV_OOT_SWORDS=1 owns the
Master Sword and Biggoron's (=broken: the broken knife).

Tested headless: as the Fierce Deity, the Equipment screen (slid down from MM's masks), the Master
Sword lit and equipped, swung at the gate guard in his fist (and the beams); the same with
Biggoron's. Not yet seen: the broken knife, and the change back to young Link with one on.

Not yet: the Double Helix's damage, MM's boss rooms.

Tested headless: the Fierce Deity's Mask equipped, the change, him at adult size by the gate guard,
warped into Gohma's room and grown to 1.5 (by the log; the room's dark). The beam: Z on the gate
guard and B, twice, a beam each (pitched up at the guard's head, 1 magic each), seen flying off;
in Gohma's corridor with Z on nothing (by the log; at 1.5 the camera sits in the corridor's wall).

### Deku Link's moves (2026-11-23, `soh/src/code/z_nv_mm_deku.c`)

MM's Deku moves on OoT's Link: our own action functions, set as Link's action (OoT's actionFunc),
using MM's animations, numbers and order; OoT's player code otherwise as it is. Three one-line hooks
in z_player.c (agreed with the user, 2026-11-23, RE1 to adjust if it needs to):
- **A, the spin** (OoT's roll handler, last in its lists, so talking and the rest come first; MM
  gives Deku Link the spin on A, B is the bubble): MM's Player_Action_95, the turn slowing 800 a
  frame from 20000, steerable, hitting as OoT's Deku Stick for 1 (MM: DMG_DEKU_SPIN, 1, radius 30)
  through OoT's own way of letting Link's body cylinder hit.
- **R, the shell guard** (OoT's shield handler): pn_gurd held while R is; hits are kept off his body
  (OoT has no shell collider).
- **Water** (where OoT would start him swimming): MM's hops, five from the ground, each MM's jump
  speed times 0.3 + 0.18 a hop already made (at least 4), the fifth spinning in the air; with none
  left he sinks, the screen goes black (20 a frame) and he's put back where he last stood (OoT's
  "down" respawn, as MM's), untouchable 40 frames. No damage (as MM).
- Each frame as Deku: MM's Deku speeds (MM's boot data for PLAYER_BOOTS_DEKU on OoT's movement
  registers: a run of 6 against young Link's 5.5); OoT's again as human.

Sounds are OoT's nearest (its Deku Scrubs', its jumps and water): MM's own wait for MM's audio.

Dev: NV_DEKU_NOHOPS starts him with no hops (to try the sinking); NV_MAGIC fills the magic meter
(giving one if there's none: the debug save has none); the walker's `press:a:N`, `press:b:N` and
`press:r:N` hold A, B or R N frames.

**The Deku flowers** (2026-11-24, `soh/src/code/z_nv_mm_etcetera.c`, MM's Obj_Etcetera; the
player's side in z_nv_mm_deku.c): the pink and gold flowers, MM's own models, animations (the rustle,
the bounce as he launches) and collision from gameplay_keep (the converter now takes a `col` entry:
Torch's OoT collision, as the scenes'). Clock Town's four are placed by MM's rooms. A on a flower
dives into it (MM's func_80836DC0 and Player_Action_93: spinning down, squashing, the depth drawn
as his shape's offset); held, it charges (10 frames: a high launch); let go, he launches (MM's
2700 or 1450, a gold flower 1.74 times as high), hits on the way up (2, radius 20), and with the
charge hovers on the two flowers in his hands (MM's Player_Action_94: their spin, the pollen
sparkles, gliding slowly down, steered, 600 units from a pink flower and 960 from a gold before he
drops; A lets go). The hand flowers are drawn at his hands (a one-line hook in z_player_lib.c,
MM's func_801271B0 with its keyframed scales). While he flies OoT is told he's airborne in an
action of its own (its midair flag: otherwise it starts its own fall), and its jump camera follows.

**The bubble on B** (2026-11-24, `soh/src/code/z_nv_mm_bubble.c`, MM's En_Arrow as its Deku
bubble): B standing as Deku takes MM's 2 magic and blows a bubble at his mouth (placed by his head,
MM's spot), growing as MM's does while B is held; he turns to aim; let go (or full for 20 frames) it
flies off, the bigger the slower, for 99 frames, bursting on what it hits (as OoT's slingshot seed,
for 1: OoT has no bubble damage). MM's look refracts the screen behind it (it reads the frame buffer,
which SoH can't sample that way): here MM's mesh is shaded plainly in MM's colour. No magic: the
error sound. Registered as MM's En_Arrow (0x00F), spawned only by him.

Not yet: dropping Deku Nuts from the air on B (MM spawns its
nut from En_Arrow; OoT's En_Arrow nut isn't made for it), MM's flight camera (OoT's jump camera
lags well behind him in tight spots: the South Clock Town flower's corner), the flower's floor
light, the hop counter on the A button, sinking into another room (put back only within the room
he's in).

Tested headless: the spin and the guard in South Clock Town (A next to the gate guard talks first),
a hop across Laundry Pool's water, the sinking and being put back on land; South Clock Town's
flower: standing on it, diving in, charging, the launch, hovering and gliding back down onto it; a
bubble blown and fired in South Clock Town.

### Young Link is MM's model in Termina (2026-11-23, `soh/src/code/z_nv_mm_link.c`)

Decided with the user. The two games' young Link skeletons are the same (21 limbs, the same joints
but the sheath's, 27 units lower in MM's), so OoT's animations and player code drive MM's model
as they are. Nothing of OoT's player code is changed: OoT reads young Link's skeleton, face and
hands from its own tables (z_player_lib.c), and on each scene's start (SoH's OnSceneInit, before
Link is made; registered in NvMmActors.cpp) those entries become MM's in Termina and go back to
OoT's elsewhere. Adult Link is never touched.

MM's: the skeleton and everything it draws (body, head, hat), the face (MM's eight eyes and four
mouths; OoT's "shock" shown as MM's wincing, its two unnamed as looking up and down), the open and
closed hands, the hand holding the Ocarina of Time. Still OoT's, sitting right on MM's body: what's
held or worn (the sword in hand, sword and shield on the back, the Deku Shield, the slingshot, the
boomerang, the bottle, the Fairy Ocarina). OoT bakes the item into the hand, MM draws hand and
item apart, so MM's way comes with MM's items in hand.

The converter now handles Link's kind of skeleton (LOD limbs: `skel_lod` in mm_objects.py).

Dev: NV_BOOT_AGE=child boots young (the debug save is adult Link, so the Termina tests before
this one all showed adult Link); NV_MM_OOT_LINK=1 keeps OoT's model in Termina, to compare.
Tested headless: young Link in the inn, MM's model next to OoT's, the skeleton the one Link is
made with, the sword drawn.

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
