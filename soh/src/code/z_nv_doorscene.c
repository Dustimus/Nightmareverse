/*
 * Nightmareverse: RE1's door scenes. RE1 covers every room change with a short 3D scene: the
 * door swinging open against black as the camera moves through. When a door (z_nv_door.c)
 * has one, the next room opens on it: RE1's own door model, texture and script
 * (re1/tools/soh_doors.py: "re1/doors/door_<anim>"), played by the same rules as
 * re1/tools/dor.py and mod1's En_ReDoor (see dor.py for the bytecode). Link waits, the HUD is
 * hidden; then the room appears, as in RE1.
 */
#include "global.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <stdio.h>
#include <string.h>

#define NV_DS_SLOTS 16
#define NV_DS_THREADS 8
#define NV_DS_LOOPS 4
// RE1's door scripts tick at 60 a second (mod1: at 60 both the double and the single door shut
// at ~4.5 s and end at ~5)
#define NV_DS_TICK_RATE 60
#define NV_DS_MAX_TICKS 1800
// RE1 projects with a focal length of 256 at 320x240 (its room cameras): 2 atan(120 / 256)
#define NV_DS_FOVY 50.0f
#define NV_DS_DEPTH_SLIVER 0.002f
#define NV_DS_CELL_W 64
#define NV_DS_CELL_H 32

typedef struct {
    s16 model;
    u8 parent;
    u8 flags;
    s16 pos[3];
    s16 rot[3];
    s16 posVel[3];
    s16 rotVel[3];
} NvDsSlot;

typedef struct {
    s32 pc;
    s32 loopDepth;
    s32 loopStart[NV_DS_LOOPS];
    s32 loopCount[NV_DS_LOOPS];
} NvDsThread;

typedef struct {
    Vtx* vtx;
    u16 cell;
    u16 numVerts;
} NvDsBatch;

typedef struct {
    NvDsBatch* batches;
    u16 numBatches;
} NvDsModel;

#define NV_DS_MAX_MODELS 32
#define NV_DS_MAX_BATCHES 512

static struct {
    s32 pending; // queued by the door, played once the next scene is up
    s32 active;
    s16 anim;
    s8 vars[8];
    s16 shortVar; // the scripts' one short variable (ops 0C, 0D, 05)
    u8* script;
    u16* routines;
    s32 numRoutines;
    s32 scriptSize;
    u16* cells; // 64x32 RGBA5551 each
    s32 cellsW;
    NvDsModel models[NV_DS_MAX_MODELS];
    s32 numModels;
    NvDsBatch batches[NV_DS_MAX_BATCHES];
    NvDsSlot slots[NV_DS_SLOTS];
    NvDsThread threads[NV_DS_THREADS];
    s16 cam[6];
    s16 camVel[6];
    s32 ticks;
    s32 tickAccum;
    s32 startFrames;
} sNvDs;

// operand bytes per opcode, the opcode included (0: not seen in a door script)
// bytes per op, from RE1's handlers (the g_doorOps table)
static const u8 sNvDsOpSize[0x26] = {
    2, 2, 2, 2, 6, 6, 4, 2, 4, 2, 4, 4, 4, 4, 6, 6,      // 0x00
    6, 14, 14, 2, 4, 8, 8, 2, 4, 8, 6, 2, 4, 10, 10, 4, // 0x10
    4, 4, 4, 4, 2, 2,                                    // 0x20
};

static void NvDoorScene_Path(char* path, size_t size, s32 anim) {
    snprintf(path, size, "re1/doors/door_%d", (int)anim);
}

/** Whether the door scene exists (the door then cuts straight to it instead of fading). */
s32 NvDoorScene_Exists(s32 anim) {
    char path[64];

    NvDoorScene_Path(path, sizeof(path), anim);
    return ResourceMgr_FileExists(path);
}

/**
 * Queue a door scene for the next room, with the door's Door_set bytes its script reads. RE1's
 * variables (Bio.exe 0x4b0de8, pointers to what 0x48d680 copies out of the door as it opens):
 * 0 byte 10, 1 byte 11, 2 the scene's number (byte 12), 3 byte 13's low 6 bits, 4 byte 15 (the
 * target stage and room), 7 byte 10 again; 5 and 6 are game state (taken as 0).
 */
void NvDoorScene_Queue(s32 anim, s32 b10, s32 b11, s32 b13, s32 b15) {
    memset(sNvDs.vars, 0, sizeof(sNvDs.vars));
    sNvDs.anim = anim;
    sNvDs.vars[0] = b10;
    sNvDs.vars[1] = b11;
    sNvDs.vars[2] = anim;
    sNvDs.vars[3] = b13 & 0x3F;
    sNvDs.vars[4] = b15;
    sNvDs.vars[7] = b10;
    sNvDs.pending = true;
}

