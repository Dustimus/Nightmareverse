# The pause menu across the games (design note)

Status: a design, not built. Written 2026-11-20 with the user, for the MM work and for RE1 (whose
status and map screens already live in the pause menu). Anything built from this goes through
the user first, and through the RE1 chat for the parts RE1 uses.

## What there is now

OoT's pause menu (`soh/src/overlays/misc/ovl_kaleido_scope/`, `soh/src/code/z_kaleido_setup.c`) is a
four-sided box: Select Item, Map, Quest Status, Equipment.
- The camera sits at one of four points 90 degrees apart (`sKaleidoSetupEyeX0/Z0`, one per
  `pageIndex`).
- L, R and Z turn the box a quarter turn, and so does moving the cursor past a page's left or
  right edge.
- The two neighbouring sides show at the screen's left and right edges.
- The save prompt is drawn on the back.

RE1 already uses it: in RE1's rooms the Map side is RE1's status screen and map screen
(`z_nv_status.c`, `z_nv_map.c`, hooked in `z_kaleido_scope_PAL.c` and `z_kaleido_setup.c`).

Majora's Mask's menu is also four sides: Select Item (24 slots), Map, Quest Status (songs,
remains, the Bomber's Notebook, and the sword and shield), and Masks (24 slots). MM has no
Equipment side: its sword and shield are upgraded for the player and shown on Quest Status, and
the Masks page takes the fourth side.

## The plan: four sides, each with pages above and below

Decided with the user (2026-11-20):
- **Shape:** the box keeps its four sides and its turning. No five- or six-sided prism: that
  would rewrite most of the pause menu's geometry and every RE1 hook in it, and make every page
  narrower.
- **Alternates in a column:** each side gets alternate pages stacked above and below it.
- **A flat column that loops** (the user, 2026-11-20; it replaced an earlier take with the
  pages tilted onto the box's ceiling and floor). The pages stand flat, one above the other,
  and the column loops: past the last page comes the first again.
  - **At rest:** the page in front is exactly as OoT has it. The bottom edge of the page above
    shows over its top border, and the top edge of the page below under its bottom border, flat,
    with a strip of wood between pages, as between the box's sides.
  - **A slide moves the column one whole page**, smoothly, never stopping halfway, so it's never
    "half and half". The page that arrives is whole and in front, its neighbours peeking.
  - Each page's edges are joined to its outer frame, not to the item grid inside it.
- **Nothing changes size.** The page, the window and the box stay exactly as OoT has them (the
  user: "if you're changing anything about the actual window size you're probably not doing it
  right"). The alternates use the ceiling and floor space that's already there.
- **Sliding:** moving the cursor past a page's top or bottom row slides that column up or down,
  the vertical twin of how OoT turns the box at a page's left or right edge. A slide, not a
  rotation: the box doesn't turn over.

| Side | Its column (opens on the one for where Link is) |
|---|---|
| Items | OoT's items; MM's items; RE1's inventory (a proper item list) |
| Equipment | OoT's equipment; MM's masks (worn, like tunics and boots; the forms change Link as the Iron Boots do) |
| Quest Status | OoT's; MM's (songs, remains, Bomber's Notebook); later maybe one page with every game's songs |
| Map | Hyrule's; Termina's; RE1's map and status screens (as now) |

**Which page opens:** the one for where Link is. MM's in Termina (`NvMm_IsScene`), RE1's in the
mansion (`NvRe1_IsScene`), OoT's anywhere else. From anywhere, the others are a slide away.

**Only installed games:** a game whose archive isn't loaded (the "mod list" idea in
`NV_GAMES.md`) has no pages, so a column with OoT's page alone has no slivers and doesn't slide.

**Items both games have** show on both pages, the same item behind both (the shared-items rules in
`NV_GAMES.md`, "Majora's Mask merged into OoT"). Only MM's own things need slots of their own:
the masks, Powder Kegs, the Pictograph Box, the trade items and the forms.

## Rules per page

- **The item and equipment grids** slide at the top and bottom rows freely.
- **Pages that already use up and down:**
  - OoT's dungeon map changes floor with them, and so does RE1's map screen;
  - there, the slide happens only past the last floor, or on a button instead (to try both).
- **The cursor keeps its column** as it slides, so up and down through a whole column is one
  straight line.
- **The name bar and button prompts** at the bottom belong to the page in front.

## How it could be built (for the RE1 chat to look at before anything is written)

One shared mechanism in the pause menu, with each game plugging its pages into it, so neither
game builds its own beside the other's:
- **A registry of alternates per side:** a name, "available here" (installed, unlocked), and
  the page's own update and draw.
  - OoT's four pages are each column's first entry, drawn by the kaleido code as now.
  - RE1's status and map screens become the Map column's RE1 entry: the same code, called
    from the registry instead of its own hook.
- **Per side:** the page shown and a slide offset.
  - Sliding draws the page in front and its neighbours one page's height (plus the wood
    between) above and below it, all moved by the slide's offset. At rest the neighbours fall
    mostly off screen and only their near edges show.
  - The kaleido draws a page from its own vertex arrays, so the column is the same page draw
    with a vertical offset, clipped to the page's column of the screen.
- **Kaleido's hooks:**
  - setup: which page opens;
  - the page update: cursor at the top or bottom edge, then slide;
  - the page draw: draw the column.

  Small, self-contained additions, with each game's pages in its own files.
- **Screen room:** the strips above and below the page are already there (the box's ceiling
  and floor wood), so nothing is resized.
  - The HUD (hearts, the buttons) and the name bar are drawn over them, as now.
  - The top HUD covers most of the strip above, so the page above shows less than the one
    below. Worth deciding where its name shows (in the mock-up it's under the hearts and
    buttons).

## Mock-up

Shared in the chat on 2026-11-20 (not committed: made from the game's own graphics). Three
rounds:
1. **The page shrunk to make room** for slivers, and the lower one against the item grid instead
   of the frame. Both wrong, per the user.
2. **The pages tilted onto the box's ceiling and floor.** The user preferred flat pages sliding
   whole.
3. **The current design:** a still at rest, and an animation of a slide up (Majora's Mask), down
   back to OoT, down to Resident Evil, and down again round the loop to Majora's Mask. The other
   games' pages are OoT's recoloured, as stand-ins.

To remake it:
- `tools/headless/pause_shots.sh` captures the four sides headless (R turns the box). Run it
  twice: once normally, and once from a temporary local build with `Interface_Draw` skipped in
  `Play_DrawOverlayElements` (`z_play.c`; not committed), for a page with no HUD.
- `tools/headless/pause_mock.py <clean> <with HUD> <out.png> [<out.gif>]` builds the column. The
  two captures' difference is the HUD, laid back over it exactly.

## Open questions (the user's)

- How much of the ceiling and floor pages shows, and where the page above's title goes, given
  the HUD over the ceiling.
- The map side's rule: past the last floor, or a button.
- Whether Quest Status gets one combined songs page for every game.
- What RE1's item list looks like as a pause page, next to its status screen.
