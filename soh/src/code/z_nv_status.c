/*
 * Nightmareverse: RE1's status screen, in the pause menu's map page while Link is in the mansion
 * (re1/README.md, "Items": the dungeon map's place, and the page the pause menu opens on there).
 *
 * Jill's slots (z_nv_items.c) as a grid and a cursor. A on an item opens its actions, as RE1's:
 * Use (the heals, by RE1's rules), Combine (then the second item: RE1's herb mixes), Check (its
 * name and count), Back. (B is the pause menu's own: it closes it.) Equip, the files and the map
 * come next. Drawn flat over the page once it has turned to face the player; the page itself keeps
 * OoT's frame.
 */
#include "global.h"
#include <stdio.h>

#define NV_ST_SLOTS 8
#define NV_ST_COLS 2

s32 NvItems_Slot(s32 i, s32* qty);
const char* NvItems_Name(s32 id);
s32 NvItems_Use(PlayState* play, s32 i);
s32 NvItems_Combine(s32 a, s32 b);

typedef enum { NV_ST_BROWSE, NV_ST_ACTIONS, NV_ST_COMBINE } NvStMode;

static const char* sNvStActions[] = { "Use", "Combine", "Check", "Back" };

static s32 sNvStCursor;
static s32 sNvStMode;
static s32 sNvStAction;      // the action list's cursor
static s32 sNvStFirst;       // Combine: the first item's slot
static char sNvStLine[48];   // the message line
static s8 sNvStHeld;         // the stick's last direction (moves happen on a new push)

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

static void NvSt_Sfx(u16 sfx) {
    Audio_PlaySfxGeneral(sfx, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultReverb);
}

/** One push of the stick or D-pad: 1 left, 2 right, 3 up, 4 down, 0 none. */
static s8 NvSt_Dir(PlayState* play) {
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
    if (dir == sNvStHeld) {
        return 0;
    }
    sNvStHeld = dir;
    return dir;
}

static void NvSt_MoveCursor(s8 dir) {
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
        NvSt_Sfx(NA_SE_SY_CURSOR);
    }
}

static void NvSt_Input(PlayState* play) {
    Input* input = &play->state.input[0];
    s8 dir = NvSt_Dir(play);
    s32 pressA = CHECK_BTN_ALL(input->press.button, BTN_A);
    s32 qty = 0;
    s32 id;

    switch (sNvStMode) {
        case NV_ST_BROWSE:
            NvSt_MoveCursor(dir);
            if (pressA && (NvItems_Slot(sNvStCursor, NULL) != 0)) {
                sNvStMode = NV_ST_ACTIONS;
                sNvStAction = 0;
                sNvStLine[0] = '\0';
                NvSt_Sfx(NA_SE_SY_DECIDE);
            }
            break;
        case NV_ST_ACTIONS:
            if ((dir == 1) && (sNvStAction > 0)) {
                sNvStAction--;
                NvSt_Sfx(NA_SE_SY_CURSOR);
            } else if ((dir == 2) && (sNvStAction < (s32)ARRAY_COUNT(sNvStActions) - 1)) {
                sNvStAction++;
                NvSt_Sfx(NA_SE_SY_CURSOR);
            }
            if (!pressA) {
                break;
            }
            id = NvItems_Slot(sNvStCursor, &qty);
            sNvStMode = NV_ST_BROWSE;
            switch (sNvStAction) {
                case 0: // Use
                    switch (NvItems_Use(play, sNvStCursor)) {
                        case 1:
                            snprintf(sNvStLine, sizeof(sNvStLine), "Used the %s.", NvItems_Name(id));
                            NvSt_Sfx(NA_SE_SY_DECIDE);
                            break;
                        case 0:
                            snprintf(sNvStLine, sizeof(sNvStLine), "It would do nothing now.");
                            NvSt_Sfx(NA_SE_SY_ERROR);
                            break;
                        default:
                            snprintf(sNvStLine, sizeof(sNvStLine), "You can't use it here.");
                            NvSt_Sfx(NA_SE_SY_ERROR);
                            break;
                    }
                    break;
                case 1: // Combine: now the second item
                    sNvStMode = NV_ST_COMBINE;
                    sNvStFirst = sNvStCursor;
                    snprintf(sNvStLine, sizeof(sNvStLine), "Combine with?");
                    NvSt_Sfx(NA_SE_SY_DECIDE);
                    break;
                case 2: // Check
                    if (qty > 1) {
                        snprintf(sNvStLine, sizeof(sNvStLine), "%s  x%d", NvItems_Name(id), (int)qty);
                    } else {
                        snprintf(sNvStLine, sizeof(sNvStLine), "%s", NvItems_Name(id));
                    }
                    NvSt_Sfx(NA_SE_SY_DECIDE);
                    break;
                default:
                    sNvStLine[0] = '\0';
                    NvSt_Sfx(NA_SE_SY_CURSOR);
                    break;
            }
            break;
        case NV_ST_COMBINE:
            NvSt_MoveCursor(dir);
            if (!pressA) {
                break;
            }
            sNvStMode = NV_ST_BROWSE;
            if (sNvStCursor == sNvStFirst) { // the same item again: never mind
                sNvStLine[0] = '\0';
                NvSt_Sfx(NA_SE_SY_CURSOR);
            } else if (NvItems_Combine(sNvStFirst, sNvStCursor)) {
                // the mix takes the first item's slot; the second may have moved up
                id = NvItems_Slot(sNvStFirst, NULL);
                sNvStCursor = sNvStFirst;
                snprintf(sNvStLine, sizeof(sNvStLine), "Made a %s.", NvItems_Name(id));
                NvSt_Sfx(NA_SE_SY_DECIDE);
            } else {
                snprintf(sNvStLine, sizeof(sNvStLine), "They don't combine.");
                NvSt_Sfx(NA_SE_SY_ERROR);
            }
            break;
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

        if ((sNvStMode == NV_ST_COMBINE) && (i == sNvStFirst)) {
            NvSt_Rect(&gfx, x - 3, y - 3, 102, 20, 60, 140, 200, 220);
        }
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
    // the message line, or the action list along it
    GfxPrint_SetPos(&printer, 7, 21);
    if (sNvStMode == NV_ST_ACTIONS) {
        for (i = 0; i < (s32)ARRAY_COUNT(sNvStActions); i++) {
            if (i == sNvStAction) {
                GfxPrint_SetColor(&printer, 255, 210, 60, 255);
                GfxPrint_Printf(&printer, ">%s ", sNvStActions[i]);
            } else {
                GfxPrint_SetColor(&printer, 150, 150, 160, 255);
                GfxPrint_Printf(&printer, " %s ", sNvStActions[i]);
            }
        }
    } else if (sNvStLine[0] != '\0') {
        GfxPrint_SetColor(&printer, 200, 160, 40, 255);
        GfxPrint_Printf(&printer, "%s", sNvStLine);
    } else {
        GfxPrint_SetColor(&printer, 140, 140, 150, 255);
        GfxPrint_Printf(&printer, "A: Actions");
    }
    gfx = GfxPrint_Close(&printer);
    GfxPrint_Destroy(&printer);
    POLY_OPA_DISP = gfx;
    CLOSE_DISPS(gfxCtx);
}

/** As the pause menu opens in the mansion: open on this page (the map page's place). */
void NvStatus_OnPause(PlayState* play) {
    sNvStMode = NV_ST_BROWSE;
    sNvStLine[0] = '\0';
}
