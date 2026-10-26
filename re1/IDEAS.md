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
