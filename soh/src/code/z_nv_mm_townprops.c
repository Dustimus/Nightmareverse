/*
 * Nightmareverse: Clock Town's small things, MM's own, ported closely (mm/README.md "Clock Town's bells,
 * the mannequin and Mutoh"):
 *   En_Cha (0x270)    the Laundry Pool's bell: rung by a hit (or by week event 60_04, set by others),
 *                     swinging; the first ring sets week event 51_04. Not yet: MM's cutscene on that
 *                     first ring (it waits for Kafei, En_Test3).
 *   Obj_Bell (0x14E)  East Clock Town's big bell: swung by hits, harder by Goron punches and harder
 *                     still by explosions (MM's damage effects 15 and 14, here OoT's nearest kinds:
 *                     the hammer's, explosives'), knocking Link back when it swings into him.
 *   En_Drs (0x2A1)    Anju's wedding dress on its mannequin, the Moon Mask on top (until week event
 *                     87_02, the Couple's Mask scene).
 *   En_Muto (0x26B)   Mutoh, the carpenters' boss: in South Clock Town on the last night (params 0), MM's lines, Deku
 *                     Link's and Kafei's Mask's. In the mayor's room (params 1) gone once the meeting is resolved or on the
 *                     last night; there he stands for the meeting (MM's
 *                     lines by its state); the meeting's own scene (Dm_..., En_Baisen, the Couple's
 *                     Mask) is not yet in, so there he only talks.
 * MM's models (tools/mm_objects.py: object_cha, object_f52_obj, object_drs, object_msmo, object_toryo).
 */
#include "global.h"
#include "z_nv_mm_meeting.h"
#include <stdio.h>
#include <stdlib.h>

#define OBJ(o, name) ("__OTR__mm/objects/" o "/" name)
#define NV_MM_TEXT(id) (0xB000 + (id))
#define MM_WE(byte, bit) (((byte) << 8) | (bit))
s32 NvMm_CheckWeekEvent(s32 flag);
void NvMm_SetWeekEvent(s32 flag);
void NvMm_ClearWeekEvent(s32 flag);
s32 NvMmMask_Worn(void);
s32 NvMmForm_Get(void); // 0 FD, 1 Goron, 2 Zora, 3 Deku, 4 human
s32 NvMmClock_IsNight(void);
s32 NvMmClock_Day(void);

#define MM_PLAYER_MASK_KAFEIS_MASK 0x02
#define MM_PLAYER_MASK_COUPLE 0x0A

static void NvMmTown_Dl(PlayState* play, const char* dl) {
    OPEN_DISPS(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dl);
    CLOSE_DISPS(play->state.gfxCtx);
}

/* ---- En_Cha: the Laundry Pool's bell ---- */

#define CHA(name) OBJ("object_cha", name)
#define WE_60_04 MM_WE(60, 0x04)                     // rung by someone else
#define WE_HIT_LAUNDRY_POOL_BELL MM_WE(51, 0x04)     // MM's WEEKEVENTREG_HIT_LAUNDRY_POOL_BELL

typedef struct {
    Actor actor;
    ColliderCylinder collider;
} NvMmCha;

const size_t gNvMmChaSize = sizeof(NvMmCha);

static ColliderCylinderInit sChaCylinderInit = {
    { COLTYPE_WOOD, AT_NONE, AC_ON | AC_HARD | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xF7CFFFFF, 0x00, 0x00 }, TOUCH_NONE | TOUCH_SFX_NORMAL, BUMP_ON,
      OCELEM_ON },
    { 10, 40, 0, { 0, 0, 0 } },
};

void NvMmCha_Init(Actor* thisx, PlayState* play) {
    NvMmCha* this = (NvMmCha*)thisx;

    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sChaCylinderInit);
    thisx->colChkInfo.mass = MASS_IMMOVABLE;
    Collider_UpdateCylinder(thisx, &this->collider);
    Actor_SetScale(thisx, 0.01f);
    thisx->home.rot.z = 0; // (MM: home.rot.z its swing's speed, home.rot.x its angle)
    thisx->home.rot.x = 0;
    NvMm_ClearWeekEvent(WE_60_04);
}

void NvMmCha_Destroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((NvMmCha*)thisx)->collider);
}

