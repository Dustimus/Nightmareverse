/*
 * Nightmareverse: MM's grass and bushes (En_Kusa, MM id 0x090), ported as they are (mm/README.md "Clock
 * Town's scenery"): MM's models (the bush from MM's gameplay_field_keep, near and fading far; the grass
 * and its stump from object_kusa; the flying leaves from gameplay_keep), MM's wind sway, MM's types:
 *   0 a bush: cut or lifted, gone;  1 grass that regrows (two seconds cut, uprooted it grows back);
 *   2 grass, cut to a stump;        3 grass with a set drop (MM's drop table, as the crates').
 * Lift, carry, throw, break as in MM.
 * Adapted: what cuts it is OoT's weapons (OoT's own grass's damage flags, as the crates and pots take
 * OoT's); the random drop (types 0 and 2) is OoT's random drop table at the same index, MM's own tables
 * aren't over yet; the bugs (bit 4: MM's En_Insect) come with MM's bugs.
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include "overlays/effects/ovl_Effect_Ss_Kakera/z_eff_ss_kakera.h"

#define KUSA(name) ((Gfx*)("__OTR__mm/objects/object_kusa/" name))
#define FIELD(name) ((Gfx*)("__OTR__mm/objects/gameplay_field_keep/" name))
#define KEEP(name) ((Gfx*)("__OTR__mm/objects/gameplay_keep/" name))

#define KUSA_TYPE(thisx) ((thisx)->params & 3)
#define KUSA_RAND_DROP(thisx) (((thisx)->params >> 8) & 0xF)
#define KUSA_NO_DROP(thisx) (((thisx)->params >> 12) & 1)
#define KUSA_BUGS(thisx) (((thisx)->params >> 4) & 1)
#define KUSA_DROP_FLAG(thisx) (((thisx)->params >> 8) & 0x7F)
#define KUSA_DROP(thisx) (((thisx)->params >> 2) & 0x3F)

enum { KUSA_BUSH, KUSA_REGROWING, KUSA_GRASS, KUSA_GRASS_2 };

s32 NvMm_Drop(s32 index); // z_nv_mm_kibako.c

struct NvMmKusa;
typedef void (*NvMmKusaFunc)(struct NvMmKusa*, PlayState*);

typedef struct NvMmKusa {
    Actor actor;
    NvMmKusaFunc actionFunc;
    ColliderCylinder collider;
    u8 timer;
    s8 mtxIdx;
    u8 isCut;
    u8 isInWater;
} NvMmKusa;

const size_t gNvMmKusaSize = sizeof(NvMmKusa);

static s16 sRotSpeedXTarget, sRotSpeedX, sRotSpeedYTarget, sRotSpeedY;
static s16 sMtxNext;
static u8 sSwayFirst = true;
static u32 sSwayFrame;
static MtxF sSway[8];
static s16 sSwayAngles[5];

// OoT's own grass's (what of OoT's cuts it), MM's size
static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_PLAYER | OC1_TYPE_2, OC2_TYPE_2,
      COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00400000, 0x00, 0x02 }, { 0x4FC00758, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_NONE, BUMP_ON,
      OCELEM_ON },
    { 6, 44, 0, { 0, 0, 0 } },
};

static CollisionCheckInfoInit sColChkInfoInit = { 0, 12, 30, MASS_IMMOVABLE };

static Vec3f sUnitDirections[] = {
    { 0.0f, 0.7071f, 0.7071f },
    { 0.7071f, 0.7071f, 0.0f },
    { 0.0f, 0.7071f, -0.7071f },
    { -0.7071f, 0.7071f, 0.0f },
};

static s16 sFragmentScales[] = { 108, 102, 96, 84, 66, 55, 42, 38 };

static void NvMmKusa_WaitForInteract(NvMmKusa* this, PlayState* play);
static void NvMmKusa_LiftedUp(NvMmKusa* this, PlayState* play);
static void NvMmKusa_Fall(NvMmKusa* this, PlayState* play);
static void NvMmKusa_CutWaitRegrow(NvMmKusa* this, PlayState* play);
static void NvMmKusa_DoNothing(NvMmKusa* this, PlayState* play);
static void NvMmKusa_UprootedWaitRegrow(NvMmKusa* this, PlayState* play);
static void NvMmKusa_Regrow(NvMmKusa* this, PlayState* play);
static void NvMmKusa_DrawBush(Actor* thisx, PlayState* play);
static void NvMmKusa_DrawGrass(Actor* thisx, PlayState* play);

/** MM's EnKusa_ApplySway: the wind's matrix added to the current one. */
static void NvMmKusa_ApplySway(MtxF* m) {
    MtxF cur;
    f32* c = &cur.mf[0][0];
    f32* add = &m->mf[0][0];
    s32 i;

    Matrix_Get(&cur); // (OoT's matrix stack has no pointer to the current one: get, add, put back)
    for (i = 0; i < 16; i++) {
        *c++ += *add++;
    }
    Matrix_Put(&cur);
}

