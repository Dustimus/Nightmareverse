# RE1's other versions: what they could add

Our baseline is the USA PC release (GOG), the files `make_mod.py` converts. This note keeps
track of what the other releases have that it doesn't, for the parts worth bringing over (as
options, from the player's own copy of that release, like everything else here).

Each line says where it comes from: **checked** (seen in the decomp's code or docs, or in the
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
- Two more wardrobe outfits (the Saturn offers them in 11C / 61C).
- The GPT summary was cut off before its Saturn section; anything it adds can go here.
