/*
 * Nightmareverse: RE1's files (documents), read where they're found and again from the status
 * screen's File tab (z_nv_status.c).
 *
 * A document lies in a room as an item (item id 0x5F + its file number, RE1's
 * file_set_collected_flag); taking it records it and opens it on its own screen over the held
 * room, as RE1 does. The pages are the player's own (re1/tools/soh_ui.py: "re1/ui/file<n>", RE1's
 * item_m2/filem_*.pix), shown whole screen, a page at a time: left and right turn the pages, A on
 * the last page or B closes it.
 */
#include "global.h"
#include "soh/OTRGlobals.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void NvSound_Menu(u16 sfx);

#define NV_FILES_MAX 32
#define NV_FILE_FIRST_ITEM 0x5F

static u8 sNvFiles[NV_FILES_MAX];
static s32 sNvFileCount;

static const u8* sNvReadPages; // the open file's "RFIL" blob, or NULL
// its backdrop from the Japanese PC release ("RFIB", re1/tools/soh_ui.py backdrop_blob: u16 width,
// height, then RGBA5551), or NULL: a scanned paper picture behind the pages, as the Japanese
// reader shows (Biohazard.exe's jpn_file_screen_render), only when the converter was given it
static const u8* sNvReadBackdrop;
static s32 sNvReadPage;
static s8 sNvReadHeld;
static s8 sNvReadAlone; // a document just taken: read on its own screen, the room held

/** Files found so far. */
s32 NvFiles_Count(void) {
    return sNvFileCount;
}

/** Whether item id is a document. */
s32 NvFiles_IsFile(s32 id) {
    return id >= NV_FILE_FIRST_ITEM;
}

/** A document picked up: RE1's file number. False if it was already there. */
static s32 NvFiles_Add(s32 file) {
    s32 k;

    for (k = 0; k < sNvFileCount; k++) {
        if (sNvFiles[k] == file) {
            return false;
        }
    }
    if (sNvFileCount < NV_FILES_MAX) {
        sNvFiles[sNvFileCount++] = file;
    }
    return true;
}

/** The k-th file's name in the list: our own plain label. */
const char* NvFiles_Name(s32 k) {
    static char sName[16];

    snprintf(sName, sizeof(sName), "File %d", (k >= 0 && k < sNvFileCount) ? sNvFiles[k] + 1 : 0);
    return sName;
}

/** Open RE1 file number n's pages (false when the converter had none for it). */
static s32 NvFiles_OpenNumber(s32 n) {
    char path[32];
    const u8* d;

    snprintf(path, sizeof(path), "re1/ui/file%d", (int)n);
    if (!ResourceMgr_FileExists(path)) {
        return false;
    }
    d = (const u8*)ResourceGetDataByName(path);
    if ((d == NULL) || (memcmp(d, "RFIL", 4) != 0)) {
        return false;
    }
    sNvReadPages = d;
    sNvReadPage = 0;
    sNvReadHeld = 0;
    sNvReadBackdrop = NULL;
    snprintf(path, sizeof(path), "re1/ui/fileb%d", (int)n);
    if (ResourceMgr_FileExists(path)) {
        d = (const u8*)ResourceGetDataByName(path);
        if ((d != NULL) && (memcmp(d, "RFIB", 4) == 0)) {
            sNvReadBackdrop = d;
        }
    }
    return true;
}

/** The File tab's k-th file. */
void NvFiles_Open(PlayState* play, s32 k) {
    if ((k >= 0) && (k < sNvFileCount)) {
        NvFiles_OpenNumber(sNvFiles[k]);
    }
}

/**
 * Taking a document (item id) in a room: record it and, as RE1 reads it at once, open it on its
 * own screen over the held room (NvFilesUi_*). True if its pages are there to read.
 */
s32 NvFiles_Take(s32 id) {
    NvFiles_Add(id - NV_FILE_FIRST_ITEM);
    if (!NvFiles_OpenNumber(id - NV_FILE_FIRST_ITEM)) {
        return false;
    }
    sNvReadAlone = true;
    return true;
}

s32 NvFiles_Reading(void) {
    return sNvReadPages != NULL;
}

void NvFiles_Close(void) {
    sNvReadPages = NULL;
}

/**
 * Each frame while a file is open: turn pages (left/right, A for the next), close on the last
 * page's A or on B. True while it stays open.
 */
s32 NvFiles_Update(Input* input, s32 stickX) {
    u16 pages;
    s8 dir = 0;

    if (sNvReadPages == NULL) {
        return false;
    }
    memcpy(&pages, sNvReadPages + 4, 2);
    if ((stickX < -30) || CHECK_BTN_ALL(input->press.button, BTN_DLEFT)) {
        dir = -1;
    } else if ((stickX > 30) || CHECK_BTN_ALL(input->press.button, BTN_DRIGHT)) {
        dir = 1;
    }
    if (CHECK_BTN_ALL(input->press.button, BTN_DLEFT) || CHECK_BTN_ALL(input->press.button, BTN_DRIGHT)) {
        sNvReadHeld = 0;
    }
    if (dir == sNvReadHeld) {
        dir = 0;
    } else {
        sNvReadHeld = dir;
    }
    if (CHECK_BTN_ALL(input->press.button, BTN_A)) {
        dir = 1;
        if (sNvReadPage >= pages - 1) {
            sNvReadPages = NULL;
            return false;
        }
    }
    if (CHECK_BTN_ALL(input->press.button, BTN_B)) {
        sNvReadPages = NULL;
        return false;
    }
    if ((dir != 0) && (sNvReadPage + dir >= 0) && (sNvReadPage + dir < pages)) {
        sNvReadPage += dir;
        NvSound_Menu(NA_SE_SY_CURSOR); // RE1's own (z_nv_sound.c)
    }
    return true;
}

