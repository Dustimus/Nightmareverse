/*
 * Nightmareverse: the Deku flowers, from MM's Obj_Etcetera (zeldaret/mm, ovl_Obj_Etcetera), ported
 * as it is (mm/README.md "Deku Link's moves"): MM's models from gameplay_keep (the pink and the gold
 * flower, at rest one display list, moving a skeleton), MM's two animations (the rustle when he walks
 * on or off, the bounce when he launches), MM's collision (a dynapoly: he stands on it). Deku Link
 * dives into it and launches out (z_nv_mm_deku.c): it asks him whether he's launching or hovering
 * above (MM's PLAYER_STATE3_200 and 2000) and how deep he is (MM's player->unk_ABC).
 *
 * Adapted from MM: the flower's floor light setting (MM's Environment_ChangeLightSetting while he's
 * in it) is left out.
 */
#include "global.h"

#define KEEP(name) ("__OTR__mm/objects/gameplay_keep/" name)

#define DEKU_FLOWER_TYPE(thisx) (((thisx)->params & 0xFF80) >> 7)
enum {
    DEKU_FLOWER_TYPE_PINK,
    DEKU_FLOWER_TYPE_PINK_WITH_INITIAL_BOUNCE,
    DEKU_FLOWER_TYPE_GOLD,
    DEKU_FLOWER_TYPE_GOLD_WITH_INITIAL_BOUNCE,
    DEKU_FLOWER_TYPE_MAX
};
#define DEKU_FLOWER_LIMB_MAX 11 // (MM: the pink's 11 with its root, the gold's the same)

struct NvMmEtcetera;
typedef void (*NvMmEtceteraFunc)(struct NvMmEtcetera*, PlayState*);

typedef struct NvMmEtcetera {
    DynaPolyActor dyna;
    SkelAnime skelAnime;
    ColliderCylinder collider;
    Vec3s jointTable[DEKU_FLOWER_LIMB_MAX];
    Vec3s morphTable[DEKU_FLOWER_LIMB_MAX];
    f32 bounceOscillationScale;
    s16 oscillationTimer;
    u16 burrowFlag;
    Gfx* dList;
    s32 animated;
    NvMmEtceteraFunc actionFunc;
} NvMmEtcetera;

const size_t gNvMmEtceteraSize = sizeof(NvMmEtcetera);

// Deku Link (z_nv_mm_deku.c): MM's PLAYER_STATE3_200 (launching), 2000 (hovering), unk_ABC, the
// speed, and whether he's at the very start of burrowing (MM: unk_ABC < 0, not yet STATE3_100)
s32 NvMmDeku_Launching(void);
s32 NvMmDeku_Hovering(void);
s32 NvMmDeku_StartingBurrow(void);

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_NONE, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0x01000202, 0x00, 0x00 }, TOUCH_NONE | TOUCH_SFX_NORMAL, BUMP_ON,
      OCELEM_NONE },
    { 20, 14, 0, { 0, 0, 0 } },
};

static f32 sOscillationTable[] = {
    -1.0f, -1.0f, -1.0f, -0.7f, 0.0f, 0.7f, 1.0f, 0.7f, 0.0f, -0.7f, -1.0f, -0.7f, 0.0f, 0.7f, 1.0f, 0.7f, 0.0f, -0.7f,
};

static void NvMmEtcetera_Setup(NvMmEtcetera* this, PlayState* play);
static void NvMmEtcetera_Idle(NvMmEtcetera* this, PlayState* play);
static void NvMmEtcetera_PlayRustleAnimation(NvMmEtcetera* this, PlayState* play);
static void NvMmEtcetera_DoBounceOscillation(NvMmEtcetera* this, PlayState* play);
void NvMmEtcetera_Draw(Actor* thisx, PlayState* play);

#define DRAW_ANIMATED(this) ((this)->animated) // (MM swaps the draw function: here a flag)

void NvMmEtcetera_Init(Actor* thisx, PlayState* play) {
    NvMmEtcetera* this = (NvMmEtcetera*)thisx;
    s32 floorBgId;
    Vec3f pos;

    pos.x = this->dyna.actor.world.pos.x;
    pos.y = this->dyna.actor.world.pos.y + 10.0f;
    pos.z = this->dyna.actor.world.pos.z;
    BgCheck_EntityRaycastFloor5(play, &play->colCtx, &this->dyna.actor.floorPoly, &floorBgId, &this->dyna.actor, &pos);
    this->dyna.actor.floorBgId = floorBgId;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->dyna.actor, &sCylinderInit);
    Collider_UpdateCylinder(&this->dyna.actor, &this->collider);
    this->actionFunc = NvMmEtcetera_Setup;
    Actor_SetScale(&this->dyna.actor, 0.01f);
    this->dyna.actor.scale.y = 0.02f;
    this->burrowFlag = 0;
    DRAW_ANIMATED(this) = 0;
}