/** MM's EnKusa_Sway: the eight wind matrices, once a frame. */
static void NvMmKusa_Sway(void) {
    f32 s[5], c[5], v[8];
    s32 i;

    sSwayAngles[0] += 0x46;
    sSwayAngles[1] += 0x12C;
    sSwayAngles[2] += 0x2BC;
    sSwayAngles[3] += 0x514;
    sSwayAngles[4] += 0x22C4;
    for (i = 0; i < 5; i++) {
        s[i] = Math_SinS(sSwayAngles[i]);
        c[i] = Math_CosS(sSwayAngles[i]);
    }
    s[3] *= 1.2f;
    s[4] *= 1.5f;
    c[3] *= 1.3f;
    c[4] *= 1.7f;
    v[0] = (s[0] - c[1]) * s[2] * c[0] * s[0] * 0.0015f;
    v[1] = (s[1] - c[2]) * s[3] * c[1] * s[0] * 0.0015f;
    v[2] = (s[2] - c[3]) * c[2] * s[0] * c[0] * 0.0015f;
    v[3] = (s[3] - c[1]) * c[3] * s[1] * c[0] * 0.0015f;
    v[4] = (s[0] - c[2]) * s[0] * s[1] * s[4] * 0.0015f;
    v[5] = (s[1] - c[3]) * s[2] * s[3] * s[4] * 0.0015f;
    v[6] = (s[2] - c[0]) * c[0] * c[1] * c[4] * 0.0015f;
    v[7] = (s[3] - c[1]) * c[2] * c[3] * c[4] * 0.0015f;
    for (i = 0; i < 8; i++) {
        f32* p = &sSway[i].mf[0][0];

        p[0] = v[1] * 0.2f;
        p[1] = v[i & 7];
        p[2] = v[(i + 1) & 7];
        p[3] = 0.0f;
        p[4] = v[(i + 2) & 7];
        p[5] = v[0];
        p[6] = v[(i + 2) & 7];
        p[7] = 0.0f;
        p[8] = v[(i + 3) & 7];
        p[9] = v[(i + 4) & 7];
        p[10] = v[3] * 0.2f;
        p[11] = p[12] = p[13] = p[14] = p[15] = 0.0f;
    }
}

static s32 NvMmKusa_SnapToFloor(NvMmKusa* this, PlayState* play) {
    CollisionPoly* poly;
    Vec3f pos;
    s32 bgId;
    f32 floorY;

    pos.x = this->actor.world.pos.x;
    pos.y = this->actor.world.pos.y + 30.0f;
    pos.z = this->actor.world.pos.z;
    floorY = BgCheck_EntityRaycastFloor5(play, &play->colCtx, &poly, &bgId, &this->actor, &pos);
    if (floorY > BGCHECK_Y_MIN) {
        this->actor.world.pos.y = floorY;
        this->actor.home.pos = this->actor.world.pos;
        return true;
    }
    return false;
}

static void NvMmKusa_DropCollectible(NvMmKusa* this, PlayState* play) {
    s32 type = KUSA_TYPE(&this->actor);

    if ((type == KUSA_GRASS) || (type == KUSA_BUSH)) {
        if (!KUSA_NO_DROP(&this->actor)) {
            Item_DropCollectibleRandom(play, NULL, &this->actor.world.pos, KUSA_RAND_DROP(&this->actor) * 0x10);
        }
    } else if (type == KUSA_REGROWING) {
        Item_DropCollectible(play, &this->actor.world.pos, ITEM00_RUPEE_RED); // (MM's 3: a red rupee)
    } else {
        s32 drop = NvMm_Drop(KUSA_DROP(&this->actor));

        if (drop != ITEM00_NONE) {
            Item_DropCollectible(play, &this->actor.world.pos, (KUSA_DROP_FLAG(&this->actor) << 8) | drop);
        }
    }
}