/** MM's EnCha_Idle (and EnCha_Update). */
void NvMmCha_Update(Actor* thisx, PlayState* play) {
    NvMmCha* this = (NvMmCha*)thisx;

    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    if (NvMm_CheckWeekEvent(WE_60_04)) {
        Audio_PlayActorSound2(thisx, NA_SE_EV_SHIP_BELL); // (MM's NA_SE_EV_DOOR_BELL: OoT has none, its ship bell)
        NvMm_ClearWeekEvent(WE_60_04);
        thisx->home.rot.z = 0x7D0;
    }
    if (this->collider.base.acFlags & AC_HIT) {
        this->collider.base.acFlags &= ~AC_HIT;
        Audio_PlayActorSound2(thisx, NA_SE_EV_SHIP_BELL);
        thisx->home.rot.z = 0x7D0;
        if (!NvMm_CheckWeekEvent(WE_HIT_LAUNDRY_POOL_BELL)) {
            NvMm_SetWeekEvent(WE_HIT_LAUNDRY_POOL_BELL);
        }
    }
    thisx->home.rot.x += thisx->home.rot.z;
    thisx->home.rot.z -= (s16)(thisx->home.rot.x * 0.1f);
    thisx->home.rot.z *= 0.96f;
    if ((thisx->shape.rot.z > -0x1F40) && (thisx->shape.rot.z < 0x1F40)) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
}

void NvMmCha_Draw(Actor* thisx, PlayState* play) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    CLOSE_DISPS(play->state.gfxCtx);
    NvMmTown_Dl(play, CHA("object_cha_DL_000710"));
    Matrix_Translate(-1094.0f, 4950.0f, 9.0f, MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD(thisx->home.rot.x), MTXMODE_APPLY);
    NvMmTown_Dl(play, CHA("object_cha_DL_000958"));
}

/* ---- Obj_Bell: East Clock Town's big bell ---- */

#define F52(name) OBJ("object_f52_obj", name)

typedef struct {
    DynaPolyActor dyna;
    ColliderJntSph collider1; // (MM's spheres: OoT has jointed spheres, one element each)
    ColliderJntSphElement elements1[1];
    ColliderJntSph collider2;
    ColliderJntSphElement elements2[1];
    s16 swingPhase;  // MM's unk_20C
    s16 acTimer;     // MM's unk_20E
    s16 swingYaw;    // MM's unk_212
    s16 pushTimer;   // MM's unk_214
    f32 swingSpeed;  // MM's unk_21C
    f32 swingAngle;  // MM's unk_220
} NvMmBell;

const size_t gNvMmBellSize = sizeof(NvMmBell);

