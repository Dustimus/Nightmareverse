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
  affecting anything else (walls are about railing/door height now). Done 2026-11-20 (README, "The bomb layer").
- Furniture tops (2026-11-20, from the user): collision on top of furniture, starting with the
  dining room's table (climbable is fine), with heights set by hand; the key to joining the dining
  room and its balcony into one room, which would feel very Zelda. The table's top is in
  (README, "Furniture tops"); the join is still to try, on a branch of its own.
  - RE1's enemies don't know heights: a zombie beside the table can grab Link standing on it. The
    user's fine with that (it grabs his legs).
  - Merged with the balcony, the only zombie is the balcony's: better that it can't fall down to
    the dining room (it stays up there).
  - Two ways to join (2026-11-21, from the user). Being tried on re1/dining-merge first: Link
    dropping through the statue's gap goes straight into the dining room with no fade, keeping
    his fall and run. The point is that he lands where he was aiming, not on a fixed spot.
    The user expects to tune this more.
  - The way the user thinks may look cleaner: a real combined room, where the balcony (202)
    includes the dining room (105) and checks all the same flags. The dining room wouldn't need
    to include the balcony; that direction is the one that gets messy. RE1's scripts for the two
    rooms are a little odd. The balcony zombie could be a problem unless it is only triggered by
    coming in through the balcony door. To decide after the user's playtest.

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
- Moonlight Sonata (the piano bar puzzle, 10F): built 2026-11-17 (`z_nv_script.c`, NvPiano_* and
  NvSonata_*), after mod1's plan (Dustimus/mod1 docs/design.md and its En_ReSong):
  - the notes A, C-down, C-right, A, C-down, C-right (the opening triplet twice);
  - learned for good when the Music Notes (item 0x23) are picked up (event flag 0xD8); their Check
    shows the buttons;
  - examining the piano once it's learned asks to play it; yes seats Link as RE1 seats Jill (the
    room's event 6, held where the playing starts) and opens OoT's ocarina staff, the notes
    showing as they're played (the puzzle is remembering them); six notes and it's judged;
  - right: OoT's correct chime, then RE1's scene goes on (the playing, the wall, 1:A5);
  - wrong: OoT's error sound, Chris's hesitant presses from his 10F set, then a word pointing back
    to the sheet music;
  - RE1's own way (Use the notes at the piano) still works too;
  - played freely in the bar it does nothing; outside the mansion it turns the clock to 19:01 (the
    Easter egg; not yet tried in play). Decided with the user (2026-11-19): 19:01, not midnight,
    so it's fully dark and the evening's windows (Dampe's tour, the evening doors, night-only
    Gold Skulltulas) are open at once; in Termina, the clock moves forward to that hour.
  - A song matches only from notes played in one go (2026-11-19): the heard notes are cleared as
    the ocarina comes out and when OoT recognises one of its own songs. The Sonata is the Song of
    Time shifted by a note, so leftovers from one Song of Time used to complete it on the next.
  - The other branch keeps one list of every ocarina song across the games with a clash check
    (`mm/tools/nv_songs.py` on the Majora's Mask branch): new RE1 songs go there too.
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
- Young Wesker teaching the warp song (from the user, 2026-11-23; brainstorm, dialogue to write):
  - Where: Kakariko. Nocturne of Shadow is taught there and the warp song already rides its slot;
    the House of Skulltula (a family turned into monsters: an "infection" a researcher would want
    to study) and the Bottom of the Well (Redeads, OoT's grimmest place, "worth samples") are both
    there and open to young Link; and the windmill man already teaches young Link a song there.
  - A possible flow: a polite man in a lab coat and sunglasses near the well or the cursed house;
    after the well, or a few Skulltula tokens, he plays "a tune from home": the warp song. Played
    as adult Link, it takes him to the mansion that man's company built.
  - His look: RE1's lab-coat zombie body (EM1000: white coat, shirt and tie; EM1011 the same coat,
    filthy green; both the same 15 parts as Wesker) with Wesker's head (EM1024), the coat repainted
    clean (maybe an Umbrella logo), Wesker's own animations if the joints line up. He keeps the
    sunglasses: RE0's lab-coat flashbacks have them too.
  - The song itself could then come from one of Wesker's themes (the user picks the phrase and
    the arrangement) instead of the basement-theme placeholder; swapping is the notes array
    (z_nv_script.c sNvSongNotes, checked against the cross-game song list) and the fanfare's
    sequence (make_mod.py --song, looked up by name in NvWarpSong_Fanfare).