void NvMmEtcetera_Destroy(Actor* thisx, PlayState* play) {
    NvMmEtcetera* this = (NvMmEtcetera*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
    Collider_DestroyCylinder(play, &this->collider);
}

static void NvMmEtcetera_DoNormalOscillation(NvMmEtcetera* this, PlayState* play) {
    if (this->oscillationTimer > 0) {
        Actor_SetScale(&this->dyna.actor,
                       (sOscillationTable[play->gameplayFrames % 18] * (0.0001f * this->oscillationTimer)) + 0.01f);
        this->dyna.actor.scale.y = 0.02f;
        this->oscillationTimer--;
    } else {
        Actor_SetScale(&this->dyna.actor, 0.01f);
        this->dyna.actor.scale.y = 0.02f;
    }
}

static void NvMmEtcetera_StartRustleAnimation(NvMmEtcetera* this) {
    Animation_Change(&this->skelAnime, (AnimationHeader*)KEEP("gDekuFlowerRustleAnim"), 1.0f, 0.0f,
                     Animation_GetLastFrame(KEEP("gDekuFlowerRustleAnim")), ANIMMODE_ONCE, 0.0f);
    DRAW_ANIMATED(this) = 1;
    this->actionFunc = NvMmEtcetera_PlayRustleAnimation;
}

static void NvMmEtcetera_Idle(NvMmEtcetera* this, PlayState* play) {
    Player* player = GET_PLAYER(play);
    s16 minOscillationTimer;

    if (NvMmDeku_Launching() && (this->dyna.actor.xzDistToPlayer < 20.0f)) { // launching out of it
        Animation_Change(&this->skelAnime, (AnimationHeader*)KEEP("gDekuFlowerBounceAnim"), 1.0f, 0.0f,
                         Animation_GetLastFrame(KEEP("gDekuFlowerBounceAnim")), ANIMMODE_ONCE, 0.0f);
        DRAW_ANIMATED(this) = 1;
        this->actionFunc = NvMmEtcetera_DoBounceOscillation;
        Actor_SetScale(&this->dyna.actor, 0.01f);
        this->dyna.actor.scale.y = 0.02f;
        this->bounceOscillationScale = 0.003f;
        this->oscillationTimer = 30;
        this->burrowFlag &= ~1;
    } else if (NvMmDeku_Hovering() && (this->dyna.actor.xzDistToPlayer < 30.0f) &&
               (this->dyna.actor.yDistToPlayer > 0.0f)) { // hovering above it
        minOscillationTimer = 10 - (s32)(this->dyna.actor.yDistToPlayer * 0.05f);
        if (this->oscillationTimer < minOscillationTimer) {
            this->oscillationTimer = minOscillationTimer;
        }
    } else {
        if (DynaPolyActor_IsPlayerOnTop(&this->dyna)) {
            if (!(this->burrowFlag & 1)) { // walking on, or falling onto it
                this->oscillationTimer = 10;
                NvMmEtcetera_StartRustleAnimation(this);
            } else if ((player->actor.speedXZ > 0.1f) || NvMmDeku_StartingBurrow()) {
                this->oscillationTimer = 10;
            }
            this->burrowFlag |= 1;
        } else {
            if (this->burrowFlag & 1) { // walking off
                this->oscillationTimer = 10;
                NvMmEtcetera_StartRustleAnimation(this);
            }
            this->burrowFlag &= ~1;
        }
    }
    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        this->oscillationTimer = 10;
        NvMmEtcetera_StartRustleAnimation(this);
    }
    NvMmEtcetera_DoNormalOscillation(this, play);
}

static void NvMmEtcetera_PlayRustleAnimation(NvMmEtcetera* this, PlayState* play) {
    if (DynaPolyActor_IsPlayerOnTop(&this->dyna)) {
        this->burrowFlag |= 1;
    } else {
        this->burrowFlag &= ~1;
    }
    if (SkelAnime_Update(&this->skelAnime)) {
        DRAW_ANIMATED(this) = 0;
        this->actionFunc = NvMmEtcetera_Idle;
    }
    NvMmEtcetera_DoNormalOscillation(this, play);
}