static void NvMmKusa_UpdateVelY(NvMmKusa* this) {
    this->actor.velocity.y += this->actor.gravity;
    if (this->actor.velocity.y < this->actor.minVelocityY) {
        this->actor.velocity.y = this->actor.minVelocityY;
    }
}

static void NvMmKusa_RandScaleVecToZero(Vec3f* vec, f32 scale) {
    scale += ((Rand_ZeroOne() * 0.2f) - 0.1f) * scale;
    vec->x -= vec->x * scale;
    vec->y -= vec->y * scale;
    vec->z -= vec->z * scale;
}

static void NvMmKusa_SetScaleSmall(NvMmKusa* this) {
    this->actor.scale.y = 0.16f;
    this->actor.scale.x = this->actor.scale.z = 0.12f;
}

static void NvMmKusa_SpawnFragments(NvMmKusa* this, PlayState* play) {
    Vec3f vel, pos;
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sUnitDirections); i++) {
        Vec3f* d = &sUnitDirections[i];

        pos.x = this->actor.world.pos.x + (d->x * this->actor.scale.x * 20.0f);
        pos.y = this->actor.world.pos.y + (d->y * this->actor.scale.y * 20.0f) + 10.0f;
        pos.z = this->actor.world.pos.z + (d->z * this->actor.scale.z * 20.0f);
        vel.x = (Rand_ZeroOne() - 0.5f) * 8.0f;
        vel.y = Rand_ZeroOne() * 10.0f;
        vel.z = (Rand_ZeroOne() - 0.5f) * 8.0f;
        EffectSsKakera_Spawn(play, &pos, &vel, &pos, -100, 64, 40, 3, 0,
                             sFragmentScales[(s32)(Rand_ZeroOne() * 111.1f) & 7], 0, 0, 80, -1, 1,
                             KEEP("gKakeraLeafMiddleDL"));
        pos.x = this->actor.world.pos.x + (d->x * this->actor.scale.x * 40.0f);
        pos.y = this->actor.world.pos.y + (d->y * this->actor.scale.y * 40.0f) + 10.0f;
        pos.z = this->actor.world.pos.z + (d->z * this->actor.scale.z * 40.0f);
        vel.x = (Rand_ZeroOne() - 0.5f) * 6.0f;
        vel.y = Rand_ZeroOne() * 10.0f;
        vel.z = (Rand_ZeroOne() - 0.5f) * 6.0f;
        EffectSsKakera_Spawn(play, &pos, &vel, &pos, -100, 64, 40, 3, 0,
                             sFragmentScales[(s32)(Rand_ZeroOne() * 111.1f) % 7], 0, 0, 80, -1, 1,
                             KEEP("gKakeraLeafTipDL"));
    }
}

static s32 NvMmKusa_IsUnderwater(NvMmKusa* this, PlayState* play) {
    WaterBox* waterBox;
    f32 surface;

    return WaterBox_GetSurface1(play, &play->colCtx, this->actor.world.pos.x, this->actor.world.pos.z, &surface,
                                &waterBox) &&
           (this->actor.world.pos.y < surface);
}

