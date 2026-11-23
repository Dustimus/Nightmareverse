/*
 * Nightmareverse: Majora's Mask's pause pages, in the shared columns (z_nv_pause.c, NV_PAUSE_MENU.md):
 * MM's items in the Items column, MM's masks in the Equipment column, each MM's own: its tiles and
 * flat colour (MM's z_kaleido_scope_NES.c), its icons and names (MM's icon_item_static and
 * item_name_static, mm/tools/mm_pause.py), MM's 6 by 4 grid and slot order (MM's SLOT_*). They open
 * in front in Termina; elsewhere they're a slide away.
 *
 * What a slot shows (one inventory, mm/README.md "Decisions"): what OoT has too shows from OoT's own
 * inventory (the ocarina, arrows, bombs, sticks, nuts, beans, the Lens, the bottles); what only MM
 * has from MM's (z_nv_mm_getitem.c). The bottles: OoT's four in MM's first four bottle slots, MM's
 * own bottled things after them (how the two games' bottles share is to settle).
 *
 * Not yet: using MM's items and wearing its masks from C buttons (a C button on these pages does
 * nothing yet), ammo counts on the icons, the masks' worn marks.
 */
#include "global.h"
#include "z_nv_pause.h"
#include "nv_mm_pause.inc"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEX(file, name) "__OTR__mm/textures/" file "/" name
#define MM_NONE 0xFF

s32 NvMmItem_Has(s32 item);

static const char* const sItemTiles[] = {
    TEX("icon_item_jpn_static", "gPauseSelectItem00ENGTex"), TEX("icon_item_static", "gPauseSelectItem01Tex"),
    TEX("icon_item_static", "gPauseSelectItem02Tex"),         TEX("icon_item_static", "gPauseSelectItem03Tex"),
    TEX("icon_item_static", "gPauseSelectItem04Tex"),         TEX("icon_item_jpn_static", "gPauseSelectItem10ENGTex"),
    TEX("icon_item_static", "gPauseSelectItem11Tex"),         TEX("icon_item_static", "gPauseSelectItem12Tex"),
    TEX("icon_item_static", "gPauseSelectItem13Tex"),         TEX("icon_item_static", "gPauseSelectItem14Tex"),
    TEX("icon_item_jpn_static", "gPauseSelectItem20ENGTex"), TEX("icon_item_static", "gPauseSelectItem21Tex"),
    TEX("icon_item_static", "gPauseSelectItem22Tex"),         TEX("icon_item_static", "gPauseSelectItem23Tex"),
    TEX("icon_item_static", "gPauseSelectItem24Tex"),
};

static const char* const sMaskTiles[] = {
    TEX("icon_item_static", "gPauseMasks00Tex"),         TEX("icon_item_static", "gPauseMasks01Tex"),
    TEX("icon_item_static", "gPauseMasks02Tex"),         TEX("icon_item_static", "gPauseMasks03Tex"),
    TEX("icon_item_static", "gPauseMasks04Tex"),         TEX("icon_item_jpn_static", "gPauseMasks10ENGTex"),
    TEX("icon_item_static", "gPauseMasks11Tex"),         TEX("icon_item_static", "gPauseMasks12Tex"),
    TEX("icon_item_static", "gPauseMasks13Tex"),         TEX("icon_item_static", "gPauseMasks14Tex"),
    TEX("icon_item_static", "gPauseMasks20Tex"),         TEX("icon_item_static", "gPauseMasks21Tex"),
    TEX("icon_item_static", "gPauseMasks22Tex"),         TEX("icon_item_static", "gPauseMasks23Tex"),
    TEX("icon_item_static", "gPauseMasks24Tex"),
};

// MM's mask page, slot by slot (MM's SLOT_MASK_* order, 0x18-0x2F): MM's item ids
static const u8 sMaskSlots[24] = {
    0x3E, 0x38, 0x47, 0x45, 0x40, 0x32, // Postman's Hat, All-Night, Blast, Stone, Great Fairy, Deku
    0x3A, 0x46, 0x39, 0x42, 0x48, 0x33, // Keaton, Bremen, Bunny Hood, Don Gero, Scents, Goron
    0x3C, 0x3D, 0x37, 0x3F, 0x36, 0x34, // Romani, Circus Leader, Kafei's, Couple's, Truth, Zora
    0x43, 0x41, 0x3B, 0x44, 0x49, 0x35, // Kamaro, Gibdo, Garo, Captain's Hat, Giant's, Fierce Deity
};