static s32 NvDoorScene_Load(void) {
    char path[64];
    u8* d;
    s32 i;
    s32 nb = 0;
    s32 cellsH;

    NvDoorScene_Path(path, sizeof(path), sNvDs.anim);
    if (!ResourceMgr_FileExists(path)) {
        return false;
    }
    d = (u8*)ResourceGetDataByName(path);
    if ((d == NULL) || (memcmp(d, "RDSC", 4) != 0)) {
        return false;
    }
    sNvDs.numRoutines = d[4] | (d[5] << 8);
    sNvDs.scriptSize = d[6] | (d[7] << 8);
    sNvDs.routines = (u16*)(d + 8);
    sNvDs.script = d + 8 + 2 * sNvDs.numRoutines;
    d = sNvDs.script + sNvDs.scriptSize;
    if ((uintptr_t)d & 1) {
        d++;
    }
    sNvDs.cellsW = d[0] | (d[1] << 8);
    cellsH = d[2] | (d[3] << 8);
    sNvDs.cells = (u16*)(d + 4);
    d += 4 + sNvDs.cellsW * cellsH * NV_DS_CELL_W * NV_DS_CELL_H * 2;
    sNvDs.numModels = MIN(d[0] | (d[1] << 8), NV_DS_MAX_MODELS);
    d += 2;
    for (i = 0; i < sNvDs.numModels; i++) {
        s32 n = d[0] | (d[1] << 8);
        s32 b;

        d += 2;
        sNvDs.models[i].batches = &sNvDs.batches[nb];
        sNvDs.models[i].numBatches = 0;
        for (b = 0; b < n; b++) {
            u16 cell = d[0] | (d[1] << 8);
            u16 nv = d[2] | (d[3] << 8);

            if (nb < NV_DS_MAX_BATCHES) {
                sNvDs.batches[nb].cell = cell;
                sNvDs.batches[nb].numVerts = nv;
                sNvDs.batches[nb].vtx = (Vtx*)(d + 4);
                nb++;
                sNvDs.models[i].numBatches++;
            }
            d += 4 + nv * sizeof(Vtx);
        }
    }
    return true;
}

static void NvDoorScene_StartThread(s32 thread, s32 routine) {
    if ((thread < NV_DS_THREADS) && (routine < sNvDs.numRoutines)) {
        sNvDs.threads[thread].pc = sNvDs.routines[routine];
        sNvDs.threads[thread].loopDepth = 0;
    }
}

