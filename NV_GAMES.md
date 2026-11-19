# Other games: brainstorm

Started 2026-11-19 with the user. Nothing here is decided yet. It's a list of candidates, how
each one could fit, and what it would take. Decisions get written in with dates when they're made,
same as RE1's notes.

## Rules that decide how hard a game is

- **Is there a decomp or a port?** The same rule as RE1: the original game's own code decides
  how things behave. Without a decomp we'd be guessing from the outside, which is much slower
  and much less faithful.
- **Is the engine built like ours?** A port that runs on libultraship, like SoH does, is
  practically a sibling. A game that plays nothing like OoT costs more to bring in.
- **Could it get us in legal trouble?** All of this assumes no game data in the repository. A
  few projects have also been taken down over their code (see GTA III).
- **How deep does it go?** It doesn't have to be the whole game. There are four depths:
  1. a whole game Link plays through;
  2. a piece of one (a level, a system, a minigame);
  3. a menu or interface, with no 3D world;
  4. an Easter egg or a borrowed map.

## The "mod list" idea: realistic, and mostly built already

Each game's converter already makes its own archive (`re1.o2r`), and SoH loads whatever is in
its `mods` folder. RE1's code stays inert outside RE1's scenes, and it already copes when an
older `re1.o2r` lacks some pieces (`z_nv_box.c`, `z_nv_status.c`). So the mod-list model is
close to the current shape:

- **One archive per game.** If it isn't in `mods`, that game simply isn't there. Nobody needs
  40 discs, only the games they own and want.
- **Ties between games check first.** For example, the Lost Woods door to the mansion appears
  only if `re1.o2r` is loaded. Each of these needs a small "is this game present" check (not
  written yet).
- **A shared randomizer pool.** It would draw only from the games that are loaded. That's a
  later, larger piece of work on top of SoH's randomizer.
- **Separate releases work the same way.** "RE1 as its own experience" would be the same build
  booting straight into the mansion: the dev boot (`nv_boot.txt`) made into a real option, maybe
  with its own title screen. GTA III or anything else could get the same treatment.

## Candidates

### Majora's Mask: the easiest fit, by far
- **Port:** a complete decomp, plus 2Ship2Harkinian, a PC port by the same team as SoH, on the
  same libultraship.
- **Fit:** same engine family, same Link, same formats for actors, scenes and assets. Termina
  could be another world Link travels to, and masks or songs could cross over.
- **Catch:** it's two codebases with the same roots but separate histories. Running MM's actors
  and its 3-day clock inside SoH is real porting work, but the scene format is mostly the same,
  so it's far less than RE1 was.
- **Depth:** anything from a few areas to the whole game.

