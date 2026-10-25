/*
 * Nightmareverse: RE1's movies, played from RE1's own files.
 *
 * cmd_fmv_set (SCD 0x29) asks for a movie by id; RE1's PC release plays MOVIE/<name>.avi from its
 * table (the exe's 0x004c39d8). The files are Cinepak video (320x240, 10 frames a second) with
 * 22050 Hz 16-bit PCM sound. They come into the mod unchanged as "re1/movies/<id>"
 * (re1/tools/soh_scene.py write_movies), and are read here: the AVI's chunks, the Cinepak frames
 * (decoded here, as the format describes them: strips of 4x4 blocks from two codebooks, V1 one
 * entry scaled up, V4 four), the sound handed to SDL (NvMovieAudio.cpp).
 *
 * While one plays the game holds (Play_Update returns early) and the frame covers the screen,
 * drawn as SoH draws a room's background. Any button skips it, as RE1's in-game movies allow.
 */
#include "global.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NV_MOVIE_W 320
#define NV_MOVIE_H 240
#define NV_MOVIE_STRIPS 32
#define NV_MOVIE_CELL_W 64 // drawn as 64x32 cells (as the door scenes draw their textures)
#define NV_MOVIE_CELL_H 32
#define NV_MOVIE_CELLS_X (NV_MOVIE_W / NV_MOVIE_CELL_W)
#define NV_MOVIE_CELLS_Y ((NV_MOVIE_H + NV_MOVIE_CELL_H - 1) / NV_MOVIE_CELL_H)

void NvMovieAudio_Play(const void* pcm, u32 bytes, s32 rate, s32 channels);
void NvMovieAudio_Stop(void);
u32 NvMovieAudio_Ticks(void);

typedef struct {
    const u8* data;
    u32 size;
} NvMovieChunk;

static s32 sNvMovieActive;
static s32 sNvMovieId;
static NvMovieChunk* sNvMovieFrames;
static s32 sNvMovieNumFrames;
static s32 sNvMovieShown; // frames decoded so far
static u32 sNvMovieUsPerFrame;
static u32 sNvMovieStart;
static u8* sNvMovieSound;
// RGBA5551, big-endian, cell by cell; two pictures, so a new one is a new texture
static u8 sNvMoviePixels[2][NV_MOVIE_CELLS_X * NV_MOVIE_CELLS_Y * NV_MOVIE_CELL_W * NV_MOVIE_CELL_H * 2];
static s32 sNvMovieBuf;
static u16 sNvMovieBook[NV_MOVIE_STRIPS][2][256][4]; // per strip: V4 and V1 codebooks, 2x2 colours

static u32 NvMovie_Le32(const u8* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((u32)p[3] << 24);
}

static u32 NvMovie_Be24(const u8* p) {
    return (p[0] << 16) | (p[1] << 8) | p[2];
}