// one tick of every thread; false once the scene is over
static s32 NvDoorScene_Tick(void) {
    s32 t;
    s32 live = false;

    for (t = 0; t < NV_DS_THREADS; t++) {
        NvDsThread* th = &sNvDs.threads[t];

        while (th->pc >= 0) {
            u8* a = &sNvDs.script[th->pc];
            u8 op = a[0];
            s32 size = (op < ARRAY_COUNT(sNvDsOpSize)) ? sNvDsOpSize[op] : 0;
            NvDsSlot* s;
            s32 k;

            if ((size == 0) || (th->pc + size > sNvDs.scriptSize)) {
                th->pc = -1; // something we don't know: stop this thread
                break;
            }
            th->pc += size;
            s = &sNvDs.slots[a[1] % NV_DS_SLOTS];
            switch (op) {
                case 0x00: // the end of the scene (door_op_end: every thread stops)
                    return false;
                case 0x01: // this thread is done
                    th->pc = -1;
                    break;
                case 0x02: // wait until thread a[1] is done
                    if (sNvDs.threads[a[1] % NV_DS_THREADS].pc >= 0) {
                        th->pc -= size;
                        goto yield;
                    }
                    break;
                case 0x03:
                    goto yield;
                case 0x04: { // 04 n k c v: run the next n bytes if var[k] (c) v, else skip them; signed bytes
                    s8 var = sNvDs.vars[a[2] & 7];
                    s8 v = (s8)a[4];
                    s32 run;

                    switch (a[3]) { // Bio.exe 0x434d40
                        case 0:
                            run = (var == v);
                            break;
                        case 1:
                            run = (var > v);
                            break;
                        case 2:
                            run = (var >= v);
                            break;
                        case 3:
                            run = (var < v);
                            break;
                        case 4:
                            run = (var <= v);
                            break;
                        case 5:
                            run = (var != v);
                            break;
                        default:
                            run = true;
                            break;
                    }
                    if (!run) {
                        th->pc += a[1];
                    }
                    break;
                }
                case 0x06:
                    if (th->loopDepth < NV_DS_LOOPS) {
                        th->loopStart[th->loopDepth] = th->pc;
                        th->loopCount[th->loopDepth] = a[2] | (a[3] << 8);
                        th->loopDepth++;
                    }
                    break;
                case 0x07: // loop back (and wait a frame), or past the loop's end at once
                    if (th->loopDepth > 0) {
                        if (--th->loopCount[th->loopDepth - 1] > 0) {
                            th->pc = th->loopStart[th->loopDepth - 1];
                            goto yield;
                        }
                        th->loopDepth--;
                    }
                    break;
                case 0x05: { // like 04, on the short variable
                    s16 v = a[4] | (a[5] << 8);
                    s16 var = sNvDs.shortVar;
                    s32 run;

                    switch (a[3]) {
                        case 0:
                            run = (var == v);
                            break;
                        case 1:
                            run = (var > v);
                            break;
                        case 2:
                            run = (var >= v);
                            break;
                        case 3:
                            run = (var < v);
                            break;
                        case 4:
                            run = (var <= v);
                            break;
                        case 5:
                            run = (var != v);
                            break;
                        default:
                            run = false;
                            break;
                    }
                    if (!run) {
                        th->pc += a[1];
                    }
                    break;
                }
                case 0x08:
                    NvDoorScene_StartThread(a[2], a[3]);
                    break;
                case 0x09: // stop thread a[1]
                    sNvDs.threads[a[1] % NV_DS_THREADS].pc = -1;
                    break;
                case 0x0A:
                    sNvDs.vars[a[1] & 7] = (s8)a[2];
                    break;
                case 0x0B:
                    sNvDs.vars[a[1] & 7] += (s8)a[2];
                    break;
                case 0x0C:
                    sNvDs.shortVar = a[2] | (a[3] << 8);
                    break;
                case 0x0D:
                    sNvDs.shortVar += (s16)(a[2] | (a[3] << 8));
                    break;
                case 0x1A:
                    for (k = 0; k < 3; k++) {
                        s->rotVel[k] = (s8)a[2 + k];
                    }
                    break;
                case 0x0E: // RE1's door sounds come with the sound work; OoT's door meanwhile
                    Sfx_PlaySfxCentered(NA_SE_OC_DOOR_OPEN);
                    break;
                case 0x0F:
                    Sfx_PlaySfxCentered(NA_SE_EV_DOOR_CLOSE);
                    break;
                case 0x10:
                    s->model = a[2];
                    s->parent = a[3];
                    s->flags = a[4];
                    for (k = 0; k < 3; k++) {
                        s->pos[k] = s->rot[k] = s->posVel[k] = s->rotVel[k] = 0;
                    }
                    break;
                case 0x11:
                case 0x12:
                    for (k = 0; k < 6; k++) {
                        s16 v = a[2 + k * 2] | (a[3 + k * 2] << 8);

                        if (op == 0x11) {
                            sNvDs.cam[k] = v;
                        } else {
                            sNvDs.camVel[k] = v;
                        }
                    }
                    break;
                case 0x13:
                    for (k = 0; k < 6; k++) {
                        sNvDs.cam[k] += sNvDs.camVel[k];
                    }
                    break;
                case 0x14:
                    sNvDs.camVel[a[2] % 6] += (s8)a[3];
                    break;
                case 0x15:
                case 0x16:
                case 0x19:
                    for (k = 0; k < 3; k++) {
                        s16 v = a[2 + k * 2] | (a[3 + k * 2] << 8);

                        if (op == 0x15) {
                            s->pos[k] = v;
                        } else if (op == 0x16) {
                            s->posVel[k] = v;
                        } else {
                            s->rot[k] = v;
                        }
                    }
                    break;
                case 0x17:
                    for (k = 0; k < 3; k++) {
                        s->pos[k] += s->posVel[k];
                    }
                    break;
                case 0x18:
                    s->posVel[a[2] % 3] += (s8)a[3];
                    break;
                case 0x1B:
                    for (k = 0; k < 3; k++) {
                        s->rot[k] += s->rotVel[k];
                    }
                    break;
                case 0x1C:
                    s->rotVel[a[2] % 3] += (s8)a[3];
                    break;
                case 0x21:
                    s->flags = a[2];
                    break;
                case 0x20: // play_sfx (RE1's door sounds come with the sound work)
                    break;
                case 0x24: // dispatch off until the sound system is ready: a frame here
                    goto yield;
                default: // 0x1D, 0x1E (model vertices), 0x1F (message), 0x22, 0x23, 0x25: nothing here
                    break;
            }
        }
        continue;
    yield:
        live = true;
    }
    for (t = 0; t < NV_DS_THREADS; t++) {
        if (sNvDs.threads[t].pc >= 0) {
            live = true;
        }
    }
    return live;
}