void NvMmKusa_Init(Actor* thisx, PlayState* play) {
    NvMmKusa* this = (NvMmKusa*)thisx;

    Actor_SetScale(thisx, 0.4f);
    thisx->gravity = -3.2f;
    thisx->minVelocityY = -17.0f;
    thisx->uncullZoneForward = 1200.0f;
    thisx->uncullZoneScale = 100.0f;
    thisx->uncullZoneDownward = 200.0f;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sCylinderInit);
    Collider_UpdateCylinder(thisx, &this->collider);
    CollisionCheck_SetInfo(&thisx->colChkInfo, NULL, &sColChkInfoInit);
    if (KUSA_TYPE(thisx) == KUSA_BUSH) {
        thisx->shape.shadowScale = 1.0f;
        thisx->shape.shadowAlpha = 60;
    } else {
        thisx->shape.shadowScale = 0.9f;
        thisx->shape.shadowAlpha = 70;
    }
    if (thisx->shape.rot.y == 0) {
        thisx->shape.rot.y = thisx->home.rot.y = thisx->world.rot.y = (s16)(Rand_Next() >> 0x10);
    }
    if (!NvMmKusa_SnapToFloor(this, play)) {
            Actor_Kill(thisx);
        return;
    }
    this->isInWater = NvMmKusa_IsUnderwater(this, play);
    this->actionFunc = NvMmKusa_WaitForInteract;
    thisx->draw = (KUSA_TYPE(thisx) == KUSA_BUSH) ? NvMmKusa_DrawBush : NvMmKusa_DrawGrass;
    if (sSwayFirst) {
        s32 i;

        for (i = 0; i < 5; i++) {
            sSwayAngles[i] = Rand_Next() >> 0x10;
        }
        sSwayFirst = false;
        NvMmKusa_Sway();
        sSwayFrame = play->gameplayFrames;
    }
    this->mtxIdx = sMtxNext & 7;
    sMtxNext++;
}

void NvMmKusa_Destroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((NvMmKusa*)thisx)->collider);
}

static void NvMmKusa_SetupCut(NvMmKusa* this) {
    this->actionFunc = (KUSA_TYPE(&this->actor) == KUSA_REGROWING) ? NvMmKusa_CutWaitRegrow : NvMmKusa_DoNothing;
    this->timer = 0;
}

static void NvMmKusa_SetupInteract(NvMmKusa* this) {
    this->actionFunc = NvMmKusa_WaitForInteract;
}

static void NvMmKusa_WaitForInteract(NvMmKusa* this, PlayState* play) {
    if (Actor_HasParent(&this->actor, play)) { // lifted
        this->actionFunc = NvMmKusa_LiftedUp;
        this->actor.room = -1;
        this->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
        SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_PL_PULL_UP_PLANT);
        this->actor.shape.shadowDraw = ActorShadow_DrawCircle;
    } else if (this->collider.base.acFlags & AC_HIT) { // cut
        this->collider.base.acFlags &= ~AC_HIT;
        if (getenv("NV_LOG") != NULL) {
            printf("NVMM grass: cut (%04X)\n", (u16)this->actor.params);
            fflush(stdout);
        }
        NvMmKusa_SpawnFragments(this, play);
        NvMmKusa_DropCollectible(this, play);
        SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EV_PLANT_BROKEN);
        if (KUSA_TYPE(&this->actor) == KUSA_BUSH) {
            Actor_Kill(&this->actor);
            return;
        }
        NvMmKusa_SetupCut(this);
        this->isCut = true;
    } else {
        if (!(this->collider.base.ocFlags1 & OC1_TYPE_PLAYER) && (this->actor.xzDistToPlayer > 12.0f)) {
            this->collider.base.ocFlags1 |= OC1_TYPE_PLAYER;
        }
        if (this->actor.xzDistToPlayer < 600.0f) {
            Collider_UpdateCylinder(&this->actor, &this->collider);
            CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
            if (this->actor.xzDistToPlayer < 400.0f) {
                CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
                if ((this->actor.xzDistToPlayer < 100.0f) && (KUSA_TYPE(&this->actor) != KUSA_GRASS_2)) {
                    Actor_OfferCarry(&this->actor, play);
                }
            }
        }
    }
}