static ColliderJntSphElementInit sBellPostElement[1] = {
    { { ELEMTYPE_UNK2, { 0x20000000, 0x00, 0x04 }, { 0x00000000, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_NORMAL,
        BUMP_NONE, OCELEM_ON },
      { 0, { { 0, 960, 0 }, 70 }, 100 } },
};
static ColliderJntSphInit sBellPostInit = {
    { COLTYPE_METAL, AT_ON | AT_TYPE_ENEMY, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_JNTSPH },
    1, sBellPostElement,
};
static ColliderJntSphElementInit sBellElement[1] = {
    { { ELEMTYPE_UNK2, { 0x00000000, 0x00, 0x00 }, { 0xF7CFFFFF, 0x00, 0x00 }, TOUCH_NONE | TOUCH_SFX_NORMAL,
        BUMP_ON, OCELEM_NONE },
      { 0, { { 0, 1100, 0 }, 74 }, 100 } },
};
static ColliderJntSphInit sBellInit = {
    { COLTYPE_METAL, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_NONE, OC2_TYPE_1, COLSHAPE_JNTSPH },
    1, sBellElement,
};

/** MM's func_80A35510: a hit's push (0 a plain hit, 1 a Goron punch, 2 an explosion). */
static s32 NvMmBell_Push(NvMmBell* this, s32 kind) {
    s32 pushed = ((kind == 0) && (this->swingSpeed < 1000.0f)) || ((kind == 1) && (this->swingSpeed < 4000.0f)) ||
                 (kind == 2);

    switch (kind) {
        case 0:
            this->swingSpeed += (this->swingSpeed > 1000.0f) ? 250.0f : 1000.0f;
            break;
        case 1:
            this->swingSpeed += (this->swingSpeed > 3000.0f) ? 750.0f : 3000.0f;
            break;
        default:
            this->swingSpeed += 9000.0f;
            break;
    }
    this->swingSpeed = CLAMP(this->swingSpeed, 0.0f, 18000.0f);
    if (pushed) {
        Vec3f hitPos;
        Vec3s* h = &this->elements2[0].info.bumper.hitPos;

        hitPos.x = h->x;
        hitPos.y = h->y;
        hitPos.z = h->z;
        this->dyna.actor.world.rot.y = Math_Vec3f_Yaw(&hitPos, &this->dyna.actor.world.pos);
        if ((this->swingPhase <= 0x4000) && (this->swingPhase >= -0x4000)) {
            this->swingPhase -= 0x4000;
        } else {
            this->swingPhase += 0x4000;
        }
    }
    return pushed;
}

/** MM's func_80A356D8: the swing. */
static void NvMmBell_Swing(NvMmBell* this) {
    f32 angle;

    this->swingYaw = this->dyna.actor.world.rot.y;
    if ((this->swingPhase >= 0x4000) || (this->swingPhase <= -0x4000)) {
        this->swingYaw -= 0x8000;
    }
    angle = Math_SinS(this->swingPhase) * this->swingSpeed;
    this->dyna.actor.world.rot.x = this->dyna.actor.home.rot.x;
    this->swingAngle = angle;
    this->dyna.actor.world.rot.x += (s16)angle;
    Math_ApproachF(&this->swingSpeed, 0.0f, 0.03f, 70.0f);
    if (this->swingSpeed > 0.0f) {
        this->swingPhase -= 0x800;
    }
}

/** MM's func_80A357A8: swinging into Link, and being hit. */
static void NvMmBell_Collide(NvMmBell* this, PlayState* play) {
    if (this->collider1.base.ocFlags1 & OC1_HIT) {
        s16 yaw = this->dyna.actor.yawTowardsPlayer - this->swingYaw;
        f32 f = this->swingSpeed / 18000.0f;

        this->collider1.base.ocFlags1 &= ~OC1_HIT;
        if ((ABS(yaw) < 0x3FFC) && (this->pushTimer == 0) && (f > 0.18f)) {
            Actor_SetPlayerKnockbackLargeNoDamage(play, &this->dyna.actor, 8.0f * f, this->dyna.actor.yawTowardsPlayer, 11.0f * f);
            this->pushTimer = 30;
        }
    }
    if (this->collider2.base.acFlags & AC_HIT) {
        ColliderInfo* hitBy = this->elements2[0].info.acHitInfo;
        u32 dmg = (hitBy != NULL) ? hitBy->toucher.dmgFlags : 0;

        this->collider2.base.acFlags &= ~AC_HIT;
        this->acTimer = 10;
        if (getenv("NV_LOG") != NULL) {
            printf("NVMM bell hit dmg=%08X speed=%d\n", (unsigned)dmg, (int)this->swingSpeed);
            fflush(stdout);
        }
        if (dmg & DMG_EXPLOSIVE) { // (MM's damage effect 14: explosives, the Powder Keg)
            Audio_PlayActorSound2(&this->dyna.actor, NA_SE_EV_SHIP_BELL);
            NvMmBell_Push(this, 2);
        } else if (dmg & (DMG_HAMMER_SWING | DMG_HAMMER_JUMP)) { // (15: the Goron's punch and spikes)
            Audio_PlayActorSound2(&this->dyna.actor, NA_SE_EV_SHIP_BELL);
            NvMmBell_Push(this, 1);
        } else {
            NvMmBell_Push(this, 0);
        }
    }
}

void NvMmBell_Init(Actor* thisx, PlayState* play) {
    NvMmBell* this = (NvMmBell*)thisx;
    CollisionHeader* colHeader = NULL;

    DynaPolyActor_Init(&this->dyna, 0);
    CollisionHeader_GetVirtual((void*)F52("object_f52_obj_Colheader_001BA8"), &colHeader);
    this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, colHeader);
    Actor_SetScale(thisx, 0.08f);
    Collider_InitJntSph(play, &this->collider1);
    Collider_SetJntSph(play, &this->collider1, thisx, &sBellPostInit, this->elements1);
    Collider_InitJntSph(play, &this->collider2);
    Collider_SetJntSph(play, &this->collider2, thisx, &sBellInit, this->elements2);
    thisx->colChkInfo.mass = MASS_IMMOVABLE;
}

