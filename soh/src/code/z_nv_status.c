/*
 * Nightmareverse: RE1's status screen, in the pause menu's map page while Link is in the mansion
 * (re1/README.md, "Items": the dungeon map's place, and the page the pause menu opens on there).
 *
 * RE1's layout in RE1's own pictures (re1/tools/soh_ui.py, from the player's files): the tabs
 * (Map, File, Exit) along the top, Jill's face and condition, the equipped weapon, and her 2 x 4
 * slots (z_nv_items.c) with RE1's cursor and counts. A on an item opens RE1's action box: Equip
 * (weapons), Use (the heals, by RE1's rules), Check, Combine (then the second item: RE1's herb
 * mixes). The File tab lists the files read (z_nv_files.c). Drawn flat over the page once it has
 * turned to face the player; the page itself keeps OoT's frame.
 */
#include "global.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <stdio.h>
#include <string.h>

#define NV_ST_SLOTS 8
#define NV_ST_COLS 2
#define NV_ST_ICON_W 40
#define NV_ST_ICON_H 30
#define NV_ST_X(i) (204 + ((i) % NV_ST_COLS) * 42) // a slot's corner (screen pixels), RE1's 2 x 4
#define NV_ST_Y(i) (56 + ((i) / NV_ST_COLS) * 31)

s32 NvItems_Slot(s32 i, s32* qty);
const char* NvItems_Name(s32 id);
s32 NvItems_Use(PlayState* play, s32 i);
s32 NvItems_Combine(s32 a, s32 b);

typedef enum { NV_ST_BROWSE, NV_ST_ACTIONS, NV_ST_COMBINE, NV_ST_FILES } NvStMode;

/** RE1's item icons ("re1/ui/icons", re1/tools/soh_ui.py): "RICN", u16 count, w, h, RGBA5551. */
static const u8* NvSt_Icons(void) {
    static const u8* sIcons;
    static s32 sTried;

    if (!sTried) {
        sTried = true;
        if (ResourceMgr_FileExists("re1/ui/icons")) {
            sIcons = (const u8*)ResourceGetDataByName("re1/ui/icons");
            if ((sIcons != NULL) && (memcmp(sIcons, "RICN", 4) != 0)) {
                sIcons = NULL;
            }
        }
    }
    return sIcons;
}

// each item's icon in ITEM_ALL.PIX: RE1's item record byte 1, less one (MenuData.cpp
// g_ItemMaxQty / g_ItemImageLookupTable: the Pythons share one, so do the grenade launcher's loads)
static const u8 sNvStIconOf[] = {
    255, 0, 1, 2, 3, 3, 4, 5, 5, 5, 6, 7, 8, 9, 10, 11,
    12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27,
    28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
    44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
    60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73,
};

/** Item id's icon (GameStart.cpp LoadItemImage), or NULL. */
static const u16* NvSt_Icon(s32 id) {
    const u8* d = NvSt_Icons();
    u16 count;

    s32 k;

    if ((d == NULL) || (id <= 0) || (id >= (s32)ARRAY_COUNT(sNvStIconOf))) {
        return NULL;
    }
    k = sNvStIconOf[id];
    memcpy(&count, d + 4, 2);
    if (k >= count) {
        return NULL;
    }
    return (const u16*)(d + 10 + k * NV_ST_ICON_W * NV_ST_ICON_H * 2);
}

// RE1's status pictures ("re1/ui/status", re1/tools/soh_ui.py STATUS_SPRITES, in this order)
typedef enum {
    NV_SPR_TABS,     // Map, Radio, File, Exit: 48 x 16 each, one under another
    NV_SPR_TAB_CUR,  // the tab cursor
    NV_SPR_ACTIONS,  // Equip, Use, Check, Combn: 48 x 24 each
    NV_SPR_ACT_CUR,  // the action cursor
    NV_SPR_COND,     // the condition words, 32 x 8 each
    NV_SPR_DIGITS,   // 0-9, 8 x 8 each
    NV_SPR_SLOT_CUR, // the slot cursor
    NV_SPR_SLOT_CUR2,
    NV_SPR_INFINITE,
    NV_SPR_SLOT,     // an item slot's background
    NV_SPR_FACE,     // Jill
    NV_SPR_MAX
} NvStSprite;