static u32 NvMovie_Be32(const u8* p) {
    return ((u32)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

/* ---- Cinepak ---- */

static u16 NvMovie_Colour(s32 r, s32 g, s32 b) {
    r = CLAMP(r, 0, 255);
    g = CLAMP(g, 0, 255);
    b = CLAMP(b, 0, 255);
    return ((r >> 3) << 11) | ((g >> 3) << 6) | ((b >> 3) << 1) | 1;
}

/** A codebook chunk: 0x20-0x27, bit 0 a partial update (a mask before each 32), bit 2 grey (4 bytes). */
static void NvMovie_Codebook(u16 (*book)[4], s32 id, const u8* p, const u8* end) {
    s32 n = (id & 0x04) ? 4 : 6;
    u32 flag = 0;
    u32 mask = 0;
    s32 i;
    s32 k;

    for (i = 0; i < 256; i++) {
        if ((id & 0x01) && !(mask >>= 1)) {
            if (p + 4 > end) {
                break;
            }
            flag = NvMovie_Be32(p);
            p += 4;
            mask = 0x80000000u;
        }
        if (!(id & 0x01) || (flag & mask)) {
            if (p + n > end) {
                break;
            }
            if (n == 4) {
                for (k = 0; k < 4; k++) {
                    book[i][k] = NvMovie_Colour(p[k], p[k], p[k]);
                }
            } else {
                s32 u = (s8)p[4];
                s32 v = (s8)p[5];

                for (k = 0; k < 4; k++) {
                    book[i][k] = NvMovie_Colour(p[k] + v * 2, p[k] - (u / 2) - v, p[k] + u * 2);
                }
            }
            p += n;
        }
    }
}

static void NvMovie_Put(s32 x, s32 y, u16 c) {
    if ((x < NV_MOVIE_W) && (y < NV_MOVIE_H)) {
        s32 cell = (y / NV_MOVIE_CELL_H) * NV_MOVIE_CELLS_X + x / NV_MOVIE_CELL_W;
        s32 i = (cell * NV_MOVIE_CELL_H + y % NV_MOVIE_CELL_H) * NV_MOVIE_CELL_W + x % NV_MOVIE_CELL_W;

        sNvMoviePixels[sNvMovieBuf][i * 2] = c >> 8;
        sNvMoviePixels[sNvMovieBuf][i * 2 + 1] = c & 0xFF;
    }
}

/** An image chunk: 0x30 every block (a V4/V1 bit each), 0x31 changed blocks only, 0x32 all V1. */
static void NvMovie_Vectors(s32 strip, s32 id, s32 x1, s32 y1, s32 x2, s32 y2, const u8* p, const u8* end) {
    u16 (*v4)[4] = sNvMovieBook[strip][0];
    u16 (*v1)[4] = sNvMovieBook[strip][1];
    s32 x;
    s32 y;
    u32 flag = 0; // (the flag bits run on through the whole strip, row after row)
    u32 mask = 0;

    for (y = y1; y < y2; y += 4) {
        for (x = x1; x < x2; x += 4) {
            if ((id & 0x01) && !(mask >>= 1)) {
                if (p + 4 > end) {
                    return;
                }
                flag = NvMovie_Be32(p);
                p += 4;
                mask = 0x80000000u;
            }
            if (!(id & 0x01) || (flag & mask)) {
                if (!(id & 0x02) && !(mask >>= 1)) {
                    if (p + 4 > end) {
                        return;
                    }
                    flag = NvMovie_Be32(p);
                    p += 4;
                    mask = 0x80000000u;
                }
                if ((id & 0x02) || (~flag & mask)) {
                    u16* c;
                    s32 i;

                    if (p >= end) {
                        return;
                    }
                    c = v1[*p++]; // one entry, each colour a 2x2
                    for (i = 0; i < 16; i++) {
                        s32 bx = i & 3;
                        s32 by = i >> 2;

                        NvMovie_Put(x + bx, y + by, c[(by >> 1) * 2 + (bx >> 1)]);
                    }
                } else if (flag & mask) {
                    s32 q;

                    if (p + 4 > end) {
                        return;
                    }
                    for (q = 0; q < 4; q++) { // four entries, one a quarter
                        u16* c = v4[p[q]];
                        s32 qx = x + (q & 1) * 2;
                        s32 qy = y + (q >> 1) * 2;

                        NvMovie_Put(qx, qy, c[0]);
                        NvMovie_Put(qx + 1, qy, c[1]);
                        NvMovie_Put(qx, qy + 1, c[2]);
                        NvMovie_Put(qx + 1, qy + 1, c[3]);
                    }
                    p += 4;
                }
            }
        }
    }
}

/** One frame: a 10-byte header (flags, size, width, height, strips), then the strips. */
static void NvMovie_Decode(const u8* p, u32 size) {
    const u8* end = p + size;
    s32 flags;
    s32 strips;
    s32 i;
    s32 y0 = 0;

    if (size < 10) {
        return; // an empty frame: the last one stays
    }
    flags = p[0];
    strips = MIN((p[8] << 8) | p[9], NV_MOVIE_STRIPS);
    p += 10;
    for (i = 0; i < strips; i++) {
        s32 x1;
        s32 y1;
        s32 x2;
        s32 y2;
        s32 stripSize;
        const u8* stripEnd;

        if (p + 12 > end) {
            return;
        }
        y1 = (p[4] << 8) | p[5];
        x1 = (p[6] << 8) | p[7];
        if (y1 == 0) { // relative to the strip before
            y1 = y0;
            y2 = y1 + ((p[8] << 8) | p[9]);
        } else {
            y2 = (p[8] << 8) | p[9];
        }
        x2 = (p[10] << 8) | p[11];
        stripSize = (s32)NvMovie_Be24(p + 1) - 12;
        if (stripSize < 0) {
            return;
        }
        p += 12;
        stripEnd = MIN(p + stripSize, end);
        if ((i > 0) && !(flags & 0x01)) { // codebooks carry over from the strip above
            memcpy(sNvMovieBook[i], sNvMovieBook[i - 1], sizeof(sNvMovieBook[i]));
        }
        {
            const u8* q = p;

            while (q + 4 <= stripEnd) {
                s32 id = q[0];
                s32 n = (s32)NvMovie_Be24(q + 1) - 4;
                const u8* c = q + 4;
                const u8* cEnd;

                if (n < 0) {
                    break;
                }
                cEnd = MIN(c + n, stripEnd);
                if ((id >= 0x20) && (id <= 0x27)) {
                    NvMovie_Codebook(sNvMovieBook[i][(id & 0x02) ? 1 : 0], id, c, cEnd);
                } else if ((id >= 0x30) && (id <= 0x32)) {
                    NvMovie_Vectors(i, id, x1, y1, x2, y2, c, cEnd);
                    break;
                }
                q = cEnd;
            }
        }
        p = stripEnd;
        y0 = y2;
    }
}

/* ---- the AVI ---- */

/** The movie's chunks: its video frames, its sound (joined), its frame time. */
static s32 NvMovie_Open(const u8* d) {
    u32 riffSize;
    const u8* p;
    const u8* end;
    s32 cap = 256;
    s32 audioStream = -1;
    s32 stream = 0;
    s32 rate = 22050;
    s32 channels = 2;
    u32 soundLen = 0;
    u32 soundCap = 0;

    if ((memcmp(d, "RIFF", 4) != 0) || (memcmp(d + 8, "AVI ", 4) != 0)) {
        return false;
    }
    riffSize = NvMovie_Le32(d + 4);
    end = d + 8 + riffSize;
    sNvMovieFrames = malloc(cap * sizeof(NvMovieChunk));
    sNvMovieNumFrames = 0;
    sNvMovieUsPerFrame = 100000;
    sNvMovieSound = NULL;
    p = d + 12;
    while (p + 8 <= end) {
        u32 size = NvMovie_Le32(p + 4);
        const u8* body = p + 8;

        if (memcmp(p, "LIST", 4) == 0) {
            if ((memcmp(body, "hdrl", 4) == 0) || (memcmp(body, "strl", 4) == 0) || (memcmp(body, "movi", 4) == 0) ||
                (memcmp(body, "rec ", 4) == 0)) {
                p = body + 4; // go inside
                continue;
            }
        } else if (memcmp(p, "avih", 4) == 0) {
            sNvMovieUsPerFrame = NvMovie_Le32(body);
        } else if (memcmp(p, "strh", 4) == 0) {
            if (memcmp(body, "auds", 4) == 0) {
                audioStream = stream;
            }
            stream++;
        } else if (memcmp(p, "strf", 4) == 0) {
            if (audioStream == stream - 1) { // WAVEFORMATEX
                channels = body[2] | (body[3] << 8);
                rate = NvMovie_Le32(body + 4);
            }
        } else if ((p[2] == 'd') && ((p[3] == 'c') || (p[3] == 'b'))) { // a video frame
            if (sNvMovieNumFrames == cap) {
                cap *= 2;
                sNvMovieFrames = realloc(sNvMovieFrames, cap * sizeof(NvMovieChunk));
            }
            sNvMovieFrames[sNvMovieNumFrames].data = body;
            sNvMovieFrames[sNvMovieNumFrames].size = size;
            sNvMovieNumFrames++;
        } else if ((p[2] == 'w') && (p[3] == 'b')) { // sound
            if (soundLen + size > soundCap) {
                soundCap = MAX(soundCap * 2, soundLen + size + 65536);
                sNvMovieSound = realloc(sNvMovieSound, soundCap);
            }
            memcpy(sNvMovieSound + soundLen, body, size);
            soundLen += size;
        }
        p = body + size + (size & 1); // chunks are word-aligned
    }
    if (sNvMovieSound != NULL) {
        NvMovieAudio_Play(sNvMovieSound, soundLen, rate, channels);
    }
    printf("NVMOVIE %d: %d frames, %u us each, sound %u bytes at %d Hz x%d\n", (int)sNvMovieId,
           (int)sNvMovieNumFrames, sNvMovieUsPerFrame, soundLen, (int)rate, (int)channels);
    fflush(stdout);
    return sNvMovieNumFrames > 0;
}

static void NvMovie_Close(void) {
    NvMovieAudio_Stop();
    free(sNvMovieFrames);
    free(sNvMovieSound);
    sNvMovieFrames = NULL;
    sNvMovieSound = NULL;
    sNvMovieActive = false;
    printf("NVMOVIE %d done\n", (int)sNvMovieId);
    fflush(stdout);
}

/* ---- the game's side ---- */

s32 NvMovie_Playing(void) {
    return sNvMovieActive;
}

/** cmd_fmv_set: start movie `id` (left out quietly if the mod has no such file). */
void NvMovie_Start(s32 id) {
    char path[32];
    u8* d;

    if (sNvMovieActive) {
        return;
    }
    snprintf(path, sizeof(path), "re1/movies/%d", (int)id);
    if (!ResourceMgr_FileExists(path)) {
        printf("NVMOVIE %d: not in the mod\n", (int)id);
        fflush(stdout);
        return;
    }
    d = (u8*)ResourceGetDataByName(path);
    sNvMovieId = id;
    if ((d == NULL) || !NvMovie_Open(d)) {
        free(sNvMovieFrames);
        free(sNvMovieSound);
        sNvMovieFrames = NULL;
        sNvMovieSound = NULL;
        return;
    }
    {
        s32 i;

        for (i = 0; i < (s32)sizeof(sNvMoviePixels) / 2; i++) { // opaque black
            sNvMoviePixels[0][i * 2] = 0;
            sNvMoviePixels[0][i * 2 + 1] = 1;
        }
    }
    sNvMovieBuf = 0;
    sNvMovieShown = 0;
    sNvMovieStart = NvMovieAudio_Ticks();
    sNvMovieActive = true;
}

/** Play_Update, first: while a movie plays, step it and hold the game (true). */
s32 NvMovie_Update(PlayState* play) {
    Input* input = &play->state.input[0];
    s32 due;

    if (!sNvMovieActive) {
        return false;
    }
    if (CHECK_BTN_ANY(input->press.button, BTN_A | BTN_B | BTN_START) && (sNvMovieShown > 1)) {
        NvMovie_Close();
        return true; // (the game goes on next frame)
    }
    // the frame due by the clock (the sound plays on its own)
    due = (s32)((u64)(NvMovieAudio_Ticks() - sNvMovieStart) * 1000 / sNvMovieUsPerFrame) + 1;
    if (getenv("NV_MOVIE_FRAMES") != NULL) { // headless: two of the game's frames a movie frame
        due = sNvMovieShown + ((play->gameplayFrames & 1) ? 1 : 0);
        due = MAX(due, 1);
    }
    while ((sNvMovieShown < due) && (sNvMovieShown < sNvMovieNumFrames)) {
        // (frames change only what moved: start from the last picture)
        memcpy(sNvMoviePixels[sNvMovieBuf ^ 1], sNvMoviePixels[sNvMovieBuf], sizeof(sNvMoviePixels[0]));
        sNvMovieBuf ^= 1;
        NvMovie_Decode(sNvMovieFrames[sNvMovieShown].data, sNvMovieFrames[sNvMovieShown].size);
        sNvMovieShown++;
    }
    if (due > sNvMovieNumFrames) {
        NvMovie_Close();
    }
    return true;
}

/** Play_DrawOverlayElements, last: black over everything, then the picture in 64x32 cells. */
void NvMovie_Draw(PlayState* play) {
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    u8* pic = sNvMoviePixels[sNvMovieBuf];
    s32 cx;
    s32 cy;

    if (!sNvMovieActive) {
        return;
    }
    OPEN_DISPS(gfxCtx);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_FILL);
    gDPSetRenderMode(OVERLAY_DISP++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(OVERLAY_DISP++, GPACK_RGBA5551(0, 0, 0, 1) << 16 | GPACK_RGBA5551(0, 0, 0, 1));
    gDPFillWideRectangle(OVERLAY_DISP++, OTRGetRectDimensionFromLeftEdge(0), 0,
                         OTRGetRectDimensionFromRightEdge(SCREEN_WIDTH), SCREEN_HEIGHT);
    gDPPipeSync(OVERLAY_DISP++);
    gDPSetCycleType(OVERLAY_DISP++, G_CYC_1CYCLE);
    gDPSetRenderMode(OVERLAY_DISP++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetTextureFilter(OVERLAY_DISP++, G_TF_POINT);
    gDPSetTexturePersp(OVERLAY_DISP++, G_TP_NONE);
    gDPSetTextureLUT(OVERLAY_DISP++, G_TT_NONE);
    gDPSetCombineMode(OVERLAY_DISP++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    for (cy = 0; cy < NV_MOVIE_CELLS_Y; cy++) {
        for (cx = 0; cx < NV_MOVIE_CELLS_X; cx++) {
            u8* cell = pic + (cy * NV_MOVIE_CELLS_X + cx) * NV_MOVIE_CELL_W * NV_MOVIE_CELL_H * 2;
            s32 x = cx * NV_MOVIE_CELL_W;
            s32 y = cy * NV_MOVIE_CELL_H;
            s32 h = MIN(NV_MOVIE_CELL_H, NV_MOVIE_H - y);

            gSPInvalidateTexCache(OVERLAY_DISP++, (uintptr_t)cell); // the same memory, a new picture
            gDPLoadTextureBlock(OVERLAY_DISP++, cell, G_IM_FMT_RGBA, G_IM_SIZ_16b, NV_MOVIE_CELL_W, NV_MOVIE_CELL_H, 0,
                                G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);
            gSPTextureRectangle(OVERLAY_DISP++, x << 2, y << 2, (x + NV_MOVIE_CELL_W) << 2, (y + h) << 2,
                                G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        }
    }
    gDPPipeSync(OVERLAY_DISP++);
    CLOSE_DISPS(gfxCtx);
}
