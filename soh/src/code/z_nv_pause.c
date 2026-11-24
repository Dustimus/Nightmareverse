/*
 * Nightmareverse: the pause menu's columns, shared by every game's pages (NV_PAUSE_MENU.md, the
 * design decided with the user on 2026-11-20). OoT's box keeps its four sides and its turning; each
 * side is a column of pages: OoT's own, then each registered alternate, looping. At rest the page in
 * front is drawn exactly where OoT draws it; its neighbours stand a whole page (and a gap) above and
 * below, so only their near edges show at the screen's top and bottom. The cursor past a page's top
 * or bottom row slides the column one whole page, smoothly; nothing changes size.
 *
 * Hooks (ovl_kaleido_scope): KaleidoScope_DrawPages draws a side through NvPause_DrawColumn (false:
 * the side has no alternates, OoT draws it as ever); the item and equipment cursors call NvPause_Edge
 * where OoT stops them at the top and bottom rows; the name bar asks NvPause_Name.
 * Not yet: the quest and map sides (RE1's status and map screens move into the Map column with the
 * RE1 side's agreement), the slivers' titles.
 */
#include "global.h"
#include "z_nv_pause.h"
#include <stdio.h>
#include <stdlib.h>

#define NV_PAUSE_MAX_PAGES 8
#define NV_PAUSE_PAGE_STEP (160 + 12) // a page's height, and the gap to the next (a sliver of each neighbour shows)
#define NV_PAUSE_SLIDE_FRAMES NvPause_SlideFrames() // 8; dev: NV_PAUSE_SLIDE=n to watch one slowly

static s32 NvPause_SlideFrames(void) {
    const char* dev = getenv("NV_PAUSE_SLIDE");

    return (dev != NULL) ? atoi(dev) : 8;
}

static const NvPausePage* sPages[NV_PAUSE_MAX_PAGES];
static s32 sPageCount;
static s32 sCur[4];       // per side: the page in front (0: OoT's own; else the alternates' order + 1)
static s32 sSlideDir[4];  // per side: the slide under way, and its frames left
static s32 sSlideLeft[4];
static u32 sLastFrame[4]; // per side: the frame it was last drawn (a gap: the menu was opened anew)

Gfx* KaleidoScope_DrawPageSections(Gfx* gfx, Vtx* vertices, void** textures);

void NvPause_Register(const NvPausePage* page) {
    if ((page != NULL) && (sPageCount < NV_PAUSE_MAX_PAGES)) {
        sPages[sPageCount++] = page;
    }
}

void NvPauseMm_Register(void);   // z_nv_mm_pause.c
void NvPauseGear_Register(void); // z_nv_gear.c

static void NvPause_Init(void) {
    static s32 sReady;

    if (!sReady) {
        sReady = true;
        NvPauseGear_Register(); // (the gear page: under OoT's Equipment, before MM's masks)
        NvPauseMm_Register();   // (each game's pages; RE1's, when its screens move in)
    }
}

/** The side's column: OoT's page (NULL), then the alternates available here. */
static s32 NvPause_Column(PlayState* play, s32 side, const NvPausePage** column) {
    s32 n = 0;
    s32 i;

    NvPause_Init();
    column[n++] = NULL;
    for (i = 0; i < sPageCount; i++) {
        if ((sPages[i]->side == side) && ((sPages[i]->available == NULL) || sPages[i]->available(play))) {
            column[n++] = sPages[i];
        }
    }
    return n;
}

static void NvPause_DrawTiles(PlayState* play, Vtx* pageVtx, const NvPausePage* page, void** ootTiles) {
    PauseContext* pauseCtx = &play->pauseCtx;

    OPEN_DISPS(play->state.gfxCtx);
    gDPPipeSync(POLY_OPA_DISP++);
    if (page == NULL) {
        gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA, G_CC_MODULATEIA);
        POLY_OPA_DISP = KaleidoScope_DrawPageSections(POLY_OPA_DISP, pageVtx, ootTiles);
    } else { // (MM's way: the tiles tinted by a flat colour, the alpha the page's)
        gDPSetCombineLERP(POLY_OPA_DISP++, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0, SHADE, 0, TEXEL0, 0, PRIMITIVE, 0,
                          TEXEL0, 0, SHADE, 0);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0, page->color[0], page->color[1], page->color[2], pauseCtx->alpha);
        POLY_OPA_DISP = KaleidoScope_DrawPageSections(POLY_OPA_DISP, pageVtx, (void**)page->tiles);
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetCombineMode(POLY_OPA_DISP++, G_CC_MODULATEIA, G_CC_MODULATEIA);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

/**
 * KaleidoScope_DrawPages' hook, for one side, its matrix already loaded: false if the side has no
 * alternates (OoT draws it as before). Else the column: the page in front and its neighbours above
 * and below, shifted while a slide runs; only the page in front at rest has its content's cursor.
 */