static void NvDoorScene_End(PlayState* play) {
    sNvDs.active = false;
    Player_SetCsActionWithHaltedActors(play, NULL, 7);
    Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_ALL);
    printf("NVDOORSCENE end ticks=%d\n", (int)sNvDs.ticks);
    fflush(stdout);
}

/** Called from Play_Update (z_play.c) every frame. */
void NvDoorScene_Update(PlayState* play) {
    s32 i;

    if (sNvDs.pending && (play->transitionTrigger == TRANS_TRIGGER_OFF) &&
        (play->transitionMode == TRANS_MODE_OFF) && (GET_PLAYER(play) != NULL)) {
        sNvDs.pending = false;
        if (NvDoorScene_Load()) {
            for (i = 0; i < NV_DS_SLOTS; i++) {
                sNvDs.slots[i].model = -1;
            }
            for (i = 0; i < NV_DS_THREADS; i++) {
                sNvDs.threads[i].pc = -1;
            }
            memset(sNvDs.cam, 0, sizeof(sNvDs.cam));
            memset(sNvDs.camVel, 0, sizeof(sNvDs.camVel));
            sNvDs.ticks = sNvDs.tickAccum = 0;
            sNvDs.shortVar = 0;
            sNvDs.threads[0].pc = sNvDs.routines[0];
            sNvDs.active = true;
            sNvDs.startFrames = 0;
            printf("NVDOORSCENE start anim=%d vars=%d,%d,%d,%d,%d\n", (int)sNvDs.anim, (int)sNvDs.vars[0],
                   (int)sNvDs.vars[1], (int)sNvDs.vars[2], (int)sNvDs.vars[3], (int)sNvDs.vars[4]);
            fflush(stdout);
        }
    }
    if (!sNvDs.active) {
        return;
    }
    // hold Link and the HUD for the length of the scene (the room's start brings them back otherwise)
    if (sNvDs.startFrames++ < 2) {
        Player_SetCsActionWithHaltedActors(play, NULL, 8);
    }
    Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_NOTHING);
    // the script's ticks at RE1's rate, whatever the game's update rate
    sNvDs.tickAccum += R_UPDATE_RATE * NV_DS_TICK_RATE;
    while (sNvDs.tickAccum >= 60) {
        sNvDs.tickAccum -= 60;
        sNvDs.ticks++;
        if (!NvDoorScene_Tick() || (sNvDs.ticks > NV_DS_MAX_TICKS)) { // a safety stop
            NvDoorScene_End(play);
            return;
        }
    }
}

/** Whether a door scene covers the room this frame (z_nv_mask.c skips the room's masks: their
 * depth would cut the door scene drawn over them). */
s32 NvDoorScene_Covering(void) {
    return sNvDs.active || sNvDs.pending;
}