void NvMmBell_Destroy(Actor* thisx, PlayState* play) {
    NvMmBell* this = (NvMmBell*)thisx;

    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
    Collider_DestroyJntSph(play, &this->collider1);
    Collider_DestroyJntSph(play, &this->collider2);
}

void NvMmBell_Update(Actor* thisx, PlayState* play) {
    NvMmBell* this = (NvMmBell*)thisx;
    ColliderJntSphElementDim* d1 = &this->elements1[0].dim;
    ColliderJntSphElementDim* d2 = &this->elements2[0].dim;

    if (this->pushTimer != 0) {
        this->pushTimer--;
    }
    NvMmBell_Collide(this, play);
    NvMmBell_Swing(this);
    // MM's func_80A358FC
    d1->worldSphere.radius = d1->modelSphere.radius * d1->scale;
    d2->worldSphere.radius = d2->modelSphere.radius * d2->scale;
    if (DECR(this->acTimer) == 0) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider2.base);
    }
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider1.base);
}

static void NvMmBell_SphereToWorld(ColliderJntSphElementDim* d) {
    Vec3f model;
    Vec3f world;

    model.x = d->modelSphere.center.x;
    model.y = d->modelSphere.center.y;
    model.z = d->modelSphere.center.z;
    Matrix_MultVec3f(&model, &world);
    d->worldSphere.center.x = world.x;
    d->worldSphere.center.y = world.y;
    d->worldSphere.center.z = world.z;
}

void NvMmBell_Draw(Actor* thisx, PlayState* play) {
    NvMmBell* this = (NvMmBell*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    // MM's func_80A35B18: the post
    Matrix_Translate(thisx->world.pos.x, thisx->world.pos.y, thisx->world.pos.z, MTXMODE_NEW);
    Matrix_Scale(thisx->scale.x, thisx->scale.y, thisx->scale.z, MTXMODE_APPLY);
    Matrix_RotateY(BINANG_TO_RAD(thisx->shape.rot.y), MTXMODE_APPLY);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)F52("object_f52_obj_DL_000570"));
    // MM's func_80A35BD4: the shadow
    Matrix_Translate(thisx->world.pos.x, thisx->world.pos.y - 4.0f, thisx->world.pos.z, MTXMODE_NEW);
    Matrix_Scale(thisx->scale.x, thisx->scale.y, thisx->scale.z, MTXMODE_APPLY);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, (Gfx*)F52("object_f52_obj_DL_000840"));
    // MM's func_80A359B4: the bell, swinging about its hook
    Matrix_Translate(thisx->world.pos.x, thisx->world.pos.y, thisx->world.pos.z, MTXMODE_NEW);
    Matrix_Scale(thisx->scale.x, thisx->scale.y, thisx->scale.z, MTXMODE_APPLY);
    Matrix_Translate(0.0f, 2600.0f, 0.0f, MTXMODE_APPLY);
    Matrix_RotateY(BINANG_TO_RAD(thisx->world.rot.y), MTXMODE_APPLY);
    Matrix_RotateX(BINANG_TO_RAD(thisx->world.rot.x), MTXMODE_APPLY);
    Matrix_RotateY(BINANG_TO_RAD(-thisx->world.rot.y), MTXMODE_APPLY);
    Matrix_Translate(0.0f, -2600.0f, 0.0f, MTXMODE_APPLY);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)F52("object_f52_obj_DL_000698"));
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)F52("object_f52_obj_DL_0008D0"));
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)F52("object_f52_obj_DL_000960"));
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)F52("object_f52_obj_DL_0007A8"));
    CLOSE_DISPS(play->state.gfxCtx);
    // the spheres follow the swinging bell (MM's, from the bell's matrix)
    NvMmBell_SphereToWorld(&this->elements1[0].dim);
    NvMmBell_SphereToWorld(&this->elements2[0].dim);
}

/* ---- En_Drs: the wedding dress's mannequin ---- */

#define DRS(name) OBJ("object_drs", name)
#define WEDDING_DRESS_MANNEQUIN_LIMB_MASK 2
#define WEDDING_DRESS_MANNEQUIN_LIMB_MAX 4
#define WE_COUPLES_MASK_CUTSCENE_STARTED MM_WE(87, 0x02)