static void NvMmKusa_LiftedUp(NvMmKusa* this, PlayState* play) {
    if (Actor_HasNoParent(&this->actor, play)) { // thrown or put down
        this->actor.room = play->roomCtx.curRoom.num;
        this->actionFunc = NvMmKusa_Fall;
        sRotSpeedXTarget = -0xBB8;
        sRotSpeedYTarget = (Rand_ZeroOne() - 0.5f) * 1600.0f;
        sRotSpeedX = sRotSpeedY = 0;
        this->timer = 0;
        this->actor.velocity.x = this->actor.speedXZ * Math_SinS(this->actor.world.rot.y);
        this->actor.velocity.z = this->actor.speedXZ * Math_CosS(this->actor.world.rot.y);
        this->actor.colChkInfo.mass = 80;
        this->actor.gravity = -0.1f;
        NvMmKusa_UpdateVelY(this);
        NvMmKusa_RandScaleVecToZero(&this->actor.velocity, 0.005f);
        Actor_UpdatePos(&this->actor);
        Actor_UpdateBgCheckInfo(play, &this->actor, 7.5f, 35.0f, 0.0f, 0xC5);
        this->actor.gravity = -3.2f;
    } else {
        Vec3f pos;
        s32 bgId;

        pos.x = this->actor.world.pos.x;
        pos.y = this->actor.world.pos.y + 20.0f;
        pos.z = this->actor.world.pos.z;
        this->actor.floorHeight =
            BgCheck_EntityRaycastFloor5(play, &play->colCtx, &this->actor.floorPoly, &bgId, &this->actor, &pos);
    }
}

static void NvMmKusa_Fall(NvMmKusa* this, PlayState* play) {
    s32 wasHit = (this->collider.base.atFlags & AT_HIT) != 0;

    if (wasHit) {
        this->collider.base.atFlags &= ~AT_HIT;
    }
    this->timer++;
    if ((this->actor.bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH | BGCHECKFLAG_WALL)) || wasHit ||
        (this->timer >= 100)) {
        if (!(this->actor.bgCheckFlags & BGCHECKFLAG_WATER)) {
            SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EV_PLANT_BROKEN);
        }
        NvMmKusa_SpawnFragments(this, play);
        NvMmKusa_DropCollectible(this, play);
        switch (KUSA_TYPE(&this->actor)) {
            case KUSA_BUSH:
            case KUSA_GRASS:
                Actor_Kill(&this->actor);
                break;
            case KUSA_REGROWING: // back home, small, and grows back
                this->actor.world.pos.x = this->actor.home.pos.x;
                this->actor.world.pos.y = this->actor.home.pos.y - 9.0f;
                this->actor.world.pos.z = this->actor.home.pos.z;
                NvMmKusa_SetScaleSmall(this);
                this->timer = 0;
                this->actor.shape.rot = this->actor.home.rot;
                this->actionFunc = NvMmKusa_UprootedWaitRegrow;
                this->actor.shape.shadowDraw = NULL;
                break;
            default:
                break;
        }
        return;
    }
    if (this->actor.bgCheckFlags & BGCHECKFLAG_WATER_TOUCH) {
        Vec3f p;
        s32 i;
        s16 angle;

        p.y = this->actor.world.pos.y + this->actor.yDistToWater;
        for (angle = 0, i = 0; i < 4; i++, angle += 0x4000) {
            p.x = this->actor.world.pos.x + (Math_SinS((s32)(Rand_ZeroOne() * 7200.0f) + angle) * 15.0f);
            p.z = this->actor.world.pos.z + (Math_CosS((s32)(Rand_ZeroOne() * 7200.0f) + angle) * 15.0f);
            EffectSsGSplash_Spawn(play, &p, NULL, NULL, 0, 190);
        }
        p.x = this->actor.world.pos.x;
        p.z = this->actor.world.pos.z;
        EffectSsGSplash_Spawn(play, &p, NULL, NULL, 0, 280);
        EffectSsGRipple_Spawn(play, &p, 300, 700, 0);
        this->actor.minVelocityY = -3.0f;
        this->actor.velocity.x *= 0.1f;
        this->actor.velocity.y *= 0.4f;
        this->actor.velocity.z *= 0.1f;
        this->actor.gravity *= 0.5f;
        sRotSpeedX >>= 1;
        sRotSpeedXTarget >>= 1;
        sRotSpeedY >>= 1;
        sRotSpeedYTarget >>= 1;
        this->actor.bgCheckFlags &= ~BGCHECKFLAG_WATER_TOUCH;
        SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, NA_SE_EV_DIVE_INTO_WATER_L);
    }
    NvMmKusa_UpdateVelY(this);
    Math_StepToS(&sRotSpeedX, sRotSpeedXTarget, 500);
    Math_StepToS(&sRotSpeedY, sRotSpeedYTarget, 170);
    this->actor.shape.rot.x += sRotSpeedX;
    this->actor.shape.rot.y += sRotSpeedY;
    NvMmKusa_RandScaleVecToZero(&this->actor.velocity, 0.05f);
    Actor_UpdatePos(&this->actor);
    Actor_UpdateBgCheckInfo(play, &this->actor, 7.5f, 35.0f, 0.0f, 0xC5);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
}

