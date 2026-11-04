/*
 * Nightmareverse: RE1's item box screen, its own screen as in RE1 (not the pause menu).
 *
 * When the box's lid settles (z_nv_items.c, check_itembox_state) the room holds and this screen
 * takes over, as RE1's MSF_MENU_MODE_ITEMBOX: RE1's status layout (MainMenu.cpp
 * menu_draw_inventory: its frame pieces at the places MenuData.cpp's tables give) with the box's
 * panel over its upper left (draw_itembox_menu), and RE1's controls (menu_itembox_interaction):
 *   1. choose one of Jill's slots (the box dimmed): stick or D-pad, A to choose, B to close;
 *   2. choose a box slot: up/down turn the box's list (it wraps, 48 places), A exchanges the two
 *      (Jill's slots stay packed), B goes back to Jill's slots.
 * The pictures are the player's own (re1/tools/soh_ui.py: "re1/ui/sheets", STATUS.TIM and
 * ITEMBOXN.TIM, plus the status screen's icons and face); the names are our own plain wording.
 */
#include "global.h"
#include "soh/OTRGlobals.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <stdio.h>
#include <string.h>

s32 NvBox_MenuOpen(void);
void NvBox_Close(void);
s32 NvItems_BoxSlot(s32 i, s32* qty);
s32 NvItems_BoxSize(void);
s32 NvItems_BoxSwap(s32 a, s32 b);
s32 NvItems_Slot(s32 i, s32* qty);
s32 NvItems_Equipped(void);
const char* NvItems_Name(s32 id);
s32 NvStatus_PicsReady(void);
void NvStatus_DrawFace(Gfx** gfxP, s32 x, s32 y);
void NvStatus_DrawItem(Gfx** gfxP, s32 id, s32 qty, s32 x, s32 y);

enum { NV_SHEET_STATUS, NV_SHEET_ITEMBOX, NV_SHEET_COUNT };

typedef struct {
    u16 w, h;
    const u16* px;
} NvSheet;

static NvSheet sNvSheets[NV_SHEET_COUNT];

static s32 NvBx_Sheets(void) {
    static s32 sState; // 0 untried, 1 loaded, -1 missing

    if (sState == 0) {
        const u8* d;
        u16 count;
        s32 k;
        s32 p = 6;

        sState = -1;
        if (!ResourceMgr_FileExists("re1/ui/sheets")) {
            return false;
        }
        d = (const u8*)ResourceGetDataByName("re1/ui/sheets");
        if ((d == NULL) || (memcmp(d, "RSHT", 4) != 0)) {
            return false;
        }
        memcpy(&count, d + 4, 2);
        if (count < NV_SHEET_COUNT) {
            return false;
        }
        for (k = 0; k < NV_SHEET_COUNT; k++) {
            memcpy(&sNvSheets[k].w, d + p, 2);
            memcpy(&sNvSheets[k].h, d + p + 2, 2);
            sNvSheets[k].px = (const u16*)(d + p + 4);
            p += 4 + sNvSheets[k].w * sNvSheets[k].h * 2;
        }
        sState = 1;
    }
    return sState > 0;
}

/**
 * A piece of a sheet: w x h from (u, v), at (x, y), in loads of at most 4 KB. flipV: mirrored top
 * to bottom (the menu tables' 0x40 flag, TEXDESC_MIRROR_V), drawn a row at a time.
 */
