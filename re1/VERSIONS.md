# RE1's other versions: what they could add

Our baseline is the USA PC release (GOG), the files `make_mod.py` converts. This note keeps
track of what the other releases have that it doesn't, for the parts worth bringing over (as
options, from the player's own copy of that release, like everything else here).

## How it would come in

Everything stays the USA PC game by default; each addition is an option, converted from the
player's own copy of that release (never committed), and off when that copy isn't given.

1. **Art and sound, no gameplay change:** the Japanese document backdrops (first pick: a small
   job in the file reader), the other releases' outfits (Link's tunic colours are the natural
   place for these), the Dual Shock and Saturn sound sets, the Saturn models and textures.
2. **Gameplay options:** auto-aim, the Japanese balancing and Beginner mode as difficulty
   settings; the Director's Cut's fast zombies, custom Beretta and infinite Python; the Saturn's
   Tick (stage 3), second Tyrant (the lab), and the Battle Game with Zombie Wesker and the Golden
   Tyrant as a mode of its own.
3. **The Arrange campaign:** the Director's Cut's moved items and enemies, extra events and
   new cameras, as a second set of room data on the same mansion (the camera system takes another
   set of camera records; the redrawn rooms need their pictures). The biggest of these; each room's
   change checked against the Director's Cut's own room data, not taken from descriptions.
4. **Extras, outside the campaign:** the Japanese vocal songs and death-montage credits, the
   unused Japanese voices, the console menus and demos.

Each line below says where it comes from: **checked** (seen in the decomp's code or docs, or in the
files themselves) or **reported** (from a summary the user had GPT write, 2026-11-04: plausible,
not checked yet; check before building on it).

## Japanese PC (Biohazard, MediaKite)

- **Document backdrops** (checked: the decomp's `MainMenu.cpp` reads them, `Biohazard.exe`
  0x004af1d8): a scanned paper backdrop behind each file (`Item_m2/FILEI*.TIM`, 14 of them,
  mapped per file), with the pages as separate TIMs (`TEXTM_*`). The USA PC shows its files on
  plain pages. Best candidate for us: the backdrop under our English pages (z_nv_files.c).
- Japanese text, font, item names (checked: ported in the decomp's `JpnTextTables.cpp`).

## Japanese PlayStation (Bio Hazard, 1996)

- Document backdrops too (reported; the Japanese PC's are presumably the same art).
- Auto-aim, easier balancing (enemy health and damage), three ink ribbons a pickup, Jill/Chris
  shown as Easy/Hard on the character select (reported).
- Two vocal songs, an opening and an ending theme, later releases replaced with instrumentals
  (reported).
- An extra credits sequence after a clear with the rocket launcher (reported).

## USA PlayStation (1996)

- Its own alternate outfits, different from the PC's (reported).
- Otherwise little the PC lacks: censored intro, unskippable doors (reported).

## Director's Cut (PlayStation, 1997)

The decomp ports the Director's Cut's Arrange mode as a mode of its own (checked:
`docs/PSX_DIRECTORS_CUT_ANALYSIS.md`, `src/game/dc/`). The user is looking into it separately.

- Arrange mode: new camera angles and some redrawn backgrounds, moved items and enemies,
  faster zombies, Forest's corpse rising, the bathroom mirror scare, the custom Beretta (its
  random head shots), new outfits including Rebecca's, a redrawn wardrobe room, the infinite
  Python (reported; the decomp has the Arrange rooms, outfits and the custom Beretta, checked).
- Training (Beginner) mode: double ammo and ribbons, more forgiving (reported).

## Director's Cut Dual Shock (1998)

- A whole replacement soundtrack (reported). An optional music set, if wanted.
- Analog and rumble (OoT has both already).

## Sega Saturn (USA, the user's copy, 2026-11-04)

From the decomp's `docs/SATURN_VERSION_ANALYSIS.md` (checked) and the disc itself:

- **The Tick** (`ENEMY/EM1016.EMD`): replaces the Hunters in stage 3 (courtyard and caves), on
  its own build of the Hunter's code in `GAME2.PRG`: a stretching-arm stab, its own grab. Planned
  for when stage 3 is built, as an option when a Saturn disc is given.
- **The Battle Game**: an arena mode with its own rooms, including Zombie Wesker
  (`EM1017.EMD`: the zombie with double health and no head shot).
- Extra room props (`ENEMY/A_*.DAT`): two Tyrant capsules, more plants, a kitchen machine.
- Extra camera pictures in a few rooms (105's plaque close-up, the heliport's views), and
  a redrawn lab Tyrant room.
- Two more wardrobe outfits (the Saturn offers them in 11C / 61C). Reported: Chris in a dark
  blue shirt and body armour, Jill in a light blue top, green trousers and beret, also pickable
  by button combination at the start.
- **A second Tyrant in Chris's game** (checked: §4g/§4h): in the same room as the first (stage 5
  room 13, the main lab), the first one's entity raised again by the Tyrant's own code, not a new
  room script.
- **The Golden Tyrant** (checked): the Battle Game's arena 15 tints the Tyrant gold (its palette
  red +4, green +3, blue -5) and makes it stronger. No new model.
- Its own models (checked: the Saturn EMDs carry a Saturn mesh and textures beside the PS1
  data, `tools/saturn_emd.py`); reported: visible differences in the players, Hunters, the snakes,
  Plant 42, zombies (Enrico's darker hair, for one).
- Redrawn backgrounds (checked: 115 of 655 camera pictures differ from the PC's noticeably, the
  lab Tyrant room re-drawn; most are the Saturn's dimmer palettes).
- Gore (reported): blood shown differently, some dismemberment staying visible, severed zombie
  heads persisting (more so in the Japanese Saturn release). Checked: Zombie Wesker can't lose
  his head, a hit there only flashes.
- Sounds (reported): different crow and zombie sounds, some music in other keys or arranged
  slightly differently. Checked: its music is a sound-driver format of its own (the decomp's
  `tools/saturn_snd.py` renders it).
- Platform matters (reported): saves, loading, door timing, video, transparency. Nothing to bring.

## Japanese Saturn (Bio Hazard)

- The same Saturn additions as the USA disc (the Tick, the second Tyrant, the Battle Game,
  Zombie Wesker, the Golden Tyrant, its outfits and models) (reported). The user's copy is the
  USA one, which has all of them.
- Japanese text, uncut movies, more gore and longer-lasting dismemberment (reported).
- A rotating "BIO HAZARD" logo on the Battle Game's results screen, drawn only by the Japanese
  disc (checked: the decomp's `tools/saturn_ranking_logo.py`).
- Its own Battle Game questions for the wardrobe (checked: `tools/saturn_wardrobe_rdt.py --jpn`).

## Japanese Dual Shock bonus disc

- Japanese voice recordings for the game's scenes that were never used (reported). Interesting
  for later, alongside the voice acting the user wants for the cutscenes.