static void NvMmKusa_SetupRegrow(NvMmKusa* this) {
    this->actionFunc = NvMmKusa_Regrow;
    NvMmKusa_SetScaleSmall(this);
    this->isCut = false;
    this->actor.shape.rot = this->actor.home.rot;
}

static void NvMmKusa_CutWaitRegrow(NvMmKusa* this, PlayState* play) {
    if (++this->timer >= 120) {
        NvMmKusa_SetupRegrow(this);
    }
}

static void NvMmKusa_DoNothing(NvMmKusa* this, PlayState* play) {
}

static void NvMmKusa_UprootedWaitRegrow(NvMmKusa* this, PlayState* play) {
    this->timer++;
    if ((this->timer > 120) && Math_StepToF(&this->actor.world.pos.y, this->actor.home.pos.y, 0.6f) &&
        (this->timer >= 170)) {
        NvMmKusa_SetupRegrow(this);
    }
}

static void NvMmKusa_Regrow(NvMmKusa* this, PlayState* play) {
    s32 grown = Math_StepToF(&this->actor.scale.y, 0.4f, 0.014f);

    grown &= Math_StepToF(&this->actor.scale.x, 0.4f, 0.011f);
    this->actor.scale.z = this->actor.scale.x;
    if (grown) {
        Actor_SetScale(&this->actor, 0.4f);
        NvMmKusa_SetupInteract(this);
        this->collider.base.ocFlags1 &= ~OC1_TYPE_PLAYER;
    }
}

void NvMmKusa_Update(Actor* thisx, PlayState* play) {
    NvMmKusa* this = (NvMmKusa*)thisx;

    this->actionFunc(this, play);
    thisx->shape.yOffset = this->isCut ? -6.25f : 0.0f;
    if (sSwayFrame != play->gameplayFrames) { // (MM: in normal rooms only; Termina's outdoor rooms all are)
        NvMmKusa_Sway();
        sSwayFrame = play->gameplayFrames;
    }
}

static void NvMmKusa_DrawOpa(PlayState* play, Gfx* dl) {
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, dl);
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmKusa_DrawBush(Actor* thisx, PlayState* play) {
    NvMmKusa* this = (NvMmKusa*)thisx;

    if ((thisx->projectedPos.z <= 1200.0f) || (this->isInWater && (thisx->projectedPos.z < 1300.0f))) {
        if ((this->actionFunc == NvMmKusa_WaitForInteract) && (thisx->projectedPos.z > -150.0f) &&
            (thisx->projectedPos.z < 400.0f)) {
            NvMmKusa_ApplySway(&sSway[this->mtxIdx]);
        }
        NvMmKusa_DrawOpa(play, FIELD("gKusaBushType1DL"));
    } else if (thisx->projectedPos.z < 1300.0f) { // (far: the flat bush, fading)
        OPEN_DISPS(play->state.gfxCtx);
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gDPSetPrimColor(POLY_XLU_DISP++, 0, 0, 255, 255, 255, (s32)((1300.0f - thisx->projectedPos.z) * 2.55f));
        gSPDisplayList(POLY_XLU_DISP++, FIELD("gKusaBushType2DL"));
        CLOSE_DISPS(play->state.gfxCtx);
    }
}

static void NvMmKusa_DrawGrass(Actor* thisx, PlayState* play) {
    NvMmKusa* this = (NvMmKusa*)thisx;

    if (this->isCut) {
        NvMmKusa_DrawOpa(play, KUSA("gKusaStumpDL"));
        return;
    }
    if ((this->actionFunc == NvMmKusa_WaitForInteract) && (thisx->projectedPos.z > -150.0f) &&
        (thisx->projectedPos.z < 400.0f)) {
        NvMmKusa_ApplySway(&sSway[this->mtxIdx]);
    }
    NvMmKusa_DrawOpa(play, KUSA("gKusaSproutDL"));
}