typedef struct {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[WEDDING_DRESS_MANNEQUIN_LIMB_MAX];
    Vec3s morphTable[WEDDING_DRESS_MANNEQUIN_LIMB_MAX];
    ColliderCylinder collider;
} NvMmDrs;

const size_t gNvMmDrsSize = sizeof(NvMmDrs);

static ColliderCylinderInit sDrsCylinderInit = {
    { COLTYPE_HIT1, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK1, { 0x00000000, 0x00, 0x00 }, { 0x00000000, 0x00, 0x00 }, TOUCH_NONE | TOUCH_SFX_NORMAL,
      BUMP_NONE, OCELEM_ON },
    { 16, 62, 0, { 0, 0, 0 } },
};

void NvMmDrs_Init(Actor* thisx, PlayState* play) {
    NvMmDrs* this = (NvMmDrs*)thisx;
    AnimationHeader* anim = (AnimationHeader*)DRS("gWeddingDressMannequinIdleAnim");

    ActorShape_Init(&thisx->shape, 0.0f, NULL, 0.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)DRS("gWeddingDressMannequinSkel"), NULL,
                       this->jointTable, this->morphTable, WEDDING_DRESS_MANNEQUIN_LIMB_MAX);
    Animation_Change(&this->skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim), ANIMMODE_LOOP, 0.0f);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sDrsCylinderInit);
    thisx->colChkInfo.mass = MASS_IMMOVABLE;
    Actor_SetScale(thisx, 0.01f);
}

void NvMmDrs_Destroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((NvMmDrs*)thisx)->collider);
}

void NvMmDrs_Update(Actor* thisx, PlayState* play) {
    NvMmDrs* this = (NvMmDrs*)thisx;

    SkelAnime_Update(&this->skelAnime);
    Collider_UpdateCylinder(thisx, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static void NvMmDrs_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    // Anju takes the Moon Mask off at the start of the Couple's Mask scene (MM's EnDrs_PostLimbDraw)
    if ((limbIndex == WEDDING_DRESS_MANNEQUIN_LIMB_MASK) && !NvMm_CheckWeekEvent(WE_COUPLES_MASK_CUTSCENE_STARTED)) {
        OPEN_DISPS(play->state.gfxCtx);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)OBJ("object_msmo", "gMoonMaskDL"));
        CLOSE_DISPS(play->state.gfxCtx);
    }
}

void NvMmDrs_Draw(Actor* thisx, PlayState* play) {
    NvMmDrs* this = (NvMmDrs*)thisx;

    Gfx_SetupDL_37Opa(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount, NULL,
                          NvMmDrs_PostLimbDraw, this);
}

/* ---- En_Muto: Mutoh ---- */

#define TORYO(name) OBJ("object_toryo", name)
#define OBJECT_TORYO_LIMB_01 1
#define OBJECT_TORYO_LIMB_0F 15
#define OBJECT_TORYO_LIMB_MAX 17
#define WE_60_80 MM_WE(60, 0x80)                         // heard his "the carnival's going ahead"
#define WE_88_08 MM_WE(88, 0x08)                         // Deku Link's talk heard
#define WE_ATTENDED_MAYOR_MEETING MM_WE(60, 0x08)
#define WE_RESOLVED_MAYOR_MEETING MM_WE(63, 0x80)

static u16 sMutoTextIds[] = { 0x2ABD, 0x2ABB, 0x0624, 0x0623, 0x2AC6 };

struct NvMmMuto;
typedef void (*NvMmMutoFunc)(struct NvMmMuto*, PlayState*);

typedef struct NvMmMuto {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[OBJECT_TORYO_LIMB_MAX];
    Vec3s morphTable[OBJECT_TORYO_LIMB_MAX];
    NvMmMutoFunc actionFunc;
    Vec3s headRot;
    Vec3s headRotTarget;
    s16 textIdIndex;
    s16 yawTowardsTarget;
    s32 isInMayorsRoom;
    ColliderCylinder collider;
    NvMmMeetingRole role; // MM's cutsceneState, targetActor (the Mayor's meeting: z_nv_mm_mayor.c)
    s32 shouldSetHeadRotation;
} NvMmMuto;

NvMmMeetingRole* NvMmMuto_Role(Actor* muto) {
    return &((NvMmMuto*)muto)->role;
}

