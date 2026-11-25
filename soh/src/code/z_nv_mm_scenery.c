/*
 * Nightmareverse: Clock Town's plain scenery, MM's own, ported as it is (mm/README.md "Clock Town's
 * scenery"):
 *   Obj_Tokei_Turret (0x221)  South Clock Town's carnival platforms and flags: tier by params & 3
 *                             (0 the base, 1 the platform top: both stood on; else the flags)
 *   Obj_Tree (0x229)          the branching tree (North Clock Town): its top stood on, its trunk
 *                             solid, swaying when hit (sword, stick, spin, Goron moves) or rolled
 *                             into; params & 0x8000 the large one, no top to stand on
 *   Bg_Market_Step (0x1CC)    West Clock Town's market stalls and the bank's adverts, day or night
 *                             by params & 1 (two placed, each in its half-days)
 *   Obj_Kinoko (0x23B)        a magic mushroom: seen only with the Mask of Scents on, glowing.
 *                             Not yet: catching it in a bottle (MM's GI_MAX, the bottle's catch).
 * MM's models and collision (tools/mm_objects.py: object_tokei_turret, object_tree,
 * object_market_obj; the mushroom's glow from gameplay_keep).
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>

#define OBJ(o, name) ("__OTR__mm/objects/" o "/" name)

s32 NvMmMask_Worn(void);

static void NvMmScenery_Dl(PlayState* play, const char* dl) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dl);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmScenery_Mesh(PlayState* play, DynaPolyActor* dyna, const char* col) {
    CollisionHeader* colHeader = NULL;

    DynaPolyActor_Init(dyna, 0);
    CollisionHeader_GetVirtual((void*)col, &colHeader);
    dyna->bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, &dyna->actor, colHeader);
}

/* ---- Obj_Tokei_Turret ---- */

typedef struct {
    DynaPolyActor dyna;
} NvMmTurret;

const size_t gNvMmTurretSize = sizeof(NvMmTurret);

#define TURRET(name) OBJ("object_tokei_turret", name)
#define TURRET_TIER(thisx) ((thisx)->params & 3)

void NvMmTurret_Init(Actor* thisx, PlayState* play) {
    NvMmTurret* this = (NvMmTurret*)thisx;
    s32 tier = TURRET_TIER(thisx);

    thisx->uncullZoneForward = 1200.0f;
    Actor_SetScale(thisx, 0.1f);
    this->dyna.bgId = BGACTOR_NEG_ONE;
    if ((tier == 0) || (tier == 1)) {
        thisx->uncullZoneDownward = thisx->uncullZoneScale = 240.0f;
        NvMmScenery_Mesh(play, &this->dyna, (tier == 0) ? TURRET("gClockTownTurretBaseCol") : TURRET("gClockTownTurretPlatformCol"));
    } else {
        thisx->uncullZoneDownward = thisx->uncullZoneScale = 1300.0f;
    }
}

void NvMmTurret_Destroy(Actor* thisx, PlayState* play) {
    NvMmTurret* this = (NvMmTurret*)thisx;

    if (this->dyna.bgId != BGACTOR_NEG_ONE) {
        DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
    }
}

void NvMmTurret_Update(Actor* thisx, PlayState* play) {
}

void NvMmTurret_Draw(Actor* thisx, PlayState* play) {
    s32 tier = TURRET_TIER(thisx);

    NvMmScenery_Dl(play, (tier == 1)   ? TURRET("gClockTownTurretPlatformTopDL")
                         : (tier == 0) ? TURRET("gClockTownTurretPlatformBaseDL")
                                       : TURRET("gClockTownFlagsDL"));
}

/* ---- Obj_Tree ---- */

typedef struct {
    DynaPolyActor dyna;
    ColliderCylinder collider;
    f32 swayAmplitude;
    s16 swayAngle;
    s16 swayVelocity;
    s16 timer;
    s16 swaying;
} NvMmTree;

const size_t gNvMmTreeSize = sizeof(NvMmTree);

#define TREE(name) OBJ("object_tree", name)
#define TREE_ISLARGE(thisx) ((thisx)->params & 0x8000)