typedef struct {
    u16 w, h;
    const u16* px;
} NvStPic;

static NvStPic sNvStPics[NV_SPR_MAX];

static s32 NvSt_Pics(void) {
    static s32 sState; // 0 untried, 1 loaded, -1 missing

    if (sState == 0) {
        const u8* d;
        u16 count;
        s32 k;
        s32 p = 6;

        sState = -1;
        if (!ResourceMgr_FileExists("re1/ui/status")) {
            return false;
        }
        d = (const u8*)ResourceGetDataByName("re1/ui/status");
        if ((d == NULL) || (memcmp(d, "RSPR", 4) != 0)) {
            return false;
        }
        memcpy(&count, d + 4, 2);
        if (count < NV_SPR_MAX) {
            return false;
        }
        for (k = 0; k < NV_SPR_MAX; k++) {
            memcpy(&sNvStPics[k].w, d + p, 2);
            memcpy(&sNvStPics[k].h, d + p + 2, 2);
            sNvStPics[k].px = (const u16*)(d + p + 4);
            p += 4 + sNvStPics[k].w * sNvStPics[k].h * 2;
        }
        sState = 1;
    }
    return sState > 0;
}

/** Draw rows [row, row + rows) of a picture made of rowH-pixel rows at (x, y), tinted. */
static void NvSt_Pic(Gfx** gfxP, s32 pic, s32 row, s32 rows, s32 rowH, s32 x, s32 y, u8 shade, u8 alpha) {
    Gfx* gfx = *gfxP;
    const NvStPic* sp = &sNvStPics[pic];
    s32 h = (rowH > 0) ? rows * rowH : sp->h;
    const u16* px = sp->px + ((rowH > 0) ? row * rowH * sp->w : 0);

    gDPSetPrimColor(gfx++, 0, 0, shade, shade, shade, alpha);
    gDPLoadTextureBlock(gfx++, px, G_IM_FMT_RGBA, G_IM_SIZ_16b, sp->w, h, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gSPTextureRectangle(gfx++, x << 2, y << 2, (x + sp->w) << 2, (y + h) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10,
                        1 << 10);
    *gfxP = gfx;
}

static void NvSt_Icon16(Gfx** gfxP, const u16* icon, s32 x, s32 y) {
    Gfx* gfx = *gfxP;

    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    gDPLoadTextureBlock(gfx++, icon, G_IM_FMT_RGBA, G_IM_SIZ_16b, NV_ST_ICON_W, NV_ST_ICON_H, 0,
                        G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                        G_TX_NOLOD);
    gSPTextureRectangle(gfx++, x << 2, y << 2, (x + NV_ST_ICON_W) << 2, (y + NV_ST_ICON_H) << 2, G_TX_RENDERTILE,
                        0, 0, 1 << 10, 1 << 10);
    *gfxP = gfx;
}

/** An item's count in RE1's digits under its icon (display_item_qty: ammo counts, not weapons'). */
static void NvSt_Count(Gfx** gfxP, s32 id, s32 qty, s32 x, s32 y) {
    char buf[4];
    s32 k;

    if ((qty <= 1) && (id > 0x0A)) {
        return;
    }
    snprintf(buf, sizeof(buf), "%d", (int)MIN(qty, 999));
    x += NV_ST_ICON_W - 2 - 8 * (s32)strlen(buf);
    for (k = 0; buf[k] != '\0'; k++) {
        NvSt_Pic(gfxP, NV_SPR_DIGITS, buf[k] - '0', 1, 8, x + k * 8, y + 20, 255, 255);
    }
}

// the page's layout (screen pixels; GfxPrint's text grid is 8)
#define NV_ST_PANEL_X 28
#define NV_ST_PANEL_Y 38
#define NV_ST_TAB_X(t) (44 + (t) * 50)
#define NV_ST_TAB_Y 54
#define NV_ST_FACE_X 40
#define NV_ST_FACE_Y 78
#define NV_ST_EQ_X 40
#define NV_ST_EQ_Y 116
#define NV_ST_ACT_X 148
#define NV_ST_ACT_Y 72

typedef enum { NV_TAB_MAP, NV_TAB_FILE, NV_TAB_EXIT, NV_TAB_COUNT } NvStTab;
static const u8 sNvStTabRow[NV_TAB_COUNT] = { 0, 2, 3 }; // their rows in the tab picture (no radio)

// the item actions, RE1's box: Equip only for what can be equipped (the box then starts at Use)
typedef enum { NV_ACT_EQUIP, NV_ACT_USE, NV_ACT_CHECK, NV_ACT_COMBINE, NV_ACT_COUNT } NvStAction;
static const char* sNvStActions[] = { "Equip", "Use", "Check", "Combine" };

s32 NvItems_Equippable(s32 id);
s32 NvItems_Equipped(void);
s32 NvItems_ToggleEquip(s32 i);
s32 NvFiles_Count(void);
void NvFiles_Open(PlayState* play, s32 k);
const char* NvFiles_Name(s32 k);

#define NV_ST_TABS_ROW NV_ST_SLOTS // the cursor on the tabs

static s32 sNvStCursor;
static s32 sNvStTab;
static s32 sNvStMode;
static s32 sNvStAction;      // the action box's cursor (an NvStAction)
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

/** The slots, and above them the tabs (only while choosing an item, not a second one). */
static void NvSt_MoveCursor(s8 dir, s32 tabs) {
    s32 c = sNvStCursor;
    s32 t = sNvStTab;

    if (c == NV_ST_TABS_ROW) {
        if ((dir == 1) && (t > 0)) {
            t--;
        } else if ((dir == 2) && (t < NV_TAB_COUNT - 1)) {
            t++;
        } else if (dir == 4) {
            c = 0;
        }
    } else if ((dir == 1) && (c % NV_ST_COLS) > 0) {
        c--;
    } else if ((dir == 2) && (c % NV_ST_COLS) < NV_ST_COLS - 1) {
        c++;
    } else if ((dir == 3) && (c >= NV_ST_COLS)) {
        c -= NV_ST_COLS;
    } else if ((dir == 3) && tabs) {
        c = NV_ST_TABS_ROW;
    } else if ((dir == 4) && (c + NV_ST_COLS < NV_ST_SLOTS)) {
        c += NV_ST_COLS;
    }
    if ((c != sNvStCursor) || (t != sNvStTab)) {
        sNvStCursor = c;
        sNvStTab = t;
        NvSt_Sfx(NA_SE_SY_CURSOR);
    }
}

/** The pause menu closes, as on START (KaleidoScope_Update). */
static void NvSt_ClosePause(PlayState* play) {
    Interface_SetDoAction(play, DO_ACTION_NONE);
    play->pauseCtx.state = 0x12;
    WREG(2) = -6240;
    func_800F64E0(0);
}

static void NvSt_TabAction(PlayState* play) {
    switch (sNvStTab) {
        case NV_TAB_MAP:
            snprintf(sNvStLine, sizeof(sNvStLine), "No map of this area.");
            NvSt_Sfx(NA_SE_SY_ERROR);
            break;
        case NV_TAB_FILE:
            if (NvFiles_Count() == 0) {
                snprintf(sNvStLine, sizeof(sNvStLine), "No files.");
                NvSt_Sfx(NA_SE_SY_ERROR);
            } else {
                sNvStMode = NV_ST_FILES;
                sNvStAction = 0;
                sNvStLine[0] = '\0';
                NvSt_Sfx(NA_SE_SY_DECIDE);
            }
            break;
        default:
            sNvStLine[0] = '\0';
            NvSt_Sfx(NA_SE_SY_DECIDE);
            NvSt_ClosePause(play);
            break;
    }
}

static void NvSt_Input(PlayState* play) {
    Input* input = &play->state.input[0];
    s8 dir = NvSt_Dir(play);
    s32 pressA = CHECK_BTN_ALL(input->press.button, BTN_A);
    s32 first;
    s32 qty = 0;
    s32 id;

    switch (sNvStMode) {
        case NV_ST_BROWSE:
            NvSt_MoveCursor(dir, true);
            if (!pressA) {
                break;
            }
            if (sNvStCursor == NV_ST_TABS_ROW) {
                NvSt_TabAction(play);
            } else if (NvItems_Slot(sNvStCursor, NULL) != 0) {
                sNvStMode = NV_ST_ACTIONS;
                sNvStAction = NvItems_Equippable(NvItems_Slot(sNvStCursor, NULL)) ? NV_ACT_EQUIP : NV_ACT_USE;
                sNvStLine[0] = '\0';
                NvSt_Sfx(NA_SE_SY_DECIDE);
            }
            break;
        case NV_ST_FILES:
            if ((dir == 3) && (sNvStAction > 0)) {
                sNvStAction--;
                NvSt_Sfx(NA_SE_SY_CURSOR);
            } else if ((dir == 4) && (sNvStAction < NvFiles_Count())) {
                sNvStAction++;
                NvSt_Sfx(NA_SE_SY_CURSOR);
            }
            if (!pressA) {
                break;
            }
            if (sNvStAction == NvFiles_Count()) { // Back
                sNvStMode = NV_ST_BROWSE;
                NvSt_Sfx(NA_SE_SY_CURSOR);
            } else {
                NvSt_Sfx(NA_SE_SY_DECIDE);
                NvFiles_Open(play, sNvStAction);
            }
            break;
        case NV_ST_ACTIONS:
            first = NvItems_Equippable(NvItems_Slot(sNvStCursor, NULL)) ? NV_ACT_EQUIP : NV_ACT_USE;
            if ((dir == 3) && (sNvStAction > first)) {
                sNvStAction--;
                NvSt_Sfx(NA_SE_SY_CURSOR);
            } else if ((dir == 4) && (sNvStAction < NV_ACT_COUNT - 1)) {
                sNvStAction++;
                NvSt_Sfx(NA_SE_SY_CURSOR);
            }
            if (!pressA) {
                break;
            }
            id = NvItems_Slot(sNvStCursor, &qty);
            sNvStMode = NV_ST_BROWSE;
            switch (sNvStAction) {
                case NV_ACT_EQUIP:
                    NvItems_ToggleEquip(sNvStCursor);
                    snprintf(sNvStLine, sizeof(sNvStLine), (NvItems_Equipped() == sNvStCursor) ? "Equipped." : "Unequipped.");
                    NvSt_Sfx(NA_SE_SY_DECIDE);
                    break;
                case NV_ACT_USE:
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
                case NV_ACT_COMBINE: // now the second item
                    sNvStMode = NV_ST_COMBINE;
                    sNvStFirst = sNvStCursor;
                    snprintf(sNvStLine, sizeof(sNvStLine), "Combine with?");
                    NvSt_Sfx(NA_SE_SY_DECIDE);
                    break;
                default: // Check
                    if (qty > 1) {
                        snprintf(sNvStLine, sizeof(sNvStLine), "%s  x%d", NvItems_Name(id), (int)qty);
                    } else {
                        snprintf(sNvStLine, sizeof(sNvStLine), "%s", NvItems_Name(id));
                    }
                    NvSt_Sfx(NA_SE_SY_DECIDE);
                    break;
            }
            break;
        case NV_ST_COMBINE:
            NvSt_MoveCursor(dir, false);
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

/** RE1's condition (menu_draw_health_bar): Link's hearts in quarters, as Jill's health. */
static s32 NvSt_ConditionRow(void) {
    s32 cap = MAX(gSaveContext.healthCapacity, 4);
    s32 q = (gSaveContext.health <= 0) ? 0 : (gSaveContext.health - 1) / (cap / 4);

    return (q >= 3) ? 0 : (q >= 1) ? 4 : 7; // Fine, Caution, Danger!
}

/** An item slot: the blue ground, the icon, the count. */
static void NvSt_DrawSlot(Gfx** gfxP, s32 i, s32 x, s32 y) {
    s32 qty = 0;
    s32 id = NvItems_Slot(i, &qty);
    const u16* icon = NvSt_Icon(id);

    NvSt_Pic(gfxP, NV_SPR_SLOT, 0, 0, 0, x, y, 255, 255);
    if (icon != NULL) {
        NvSt_Icon16(gfxP, icon, x, y);
        NvSt_Count(gfxP, id, qty, x, y);
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
    s32 pics;
    s32 i;

    if (!interactive || (pauseCtx->state != 6) || (pauseCtx->unk_1E4 != 0)) {
        return;
    }
    if (pauseCtx->debugState == 0) {
        NvSt_Input(play);
    }
    pics = NvSt_Pics() && (NvSt_Icons() != NULL);

    OPEN_DISPS(gfxCtx);
    gfx = POLY_OPA_DISP;
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gSPClearGeometryMode(gfx++, G_ZBUFFER);
    NvSt_Rect(&gfx, NV_ST_PANEL_X, NV_ST_PANEL_Y, 264, 150, 0, 0, 0, 235);

    if (pics) {
        s32 eq = NvItems_Equipped();

        // RE1's pictures, colour 0 clear (TEXEL0 alpha), tinted by the primitive colour
        gDPPipeSync(gfx++);
        gDPSetCombineLERP(gfx++, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0,
                          0, PRIMITIVE, 0);
        gDPSetTexturePersp(gfx++, G_TP_NONE);
        gDPSetTextureFilter(gfx++, G_TF_POINT);
        gDPSetTextureLUT(gfx++, G_TT_NONE);

        // the tabs, the one under the cursor lit (RE1's red cursor over it, see-through)
        for (i = 0; i < NV_TAB_COUNT; i++) {
            s32 on = (sNvStCursor == NV_ST_TABS_ROW) && (sNvStTab == i);

            NvSt_Pic(&gfx, NV_SPR_TABS, sNvStTabRow[i], 1, 16, NV_ST_TAB_X(i), NV_ST_TAB_Y, on ? 255 : 150, 255);
            if (on && (sNvStMode == NV_ST_BROWSE)) {
                NvSt_Pic(&gfx, NV_SPR_TAB_CUR, 0, 0, 0, NV_ST_TAB_X(i), NV_ST_TAB_Y, 255, 90);
            }
        }
        // Jill's face, her condition, and the equipped weapon
        NvSt_Pic(&gfx, NV_SPR_FACE, 0, 0, 0, NV_ST_FACE_X, NV_ST_FACE_Y, 255, 255);
        NvSt_Pic(&gfx, NV_SPR_COND, NvSt_ConditionRow(), 1, 8, NV_ST_FACE_X + 38, NV_ST_FACE_Y + 11, 255, 255);
        if (eq >= 0) {
            NvSt_DrawSlot(&gfx, eq, NV_ST_EQ_X, NV_ST_EQ_Y);
        } else {
            NvSt_Pic(&gfx, NV_SPR_SLOT, 0, 0, 0, NV_ST_EQ_X, NV_ST_EQ_Y, 255, 255);
        }
        // the slots, the cursor's and (choosing the second item to combine) the first's
        for (i = 0; i < NV_ST_SLOTS; i++) {
            NvSt_DrawSlot(&gfx, i, NV_ST_X(i), NV_ST_Y(i));
        }
        if ((sNvStMode == NV_ST_COMBINE) && (sNvStFirst != sNvStCursor)) {
            NvSt_Pic(&gfx, NV_SPR_SLOT_CUR2, 0, 0, 0, NV_ST_X(sNvStFirst), NV_ST_Y(sNvStFirst), 255, 255);
        }
        if (sNvStCursor < NV_ST_SLOTS) {
            NvSt_Pic(&gfx, NV_SPR_SLOT_CUR, 0, 0, 0, NV_ST_X(sNvStCursor), NV_ST_Y(sNvStCursor), 255, 255);
        }
        // the action box beside them
        if (sNvStMode == NV_ST_ACTIONS) {
            s32 first = NvItems_Equippable(NvItems_Slot(sNvStCursor, NULL)) ? NV_ACT_EQUIP : NV_ACT_USE;

            NvSt_Pic(&gfx, NV_SPR_ACTIONS, first, NV_ACT_COUNT - first, 24, NV_ST_ACT_X, NV_ST_ACT_Y, 255, 255);
            NvSt_Pic(&gfx, NV_SPR_ACT_CUR, 0, 0, 0, NV_ST_ACT_X, NV_ST_ACT_Y + (sNvStAction - first) * 24, 255,
                     255);
        }
    }
    gDPPipeSync(gfx++);

    GfxPrint_Init(&printer);
    GfxPrint_Open(&printer, gfx);
    if (!pics) {
        // without RE1's pictures (an older re1.o2r): the names as text
        for (i = 0; i < NV_ST_SLOTS; i++) {
            s32 qty = 0;
            s32 id = NvItems_Slot(i, &qty);

            GfxPrint_SetPos(&printer, NV_ST_X(i) / 8 - 2, NV_ST_Y(i) / 8 + 1);
            GfxPrint_SetColor(&printer, (i == sNvStCursor) ? 255 : 200, (i == sNvStCursor) ? 210 : 200, 120, 255);
            GfxPrint_Printf(&printer, "%.6s", (id == 0) ? "-" : NvItems_Name(id));
        }
    }
    if (pics && (NvItems_Equipped() >= 0)) {
        GfxPrint_SetColor(&printer, 255, 210, 60, 255);
        GfxPrint_SetPos(&printer, (NV_ST_X(NvItems_Equipped()) + 32) / 8, NV_ST_Y(NvItems_Equipped()) / 8 - 1);
        GfxPrint_Printf(&printer, "E");
    }
    if (sNvStMode == NV_ST_FILES) {
        // the files read so far, and Back
        for (i = 0; i <= NvFiles_Count(); i++) {
            GfxPrint_SetPos(&printer, 18, 7 + i);
            GfxPrint_SetColor(&printer, (i == sNvStAction) ? 255 : 150, (i == sNvStAction) ? 210 : 150,
                              (i == sNvStAction) ? 60 : 160, 255);
            GfxPrint_Printf(&printer, "%c%.20s", (i == sNvStAction) ? '>' : ' ',
                            (i == NvFiles_Count()) ? "Back" : NvFiles_Name(i));
        }
    }
    // the message line: the item under the cursor's name, or what just happened
    GfxPrint_SetPos(&printer, 5, 19);
    if (sNvStLine[0] != '\0') {
        GfxPrint_SetColor(&printer, 200, 160, 40, 255);
        GfxPrint_Printf(&printer, "%.19s", sNvStLine);
        if (strlen(sNvStLine) > 19) {
            GfxPrint_SetPos(&printer, 5, 20);
            GfxPrint_Printf(&printer, "%.19s", sNvStLine + 19);
        }
    } else if (sNvStMode == NV_ST_ACTIONS) {
        GfxPrint_SetColor(&printer, 230, 230, 230, 255);
        GfxPrint_Printf(&printer, "%s", sNvStActions[sNvStAction]);
    } else if ((sNvStCursor < NV_ST_SLOTS) && (NvItems_Slot(sNvStCursor, NULL) != 0)) {
        GfxPrint_SetColor(&printer, 230, 230, 230, 255);
        GfxPrint_Printf(&printer, "%.19s", NvItems_Name(NvItems_Slot(sNvStCursor, NULL)));
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