s32 NvPause_DrawColumn(PlayState* play, s32 side, Vtx* pageVtx, void** ootTiles, void (*ootDraw)(PlayState* play)) {
    PauseContext* pauseCtx = &play->pauseCtx;
    const NvPausePage* column[NV_PAUSE_MAX_PAGES + 1];
    s32 n = NvPause_Column(play, side, column);
    s32 k;
    f32 shift;
    u32 frame = play->state.frames;

    if (n <= 1) {
        sCur[side] = 0;
        return false;
    }
    if ((frame - sLastFrame[side]) > 2) { // opened anew: the page for where Link is
        s32 i;

        sCur[side] = 0;
        sSlideLeft[side] = 0;
        for (i = 1; i < n; i++) {
            if ((column[i]->home != NULL) && column[i]->home(play)) {
                sCur[side] = i;
                if (column[i]->enter != NULL) {
                    column[i]->enter(play, 0);
                }
                break;
            }
        }
    }
    if (sLastFrame[side] != frame) {
        sLastFrame[side] = frame;
        if (sSlideLeft[side] > 0) {
            sSlideLeft[side]--;
        }
    }
    sCur[side] %= n;
    shift = -sSlideDir[side] * NV_PAUSE_PAGE_STEP * ((f32)sSlideLeft[side] / NV_PAUSE_SLIDE_FRAMES);

    OPEN_DISPS(play->state.gfxCtx);
    for (k = -1; k <= 1; k++) {
        const NvPausePage* page = column[(sCur[side] + k + n) % n];
        f32 y = -k * NV_PAUSE_PAGE_STEP + shift;
        s32 inFront = (k == 0) && (sSlideLeft[side] == 0);

        Matrix_Push();
        Matrix_Translate(0.0f, y, 0.0f, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        NvPause_DrawTiles(play, pageVtx, page, ootTiles);
        if (page == NULL) {
            if (k == 0) { // (OoT's content only in front: its cursor code runs in its draw)
                ootDraw(play);
            }
        } else if (page->draw != NULL) {
            page->draw(play, inFront && (pauseCtx->pageIndex == side) && (pauseCtx->state == 6) &&
                                 (pauseCtx->unk_1E4 == 0));
        }
        Matrix_Pop();
    }
    CLOSE_DISPS(play->state.gfxCtx);
    return true;
}

/**
 * Our pages' cursor steps (MM's and the gear page): the stick or the D-pad as one step, then, held,
 * a step every 4 frames after 10 (OoT's own pages' feel). *dx, *dy: -1, 0 or 1 (up is -1).
 */
void NvPause_Steps(PlayState* play, s32* dx, s32* dy) {
    static s32 sHeldX, sHeldY, sTimerX, sTimerY;
    PauseContext* pauseCtx = &play->pauseCtx;
    Input* input = &play->state.input[0];
    s32 x = ((pauseCtx->stickRelX < -30) || CHECK_BTN_ALL(input->cur.button, BTN_DLEFT))    ? -1
            : ((pauseCtx->stickRelX > 30) || CHECK_BTN_ALL(input->cur.button, BTN_DRIGHT)) ? 1
                                                                                          : 0;
    s32 y = ((pauseCtx->stickRelY > 30) || CHECK_BTN_ALL(input->cur.button, BTN_DUP))      ? -1
            : ((pauseCtx->stickRelY < -30) || CHECK_BTN_ALL(input->cur.button, BTN_DDOWN)) ? 1
                                                                                          : 0;

    *dx = *dy = 0;
    if (x != sHeldX) {
        sHeldX = x;
        sTimerX = 10;
        *dx = x;
    } else if ((x != 0) && (--sTimerX <= 0)) {
        sTimerX = 4;
        *dx = x;
    }
    if (y != sHeldY) {
        sHeldY = y;
        sTimerY = 10;
        *dy = y;
    } else if ((y != 0) && (--sTimerY <= 0)) {
        sTimerY = 4;
        *dy = y;
    }
}

/** The cursor went past the page's top (dir -1) or bottom row (dir 1): the column slides a page. */
void NvPause_Edge(PlayState* play, s32 side, s32 dir) {
    PauseContext* pauseCtx = &play->pauseCtx;
    const NvPausePage* column[NV_PAUSE_MAX_PAGES + 1];
    s32 n = NvPause_Column(play, side, column);

    if ((n <= 1) || (sSlideLeft[side] > 0)) {
        return;
    }
    sCur[side] = (sCur[side] + dir + n) % n;
    sSlideDir[side] = dir;
    sSlideLeft[side] = NV_PAUSE_SLIDE_FRAMES;
    Audio_PlaySfxGeneral(NA_SE_SY_WIN_SCROLL_LEFT, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
    if (column[sCur[side]] != NULL) {
        if (column[sCur[side]]->enter != NULL) {
            column[sCur[side]]->enter(play, dir);
        }
    } else if (side == PAUSE_ITEM) { // back on OoT's items: its cursor's item again
        u8 item = gSaveContext.inventory.items[pauseCtx->cursorPoint[PAUSE_ITEM]];

        pauseCtx->cursorSlot[PAUSE_ITEM] = pauseCtx->cursorPoint[PAUSE_ITEM];
        pauseCtx->cursorItem[PAUSE_ITEM] = (item != ITEM_NONE) ? item : PAUSE_ITEM_NONE;
    } else if (side == PAUSE_EQUIP) {
        pauseCtx->cursorItem[PAUSE_EQUIP] = PAUSE_ITEM_NONE; // (OoT's equipment cursor sets it again)
    }
    if (getenv("NV_LOG") != NULL) {
        printf("NVPAUSE side %d: %s\n", (int)side,
               (column[sCur[side]] != NULL) ? column[sCur[side]]->name : "OoT's page");
        fflush(stdout);
    }
}

/** The name bar's hook: for an alternate page in front, its name texture (NULL with *named false:
 * nothing to name); NULL with *named true: OoT's page, OoT names it. */
const char* NvPause_Name(PlayState* play, s32* named) {
    PauseContext* pauseCtx = &play->pauseCtx;
    const NvPausePage* column[NV_PAUSE_MAX_PAGES + 1];
    s32 side = pauseCtx->pageIndex;
    s32 n;

    *named = true;
    if ((side < 0) || (side > 3)) {
        return NULL;
    }
    n = NvPause_Column(play, side, column);
    if ((n <= 1) || (column[sCur[side] % n] == NULL)) {
        return NULL;
    }
    *named = false;
    return (column[sCur[side] % n]->nameTex != NULL) ? column[sCur[side] % n]->nameTex(play) : NULL;
}