static void NvBx_Piece(Gfx** gfxP, s32 sheet, s32 x, s32 y, s32 w, s32 h, s32 u, s32 v, s32 flipV) {
    Gfx* gfx = *gfxP;
    const NvSheet* sh = &sNvSheets[sheet];
    s32 strip = flipV ? 1 : MAX(1, 4096 / (w * 2));
    s32 r;

    if ((w <= 0) || (h <= 0) || (u + w > sh->w) || (v + h > sh->h)) {
        return;
    }
    for (r = 0; r < h; r += strip) {
        s32 n = MIN(strip, h - r);
        s32 srcRow = flipV ? (v + h - 1 - r) : (v + r);

        gDPLoadTextureTile(gfx++, sh->px, G_IM_FMT_RGBA, G_IM_SIZ_16b, sh->w, sh->h, u, srcRow, u + w - 1,
                           srcRow + n - 1, 0, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK,
                           G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPTextureRectangle(gfx++, x << 2, (y + r) << 2, (x + w) << 2, (y + r + n) << 2, G_TX_RENDERTILE, u << 5,
                            srcRow << 5, 1 << 10, 1 << 10);
    }
    *gfxP = gfx;
}

static void NvBx_Rect(Gfx** gfxP, s32 x, s32 y, s32 w, s32 h, u8 r, u8 g, u8 b, u8 a) {
    Gfx* gfx = *gfxP;

    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(gfx++, 0, 0, r, g, b, a);
    gDPFillRectangle(gfx++, x, y, x + w, y + h);
    gDPPipeSync(gfx++);
    gDPSetCombineLERP(gfx++, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0,
                      PRIMITIVE, 0);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
    *gfxP = gfx;
}

/* ---- RE1's status layout (MenuData.cpp g_MenuFrameDataBlock, read as menu_draw_inventory reads it) ---- */

typedef struct {
    s16 x, y;
    u16 w, h;
    u8 u, v;
} NvBxPiece;

typedef struct {
    NvBxPiece p;
    u8 flags; // 0x40: mirrored top to bottom
    u8 count; // tiles; 0x80: down rather than across
} NvBxTiled;

// the frame pieces (g_MainMenuFrames2Pos, in draw order)
static const NvBxPiece sNvBxFrames[] = {
    { 120, 216, 200, 16, 0, 168 }, { 0, 216, 120, 16, 0, 136 }, { 192, 182, 128, 2, 0, 166 },
    { 0, 182, 192, 2, 0, 166 },    { 304, 176, 8, 8, 144, 80 }, { 208, 176, 8, 8, 136, 80 },
    { 300, 176, 4, 8, 124, 128 },  { 216, 176, 4, 8, 104, 128 }, { 208, 146, 8, 30, 184, 46 },
    { 204, 48, 104, 4, 0, 132 },   { 204, 12, 104, 4, 0, 128 }, { 148, 142, 60, 38, 132, 186 },
    { 8, 140, 128, 40, 0, 184 },   { 32, 12, 160, 12, 0, 152 }, { 192, 20, 8, 112, 176, 0 },
    { 8, 20, 24, 112, 152, 0 },    { 16, 132, 16, 8, 64, 120 }, { 16, 12, 16, 8, 48, 120 },
    { 192, 132, 8, 8, 96, 120 },   { 192, 12, 8, 8, 96, 112 },  { 8, 132, 8, 8, 88, 120 },
    { 8, 12, 8, 8, 80, 120 },
};

// the tiled borders (g_MainMenuFrames3Pos, and g_MainMenuFrames4Pos's for Jill's 8 slots)
static const NvBxTiled sNvBxTiles[] = {
    { { 220, 176, 16, 8, 108, 128 }, 0, 5 },     { { 304, 16, 4, 16, 124, 136 }, 0, 130 },
    { { 204, 16, 4, 16, 120, 136 }, 0, 130 },    { { 32, 128, 160, 12, 0, 152 }, 0x40, 1 },
    { { 304, 56, 8, 15, 96, 96 }, 0, 136 },      { { 300, 56, 4, 30, 124, 96 }, 0, 132 },
    { { 216, 56, 4, 30, 104, 96 }, 0, 132 },     { { 208, 56, 8, 15, 184, 40 }, 0, 134 },
    { { 304, 48, 8, 8, 128, 80 }, 0, 1 },        { { 300, 48, 4, 8, 124, 88 }, 0, 1 },
    { { 220, 48, 16, 8, 108, 88 }, 0, 5 },       { { 216, 48, 4, 8, 104, 88 }, 0, 1 },
};

// the tabs: Map, File, Radio, Exit (g_MainMenuTopOptionsPos, and Exit in the fourth place)
static const NvBxPiece sNvBxTabs[] = {
    { 208, 16, 48, 16, 0, 0 }, { 256, 16, 48, 16, 0, 32 }, { 208, 32, 48, 16, 0, 16 }, { 256, 32, 48, 16, 0, 48 },
};

// the black under it all (the rects before g_inventorySlotsPos)
static const s16 sNvBxBlack[][4] = { { 0, 140, 320, 100 }, { 199, 12, 121, 128 }, { 0, 12, 9, 128 }, { 0, 0, 320, 12 } };

#define NV_BX_SLOT_X(i) (220 + ((i) & 1) * 40) // g_inventorySlotsPos, Jill's 8
#define NV_BX_SLOT_Y(i) (56 + ((i) >> 1) * 30)
#define NV_BX_EQUIP_X 160
#define NV_BX_EQUIP_Y 146
#define NV_BX_FACE_X 22
#define NV_BX_FACE_Y 146

/* ---- the box's panel (draw_itembox_menu's tables) ---- */

// s_itemboxFramePartsA: top and bottom lines, the arrows, the preview's border
static const NvBxPiece sNvBxPanelA[] = {
    { 91, 87, 42, 32, 16, 48 }, { 175, 83, 8, 7, 72, 64 }, { 175, 24, 8, 7, 72, 56 },
    { 41, 80, 128, 1, 0, 47 },  { 41, 33, 128, 1, 0, 0 },
};
// s_itemboxFramePartsC: the list's inside and sides, the scroll strip's two halves
static const NvBxPiece sNvBxPanelC[] = {
    { 175, 64, 8, 18, 8, 48 },
    { 175, 32, 8, 32, 0, 48 },
    { 41, 34, 128, 46, 0, 1 },
};
// s_itemboxMaskRects: the black around the list and the preview
static const s16 sNvBxMasks[][4] = {
    { 92, 119, 40, 9 }, { 133, 81, 35, 15 }, { 91, 81, 42, 6 }, { 42, 81, 49, 15 }, { 42, 24, 126, 9 },
};

/* ---- state ---- */

static s32 sNvBxMode;    // DAT_00ae9f20: 1 choosing Jill's slot, 2 choosing the box's
static s32 sNvBxSlot;    // Jill's slot under the cursor
static s32 sNvBxBox;     // DAT_00ae9f24: the box slot in the preview
static s8 sNvBxHeld;     // the stick's last direction
static s8 sNvBxOpen;

void NvSound_Menu(u16 sfx);

static void NvBx_Sfx(u16 sfx) {
    NvSound_Menu(sfx); // RE1's own (z_nv_sound.c)
}

/** One push: 1 left, 2 right, 3 up, 4 down, 0 none. */
static s8 NvBx_Dir(Input* input) {
    s8 dir = 0;

    if (input->rel.stick_x < -30) {
        dir = 1;
    } else if (input->rel.stick_x > 30) {
        dir = 2;
    } else if (input->rel.stick_y > 30) {
        dir = 3;
    } else if (input->rel.stick_y < -30) {
        dir = 4;
    }
    if (CHECK_BTN_ALL(input->press.button, BTN_DLEFT)) {
        dir = 1, sNvBxHeld = 0;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DRIGHT)) {
        dir = 2, sNvBxHeld = 0;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DUP)) {
        dir = 3, sNvBxHeld = 0;
    } else if (CHECK_BTN_ALL(input->press.button, BTN_DDOWN)) {
        dir = 4, sNvBxHeld = 0;
    }
    if (dir == sNvBxHeld) {
        return 0;
    }
    sNvBxHeld = dir;
    return dir;
}

