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
- **Peeking:** a sliver of the page above and the page below shows at the top and bottom
  edges, the way the next sides show at the left and right.
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
  - Sliding draws the page in front and its neighbours moved up or down by a page's height.
  - Clipping (a scissor) keeps only the slivers of the neighbours on screen.
- **Kaleido's hooks:**
  - setup: which page opens;
  - the page update: cursor at the top or bottom edge, then slide;
  - the page draw: draw the column.

  Small, self-contained additions, with each game's pages in its own files.
- **Screen room:** the slivers sit between the page and the title above it and the name bar
  below it. The page may need to shrink a little, which the mock-up shows.

## Mock-up

Shared in the chat on 2026-11-20 (not committed: it's made from the game's own graphics). It shows
the Items side with OoT's page in front, the side pages at the left and right as now, and a
sliver of "Majora's Mask: Items" above and "Resident Evil: Inventory" below. In it, the
alternates are OoT's page recoloured as stand-ins. Remake it with `tools/headless/pause_shots.sh` (the four
pages headless, R turning the box) and ImageMagick.

## Open questions (the user's)

- How deep the slivers should be: a thin edge, or enough to read the page's title.
- The map side's rule: past the last floor, or a button.
- Whether Quest Status gets one combined songs page for every game.
- What RE1's item list looks like as a pause page, next to its status screen.