static ColliderCylinderInit sTreeCylinderInit = {
    { COLTYPE_TREE, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    // (what sways it, as OoT's weapons: MM's table takes the stick, sword, spin, Goron moves, thrown things)
    { ELEMTYPE_UNK1, { 0x00000000, 0x00, 0x00 },
      { DMG_DEKU_STICK | DMG_SLASH | DMG_SPIN_ATTACK | DMG_JUMP_MASTER | DMG_JUMP_KOKIRI | DMG_JUMP_GIANT |
            DMG_HAMMER_SWING | DMG_HAMMER_JUMP,
        0x00, 0x00 },
      TOUCH_NONE | TOUCH_SFX_NORMAL, BUMP_ON, OCELEM_ON },
    { 28, 120, 0, { 0, 0, 0 } },
};

void NvMmTree_Init(Actor* thisx, PlayState* play) {
    NvMmTree* this = (NvMmTree*)thisx;

    this->dyna.bgId = BGACTOR_NEG_ONE;
    if (TREE_ISLARGE(thisx)) {
        Actor_SetScale(thisx, 0.15f);
        thisx->uncullZoneForward = 4000.0f;
    } else {
        CollisionHeader* colHeader = NULL;

        Actor_SetScale(thisx, 0.1f);
        DynaPolyActor_Init(&this->dyna, DPM_PLAYER); // (MM's DYNA_TRANSFORM_POS: it moves with the tree's position)
        CollisionHeader_GetVirtual((void*)TREE("gTreeTopCol"), &colHeader);
        this->dyna.bgId = DynaPoly_SetBgActor(play, &play->colCtx.dyna, thisx, colHeader);
    }
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sTreeCylinderInit);
    thisx->colChkInfo.mass = MASS_HEAVY;
    if (TREE_ISLARGE(thisx)) {
        this->collider.dim.height = 220;
    }
}

void NvMmTree_Destroy(Actor* thisx, PlayState* play) {
    NvMmTree* this = (NvMmTree*)thisx;

    if (this->dyna.bgId != BGACTOR_NEG_ONE) {
        DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
    }
    Collider_DestroyCylinder(play, &this->collider);
}

void NvMmTree_Update(Actor* thisx, PlayState* play) {
    NvMmTree* this = (NvMmTree*)thisx;

    if (this->collider.base.acFlags & AC_HIT) { // (MM: the hit starts the sway, home.rot.y set to 1)
        this->collider.base.acFlags &= ~AC_HIT;
        thisx->home.rot.y = 1;
    }
    if (this->swaying) { // MM's ObjTree_Sway
        if (this->timer > 80) {
            this->swaying = false;
        } else {
            Math_SmoothStepToF(&this->swayAmplitude, 0.0f, 0.1f, 91.0f, 18.0f);
            this->swayVelocity += 1 * 0x10000 / 360;
            this->swayAngle += this->swayVelocity;
            thisx->shape.rot.x = Math_SinS(this->swayAngle) * this->swayAmplitude;
            thisx->shape.rot.z = Math_CosS(this->swayAngle) * this->swayAmplitude;
            this->timer++;
        }
    }
    Collider_UpdateCylinder(thisx, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    if (thisx->xzDistToPlayer < 600.0f) {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
        if (thisx->home.rot.y == 1) { // MM's ObjTree_SetupSway
            thisx->home.rot.y = 0;
            this->timer = 0;
            this->swayAmplitude = 546.0f;
            this->swayVelocity = 35 * 0x10000 / 360;
            this->swaying = true;
            Audio_PlayActorSound2(thisx, NA_SE_EV_TREE_SWING);
        }
    }
}

void NvMmTree_Draw(Actor* thisx, PlayState* play) {
    s16 xRot = thisx->shape.rot.x;
    s16 zRot = thisx->shape.rot.z;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)TREE("gTreeBodyDL"));
    Matrix_RotateZYX(xRot, 0, zRot, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)TREE("gTreeLeavesDL"));
    CLOSE_DISPS(play->state.gfxCtx);
}

/* ---- Bg_Market_Step ---- */

typedef struct {
    Actor actor;
} NvMmMarketStep;

const size_t gNvMmMarketStepSize = sizeof(NvMmMarketStep);

#define MARKET(name) OBJ("object_market_obj", name)

void NvMmMarketStep_Init(Actor* thisx, PlayState* play) {
    thisx->uncullZoneForward = 1.0f; // (MM's: 1, 1, 1 with the actor's draw culling off)
    thisx->uncullZoneScale = 1.0f;
    thisx->uncullZoneDownward = 1.0f;
    Actor_SetScale(thisx, 1.0f);
}

void NvMmMarketStep_Destroy(Actor* thisx, PlayState* play) {
}

void NvMmMarketStep_Update(Actor* thisx, PlayState* play) {
}

void NvMmMarketStep_Draw(Actor* thisx, PlayState* play) {
    s32 night = thisx->params & 1;

    NvMmScenery_Dl(play, night ? MARKET("gWestClockTownMarketNightDL") : MARKET("gWestClockTownMarketDayDL"));
    NvMmScenery_Dl(play, night ? MARKET("gWestClockTownMarketBankAdvertisementsAndDoorNightDL")
                               : MARKET("gWestClockTownMarketBankAdvertisementsAndDoorDayDL"));
}

/* ---- Obj_Kinoko ---- */

typedef struct {
    Actor actor;
} NvMmKinoko;

const size_t gNvMmKinokoSize = sizeof(NvMmKinoko);

#define MM_PLAYER_MASK_SCENTS 0x13