## RE1 items outside the mansion (brainstorm, 2026-10-30)
- Flare: a flash that stuns (as a Deku Nut) or lights a dark place for a while.
- V-Jolt: withers Deku Babas or vines and weeds blocking a path.
- Lockpick: only some of Hyrule's locks (Kakariko's houses, a few chests); OoT's keys stay.
- Oil + lighter: burn one specific thing (a blocked passage, a hive).
- Jewels, medals, crests: worth rupees, but only once they've done their puzzle job.
- Radio: "call Brad", a hint line with RE flavour, or the helicopter at the end.
- Ammo in tall grass: off by default; maybe only once Link owns the gun, rarely.

- Status screen background (user, 2026-10-31): the RE1 page now sits at the right size inside the
  map page's border; later, give it its own custom background in place of OoT's map page art, and
  tune the scaling.

- Get-item moments (decided with the user, 2026-11-03): two tiers, each only the first time.
  - The big one (Link holds it up, OoT's chest jingle, RE1's item-viewer model turning above him,
    a short line in OoT's text box): each new gun (shotgun, Colt Python, grenade launcher, rocket
    launcher), each map (its line also says it's on the pause screen now), the lockpick (Barry
    hands it over).
  - The small one (the quick jingle and one short tutorial line, like a first Deku Nut): the first
    green, red and blue herb, first aid spray, ink ribbon.
  - None: the mansion keys (all or none: none, unless this ever replaces RE1's Check viewer), the
    crests (treated as keys), the medals, puzzle items, files, ammo, repeats. RE1's own Check viewer stays.
  - Seen-it flags kept with the save system. Our own wording for the lines.

## Remixed dungeons (from the user, 2026-11-04)

- Eventually, remix OoT's dungeons with RE1's enemies: Hunters in the Forest Temple's halls,
  crows over Death Mountain, Cerberus in the Shadow Temple, and so on. The ported enemies already
  run on their own RE1 entity and room collision, so placing them in an OoT room would mostly
  mean giving them its floor and walls and a spawn list.
- The same goes for other games, if the larger project takes in more of them: their enemies
  mixed into OoT's dungeons (or each other's rooms) as a remix mode, separate from the faithful
  playthrough.

## Costumes on Link (from the user, 2026-11-04)

- Look at the other releases' outfits (PS1, Director's Cut, Saturn: re1/VERSIONS.md) for how
  well they'd suit Link: as tunic colours, or further.
- Maybe: Link's head on an RE1 character's body (or the other way round) for the costumes. A
  headache (the skeletons and proportions differ), so a maybe.

## Jill as Navi (from the user, 2026-11-05)

- Navi speaks Jill's lines in the mansion: the scenes' and the examine text's voice lines
  played as Navi's, with Navi's text box (it's blue already). Link stays Link on screen.
- The bigger joke: Navi is Jill for the whole mod, her name and her voice triggers changed
  (not her model). Every Navi line in OoT would need a Jill reading.
- The voice: RE1's own Jill lines come from the player's files and need no new recording; new
  lines (all of Navi's) would need a voice. Cloning the original actress's voice without her
  consent is the uncomfortable route, even for friends only; a sound-alike performer who agrees
  to it (with a similar filter on top) is the clean one, and the only one for a wider release.

- Who Jill is (from the user, 2026-11-21; to decide after the user's playtest, which they'll play
  through with it in mind): a character of her own, the fairy, Navi herself, or something else.
  The question is how jarring each would be.

## Longer days in Hyrule (from the user, 2026-11-19; for later, not planned)

- MM-length days in Hyrule (if Termina's clock ever comes over) would break less than expected:
  the Moonlight Sonata already jumps to 19:01, so the evening's windows stay quick to reach.
- The pain points are before the Sun's Song: being stuck in Hyrule Field at night, and the Weird
  Egg. OoT hatches it at dawn, either natural morning (z_kankyo.c's day change) or the Sun's Song
  (z_play.c's nextDayTime). It's not a timer or a script, so a longer night means a longer wait
  on the way to Zelda.
- The user dislikes the Switch version's fix: it stays day until Zelda is first met, the
  drawbridge stays down at night, and the egg hatches partway to the castle for no visible
  reason. It's an overcorrection. The original's short wait felt right, and it's what makes you
  enter the castle by day, so the night patrol doesn't confuse you. Any change here should keep
  that.
