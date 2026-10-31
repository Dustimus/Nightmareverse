/*
 * Nightmareverse: RE1's status screen, in the pause menu's map page while Link is in the mansion
 * (re1/README.md, "Items": the dungeon map's place, and the page the pause menu opens on there).
 *
 * First version: Jill's slots (z_nv_items.c) as a grid, a cursor, and Check (the item's name and
 * count). Use, Equip, Combine, the files and the map come next. Drawn flat over the page once it
 * has turned to face the player; the page itself keeps OoT's frame.
 */
#include "global.h"
#include <stdio.h>

#define NV_ST_SLOTS 8
#define NV_ST_COLS 2

s32 NvItems_Slot(s32 i, s32* qty);
const char* NvItems_Name(s32 id);

static s32 sNvStCursor;
static s32 sNvStChecked = -1; // the slot Check shows, or -1
static s8 sNvStHeld;          // the stick's last direction (moves happen on a new push)

/** Whether the pause menu shows RE1's screen: Link is in one of RE1's rooms. */
s32 NvStatus_InMansion(PlayState* play) {
    return play->sceneNum >= SCENE_RE1_100;
}

static void NvSt_Rect(Gfx** gfxP, s32 x, s32 y, s32 w, s32 h, u8 r, u8 g, u8 b, u8 a) {
    Gfx* gfx = *gfxP;

    gDPSetPrimColor(gfx++, 0, 0, r, g, b, a);
    gDPFillRectangle(gfx++, x, y, x + w, y + h);
    *gfxP = gfx;
}

/** The stick (or D-pad) moves the cursor one slot per push. */
static void NvSt_Input(PlayState* play) {
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = &play->state.input[0];
    s8 dir = 0;

    if (pauseCtx->stickRelX < -30) {
        dir = 1;
    } else if (pauseCtx->stickRelX > 30) {
        dir = 2;
    } else if (pauseCtx->stickRelY > 30) {
        dir = 3;
    } else if (pauseCtx->stickRelY < -30) {
        dir = 4;
    }
    if (CHECK_BTN_ALL(input->press.button, BTN_DLEFT)) {
        dir = 1, sNvStHeld = 0;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DRIGHT)) {
        dir = 2, sNvStHeld = 0;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DUP)) {
        dir = 3, sNvStHeld = 0;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DDOWN)) {
        dir = 4, sNvStHeld = 0;
    }
    if ((dir != 0) && (dir != sNvStHeld)) {
        s32 c = sNvStCursor;

        if ((dir == 1) && (c % NV_ST_COLS) > 0) {
            c--;
        } else if ((dir == 2) && (c % NV_ST_COLS) < NV_ST_COLS - 1) {
            c++;
        } else if ((dir == 3) && (c >= NV_ST_COLS)) {
            c -= NV_ST_COLS;
        } else if ((dir == 4) && (c + NV_ST_COLS < NV_ST_SLOTS)) {
            c += NV_ST_COLS;
        }
        if (c != sNvStCursor) {
            sNvStCursor = c;
            sNvStChecked = -1;
            Audio_PlaySfxGeneral(NA_SE_SY_CURSOR, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
    }
    sNvStHeld = dir;
    if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
        s32 qty;

        if (NvItems_Slot(sNvStCursor, &qty) != 0) {
            sNvStChecked = (sNvStChecked == sNvStCursor) ? -1 : sNvStCursor;
            Audio_PlaySfxGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                                 &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
        }
    }
}

/**
 * The page's content (z_kaleido_scope_PAL.c, in the map page's place). interactive: the page faces
 * the player and takes input; otherwise (turning past) nothing is drawn over it.
 */
void NvStatus_Draw(PlayState* play, GraphicsContext* gfxCtx, s32 interactive) {
    PauseContext* pauseCtx = &play->pauseCtx;
    GfxPrint printer;
    Gfx* gfx;
    s32 i;

    if (!interactive || (pauseCtx->state != 6) || (pauseCtx->unk_1E4 != 0)) {
        return;
    }
    if (pauseCtx->debugState == 0) {
        NvSt_Input(play);
    }

    OPEN_DISPS(gfxCtx);
    gfx = POLY_OPA_DISP;
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gSPClearGeometryMode(gfx++, G_ZBUFFER);

    // the panel, Jill's slots in two columns, and the cursor
    NvSt_Rect(&gfx, 48, 52, 224, 140, 0, 0, 0, 170);
    for (i = 0; i < NV_ST_SLOTS; i++) {
        s32 x = 60 + (i % NV_ST_COLS) * 104;
        s32 y = 70 + (i / NV_ST_COLS) * 24; // (text cells are 8 pixels: row 9 + 3 per slot row)

        if (i == sNvStCursor) {
            NvSt_Rect(&gfx, x - 3, y - 3, 102, 20, 200, 160, 40, 220);
        }
        NvSt_Rect(&gfx, x, y, 96, 14, 40, 40, 50, 230);
    }
    gDPPipeSync(gfx++);

    GfxPrint_Init(&printer);
    GfxPrint_Open(&printer, gfx);
    GfxPrint_SetColor(&printer, 230, 230, 230, 255);
    GfxPrint_SetPos(&printer, 7, 7);
    GfxPrint_Printf(&printer, "STATUS");
    for (i = 0; i < NV_ST_SLOTS; i++) {
        s32 qty = 0;
        s32 id = NvItems_Slot(i, &qty);

        GfxPrint_SetPos(&printer, 8 + (i % NV_ST_COLS) * 13, 9 + (i / NV_ST_COLS) * 3);
        if (id == 0) {
            GfxPrint_SetColor(&printer, 90, 90, 100, 255);
            GfxPrint_Printf(&printer, "-");
        } else {
            GfxPrint_SetColor(&printer, 230, 230, 230, 255);
            GfxPrint_Printf(&printer, "%.12s", NvItems_Name(id));
        }
    }
    // Check: the item's name and how many
    GfxPrint_SetColor(&printer, 200, 160, 40, 255);
    GfxPrint_SetPos(&printer, 7, 21);
    if ((sNvStChecked >= 0) && (NvItems_Slot(sNvStChecked, NULL) != 0)) {
        s32 qty = 0;
        s32 id = NvItems_Slot(sNvStChecked, &qty);

        GfxPrint_Printf(&printer, "%s", NvItems_Name(id));
        if (qty > 1) {
            GfxPrint_SetColor(&printer, 230, 230, 230, 255);
            GfxPrint_Printf(&printer, "  x%d", (int)qty);
        }
    } else {
        GfxPrint_SetColor(&printer, 140, 140, 150, 255);
        GfxPrint_Printf(&printer, "A: Check");
    }
    gfx = GfxPrint_Close(&printer);
    GfxPrint_Destroy(&printer);
    POLY_OPA_DISP = gfx;
    CLOSE_DISPS(gfxCtx);
}

/** As the pause menu opens in the mansion: open on this page (the map page's place). */
void NvStatus_OnPause(PlayState* play) {
    sNvStChecked = -1;
}
