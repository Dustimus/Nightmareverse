/*
 * Nightmareverse: the bobblehead cow toy (the inn's desk, Clock Town's homes), from MM's Obj_Hariko
 * (zeldaret/mm, ovl_Obj_Hariko), ported as it is (mm/README.md): MM's model (object_hariko); its
 * head bobs while the ground quakes (MM's Quake_GetNumActiveQuakes: OoT's quake requests).
 */
#include "global.h"

#define HARIKO(name) ((Gfx*)("__OTR__mm/objects/object_hariko/" name))

typedef struct NvMmHariko {
    Actor actor;
    Vec3s headRot;
    s16 headOffset;
    f32 bobbleStep;
} NvMmHariko;

const size_t gNvMmHarikoSize = sizeof(NvMmHariko);

extern QuakeRequest sQuakeRequest[4]; // (z_quake.c)

/** MM's Quake_GetNumActiveQuakes. */
static s32 NvMmHariko_Quakes(void) {
    s32 n = 0;
    s32 i;

    for (i = 0; i < 4; i++) {
        n += (sQuakeRequest[i].callbackIdx != 0);
    }
    return n;
}

void NvMmHariko_Init(Actor* thisx, PlayState* play) {
    Actor_SetScale(thisx, 0.1f);
}

void NvMmHariko_Destroy(Actor* thisx, PlayState* play) {
}

void NvMmHariko_Update(Actor* thisx, PlayState* play) {
    NvMmHariko* this = (NvMmHariko*)thisx;

    if (this->bobbleStep > 0.0f) { // MM's ObjHariko_BobHead
        this->headOffset += 0x1555;
        this->headRot.x = Math_SinS(this->headOffset) * this->bobbleStep;
        this->headRot.y = Math_CosS(this->headOffset) * this->bobbleStep;
        Math_SmoothStepToF(&this->bobbleStep, 0, 0.5f, 18.0f, 18.0f);
        if (this->bobbleStep < 182.0f) {
            this->bobbleStep = 0.0f;
        }
    }
    if (NvMmHariko_Quakes() != 0) { // MM's ObjHariko_CheckForQuakes
        this->bobbleStep = 2730.0f;
    }
}

void NvMmHariko_Draw(Actor* thisx, PlayState* play) {
    NvMmHariko* this = (NvMmHariko*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    Matrix_Push();
    Matrix_RotateX(BINANG_TO_RAD(this->headRot.x), MTXMODE_APPLY);
    Matrix_RotateY(BINANG_TO_RAD(this->headRot.y), MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, HARIKO("gHarikoBodyDL"));
    gSPDisplayList(POLY_OPA_DISP++, HARIKO("gHarikoFaceDL"));
    Matrix_Pop();
    CLOSE_DISPS(play->state.gfxCtx);
}