static void NvBx_Close(void) {
    sNvBxOpen = false;
    NvBox_Close();
}

/**
 * Each frame (z_play.c, before the room updates): while the box screen is up it takes the input
 * and holds the room. True while it holds.
 */
s32 NvBoxUi_Update(PlayState* play) {
    Input* input = &play->state.input[0];
    s32 size = NvItems_BoxSize();
    s8 dir;

    if (!NvBox_MenuOpen()) {
        sNvBxOpen = false;
        return false;
    }
    if (!sNvBxOpen) { // menu_itembox_interaction's state 0
        sNvBxOpen = true;
        sNvBxMode = 1;
        sNvBxSlot = 0;
        sNvBxBox = 0;
        sNvBxHeld = 0;
        return true; // (the button that opened it is still down)
    }
    dir = NvBx_Dir(input);
    if (CHECK_BTN_ALL(input->press.button, BTN_START)) {
        NvBx_Sfx(NA_SE_SY_DECIDE);
        NvBx_Close();
        return true;
    }
    if (sNvBxMode == 1) {
        if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
            NvBx_Sfx(NA_SE_SY_DECIDE);
            NvBx_Close();
            return true;
        }
        if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
            sNvBxMode = 2;
            NvBx_Sfx(NA_SE_SY_DECIDE);
            return true;
        }
        if ((dir == 1) || (dir == 2)) { // the other column
            sNvBxSlot ^= 1;
        } else if (dir == 3) { // a row up, round to the bottom
            sNvBxSlot = (sNvBxSlot >= 2) ? sNvBxSlot - 2 : sNvBxSlot + 6;
        } else if (dir == 4) {
            sNvBxSlot = (sNvBxSlot < 6) ? sNvBxSlot + 2 : sNvBxSlot - 6;
        }
        if (dir != 0) {
            NvBx_Sfx(NA_SE_SY_CURSOR);
        }
        return true;
    }
    // choosing the box's slot
    if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
        sNvBxMode = 1;
        NvBx_Sfx(NA_SE_SY_DECIDE);
        return true;
    }
    if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
        if ((NvItems_BoxSlot(sNvBxBox, NULL) != 0) || (NvItems_Slot(sNvBxSlot, NULL) != 0)) {
            NvItems_BoxSwap(sNvBxSlot, sNvBxBox);
            printf("NVBOX exchanged slot %d with box %d\n", (int)sNvBxSlot, (int)sNvBxBox);
            fflush(stdout);
            sNvBxMode = 1;
            NvBx_Sfx(NA_SE_SY_DECIDE);
        }
        return true;
    }
    if (dir == 3) { // the list turns back (L1), round from the first to the last
        sNvBxBox = (sNvBxBox + size - 1) % size;
        NvBx_Sfx(NA_SE_SY_CURSOR);
    } else if (dir == 4) { // and on
        sNvBxBox = (sNvBxBox + 1) % size;
        NvBx_Sfx(NA_SE_SY_CURSOR);
    }
    return true;
}