/** OoT's bottle contents as MM's item (its icon); MM_NONE for what MM hasn't (Ruto's Letter). */
static u8 NvMmPause_Bottle(u8 oot) {
    switch (oot) {
        case ITEM_BOTTLE: return 0x12;
        case ITEM_POTION_RED: return 0x13;
        case ITEM_POTION_GREEN: return 0x14;
        case ITEM_POTION_BLUE: return 0x15;
        case ITEM_FAIRY: return 0x16;
        case ITEM_FISH: return 0x1A;
        case ITEM_MILK_BOTTLE: return 0x18;
        case ITEM_MILK_HALF: return 0x19;
        case ITEM_BLUE_FIRE: return 0x1C;
        case ITEM_BUG: return 0x1B;
        case ITEM_POE: return 0x1D;
        case ITEM_BIG_POE: return 0x1E;
        default: return MM_NONE;
    }
}

/** The first of MM's items in [first, last] that Link has (a trade slot shows the latest one). */
static u8 NvMmPause_Latest(s32 first, s32 last) {
    s32 i;

    for (i = last; i >= first; i--) {
        if (NvMmItem_Has(i)) {
            return i;
        }
    }
    return MM_NONE;
}

/** MM's item page, slot by slot (MM's SLOT_* order): the MM item shown there, or MM_NONE. */
static u8 NvMmPause_ItemAt(s32 slot) {
    switch (slot) {
        case 0: // the ocarina: OoT's
            return (INV_CONTENT(ITEM_OCARINA_TIME) == ITEM_OCARINA_TIME)     ? 0x00
                   : (INV_CONTENT(ITEM_OCARINA_FAIRY) == ITEM_OCARINA_FAIRY) ? 0x05
                                                                             : MM_NONE;
        case 1: // the Hero's Bow: young Link's own (MM's)
            return NvMmItem_Has(0x01) ? 0x01 : MM_NONE;
        case 2:
            return (INV_CONTENT(ITEM_ARROW_FIRE) == ITEM_ARROW_FIRE) ? 0x02 : MM_NONE;
        case 3:
            return (INV_CONTENT(ITEM_ARROW_ICE) == ITEM_ARROW_ICE) ? 0x03 : MM_NONE;
        case 4:
            return (INV_CONTENT(ITEM_ARROW_LIGHT) == ITEM_ARROW_LIGHT) ? 0x04 : MM_NONE;
        case 5: // the Moon's Tear and the deeds
            return NvMmPause_Latest(0x28, 0x2C);
        case 6:
            return (INV_CONTENT(ITEM_BOMB) == ITEM_BOMB) ? 0x06 : MM_NONE;
        case 7:
            return (INV_CONTENT(ITEM_BOMBCHU) == ITEM_BOMBCHU) ? 0x07 : MM_NONE;
        case 8:
            return (INV_CONTENT(ITEM_STICK) == ITEM_STICK) ? 0x08 : MM_NONE;
        case 9:
            return (INV_CONTENT(ITEM_NUT) == ITEM_NUT) ? 0x09 : MM_NONE;
        case 10:
            return (INV_CONTENT(ITEM_BEAN) == ITEM_BEAN) ? 0x0A : MM_NONE;
        case 11: // the Room Key, the Letter to Mama
            return NvMmPause_Latest(0x2D, 0x2E);
        case 12:
            return NvMmItem_Has(0x0C) ? 0x0C : MM_NONE;
        case 13:
            return NvMmItem_Has(0x0D) ? 0x0D : MM_NONE;
        case 14:
            return (INV_CONTENT(ITEM_LENS) == ITEM_LENS) ? 0x0E : MM_NONE;
        case 15: // MM's Hookshot: young Link's own
            return NvMmItem_Has(0x0F) ? 0x0F : MM_NONE;
        case 16:
            return NvMmItem_Has(0x10) ? 0x10 : MM_NONE;
        case 17: // the Letter to Kafei, the Pendant of Memories
            return NvMmPause_Latest(0x2F, 0x30);
        case 18:
        case 19:
        case 20:
        case 21: // OoT's bottles
            return NvMmPause_Bottle(gSaveContext.inventory.items[SLOT_BOTTLE_1 + slot - 18]);
        default: { // MM's own bottled things (those OoT has none of)
            s32 nth = slot - 22;
            s32 i;

            for (i = 0x1F; i <= 0x27; i++) {
                if (NvMmItem_Has(i) && (nth-- == 0)) {
                    return i;
                }
            }
            if (NvMmItem_Has(0x17) && (nth == 0)) { // (the Deku Princess)
                return 0x17;
            }
            return MM_NONE;
        }
    }
}

