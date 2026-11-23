/*
 * Nightmareverse: Deku Link's bubble (mm/README.md "Deku Link's moves"), MM's En_Arrow as its
 * ARROW_TYPE_DEKU_BUBBLE (zeldaret/mm, ovl_En_Arrow): blown at his mouth while B is held (growing to
 * 16, MM's 0.07/1.8 steps; held 20 frames full, it goes on its own), then off: the bigger, the
 * slower (16 - its size, at least 1), flying straight for 99 frames, bursting on what it hits.
 * MM's mesh from gameplay_keep (blown: translucent, flying: solid) with MM's colour and its squash
 * with speed. Spawned by z_nv_mm_deku.c, which takes its magic (MM's 2). MM's look refracts the
 * screen behind it (its setup list reads the frame buffer, segment 0x0F, which SoH can't sample
 * so): here it's MM's mesh plainly shaded in MM's colour instead.
 *
 * Adapted from MM: it hits as OoT's slingshot seed does, for 1 (MM: DMG_DEKU_BUBBLE: OoT has none),
 * with a cylinder its size (MM's is a quad from where it was to where it is); bouncing off shields
 * (MM's AT_BOUNCED turn), the splash into water and the burst's wall mark (MM's EffectSsSbn) are
 * left out; sounds are OoT's nearest (MM's breath, fire and vanish are MM's own).
 */
#include "global.h"

#define KEEP(name) ((Gfx*)("__OTR__mm/objects/gameplay_keep/" name))

typedef struct NvMmBubble {
    Actor actor;
    ColliderCylinder collider;
    f32 size;     // MM's bubble.unk_144
    s16 fullFor;  // MM's bubble.unk_148: frames at full size, still blown
    s16 life;     // MM's unk_260
    s32 fired;
} NvMmBubble;

const size_t gNvMmBubbleSize = sizeof(NvMmBubble);

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_NONE, OC1_NONE, OC2_TYPE_PLAYER, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2, { DMG_SLINGSHOT, 0x00, 0x01 }, { 0x00000000, 0x00, 0x00 }, TOUCH_ON | TOUCH_NEAREST | TOUCH_SFX_NONE,
      BUMP_NONE, OCELEM_NONE },
    { 8, 16, -8, { 0, 0, 0 } },
};

void NvMmBubble_Init(Actor* thisx, PlayState* play) {
    NvMmBubble* this = (NvMmBubble*)thisx;

    Actor_SetScale(&this->actor, 1.0f);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    this->size = 0.0f;
    this->life = 99;
}

void NvMmBubble_Destroy(Actor* thisx, PlayState* play) {
    NvMmBubble* this = (NvMmBubble*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

/** z_nv_mm_deku.c: let it go (MM's func_8088A594 once the parent's gone). */
void NvMmBubble_Fire(Actor* thisx, PlayState* play) {
    NvMmBubble* this = (NvMmBubble*)thisx;

    if (this->fired) {
        return;
    }
    this->fired = true;
    this->actor.parent = NULL;
    if (this->size < 3.5f) {
        this->size = 3.5f;
    }
    this->actor.speedXZ = CLAMP(16.0f - this->size, 1.0f, 80.0f) * Math_CosS(this->actor.world.rot.x);
    this->actor.velocity.y = -CLAMP(16.0f - this->size, 1.0f, 80.0f) * Math_SinS(this->actor.world.rot.x);
    this->life = 99;
    Audio_PlayActorSound2(&this->actor, NA_SE_IT_SLING_SHOT); // (MM: NA_SE_PL_DEKUNUTS_FIRE)
}

/** z_nv_mm_deku.c: still being blown (not off on its own yet). */
s32 NvMmBubble_Blowing(Actor* thisx) {
    return !((NvMmBubble*)thisx)->fired;
}

static void NvMmBubble_Burst(NvMmBubble* this, PlayState* play) {
    SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EN_AWA_BREAK); // (MM: BUBLE_VANISH)
    Actor_Kill(&this->actor);
}

void NvMmBubble_Update(Actor* thisx, PlayState* play) {
    NvMmBubble* this = (NvMmBubble*)thisx;
    Player* player = GET_PLAYER(play);

    if (!this->fired) { // blown: at his mouth (placed by his head: z_nv_mm_deku.c), growing
        this->actor.world.rot.x = player->actor.focus.rot.x;
        if (Math_SmoothStepToF(&this->size, 16.0f, 0.07f, 1.8f, 0.0f) > 0.5f) {
            Actor_PlaySfx_Flagged(&this->actor, NA_SE_EN_BUBLE_UP - SFX_FLAG); // (MM: NA_SE_PL_DEKUNUTS_BUBLE_BREATH)
        } else if (++this->fullFor > 20) { // (full for 20 frames: off it goes)
            NvMmBubble_Fire(&this->actor, play);
        }
        return;
    }
    if ((--this->life <= 0) || (this->collider.base.atFlags & AT_HIT) ||
        (this->actor.bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_WALL | BGCHECKFLAG_CEILING))) {
        NvMmBubble_Burst(this, play);
        return;
    }
    Actor_MoveXZGravity(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, this->size, this->size, this->size,
                            7 /* (walls, floor, ceiling) */);
    this->collider.dim.radius = this->size * 1.5f;
    this->collider.dim.height = this->size * 3.0f;
    this->collider.dim.yShift = -this->size * 1.5f;
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
}

/** MM's EnArrow_Draw for the bubble: blown, translucent and facing the camera; flying, solid. */
void NvMmBubble_Draw(Actor* thisx, PlayState* play) {
    NvMmBubble* this = (NvMmBubble*)thisx;
    s32 alpha = 255 - (s32)(this->size * 4.0f);
    f32 stretch = (this->actor.speedXZ * 0.1f) + 1.0f;
    f32 squash = 1.0f / stretch;

    if (!this->fired && (this->size <= 0.0f)) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    squash *= 0.002f;
    stretch *= 0.002f;
    Matrix_Scale(this->size * squash, this->size * squash, this->size * stretch, MTXMODE_APPLY);
    Matrix_Translate(0.0f, 0.0f, 460.0f, MTXMODE_APPLY);
    if (!this->fired) { // (blown: translucent, facing the camera)
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gDPSetCombineLERP(POLY_XLU_DISP++, PRIMITIVE, 0, SHADE, 0, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, SHADE, 0, 0, 0, 0,
                          PRIMITIVE);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0x7F, 230, 225, 150, alpha / 2);
        Matrix_ReplaceRotation(&play->billboardMtxF);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, KEEP("gameplay_keep_DL_06F9F0"));
    } else { // (flying: solid)
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gDPSetCombineLERP(POLY_OPA_DISP++, PRIMITIVE, 0, SHADE, 0, 0, 0, 0, PRIMITIVE, PRIMITIVE, 0, SHADE, 0, 0, 0, 0,
                          PRIMITIVE);
        gDPSetPrimColor(POLY_OPA_DISP++, 0, 0x7F, 230, 225, 150, 255);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, KEEP("gameplay_keep_DL_06FAE0"));
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
