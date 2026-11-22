/*
 * Nightmareverse: Clock Town's smoking chimneys, from MM's Obj_Entotu (zeldaret/mm, ovl_Obj_Entotu),
 * ported closely (mm/README.md). The chimney and its smoke are MM's object_f53_obj, drawn by path;
 * the smoke's seven vertices are the actor's own (ovl_Obj_Entotu's data, brought over from the
 * player's ROM as mm/objects/ovl_Obj_Entotu): copied, their alpha faded in and out, and set in
 * segment 9 as in MM.
 *
 * Both are flat cards turned to face the camera. The smoke rises at mealtimes, Termina's clock:
 * 6:00-9:00, 11:00-14:00 and 16:00-20:00, each chimney starting and stopping a random few minutes
 * into the hour (MM's func_80A34700).
 *
 * Adapted from MM: the time is Termina's clock (z_nv_mm_clock.c); OoT's matrix calls (radians).
 */
#include "global.h"
#include "soh/ResourceManagerHelpers.h"

#define MM_F53(name) ((Gfx*)"__OTR__mm/objects/object_f53_obj/" name)
#define DL_CHIMNEY MM_F53("object_f53_obj_DL_000158")
#define DL_SMOKE MM_F53("object_f53_obj_DL_001C00")
static char sSmokeVtx[] = "__OTR__mm/objects/ovl_Obj_Entotu/ovl_Obj_Entotu_Vtx_000D10";
#define SMOKE_VTX_COUNT 7

typedef struct NvMmEntotu {
    Actor actor;
    Vtx vtx[SMOKE_VTX_COUNT];
    Vec3f smoke; // x: how much smoke (0-1), y and z: the two texture scrolls
    u8 mealtime;
    s16 minutes;
} NvMmEntotu;

const size_t gNvMmEntotuSize = sizeof(NvMmEntotu);

u16 NvMmClock_Time(void);

/** Which mealtime it is (1-3), or 0: each one starts `minutes` into its first hour and ends as many into its last. */
static s32 NvMmEntotu_Mealtime(s16 minutes) {
    s32 ret = 0;
    s16 time = (s16)(NvMmClock_Time() * (24.0f * 60.0f / 0x10000));
    s32 hours = time / 60;
    s32 currMinutes = time % 60;

    if (hours >= 16) {
        if ((hours == 16) && (currMinutes < minutes)) {
            return 0;
        }
        if (hours >= 20) {
            return 0;
        }
        if ((hours == 19) && (minutes < currMinutes)) {
            return 0;
        }
        ret = 3;
    } else if (hours >= 11) {
        if ((hours == 11) && (currMinutes < minutes)) {
            return 0;
        }
        if (hours >= 14) {
            return 0;
        }
        if ((hours == 13) && (minutes < currMinutes)) {
            return 0;
        }
        ret = 2;
    } else if (hours >= 6) {
        if ((hours == 6) && (currMinutes < minutes)) {
            return 0;
        }
        if (hours >= 9) {
            return 0;
        }
        if ((hours == 8) && (minutes < currMinutes)) {
            return 0;
        }
        ret = 1;
    }
    return ret;
}

static void NvMmEntotu_UpdateSmoke(NvMmEntotu* this) {
    s32 mealtime = NvMmEntotu_Mealtime(this->minutes);

    if (mealtime != this->mealtime) {
        this->mealtime = mealtime;
        this->minutes = Rand_S16Offset(0, 59);
    }
    Math_ApproachF(&this->smoke.x, (this->mealtime == 0) ? 0.0f : 1.0f, 0.02f, 1000.0f);
}

static void NvMmEntotu_FaceCamera(NvMmEntotu* this, PlayState* play) {
    Matrix_Translate(this->actor.world.pos.x, this->actor.world.pos.y, this->actor.world.pos.z, MTXMODE_NEW);
    this->actor.shape.rot.y = (s16)(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) + 0x8000);
    Matrix_RotateY(BINANG_TO_RAD(this->actor.shape.rot.y), MTXMODE_APPLY);
    Matrix_Scale(0.1f, 0.1f, 0.0f, MTXMODE_APPLY);
}

static void NvMmEntotu_DrawChimney(NvMmEntotu* this, PlayState* play) {
    NvMmEntotu_FaceCamera(this, play);
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, DL_CHIMNEY);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmEntotu_DrawSmoke(NvMmEntotu* this, PlayState* play) {
    Vtx* src = ResourceMgr_LoadVtxByName(sSmokeVtx);
    u8 scroll1;
    u8 scroll2;
    s32 i;

    if (src == NULL) {
        return;
    }
    this->smoke.y += 1.8f;
    this->smoke.z += 0.6f;
    scroll1 = 0x7F - (u8)this->smoke.y;
    scroll2 = 0x7F - (u8)this->smoke.z;
    this->smoke.x = CLAMP(this->smoke.x, 0.0f, 1.0f);
    for (i = 0; i < SMOKE_VTX_COUNT; i++) {
        this->vtx[i].v.cn[3] = src[i].v.cn[3] * this->smoke.x;
    }
    if (this->smoke.x > 0.0f) {
        NvMmEntotu_FaceCamera(this, play);
        OPEN_DISPS(play->state.gfxCtx);
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gSPSegment(POLY_XLU_DISP++, 0x08,
                   Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, scroll1, 0x20, 0x20, 1, 0, scroll2, 0x20, 0x20));
        gSPSegment(POLY_XLU_DISP++, 0x09, this->vtx);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, DL_SMOKE);
        CLOSE_DISPS(play->state.gfxCtx);
    }
}

void NvMmEntotu_Init(Actor* thisx, PlayState* play) {
    NvMmEntotu* this = (NvMmEntotu*)thisx;
    Vtx* src = ResourceMgr_LoadVtxByName(sSmokeVtx);

    if (src != NULL) {
        memcpy(this->vtx, src, sizeof(this->vtx));
    }
    this->minutes = Rand_S16Offset(0, 59);
    this->mealtime = 0;
}

void NvMmEntotu_Destroy(Actor* thisx, PlayState* play) {
}

void NvMmEntotu_Update(Actor* thisx, PlayState* play) {
    NvMmEntotu_UpdateSmoke((NvMmEntotu*)thisx);
}

void NvMmEntotu_Draw(Actor* thisx, PlayState* play) {
    NvMmEntotu* this = (NvMmEntotu*)thisx;

    NvMmEntotu_DrawSmoke(this, play);
    NvMmEntotu_DrawChimney(this, play);
}