static u8 NvMmPause_MaskAt(s32 slot) {
    return NvMmItem_Has(sMaskSlots[slot]) ? sMaskSlots[slot] : MM_NONE;
}

typedef struct {
    s32 side;
    u8 (*at)(s32 slot);
    s16 cursor; // the slot under the cursor (0-23)
} NvMmPauseGrid;

static NvMmPauseGrid sItems = { PAUSE_ITEM, NvMmPause_ItemAt, 0 };
static NvMmPauseGrid sMasks = { PAUSE_EQUIP, NvMmPause_MaskAt, 0 };

static s32 NvMmPause_Available(PlayState* play) {
    static s32 sLogged;
    s32 ok = ResourceMgr_FileExists("mm/textures/icon_item_static/gPauseSelectItem01Tex"); // (MM's archive loaded)

    if (!sLogged && getenv("NV_LOG")) {
        sLogged = true;
        printf("NVPAUSE MM pages available: %d\n", (int)ok);
        fflush(stdout);
    }
    return ok;
}

static s32 NvMmPause_Home(PlayState* play) {
    return NvMm_IsScene(play->sceneNum);
}

/** MM's cursor on its grid (the stick or the D-pad: one step a push), the page's edges: past the
 * left or right column the box turns (OoT's page arrows), past the top or bottom row the column
 * slides. */
static void NvMmPause_Input(PlayState* play, NvMmPauseGrid* g) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = &play->state.input[0];
    s16 x = g->cursor % 6;
    s16 y = g->cursor / 6;
    s16 old = g->cursor;

    if (pauseCtx->cursorSpecialPos != 0) { // on a page arrow: back onto the grid
        if ((pauseCtx->cursorSpecialPos == PAUSE_CURSOR_PAGE_LEFT) && (pauseCtx->stickRelX > 30)) {
            pauseCtx->cursorSpecialPos = 0;
            g->cursor = y * 6;
        } else if ((pauseCtx->cursorSpecialPos == PAUSE_CURSOR_PAGE_RIGHT) && (pauseCtx->stickRelX < -30)) {
            pauseCtx->cursorSpecialPos = 0;
            g->cursor = y * 6 + 5;
        }
    } else if ((pauseCtx->stickRelX < -30) || CHECK_BTN_ALL(input->press.button, BTN_DLEFT)) {
        if (x == 0) {
            KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_LEFT);
        } else {
            g->cursor--;
        }
    } else if ((pauseCtx->stickRelX > 30) || CHECK_BTN_ALL(input->press.button, BTN_DRIGHT)) {
        if (x == 5) {
            KaleidoScope_MoveCursorToSpecialPos(play, PAUSE_CURSOR_PAGE_RIGHT);
        } else {
            g->cursor++;
        }
    } else if ((pauseCtx->stickRelY > 30) || CHECK_BTN_ALL(input->press.button, BTN_DUP)) {
        if (y == 0) {
            NvPause_Edge(play, g->side, -1);
            return;
        }
        g->cursor -= 6;
    } else if ((pauseCtx->stickRelY < -30) || CHECK_BTN_ALL(input->press.button, BTN_DDOWN)) {
        if (y == 3) {
            NvPause_Edge(play, g->side, 1);
            return;
        }
        g->cursor += 6;
    }
    if (g->cursor != old) {
        Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    }
}