static void NvMmEtcetera_DoBounceOscillation(NvMmEtcetera* this, PlayState* play) {
    f32 scaleTemp;

    if (DynaPolyActor_IsPlayerOnTop(&this->dyna)) {
        this->burrowFlag |= 1;
    } else {
        this->burrowFlag &= ~1;
    }
    SkelAnime_Update(&this->skelAnime);
    if (this->oscillationTimer > 0) {
        this->oscillationTimer--;
    } else {
        DRAW_ANIMATED(this) = 0;
        this->actionFunc = NvMmEtcetera_Idle;
        Actor_SetScale(&this->dyna.actor, 0.01f);
        this->dyna.actor.scale.y = 0.02f;
        this->oscillationTimer = 0;
        this->bounceOscillationScale = 0.0f;
        return;
    }
    this->bounceOscillationScale *= 0.8f;
    this->bounceOscillationScale -= (this->dyna.actor.scale.x - 0.01f) * 0.4f;
    scaleTemp = this->dyna.actor.scale.x + this->bounceOscillationScale;
    Actor_SetScale(&this->dyna.actor, scaleTemp);
    this->dyna.actor.scale.y = 2.0f * scaleTemp;
}

static void NvMmEtcetera_Setup(NvMmEtcetera* this, PlayState* play) {
    CollisionHeader* colHeader = NULL;
    s32 type = DEKU_FLOWER_TYPE(&this->dyna.actor);
    s32 gold;

    if ((type < DEKU_FLOWER_TYPE_PINK) || (type >= DEKU_FLOWER_TYPE_MAX)) {
        type = DEKU_FLOWER_TYPE_PINK;
    }
    gold = (type >= DEKU_FLOWER_TYPE_GOLD);
    DynaPolyActor_Init(&this->dyna, DPM_PLAYER);
    CollisionHeader_GetVirtual(gold ? KEEP("gGoldDekuFlowerCol") : KEEP("gPinkDekuFlowerCol"), &colHeader);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, &this->dyna.actor, colHeader);
    if (!gold) {
        SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)KEEP("gPinkDekuFlowerSkel"),
                       (AnimationHeader*)KEEP("gDekuFlowerBounceAnim"), this->jointTable, this->morphTable,
                       DEKU_FLOWER_LIMB_MAX);
        this->dList = (Gfx*)KEEP("gPinkDekuFlowerIdleDL");
    } else {
        this->dList = (Gfx*)KEEP("gGoldDekuFlowerIdleDL");
        SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)KEEP("gGoldDekuFlowerSkel"),
                       (AnimationHeader*)KEEP("gDekuFlowerBounceAnim"), this->jointTable, this->morphTable,
                       DEKU_FLOWER_LIMB_MAX);
        this->collider.dim.height = 20;
    }
    this->dyna.actor.focus.pos.y = this->dyna.actor.home.pos.y + 10.0f;
    this->dyna.actor.targetMode = 3;
    if ((type == DEKU_FLOWER_TYPE_PINK) || (type == DEKU_FLOWER_TYPE_GOLD)) {
        DRAW_ANIMATED(this) = 0;
        this->actionFunc = NvMmEtcetera_Idle;
        Actor_SetScale(&this->dyna.actor, 0.01f);
        this->dyna.actor.scale.y = 0.02f;
    } else { // (MM: a Mad Scrub's flower, bouncing up as it appears)
        Animation_Change(&this->skelAnime, (AnimationHeader*)KEEP("gDekuFlowerBounceAnim"), 1.0f, 0.0f,
                         Animation_GetLastFrame(KEEP("gDekuFlowerBounceAnim")), ANIMMODE_ONCE, 0.0f);
        DRAW_ANIMATED(this) = 1;
        this->actionFunc = NvMmEtcetera_DoBounceOscillation;
        Actor_SetScale(&this->dyna.actor, 0.0f);
        this->oscillationTimer = 30;
        this->bounceOscillationScale = 0.0f;
    }
}

void NvMmEtcetera_Update(Actor* thisx, PlayState* play) {
    NvMmEtcetera* this = (NvMmEtcetera*)thisx;

    this->actionFunc(this, play);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
}

/** z_nv_mm_deku.c: whether an actor (a dynapoly Link stands on) is a Deku flower, and a gold one. */
s32 NvMmEtcetera_IsFlower(Actor* actor, s32* gold) {
    if ((actor == NULL) || (actor->update != NvMmEtcetera_Update)) {
        return false;
    }
    if (gold != NULL) {
        *gold = (actor->params & 0x100) != 0; // (MM: params & 0x100, the gold ones)
    }
    return true;
}

/** MM's ObjEtcetera_DrawIdle and ObjEtcetera_DrawAnimated. */
void NvMmEtcetera_Draw(Actor* thisx, PlayState* play) {
    NvMmEtcetera* this = (NvMmEtcetera*)thisx;

    if (this->actionFunc == NvMmEtcetera_Setup) {
        return;
    }
    if (!DRAW_ANIMATED(this)) {
        OPEN_DISPS(play->state.gfxCtx);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gSPDisplayList(POLY_OPA_DISP++, this->dList);
        CLOSE_DISPS(play->state.gfxCtx);
    } else {
        Gfx_SetupDL_37Opa(play->state.gfxCtx);
        SkelAnime_DrawOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, NULL, NULL, &this->dyna.actor);
    }
}
