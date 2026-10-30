# Ideas (not planned work)

Suggestions collected from showing the mod off (movie night, 2026-10-25). Notes only: nothing
here is agreed or scheduled.

## Getting there and back
- Hop between Hyrule and the mansion freely: a warp song or a door.
- A mansion warp song of its own (the RE1 credits guitar riff as the notes was floated); the
  Scarecrow's song as a free custom option.
- A save-room theme song that warps to a typewriter; a joke song that drops you in the remake's
  fridge.
- The front door as a Lost Woods wrong turn for now (already noted in README); maybe combine
  with randomizers one day.
- Epona to the RPD. The Spencer mansion "canonically in Kakariko" (a gag).

## Content
- Hyrule rooms with RE enemies or characters, maybe even fixed cameras.
- Zombies outside the mansion too, once the story flags are in.
- The RE4 merchant somewhere to spend rupees, behind a cracked wall to bomb open.
- Navi giving RE hints and lines ("Forest is dead...").
- The toilet hand, a peeing QTE.
- Lens of Truth puzzles in the mansion.
- A grenade launcher / Halo-style gun for Link.
- Equipping a gun switches to full tank controls.

## Combat and systems
- Hookshot on zombies (on ReDeads it pulls Link in).
- Sun's Song on zombies, as it works on ReDeads.
- Deku Nuts stunning enemies.
- Zombies properly targetable, so guns (and Z-targeting) work. (In progress: the faithful zombie
  port with the Zelda layer on top.)
- Bosses: how they'll work.
- Bomb collision on a layer of its own ("BombWall"), so bombs can bounce off room bounds without
  affecting anything else (walls are about railing/door height now).

## What people liked
- Link's moveset in RE: fun, overpowered.
- Leaving the mansion doesn't break the rest of OoT.
- The prerendered rooms; enemy scripting is the harder part.

## Movies and music (from the user, 2026-10-25)
- GOG's purchase includes the Japanese PC version (and a few others). Pick up its opening and
  endings (the region-specific files: USA names OU, PU, EU4, EU5) when present, with a setting to
  choose the USA or Japanese soundtrack for those movies. No PS1 disc support needed.
- First time the warp song to the Spencer mansion is played: play the whole opening movie, then
  warp (a saved flag so it only happens once; skippable like any movie).
- The opening song as music: its audio is plain PCM in the AVI, so it can be taken out losslessly
  at conversion time (from the user's own copy) and streamed, or fed to SoH's custom music. The
  movie's track has the voice-over on it; a clean version would be the user's own file (or a
  vocal-separation pass done by the user), dropped in a folder that make_mod packs into their
  re1.o2r. Never committed to the repo, like all game data.
- Music files: the converter looks for a fixed name (e.g. "opening") with any common extension
  and any quality (ogg, mp3, wav, flac) and packs it as is; SoH already decodes Vorbis and MP3
  for its custom audio (AudioSampleFactory.cpp, libvorbis + dr_libs), so nothing is converted
  and any version the user finds just drops in.

## Getting to the mansion (from the user, 2026-10-26)
- First arrival: by a warp song; the first time, the whole opening movie plays, then the game
  starts in the mansion.
- Keep the mansion's front door as RE1 has it (its movie with the dog when you try it), rather
  than making it the way in and out.
- A warp point somewhere like the courtyard instead, for coming and going.
- Still wanted: a physical door in the Lost Woods (see the Lost Woods note in design.md), just not
  wired to the front door.
- The warp song lands Link at the front door, inside the foyer. Small warps around the mansion:
  probably too much.
- The warp song's music: "Icy Gaze" (Japanese or English, the user's own file) or the end credits
  song, heard when it's played after the first time (the first time plays the opening movie).
- Moonlight Sonata (the piano bar puzzle, 10F) as an ocarina song of its own.
- Both songs as two more notes on the Quest screen next to OoT's songs.
- Decided (2026-10-29): after the first time, the warp song plays the opening movie's own audio
  from just after the narration ends, where the cast list starts (the user will give the exact
  time in ou.avi). Taken from whichever version's movie the user has (USA or Japanese), so it
  echoes the movie that played the first time. Sound effects and voice-over in that stretch are
  fine. Ocarina notes: the user picks a short phrase by ear from OoT's five notes and sends the
  buttons; check it doesn't start like an existing song.

## Young Link at the mansion (from the user, 2026-10-29)
- Young Link (seven years earlier, before the outbreak) arrives in the foyer with every door
  locked: no enemies, no story, just the place.
- Easter egg: Spencer wouldn't be there by then, but Wesker plausibly could be. A brief Wesker
  sighting (his model and animations are already in the game) would be a fun nod.
- Bigger idea kept for later: young Link as Chris's campaign (fewer slots, no lockpick; the room
  scripts already hold both characters' branches).
- Where the song comes from: the first visit by the Lost Woods door; the song learned inside
  (Richard's radio, sheet music, or Brad's helicopter signal) to get back out.
- Young Link's Wesker: a little dismissive, like the Gerudo guard turning young Link away. In an
  outfit fit for 1991 (in RE0's flashbacks he already has the slicked blond hair and sunglasses),
  so a clothing retexture: done at conversion from the user's own files (a recolor step or a
  texture the user supplies), never committed.

## RE1 items outside the mansion (brainstorm, 2026-10-30)
- Flare: a flash that stuns (as a Deku Nut) or lights a dark place for a while.
- V-Jolt: withers Deku Babas or vines and weeds blocking a path.
- Lockpick: only some of Hyrule's locks (Kakariko's houses, a few chests); OoT's keys stay.
- Oil + lighter: burn one specific thing (a blocked passage, a hive).
- Jewels, medals, crests: worth rupees, but only once they've done their puzzle job.
- Radio: "call Brad", a hint line with RE flavour, or the helicopter at the end.
- Ammo in tall grass: off by default; maybe only once Link owns the gun, rarely.