void NvMmKinoko_Draw(Actor* thisx, PlayState* play);

void NvMmKinoko_Init(Actor* thisx, PlayState* play) {
    thisx->world.pos.y += 4.0f;
}

void NvMmKinoko_Destroy(Actor* thisx, PlayState* play) {
}

/** MM's ObjKinoko_Update: seen with the Mask of Scents, pulsing (speed holds the glow's alpha,
 * velocity.x the scale it grows to). */
void NvMmKinoko_Update(Actor* thisx, PlayState* play) {
    if (NvMmMask_Worn() != MM_PLAYER_MASK_SCENTS) {
        thisx->draw = NULL;
        thisx->flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        return;
    }
    thisx->draw = NvMmKinoko_Draw;
    thisx->flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    if (Math_SmoothStepToF(&thisx->speedXZ, 0.0f, 0.04f, 2.0f, 0.5f) < 0.5f) {
        thisx->scale.x = 0.0f;
        thisx->speedXZ = 110.0f;
        thisx->velocity.x = 0.2f;
    }
    if (Math_SmoothStepToF(&thisx->scale.x, thisx->velocity.x, 0.04f, 0.004f, 0.001f) <
        ((thisx->velocity.x == 0.0f) ? 0.0f : 0.05f)) {
        thisx->velocity.x = 0.0f;
    }
    thisx->scale.y = thisx->scale.z = thisx->scale.x;
    thisx->shape.rot.y = Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) + 0x8000;
}

/** MM's ObjKinoko_Draw: gameplay_keep's glow, twice, the second turned flat. */
void NvMmKinoko_Draw(Actor* thisx, PlayState* play) {
    Gfx* glow = ResourceMgr_LoadGfxByName(OBJ("gameplay_keep", "gameplay_keep_DL_029D10"));

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Xlu(play->state.gfxCtx);
    gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 169, 63, 186, (u8)thisx->speedXZ);
    gDPSetEnvColor(POLY_XLU_DISP++, 110, 44, 200, 100);
    gDPSetRenderMode(POLY_XLU_DISP++, G_RM_PASS, G_RM_ZB_CLD_SURF2);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, &glow[4]); // (past its own setup, as 2Ship2Harkinian's port: MM's index 2)
    Matrix_RotateX(-M_PI / 2, MTXMODE_APPLY);
    gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_XLU_DISP++, &glow[4]);
    CLOSE_DISPS(play->state.gfxCtx);
}

/* ---- En_Hit_Tag ---- */

/*
 * MM's hidden rupees (En_Hit_Tag, 0x265): an invisible spot (Clock Town's walls and posts) that gives
 * three green rupees the first time it's hit, as MM. Its switch flag (params >> 9), when set, keeps it
 * away (MM never sets it itself). Hit by OoT's weapons (OoT's sign's flags: everything).
 */
typedef struct {
    Actor actor;
    ColliderCylinder collider;
} NvMmHitTag;

const size_t gNvMmHitTagSize = sizeof(NvMmHitTag);

static ColliderCylinderInit sHitTagCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_PLAYER, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xFFCFFFFF, 0x00, 0x00 }, TOUCH_NONE | TOUCH_SFX_NORMAL, BUMP_ON,
      OCELEM_NONE },
    { 16, 32, 0, { 0, 0, 0 } },
};

void NvMmHitTag_Init(Actor* thisx, PlayState* play) {
    NvMmHitTag* this = (NvMmHitTag*)thisx;

    Actor_SetScale(thisx, 1.0f);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sHitTagCylinderInit);
    Collider_UpdateCylinder(thisx, &this->collider);
    s32 flag = (thisx->params & 0xFE00) >> 9;

    if ((flag != 0x7F) && Flags_GetSwitch(play, flag)) { // (0x7F: none; OoT's switch flags stop at 0x3F)
        Actor_Kill(thisx);
    }
}

void NvMmHitTag_Destroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((NvMmHitTag*)thisx)->collider);
}

void NvMmHitTag_Update(Actor* thisx, PlayState* play) {
    NvMmHitTag* this = (NvMmHitTag*)thisx;

    if (this->collider.base.acFlags & AC_HIT) {
        s32 i;

        Sfx_PlaySfxCentered(NA_SE_SY_GET_RUPY);
        Actor_Kill(thisx);
        for (i = 0; i < 3; i++) {
            Item_DropCollectible(play, &thisx->world.pos, ITEM00_RUPEE_GREEN);
        }
        if (getenv("NV_LOG") != NULL) {
            printf("NVMM hidden rupees at %d,%d,%d\n", (int)thisx->world.pos.x, (int)thisx->world.pos.y,
                   (int)thisx->world.pos.z);
            fflush(stdout);
        }
    } else {
        CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
    }
}

void NvMmHitTag_Draw(Actor* thisx, PlayState* play) {
}