/** Over everything, while the box screen is up (z_play.c, as the movies). */
void NvBoxUi_Draw(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    GfxPrint printer;
    Gfx* gfx;
    s32 size = NvItems_BoxSize();
    s32 i;
    s32 k;

    if (!sNvBxOpen || !NvBox_MenuOpen()) {
        return;
    }
    OPEN_DISPS(gfxCtx);
    gfx = OVERLAY_DISP;
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gSPClearGeometryMode(gfx++, G_ZBUFFER);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    // the whole screen black (the room held behind it), wider than the 4:3 picture too
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(gfx++, GPACK_RGBA5551(0, 0, 0, 1) << 16 | GPACK_RGBA5551(0, 0, 0, 1));
    gDPFillWideRectangle(gfx++, OTRGetRectDimensionFromLeftEdge(0), 0, OTRGetRectDimensionFromRightEdge(SCREEN_WIDTH),
                         SCREEN_HEIGHT);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    NvBx_Rect(&gfx, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0, 0, 255); // (the 4:3 picture, surely)
    if (NvBx_Sheets() && NvStatus_PicsReady()) { // (an older re1.o2r: only the names below)
        for (k = 0; k < (s32)ARRAY_COUNT(sNvBxBlack); k++) {
            NvBx_Rect(&gfx, sNvBxBlack[k][0], sNvBxBlack[k][1], sNvBxBlack[k][2], sNvBxBlack[k][3], 0, 0, 0, 255);
        }
        // RE1's status layout: the frame, the tabs, the tiled borders
        for (k = 0; k < (s32)ARRAY_COUNT(sNvBxFrames); k++) {
            const NvBxPiece* p = &sNvBxFrames[k];

            NvBx_Piece(&gfx, NV_SHEET_STATUS, p->x, p->y, p->w, p->h, p->u, p->v, false);
        }
        for (k = 0; k < (s32)ARRAY_COUNT(sNvBxTabs); k++) {
            const NvBxPiece* p = &sNvBxTabs[k];

            NvBx_Piece(&gfx, NV_SHEET_STATUS, p->x, p->y, p->w, p->h, p->u, p->v, false);
        }
        for (k = 0; k < (s32)ARRAY_COUNT(sNvBxTiles); k++) {
            const NvBxTiled* t = &sNvBxTiles[k];
            s32 n = t->count & 0x7F;
            s32 x = t->p.x;
            s32 y = t->p.y;

            while (n-- > 0) {
                NvBx_Piece(&gfx, NV_SHEET_STATUS, x, y, t->p.w, t->p.h, t->p.u, t->p.v, (t->flags & 0x40) != 0);
                if (t->count & 0x80) {
                    y += t->p.h;
                } else {
                    x += t->p.w;
                }
            }
        }
        // the face, the equipped weapon, Jill's slots and the cursor on hers
        NvStatus_DrawFace(&gfx, NV_BX_FACE_X, NV_BX_FACE_Y);
        {
            s32 eq = NvItems_Equipped();
            s32 qty = 0;
            s32 id = (eq >= 0) ? NvItems_Slot(eq, &qty) : 0;

            NvStatus_DrawItem(&gfx, id, qty, NV_BX_EQUIP_X, NV_BX_EQUIP_Y);
        }
        for (i = 0; i < 8; i++) {
            s32 qty = 0;
            s32 id = NvItems_Slot(i, &qty);

            NvStatus_DrawItem(&gfx, id, qty, NV_BX_SLOT_X(i), NV_BX_SLOT_Y(i));
        }
        gDPPipeSync(gfx++);
        gDPSetCombineLERP(gfx++, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0,
                          0, PRIMITIVE, 0);
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
        NvBx_Piece(&gfx, NV_SHEET_STATUS, NV_BX_SLOT_X(sNvBxSlot), NV_BX_SLOT_Y(sNvBxSlot), 40, 30, 0x80, 0xE0,
                   false); // menu_draw_cursor's slot cursor

        // the box's panel: the list's inside, the scroll strip, the preview slot
        for (k = 0; k < (s32)ARRAY_COUNT(sNvBxPanelC); k++) {
            const NvBxPiece* p = &sNvBxPanelC[k];

            NvBx_Piece(&gfx, NV_SHEET_ITEMBOX, p->x, p->y, p->w, p->h, p->u, p->v, false);
        }
        {
            s32 qty = 0;
            s32 id = NvItems_BoxSlot(sNvBxBox, &qty);

            NvStatus_DrawItem(&gfx, id, qty, 0x5C, 0x58);
        }
        gDPPipeSync(gfx++);
        gDPSetCombineLERP(gfx++, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0,
                          0, PRIMITIVE, 0);
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
        // where the list is: a 3-place marker beside it (the 48 places down the strip)
        for (k = 0; k < 3; k++) {
            s32 at = (sNvBxBox + size - 1 + k) % size;

            NvBx_Piece(&gfx, NV_SHEET_ITEMBOX, 0xB0, at + 0x21, 6, 1, 0x40, 0x38, false);
        }
    }
    gDPPipeSync(gfx++);

    // the names: the box's three places (the one before, the preview's, the one after), and the
    // chosen item of Jill's along the bottom
    GfxPrint_Init(&printer);
    GfxPrint_Open(&printer, gfx);
    for (k = 0; k < 3; k++) {
        s32 at = (sNvBxBox + size - 1 + k) % size;
        s32 id = NvItems_BoxSlot(at, NULL);

        GfxPrint_SetPosPx(&printer, 0x2A, 0x23 + k * 15 + 3);
        GfxPrint_SetColor(&printer, (id != 0) ? 230 : 140, (id != 0) ? 230 : 140, (id != 0) ? 230 : 150, 255);
        GfxPrint_Printf(&printer, "%.15s", (id != 0) ? NvItems_Name(id) : "Nothing");
    }
    {
        s32 id = NvItems_Slot(sNvBxSlot, NULL);

        if (id != 0) {
            GfxPrint_SetPosPx(&printer, 16, 0xBA + 3);
            GfxPrint_SetColor(&printer, 230, 230, 230, 255);
            GfxPrint_Printf(&printer, "%s", NvItems_Name(id));
        }
    }
    gfx = GfxPrint_Close(&printer);
    GfxPrint_Destroy(&printer);

    if (NvBx_Sheets() && NvStatus_PicsReady()) {
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
        gDPSetTexturePersp(gfx++, G_TP_NONE);
        gDPSetTextureFilter(gfx++, G_TF_POINT);
        gDPSetTextureLUT(gfx++, G_TT_NONE);
        // the shadows over the list's first and third rows; choosing Jill's slot, over all of it
        NvBx_Rect(&gfx, 42, 34, 126, 15, 0, 0, 0, 120);
        NvBx_Rect(&gfx, 42, 64, 126, 16, 0, 0, 0, 120);
        if (sNvBxMode < 2) {
            NvBx_Rect(&gfx, 42, 34, 126, 46, 0, 0, 0, 120);
            NvBx_Rect(&gfx, 0x5C, 0x58, 0x28, 0x1E, 0, 0, 0, 120);
        }
        // the tabs but Exit go dark in the box (the "small rects")
        NvBx_Rect(&gfx, 0xD2, 0x10, 0x5E, 0x10, 0, 0, 0, 255);
        NvBx_Rect(&gfx, 0xD2, 0x20, 0x2F, 0x10, 0, 0, 0, 255);
        // the panel's lines, arrows and the preview's border, then the black around them
        for (k = 0; k < (s32)ARRAY_COUNT(sNvBxPanelA); k++) {
            const NvBxPiece* p = &sNvBxPanelA[k];

            NvBx_Piece(&gfx, NV_SHEET_ITEMBOX, p->x, p->y, p->w, p->h, p->u, p->v, false);
        }
        for (k = 0; k < (s32)ARRAY_COUNT(sNvBxMasks); k++) {
            NvBx_Rect(&gfx, sNvBxMasks[k][0], sNvBxMasks[k][1], sNvBxMasks[k][2], sNvBxMasks[k][3], 0, 0, 0, 255);
        }
    }
    gDPPipeSync(gfx++);
    OVERLAY_DISP = gfx;
    CLOSE_DISPS(gfxCtx);
}