const size_t gNvMmMutoSize = sizeof(NvMmMuto);

static ColliderCylinderInit sMutoCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xF7CFFFFF, 0x00, 0x00 }, TOUCH_NONE | TOUCH_SFX_NORMAL,
      BUMP_NONE, OCELEM_ON },
    { 20, 60, 0, { 0, 0, 0 } },
};

static void NvMmMuto_Idle(NvMmMuto* this, PlayState* play);
static void NvMmMuto_Talk(NvMmMuto* this, PlayState* play);

static s32 NvMmMuto_LastNight(void) {
    return (NvMmClock_Day() == 3) && NvMmClock_IsNight();
}

static void NvMmMuto_SetupIdle(NvMmMuto* this) {
    AnimationHeader* anim = (AnimationHeader*)TORYO("object_toryo_Anim_000E50");

    Animation_Change(&this->skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim), ANIMMODE_LOOP, -4.0f);
    this->actionFunc = NvMmMuto_Idle;
}

void NvMmMuto_Init(Actor* thisx, PlayState* play) {
    NvMmMuto* this = (NvMmMuto*)thisx;

    thisx->colChkInfo.mass = MASS_IMMOVABLE;
    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 40.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)TORYO("object_toryo_Skel_007150"),
                       (AnimationHeader*)TORYO("object_toryo_Anim_000E50"), this->jointTable, this->morphTable,
                       OBJECT_TORYO_LIMB_MAX);
    this->isInMayorsRoom = thisx->params;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sMutoCylinderInit);
    if (!this->isInMayorsRoom) {
        this->textIdIndex = NvMm_CheckWeekEvent(WE_60_80) ? 3 : 2;
        if (!NvMmMuto_LastNight()) { // (out in town only on the last night: "the moon won't fall")
            Actor_Kill(thisx);
            return;
        }
    } else {
        this->collider.dim.radius = 30;
        if (NvMm_CheckWeekEvent(WE_RESOLVED_MAYOR_MEETING) || NvMmMuto_LastNight()) {
            Actor_Kill(thisx);
            return;
        }
    }
    thisx->targetMode = 6;
    thisx->gravity = -3.0f;
    NvMmMuto_SetupIdle(this);
}

void NvMmMuto_Destroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((NvMmMuto*)thisx)->collider);
}

/** MM's EnMuto_InDialogue, in the Mayor's room: looking at whoever speaks while the mayor leads. */
static void NvMmMuto_InMeeting(NvMmMuto* this, PlayState* play) {
    u16 cur = play->msgCtx.textId;

    if ((this->role.targetActor == &this->actor) || (this->role.targetActor == NULL)) {
        this->yawTowardsTarget = this->actor.world.rot.y;
        if (this->skelAnime.playSpeed == 0.0f) {
            this->skelAnime.playSpeed = 1.0f;
        }
    } else {
        this->yawTowardsTarget = Math_Vec3f_Yaw(&this->actor.world.pos, &this->role.targetActor->world.pos);
        if (this->skelAnime.curFrame >= Animation_GetLastFrame(this->skelAnime.animation)) {
            this->skelAnime.playSpeed = 0.0f;
        }
    }
    if ((cur == NV_MM_TEXT(0x2AC6)) || (cur == NV_MM_TEXT(0x2AC7)) || (cur == NV_MM_TEXT(0x2AC8))) {
        this->skelAnime.playSpeed = 0.0f;
        this->yawTowardsTarget = this->actor.yawTowardsPlayer;
        this->skelAnime.curFrame = 30.0f;
    }
    if (cur == NV_MM_TEXT(0x2ACF)) {
        this->skelAnime.playSpeed = 0.0f;
    }
    if (this->role.cutsceneState == 2) {
        NvMmMuto_SetupIdle(this);
    }
}

/** MM's EnMuto_SetupDialogue. */
static void NvMmMuto_SetupDialogue(NvMmMuto* this, PlayState* play) {
    if (this->isInMayorsRoom) {
        if (this->role.targetActor != NULL) {
            this->shouldSetHeadRotation = true;
            this->role.cutsceneState = 1;
        }
        this->actionFunc = NvMmMuto_InMeeting;
    } else {
        this->actionFunc = NvMmMuto_Talk;
    }
}