/** A page's content: the icons on MM's grid (OoT's item page's slots, the same 6 by 4), and in
 * front, the cursor. */
static void NvMmPause_DrawGrid(PlayState* play, NvMmPauseGrid* g, s32 front) {
    PauseContext* pauseCtx = &play->pauseCtx;
    s32 i;

    if (front) {
        NvMmPause_Input(play, g);
        u8 at = g->at(g->cursor);

        pauseCtx->cursorSlot[g->side] = g->cursor;
        // (cursorItem: what the name bar keys on; MM's items kept apart from OoT's ids)
        pauseCtx->cursorItem[g->side] = (at != MM_NONE) ? (0x200 + at) : PAUSE_ITEM_NONE;
        pauseCtx->cursorColorSet = (at != MM_NONE) ? 4 : 0;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_42Opa(play->state.gfxCtx);
    gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    for (i = 0; i < 24; i++) {
        u8 item = g->at(i);

        if ((item == MM_NONE) || (item >= ARRAY_COUNT(sMmItemIcons))) {
            continue;
        }
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, 255, 255, 255, pauseCtx->alpha);
        gSPVertex(POLY_OPA_DISP++, &pauseCtx->itemVtx[i * 4], 4, 0);
        KaleidoScope_DrawQuadTextureRGBA32(play->state.gfxCtx, (void*)sMmItemIcons[item], 32, 32, 0);
    }
    CLOSE_DISPS(play->state.gfxCtx);
    if (front && (pauseCtx->cursorSpecialPos == 0)) {
        KaleidoScope_SetCursorVtx(pauseCtx, g->cursor * 4, pauseCtx->itemVtx);
        KaleidoScope_DrawCursor(play, g->side);
    }
}

static void NvMmPause_DrawItems(PlayState* play, s32 front) {
    NvMmPause_DrawGrid(play, &sItems, front);
}

static void NvMmPause_DrawMasks(PlayState* play, s32 front) {
    NvMmPause_DrawGrid(play, &sMasks, front);
}

/** MM's English name for the item (a few MM never named in English, the Fairy Ocarina among them,
 * show none). */
static const char* NvMmPause_NameOf(u8 item) {
    if ((item == MM_NONE) || (item >= ARRAY_COUNT(sMmItemNames)) || (strstr(sMmItemNames[item], "JPN") != NULL)) {
        return NULL;
    }
    return sMmItemNames[item];
}

static const char* NvMmPause_ItemName(PlayState* play) {
    return NvMmPause_NameOf(sItems.at(sItems.cursor));
}

static const char* NvMmPause_MaskName(PlayState* play) {
    return NvMmPause_NameOf(sMasks.at(sMasks.cursor));
}

/** Arrived in front: the cursor on the row it came in from (the top coming down, the bottom coming
 * up), in the column it had. */
static void NvMmPause_Enter(NvMmPauseGrid* g, s32 dir) {
    if (dir > 0) {
        g->cursor = g->cursor % 6;
    } else if (dir < 0) {
        g->cursor = 18 + g->cursor % 6;
    }
}

static void NvMmPause_EnterItems(PlayState* play, s32 dir) {
    NvMmPause_Enter(&sItems, dir);
}

static void NvMmPause_EnterMasks(PlayState* play, s32 dir) {
    NvMmPause_Enter(&sMasks, dir);
}

// MM's pages' flat colour (MM's KaleidoScope_DrawPages: 180, 180, 120)
static const NvPausePage sItemPage = {
    "MM's items", PAUSE_ITEM, NvMmPause_Available, NvMmPause_Home, sItemTiles, { 180, 180, 120 },
    NvMmPause_DrawItems, NvMmPause_ItemName, NvMmPause_EnterItems,
};
static const NvPausePage sMaskPage = {
    "MM's masks", PAUSE_EQUIP, NvMmPause_Available, NvMmPause_Home, sMaskTiles, { 180, 180, 120 },
    NvMmPause_DrawMasks, NvMmPause_MaskName, NvMmPause_EnterMasks,
};

void NvPauseMm_Register(void) {
    NvPause_Register(&sItemPage);
    NvPause_Register(&sMaskPage);
}