### Pokémon XD (and Colosseum)
- **Port:** XD has an active decomp, [TeamOrre/xd-decomp](https://github.com/TeamOrre/xd-decomp).
  Colosseum is fully matched (100%, NTSC-U GC6E01):
  [dougchansan/pkmn-colosseum](https://github.com/dougchansan/pkmn-colosseum), AI-assisted,
  continued in [grant-might/Colosseum_Decomp](https://github.com/grant-might/Colosseum_Decomp).
  pret's `colosseum-mb` is a different program (the multiboot bonus on the GBA games). The two
  games share an engine.
- **Fit:** the battle system as a self-contained mode, triggered from encounters or NPCs,
  either in Orre-themed rooms or in Hyrule. Snagging (catching other trainers' Pokémon) feeds
  the randomizer idea nicely. Their 3D overworld rooms could also convert into scenes the way
  RE1's did.
- **Depth:** a battle-system port plus some maps, not the whole story.

### Animal Crossing (GameCube)
- **Port:** a complete decomp (ac-decomp) and a native PC port from March 2026.
- **Fit:** a town Link lives in, running on the real clock:
  - villagers, fishing and bug-catching, the museum;
  - letters as a randomizer source;
  - the NES games inside it, a ready-made "Link at a computer" moment.
- **Catch:** its world is built differently (rolling tile grid, acre system). The town would
  need its own scene logic and couldn't just convert to OoT rooms.

### Banjo-Kazooie
- **Port:** a complete decomp, plus Banjo: Recompiled (January 2026). Another port,
  "Lighthouse", was announced with its own randomizer.
- **Fit:** the user's worry is right that its movement and camera are a different game. Better
  fits:
  - **its worlds as places Link visits:** Mumbo's Mountain or Gruntilda's Lair, converted as
    scenes, with Link moving the way OoT moves;
  - **pieces:** jiggies and notes as collectibles or randomizer items, Mumbo's transformations
    as mask-like items, enemies brought into OoT.
- **Depth:** levels as maps, without BK's controls. Same engine generation (N64), so its formats
  are closer to OoT's than RE1's were.

### Shivers (PC, 1995): Link at the computer
- **Engine:** ScummVM supports it, rated "Excellent", Windows version.
- **Fit:** this fits the user's "Link at a computer" idea directly. ScummVM runs inside SoH as
  a screen: Link walks up to a PC in some room, and the game plays in a window or full screen.
  Things that happen in it (bottling a spirit, solving a puzzle) report back as randomizer
  checks or flags.
- **Catch:** ScummVM is GPL-3 and a big codebase. Embedding it means building it as a library
  and handing it SoH's window, input and audio. That's possible but not small. Shivers'
  ghost-trapping puzzle box could also suit an RE1-style tie-in.
- **Bonus:** once ScummVM is in, every game it supports comes along with it.

### Monster Rancher 2
- **Port:** I found no existing decomp (PS1). We could make our own: PS1 decomp tooling is
  mature, and the Colosseum decomp shows how far AI-assisted decompiling gets. It would be
  targeted: just the ranch rules (training, growth, tournaments, how a disc becomes a monster),
  not a full matching decomp. Fan research on stats and breeds is there to check against, and
  there's also the official 2021 remaster (Monster Rancher 1 & 2 DX).
- **Fit:** the user is right that it's mostly a menu game:
  - a ranch screen with training, tournaments and growth;
  - its own 2D/3D presentation, without a big world to convert;
  - the famous disc gimmick (a new monster from any CD you insert) could become monsters from
    the games' archives, or from OoT data. That's a fun way to tie the games together.
- **Catch:** the decomp step comes first, before any of the rest.

### GTA III ("Ocarina of Crime")
- **Port:** re3 is a real decompilation (reverse-engineered from the PC executable, not leaked
  source), the same kind of project as the others here. Take-Two had it removed from GitHub in
  2021 and sued its developers. I haven't looked up how that case ended. The user is
  researching it themselves, including whether there's another decomp and whether re3's
  takedown came from something specific to how that project was done.
- **Fit:** a mute protagonist, a radio, a city. It works best as its own separate release, as
  the user said, not inside the main build.
- **Risk:** the difference from the others is Take-Two's history of going after this exact
  project, not the decomp itself. Whether it's worth it is the user's call. It could start
  small anyway (a radio station, an "Ocarina of Crime" title card).
- **If it happens (talked over with the user, 2026-11-19):** its own repository, ideally
  private, not a branch here. A DMCA notice takes down the whole repository, every branch
  included. It would plug in as an add-on, with its own converter, `gta3.o2r` and engine files,
  so the main repository holds nothing of GTA's.

### Medal of Honor: Rising Sun (PS2)
- **Port:** I know of no decomp.
- **Fit:** without one it can only be "inspired by": original levels in the style of Pearl
  Harbor, not converted ones. Possible later as a small Easter egg, too much as a real game.

### Elder Scrolls (pipe dream)
- **Port:** OpenMW (Morrowind) and Daggerfall Unity are both full engine reimplementations
  that read your own data files.
- **Fit:** the whole game is far too big. Possible small versions:
  - one Morrowind dungeon or interior converted to a scene;
  - its alchemy as a system;
  - a Daggerfall-style dungeon.

  The mod-list idea itself is already a nod to it.

### Other Resident Evil games
Handed to the RE1 chat, as the user said. RE2 and RE3 share most of RE1's room formats, so they
reuse almost everything.

## Ideas that cross games

- **Cross-game randomizer.**
  - OoT and RE1 already behave like randomizers: item boxes, keys gating rooms.
  - A shared pool across whichever archives are loaded: RE1 keys behind Hyrule checks, Pokémon
    snagged in Shivers' museum, and so on.
  - Big, but it's the strongest glue between all of this.
- **RE1 enemies in OoT** (the user's longer-term goal).
  - Fits best where OoT is already horror: the Shadow Temple, Bottom of the Well, Redeads.
  - Or as an "infection" that spreads only after the mansion is visited.
- **Link at a computer.** A ScummVM "PC" and the Animal Crossing NES games both fit this.

## Suggested first step (for the user to decide)

1. **Majora's Mask:** the most return for the least risk; it proves the second-game pattern.
2. **Shivers through ScummVM:** a completely new kind of tie-in (a game inside the game), and
   it opens up every ScummVM game.
3. **The "is this archive loaded?" check plus a shared randomizer pool:** small, and it's what
   makes the mod-list idea real.

Pokémon Colosseum/XD and Animal Crossing come next once the pattern is proven. Monster
Rancher 2 needs its own (targeted) decomp first. GTA III is the user's call on the takedown
risk.