static void NvMmMuto_Talk(NvMmMuto* this, PlayState* play) {
    if (!this->isInMayorsRoom) {
        this->yawTowardsTarget = this->actor.yawTowardsPlayer;
    }
    if ((Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT || Message_GetState(&play->msgCtx) == TEXT_STATE_DONE) &&
        Message_ShouldAdvance(play)) {
        Message_CloseTextbox(play);
        if (this->actor.textId == NV_MM_TEXT(0x62C)) {
            NvMm_SetWeekEvent(WE_88_08);
        }
        if (this->actor.textId == NV_MM_TEXT(0x624)) {
            NvMm_SetWeekEvent(WE_60_80);
        }
        if (!this->isInMayorsRoom) {
            this->textIdIndex = 3;
        }
        NvMmMuto_SetupIdle(this);
    }
}

/** MM's EnMuto_Idle: his line for the moment. */
static void NvMmMuto_Idle(NvMmMuto* this, PlayState* play) {
    u16 textId = sMutoTextIds[this->textIdIndex];

    if (!this->isInMayorsRoom) {
        if (NvMmForm_Get() == 3) {
            textId = NvMm_CheckWeekEvent(WE_88_08) ? 0x62B : 0x62C;
        }
        if (NvMmMask_Worn() == MM_PLAYER_MASK_KAFEIS_MASK) {
            textId = 0x2363;
        }
    } else {
        this->textIdIndex = NvMm_CheckWeekEvent(WE_ATTENDED_MAYOR_MEETING) ? 1 : 0;
        if (NvMmMask_Worn() == MM_PLAYER_MASK_COUPLE) {
            this->textIdIndex = 4;
        }
        textId = sMutoTextIds[this->textIdIndex];
        if (this->role.cutsceneState == 1) { // (the mayor started the meeting's talk)
            NvMmMuto_SetupDialogue(this, play);
            return;
        }
    }
    this->actor.textId = NV_MM_TEXT(textId);
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        NvMmMuto_SetupDialogue(this, play);
        return;
    }
    if (!this->isInMayorsRoom) {
        s16 yaw = this->actor.yawTowardsPlayer - this->actor.world.rot.y;

        this->yawTowardsTarget = this->actor.yawTowardsPlayer;
        if (ABS(yaw) > 0x2890) {
            return;
        }
    }
    Actor_OfferTalk(&this->actor, play, 80.0f);
}

void NvMmMuto_Update(Actor* thisx, PlayState* play) {
    NvMmMuto* this = (NvMmMuto*)thisx;

    SkelAnime_Update(&this->skelAnime);
    if (!this->isInMayorsRoom || this->shouldSetHeadRotation) { // MM's EnMuto_SetHeadRotation
        s16 d = this->yawTowardsTarget - thisx->world.rot.y;

        this->headRotTarget.y = 0;
        if ((thisx->xzDistToPlayer < 200.0f) && (ABS(d) < 0x4E20)) {
            this->headRotTarget.y = CLAMP(d, -0x2710, 0x2710);
        }
    }
    if (this->isInMayorsRoom && NvMmMuto_LastNight()) {
        Actor_Kill(thisx);
        return;
    }
    this->actionFunc(this, play);
    Actor_SetScale(thisx, 0.01f);
    thisx->shape.rot.y = thisx->world.rot.y;
    Actor_SetFocus(thisx, 60.0f);
    Actor_MoveXZGravity(thisx);
    Math_SmoothStepToS(&this->headRot.y, this->headRotTarget.y, 1, 0xBB8, 0);
    Math_SmoothStepToS(&this->headRot.x, this->headRotTarget.x, 1, 0x3E8, 0);
    Actor_UpdateBgCheckInfo(play, thisx, 20.0f, 20.0f, 50.0f, 0x1D);
    Collider_UpdateCylinder(thisx, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static s32 NvMmMuto_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                     void* thisx) {
    NvMmMuto* this = (NvMmMuto*)thisx;

    if (limbIndex == OBJECT_TORYO_LIMB_0F) {
        rot->x += this->headRot.y;
        rot->z += this->headRot.x;
    }
    return false;
}

void NvMmMuto_Draw(Actor* thisx, PlayState* play) {
    NvMmMuto* this = (NvMmMuto*)thisx;

    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          NvMmMuto_OverrideLimbDraw, NULL, this);
}
