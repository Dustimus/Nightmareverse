/*
 * Nightmareverse: the Fierce Deity's sword beam (mm/README.md "The Fierce Deity"), MM's En_M_Thunder as its
 * sword beam (zeldaret/mm, ovl_En_M_Thunder, ENMTHUNDER_SUBTYPE_SWORDBEAM_REGULAR): from his waist, along his
 * facing, pitched at what he's targeting; 80 units a frame for 20 frames, growing to 12, fading, MM's
 * light with it; MM's model (gameplay_keep's gSwordBeamDL, its scrolling texture, MM's blue). Spawned by
 * z_nv_mm_form.c (his swing while targeting, 1 magic, as MM's func_808332A0).
 *
 * It hits as OoT's light arrow does (the decision, 2026-11-24: OoT has no beam damage), for MM's 3.
 */
#include "global.h"

#define KEEP(name) ((Gfx*)("__OTR__mm/objects/gameplay_keep/" name))

typedef struct NvMmBeam {
    Actor actor;
    ColliderCylinder collider;
    LightNode* lightNode;
    LightInfo lightInfo;
    f32 lightColorFrac;
    f32 alphaFrac;
    f32 scroll;
} NvMmBeam;

const size_t gNvMmBeamSize = sizeof(NvMmBeam);

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2, { DMG_ARROW_LIGHT, 0x00, 0x03 }, { 0x00000000, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_NONE,
      BUMP_NONE, OCELEM_NONE },
    { 0, 40, -20, { 0, 0, 0 } },
};

void NvMmBeam_Init(Actor* thisx, PlayState* play) {
    NvMmBeam* this = (NvMmBeam*)thisx;
    Player* player = GET_PLAYER(play);

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    Lights_PointNoGlowSetInfo(&this->lightInfo, this->actor.world.pos.x, this->actor.world.pos.y,
                              this->actor.world.pos.z, 255, 255, 255, 0);
    this->lightNode = LightContext_InsertLight(play, &play->lightCtx, &this->lightInfo);
    this->actor.world.pos = player->bodyPartsPos[PLAYER_BODYPART_WAIST];
    this->actor.shape.rot.y = player->actor.shape.rot.y + 0x8000;
    this->actor.shape.rot.x = -this->actor.world.rot.x;
    this->actor.room = -1;
    Actor_SetScale(&this->actor, 0.1f);
    this->lightColorFrac = 1.0f;
    Audio_PlayActorSound2(&player->actor, NA_SE_IT_ROLLING_CUT_LV1);
}

void NvMmBeam_Destroy(Actor* thisx, PlayState* play) {
    NvMmBeam* this = (NvMmBeam*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
    LightContext_RemoveLight(play, &play->lightCtx, this->lightNode);
}

/** MM's EnMThunder_SwordBeam_Attack. */
void NvMmBeam_Update(Actor* thisx, PlayState* play) {
    NvMmBeam* this = (NvMmBeam*)thisx;
    f32 step;

    this->alphaFrac = (this->lightColorFrac > 0.9f) ? 1.0f : (this->lightColorFrac * (10.0f / 9.0f));
    if (Math_StepToF(&this->lightColorFrac, 0.0f, 0.05f)) {
        Actor_Kill(&this->actor);
        return;
    }
    step = -80.0f * Math_CosS(this->actor.world.rot.x);
    this->actor.world.pos.x += step * Math_SinS(this->actor.shape.rot.y);
    this->actor.world.pos.z += step * Math_CosS(this->actor.shape.rot.y);
    this->actor.world.pos.y += -80.0f * Math_SinS(this->actor.world.rot.x);
    Math_SmoothStepToF(&this->actor.scale.x, 12.0f, 0.6f, 2.0f, 0.0f);
    Actor_SetScale(&this->actor, this->actor.scale.x);
    this->collider.dim.radius = this->actor.scale.x * 5.0f;
    this->collider.dim.pos.x = (Math_SinS(this->actor.shape.rot.y) * -5.0f * this->actor.scale.x) + this->actor.world.pos.x;
    this->collider.dim.pos.y = this->actor.world.pos.y;
    this->collider.dim.pos.z = (Math_CosS(this->actor.shape.rot.y) * -5.0f * this->actor.scale.z) + this->actor.world.pos.z;
    CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    this->scroll += 1.0f; // (its texture's scroll: MM's)
    Lights_PointNoGlowSetInfo(&this->lightInfo, this->actor.world.pos.x, this->actor.world.pos.y,
                              this->actor.world.pos.z, this->lightColorFrac * 255.0f, this->lightColorFrac * 255.0f,
                              this->lightColorFrac * 100.0f, this->lightColorFrac * 800.0f);
}

/** MM's EnMThunder_Draw, the regular beam. */
void NvMmBeam_Draw(Actor* thisx, PlayState* play) {
    NvMmBeam* this = (NvMmBeam*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    Matrix_Scale(0.02f, 0.02f, 0.02f, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPSegment(POLY_XLU_DISP++, 0x08,
               Gfx_TwoTexScroll(play->state.gfxCtx, 0, 0, 0, 16, 64, 1, 0,
                                0x1FF - ((u16)(s32)(this->scroll * 10.0f) & 0x1FF), 32, 128));
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x80, 170, 255, 255, (u16)(this->alphaFrac * 255.0f));
    gDPSetEnvColor(POLY_XLU_DISP++, 0, 100, 255, 128);
    gSPDisplayList(POLY_XLU_DISP++, KEEP("gSwordBeamDL"));
    CLOSE_DISPS(play->state.gfxCtx);
}
