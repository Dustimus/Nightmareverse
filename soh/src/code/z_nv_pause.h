#ifndef Z_NV_PAUSE_H
#define Z_NV_PAUSE_H
/*
 * Nightmareverse: the pause menu's columns (z_nv_pause.c; NV_PAUSE_MENU.md). Each side of OoT's box
 * keeps OoT's page and can have alternate pages stacked above and below it, a flat column that
 * loops; the cursor past the page's top or bottom row slides the column a whole page. A game adds
 * a page by registering it; a side with no alternates is OoT's alone, exactly as before.
 */
#include "global.h"

typedef struct NvPausePage {
    const char* name; // for the log
    s32 side;         // PAUSE_ITEM, PAUSE_EQUIP, ...
    // whether it's in the column (its game installed, something to show)
    s32 (*available)(PlayState* play);
    // the page the column opens on here (where Link is: its game's place)
    s32 (*home)(PlayState* play);
    // its frame: the 15 tiles (80x32 IA8) of the page's background, and the flat colour they're
    // tinted (MM's pages are a primitive colour, OoT's are vertex colours)
    const char* const* tiles;
    u8 color[3];
    // its content, the page's matrix loaded: `front` when it's the page in front, at rest, on the
    // side being looked at (the cursor and the buttons are its then)
    void (*draw)(PlayState* play, s32 front);
    // the name bar: the texture of what the cursor is on (NULL: nothing named)
    const char* (*nameTex)(PlayState* play);
    // it's arrived in front (the cursor set up), from above (dir -1) or below (dir 1)
    void (*enter)(PlayState* play, s32 dir);
} NvPausePage;

void NvPause_Register(const NvPausePage* page);

// the kaleido's hooks (ovl_kaleido_scope)
s32 NvPause_DrawColumn(PlayState* play, s32 side, Vtx* pageVtx, void** ootTiles, void (*ootDraw)(PlayState* play));
void NvPause_Edge(PlayState* play, s32 side, s32 dir);
const char* NvPause_Name(PlayState* play, s32* named);

#endif