/** Called from Play_DrawOverlayElements (z_play.c): over the room, black and the door. */
void NvDoorScene_Draw(PlayState* play) {
    static u16 sPerspNorm;
    GraphicsContext* gfxCtx = play->state.gfxCtx;
    Mtx* proj;
    Mtx* view;
    s32 i;

    if (!sNvDs.active && !sNvDs.pending) {
        return;
    }
    OPEN_DISPS(gfxCtx);

    // black over the room, as OoT's own fades paint it (opaque, in the translucent pass: after the
    // room and Link)
    CLOSE_DISPS(gfxCtx);
    Environment_FillScreen(gfxCtx, 0, 0, 0, 255, FILL_SCREEN_XLU);
    OPEN_DISPS(gfxCtx);


    if (sNvDs.active) {
        proj = Graph_Alloc(gfxCtx, sizeof(Mtx));
        view = Graph_Alloc(gfxCtx, sizeof(Mtx));
        // RE1's camera, in RE1's own space (y down: "up" is -y, so nothing comes out mirrored)
        {
            // the door's depth squeezed into the nearest sliver of the depth range, so it lies in
            // front of whatever depth the room left (SoH's renderer won't write depth without the
            // depth test, so the buffer can't simply be cleared) while its own faces still sort:
            // z' = k z - (1 - k) w maps NDC depth [-1, 1] to [-1, 2k - 1]
            MtxF mf;
            s32 row;

            guPerspectiveF(mf.mf, &sPerspNorm, NV_DS_FOVY, (f32)SCREEN_WIDTH / SCREEN_HEIGHT, 30.0f, 40000.0f, 1.0f);
            for (row = 0; row < 4; row++) {
                mf.mf[row][2] = NV_DS_DEPTH_SLIVER * mf.mf[row][2] - (1.0f - NV_DS_DEPTH_SLIVER) * mf.mf[row][3];
            }
            Matrix_MtxFToMtx(&mf, proj); // (as guMtxF2L: the same row-vector layout)
        }
        guLookAt(view, sNvDs.cam[0], sNvDs.cam[1], sNvDs.cam[2], sNvDs.cam[3], sNvDs.cam[4], sNvDs.cam[5], 0.0f,
                 -1.0f, 0.0f);
        gSPPerspNormalize(OVERLAY_DISP++, sPerspNorm);
        gSPMatrix(OVERLAY_DISP++, proj, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
        gSPMatrix(OVERLAY_DISP++, view, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_PROJECTION);

        // point sampled and unlit, as on the PSX; colour 0 (alpha 0) is see-through
        gDPPipeSync(OVERLAY_DISP++);
        gDPSetCycleType(OVERLAY_DISP++, G_CYC_1CYCLE);
        gDPSetDepthSource(OVERLAY_DISP++, G_ZS_PIXEL); // (render modes leave the depth source alone)
        gDPSetRenderMode(OVERLAY_DISP++, G_RM_AA_ZB_TEX_EDGE, G_RM_AA_ZB_TEX_EDGE2);
        gDPSetTextureFilter(OVERLAY_DISP++, G_TF_POINT);
        gDPSetTexturePersp(OVERLAY_DISP++, G_TP_PERSP);
        gDPSetTextureLUT(OVERLAY_DISP++, G_TT_NONE);
        gDPSetCombineMode(OVERLAY_DISP++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        gSPClearGeometryMode(OVERLAY_DISP++, G_LIGHTING | G_FOG | G_CULL_BOTH | G_TEXTURE_GEN);
        gSPSetGeometryMode(OVERLAY_DISP++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
        gSPTexture(OVERLAY_DISP++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);

        for (i = 0; i < NV_DS_SLOTS; i++) {
            NvDsSlot* s = &sNvDs.slots[i];
            NvDsModel* m;
            s32 b;

            if ((s->model < 0) || (s->model >= sNvDs.numModels)) {
                continue;
            }
            Matrix_Translate(0.0f, 0.0f, 0.0f, MTXMODE_NEW);
            if ((s->parent < NV_DS_SLOTS) && (sNvDs.slots[s->parent].model >= 0)) {
                NvDsSlot* p = &sNvDs.slots[s->parent];

                Matrix_Translate(p->pos[0], p->pos[1], p->pos[2], MTXMODE_APPLY);
                Matrix_RotateZYX(p->rot[0] * 16, p->rot[1] * 16, p->rot[2] * 16, MTXMODE_APPLY);
            }
            Matrix_Translate(s->pos[0], s->pos[1], s->pos[2], MTXMODE_APPLY);
            Matrix_RotateZYX(s->rot[0] * 16, s->rot[1] * 16, s->rot[2] * 16, MTXMODE_APPLY); // 4096 a turn
            gSPMatrix(OVERLAY_DISP++, MATRIX_NEWMTX(gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

            m = &sNvDs.models[s->model];
            for (b = 0; b < m->numBatches; b++) {
                NvDsBatch* batch = &m->batches[b];
                s32 v;

                gDPLoadTextureBlock(OVERLAY_DISP++, sNvDs.cells + batch->cell * NV_DS_CELL_W * NV_DS_CELL_H,
                                    G_IM_FMT_RGBA, G_IM_SIZ_16b, NV_DS_CELL_W, NV_DS_CELL_H, 0,
                                    G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK,
                                    G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
                for (v = 0; v + 3 <= batch->numVerts; v += 30) {
                    s32 n = MIN(30, batch->numVerts - v);
                    s32 k;

                    gSPVertex(OVERLAY_DISP++, batch->vtx + v, n, 0);
                    for (k = 0; k + 3 <= n; k += 3) {
                        gSP1Triangle(OVERLAY_DISP++, k, k + 1, k + 2, 0);
                    }
                }
            }
        }
        // hand the game's own camera back to whatever draws next
        gSPPerspNormalize(OVERLAY_DISP++, play->view.normal);
        gSPMatrix(OVERLAY_DISP++, play->view.projectionPtr, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
        gSPMatrix(OVERLAY_DISP++, play->view.viewingPtr, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_PROJECTION);
    }

    CLOSE_DISPS(gfxCtx);
}