/**
 * The open page, whole screen over everything ("RFIL": u16 pages, width, height, then the pages
 * as RGBA5551), in strips small enough for one texture load each.
 */
Gfx* NvFiles_Draw(Gfx* gfx) {
    u16 pages, w, h;
    const u16* px;
    s32 y;
    s32 strip;

    if (sNvReadPages == NULL) {
        return gfx;
    }
    memcpy(&pages, sNvReadPages + 4, 2);
    memcpy(&w, sNvReadPages + 6, 2);
    memcpy(&h, sNvReadPages + 8, 2);
    px = (const u16*)(sNvReadPages + 10) + (size_t)sNvReadPage * w * h;
    strip = MAX(1, 2048 / w);

    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 255);
    gDPFillRectangle(gfx++, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    // the Japanese backdrop, behind the page, where its reader puts it (48, 48)
    if (sNvReadBackdrop != NULL) {
        u16 bw;
        u16 bh;
        const u16* bp = (const u16*)(sNvReadBackdrop + 8);
        s32 bs;
        s32 by;

        memcpy(&bw, sNvReadBackdrop + 4, 2);
        memcpy(&bh, sNvReadBackdrop + 6, 2);
        bs = MAX(1, 2048 / bw);
        for (by = 0; by < bh; by += bs) {
            s32 rows = MIN(bs, bh - by);

            gDPLoadTextureBlock(gfx++, bp + by * bw, G_IM_FMT_RGBA, G_IM_SIZ_16b, bw, rows, 0,
                                G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);
            gSPTextureRectangle(gfx++, 48 << 2, (48 + by) << 2, (48 + bw) << 2, (48 + by + rows) << 2,
                                G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        }
    }
    // RE1's pages are 256 tall: the screen shows the middle 240
    for (y = MAX(0, (h - SCREEN_HEIGHT) / 2); y < MIN(h, (h + SCREEN_HEIGHT) / 2); y += strip) {
        s32 rows = MIN(strip, MIN(h, (h + SCREEN_HEIGHT) / 2) - y);
        s32 sx = (SCREEN_WIDTH - w) / 2;
        s32 sy = (SCREEN_HEIGHT - h) / 2 + y;

        gDPLoadTextureBlock(gfx++, px + y * w, G_IM_FMT_RGBA, G_IM_SIZ_16b, w, rows, 0, G_TX_NOMIRROR | G_TX_CLAMP,
                            G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gSPTextureRectangle(gfx++, sx << 2, sy << 2, (sx + w) << 2, (sy + rows) << 2, G_TX_RENDERTILE, 0, 0,
                            1 << 10, 1 << 10);
    }
    gDPPipeSync(gfx++);
    return gfx;
}

/**
 * The reader on its own screen (a document just taken), from z_play.c as the box screen: it takes
 * the input and holds the room while it's open. True while it holds.
 */
s32 NvFilesUi_Update(PlayState* play) {
    Input* input = &play->state.input[0];

    // testing: NV_FILE_AT="n,frame" takes file n that many frames in, as if found there
    static s32 sDevDone;

    if ((getenv("NV_FILE_AT") != NULL) && !sDevDone) {
        int n, at;

        if ((sscanf(getenv("NV_FILE_AT"), "%d,%d", &n, &at) == 2) && (play->gameplayFrames >= (u32)at)) {
            sDevDone = true;
            NvFiles_Take(NV_FILE_FIRST_ITEM + n);
        }
    }
    if (!sNvReadAlone) {
        return false;
    }
    if (sNvReadPages == NULL) {
        sNvReadAlone = false;
        return false;
    }
    if (!NvFiles_Update(input, input->rel.stick_x)) {
        sNvReadAlone = false;
    }
    return true;
}

void NvFilesUi_Draw(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;

    if (!sNvReadAlone || (sNvReadPages == NULL)) {
        return;
    }
    OPEN_DISPS(gfxCtx);
    // the whole screen black first, past the 4:3 picture too
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_FILL);
    gDPSetRenderMode(OVERLAY_DISP++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(OVERLAY_DISP++, GPACK_RGBA5551(0, 0, 0, 1) << 16 | GPACK_RGBA5551(0, 0, 0, 1));
    gDPFillWideRectangle(OVERLAY_DISP++, OTRGetRectDimensionFromLeftEdge(0), 0,
                         OTRGetRectDimensionFromRightEdge(SCREEN_WIDTH), SCREEN_HEIGHT);
    gDPPipeSync(OVERLAY_DISP++);
    OVERLAY_DISP = NvFiles_Draw(OVERLAY_DISP);
    CLOSE_DISPS(gfxCtx);
}
