/*
 * Nightmareverse: Majora's Mask's pots, from MM's Obj_Tsubo (zeldaret/mm, ovl_Obj_Tsubo), ported
 * closely (mm/README.md: in Termina, MM's own props). The pot is MM's, drawn by path: object_tsubo's
 * (type 2) or MM's gameplay_dangeon_keep's (types 0, 3).
 *
 * Params as MM's: the drop (& 0x3F, MM's drop table: NvMm_Drop, z_nv_mm_kibako.c), its collectible
 * flag ((>> 9) & 0x7F), the type ((>> 7) & 3); type 3 drops at random instead ((& 0xF) << 4, unless
 * bit 4). Lift, throw, break, sink, as in MM.
 *
 * Adapted from MM:
 * - MM's z-rotation spawns (1: Boes, 2: a Gold Skulltula) wait for those actors;
 * - type 3's random drop is OoT's random drop table (Item_DropCollectibleRandom, the same call);
 * - its damage flags (what it hits when thrown, what breaks it) are OoT's pots', as OoT's weapons
 *   and enemies are the ones hitting (MM's own numbers mean other things in OoT);
 * - its shadow is MM's circle (MM's gameplay_keep gCircleShadowDL) through OoT's shadow call;
 * - MM's big-fairy pots (a drop of 17) give nothing yet (as the crates);
 * - the Goht and Twinmold arenas' pots that come back aren't here (not those scenes).
 */
#include "global.h"
#include "overlays/effects/ovl_Effect_Ss_Kakera/z_eff_ss_kakera.h"

#define OBJ_TSUBO_P000F(thisx) ((thisx)->params & 0x0F)
#define OBJ_TSUBO_P003F(thisx) ((thisx)->params & 0x3F)
#define OBJ_TSUBO_P0010(thisx) (((thisx)->params >> 4) & 1)
#define OBJ_TSUBO_GET_TYPE(thisx) (((thisx)->params >> 7) & 3)
#define OBJ_TSUBO_PFE00(thisx) (((thisx)->params >> 9) & 0x7F)
#define OBJ_TSUBO_ZROT(thisx) ((thisx)->home.rot.z)
#define OBJ_TSUBO_TYPE_3 3

#define BGCHECK_HELD (4 | 0x40)            // floor, water (MM's UPDBGCHECKINFO_FLAG_4 | 40)
#define BGCHECK_THROWN (1 | 4 | 0x40 | 0x80) // and walls (MM's FLAG_1 | 4 | 40 | 80)

s32 NvMm_Drop(s32 index);
s32 NvMm_DropOnLift(s32 index);

struct NvMmTsubo;
typedef void (*NvMmTsuboFunc)(struct NvMmTsubo*, PlayState*);

typedef struct NvMmTsubo {
    Actor actor;
    NvMmTsuboFunc func;
    ColliderCylinder collider;
    s8 timer;
    s8 onStillGround;
    s8 homeRoom;
    s8 isDropCollected;
} NvMmTsubo;

const size_t gNvMmTsuboSize = sizeof(NvMmTsubo);

typedef void (*NvMmTsuboBreakFunc)(struct NvMmTsubo*, PlayState*);

typedef struct {
    f32 scale;
    Gfx* modelDL;
    Gfx* shardDL;
    s16 radius;
    s16 height;
    NvMmTsuboBreakFunc breakPot1; // in the air
    NvMmTsuboBreakFunc breakPot2; // landing in water
    NvMmTsuboBreakFunc breakPot3; // under water
} NvMmTsuboData;

static void NvMmTsubo_Break(struct NvMmTsubo* this, PlayState* play);
static void NvMmTsubo_WaterBreak(struct NvMmTsubo* this, PlayState* play);
static void NvMmTsubo_UnderwaterBreak(struct NvMmTsubo* this, PlayState* play);
static void NvMmTsubo_MagicBreak(struct NvMmTsubo* this, PlayState* play);
static void NvMmTsubo_MagicWaterBreak(struct NvMmTsubo* this, PlayState* play);
static void NvMmTsubo_MagicUnderwaterBreak(struct NvMmTsubo* this, PlayState* play);

#define DL_DANGEON_POT ((Gfx*)"__OTR__mm/objects/gameplay_dangeon_keep/gameplay_dangeon_keep_DL_017EA0")
#define DL_DANGEON_POT_SHARD ((Gfx*)"__OTR__mm/objects/gameplay_dangeon_keep/gameplay_dangeon_keep_DL_018090")
#define POT_BREAKS NvMmTsubo_Break, NvMmTsubo_WaterBreak, NvMmTsubo_UnderwaterBreak
static NvMmTsuboData sPotTypeData[4] = {
    { 0.197f, DL_DANGEON_POT, DL_DANGEON_POT_SHARD, 12, 32, POT_BREAKS },
    { 0.29549998f, (Gfx*)"__OTR__mm/objects/object_racetsubo/gMagicPotDL",
      (Gfx*)"__OTR__mm/objects/object_racetsubo/gMagicPotShardDL", 18, 45, NvMmTsubo_MagicBreak,
      NvMmTsubo_MagicWaterBreak, NvMmTsubo_MagicUnderwaterBreak },
    { 0.197f, (Gfx*)"__OTR__mm/objects/object_tsubo/gPotDL", (Gfx*)"__OTR__mm/objects/object_tsubo/gPotShardDL", 12,
      32, POT_BREAKS },
    { 0.197f, DL_DANGEON_POT, DL_DANGEON_POT_SHARD, 12, 32, POT_BREAKS },
};

static s16 sSpinXTarget = 0;
static s16 sSpinX = 0;
static s16 sSpinYTarget = 0;
static s16 sSpinY = 0;

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_HARD, AT_ON | AT_TYPE_PLAYER, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_2,
      COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000002, 0x00, 0x01 }, { 0x4FC1FFFE, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_NORMAL, BUMP_ON,
      OCELEM_ON },
    { 12, 30, 0, { 0, 0, 0 } },
};

static void NvMmTsubo_Draw(Actor* thisx, PlayState* play);

static void NvMmTsubo_CircleShadow(Actor* actor, Lights* lights, PlayState* play) {
    ActorShadow_Draw(actor, lights, play, (Gfx*)"__OTR__mm/objects/gameplay_keep/gCircleShadowDL", NULL);
}
static void NvMmTsubo_Idle(NvMmTsubo* this, PlayState* play);
static void NvMmTsubo_Held(NvMmTsubo* this, PlayState* play);
static void NvMmTsubo_Thrown(NvMmTsubo* this, PlayState* play);

/** Type 3: a random drop (OoT's table), unless bit 4 says none. */
static void NvMmTsubo_DropRandom(NvMmTsubo* this, PlayState* play) {
    if (!OBJ_TSUBO_P0010(&this->actor) && (OBJ_TSUBO_ZROT(&this->actor) != 2)) {
        Item_DropCollectibleRandom(play, NULL, &this->actor.world.pos, OBJ_TSUBO_P000F(&this->actor) << 4);
    }
}

static void NvMmTsubo_DropCollectible(NvMmTsubo* this, PlayState* play) {
    s32 drop;

    if (!this->isDropCollected && (OBJ_TSUBO_ZROT(&this->actor) != 2)) {
        drop = NvMm_Drop(OBJ_TSUBO_P003F(&this->actor));
        if (drop != ITEM00_NONE) {
            Item_DropCollectible(play, &this->actor.world.pos, ((OBJ_TSUBO_PFE00(&this->actor) & 0x3F) << 8) | drop);
            this->isDropCollected = true;
        }
    }
}

static void NvMmTsubo_Drop(NvMmTsubo* this, PlayState* play) {
    if (OBJ_TSUBO_GET_TYPE(&this->actor) == OBJ_TSUBO_TYPE_3) {
        NvMmTsubo_DropRandom(this, play);
    } else {
        NvMmTsubo_DropCollectible(this, play);
    }
}

static void NvMmTsubo_CheckRoom(NvMmTsubo* this, PlayState* play) {
    if (!this->isDropCollected && (play->roomCtx.curRoom.num != this->homeRoom)) {
        this->isDropCollected = true;
    }
}

/** Broken in the air (MM's ObjTsubo_PotBreak1). */
static void NvMmTsubo_Break(NvMmTsubo* this, PlayState* play) {
    NvMmTsuboData* typeData = &sPotTypeData[OBJ_TSUBO_GET_TYPE(&this->actor)];
    Vec3f pos;
    Vec3f velocity;
    s16 rot;
    s32 i;

    for (i = 0, rot = 0; i < 18; rot += 0x4E20, i++) {
        f32 r;
        s32 arg5;

        pos.x = Math_SinS(rot) * 8.0f;
        pos.y = Rand_ZeroOne() * 12.0f + 2.0f;
        pos.z = Math_CosS(rot) * 8.0f;
        velocity.x = pos.x * 0.23f;
        velocity.y = Rand_ZeroOne() * 5.0f + 2.5f;
        velocity.z = pos.z * 0.23f;
        Math_Vec3f_Sum(&pos, &this->actor.world.pos, &pos);
        r = Rand_ZeroOne();
        arg5 = (r < 0.2f) ? 0x60 : (r < 0.6f) ? 0x40 : 0x20;
        EffectSsKakera_Spawn(play, &pos, &velocity, &this->actor.world.pos, -260, arg5, 20, 0, 0,
                             Rand_ZeroOne() * 110.0f + 15.0f, 0, 0, 50, KAKERA_COLOR_NONE, OBJECT_GAMEPLAY_KEEP,
                             typeData->shardDL);
    }
    func_80033480(play, &this->actor.world.pos, 30.0f, 2, 20, 50, true);
    func_80033480(play, &this->actor.world.pos, 30.0f, 2, 10, 80, true);
}

/** Broken landing in water (MM's ObjTsubo_PotBreak2). */
static void NvMmTsubo_WaterBreak(NvMmTsubo* this, PlayState* play) {
    NvMmTsuboData* typeData = &sPotTypeData[OBJ_TSUBO_GET_TYPE(&this->actor)];
    Vec3f* worldPos = &this->actor.world.pos;
    Vec3f pos;
    Vec3f velocity;
    s16 rot;
    s32 i;

    pos.y = worldPos->y + this->actor.yDistToWater;
    for (rot = 0, i = 0; i < 5; i++, rot += 0x10000 / 5) {
        pos.x = worldPos->x + Math_SinS((s32)(Rand_ZeroOne() * 6000) + rot) * 15.0f;
        pos.z = worldPos->z + Math_CosS((s32)(Rand_ZeroOne() * 6000) + rot) * 15.0f;
        EffectSsGSplash_Spawn(play, &pos, NULL, NULL, 0, 350);
    }
    pos.x = worldPos->x;
    pos.z = worldPos->z;
    EffectSsGRipple_Spawn(play, &pos, 200, 600, 0);
    for (i = 0, rot = 0; i < 13; rot += 0x4E20, i++) {
        pos.x = Math_SinS(rot) * 8.0f;
        pos.y = (Rand_ZeroOne() * 5.0f) + 2.0f;
        pos.z = Math_CosS(rot) * 8.0f;
        velocity.x = pos.x * 0.2f;
        velocity.y = (Rand_ZeroOne() * 4.0f) + 2.0f;
        velocity.z = pos.z * 0.2f;
        Math_Vec3f_Sum(&pos, worldPos, &pos);
        EffectSsKakera_Spawn(play, &pos, &velocity, worldPos, -170, (Rand_ZeroOne() < .2f) ? 0x40 : 0x20, 50, 5, 0,
                             Rand_ZeroOne() * 105.0f + 10.0f, 0, 0, 70, KAKERA_COLOR_NONE, OBJECT_GAMEPLAY_KEEP,
                             typeData->shardDL);
    }
}

/** Broken under water (MM's ObjTsubo_PotBreak3). */
static void NvMmTsubo_UnderwaterBreak(NvMmTsubo* this, PlayState* play) {
    NvMmTsuboData* typeData = &sPotTypeData[OBJ_TSUBO_GET_TYPE(&this->actor)];
    Vec3f pos;
    Vec3f velocity;
    s16 rot;
    s32 i;

    for (i = 0, rot = 0; i < 13; i++, rot += 0x4E20) {
        f32 r = Rand_ZeroOne();
        f32 dist = (1.0f - SQ(r)) * 4.8f + 3.2f;

        pos.x = Math_SinS(rot) * dist;
        pos.y = (Rand_ZeroOne() * 15.0f) + 2.0f;
        pos.z = Math_CosS(rot) * dist;
        velocity.x = pos.x * 0.4f;
        velocity.y = (Rand_ZeroOne() * 4.0f) + 4.0f;
        velocity.z = pos.z * 0.4f;
        Math_Vec3f_Sum(&pos, &this->actor.world.pos, &pos);
        EffectSsKakera_Spawn(play, &pos, &velocity, &this->actor.world.pos, -170,
                             (Rand_ZeroOne() < 0.2f) ? 0x40 : 0x20, 50, 3, 0, (Rand_ZeroOne() * 105.0f) + 10.0f, 0, 0,
                             70, KAKERA_COLOR_NONE, OBJECT_GAMEPLAY_KEEP, typeData->shardDL);
    }
    for (i = 0; i < 7; i++) {
        EffectSsBubble_Spawn(play, &this->actor.world.pos, 20.0f, 30.0f, 40.0f, (Rand_ZeroOne() * 0.06f) + 0.09f);
        EffectSsBubble_Spawn(play, &this->actor.world.pos, 10.0f, 10.0f, 10.0f, (Rand_ZeroOne() * 0.08f) + 0.09f);
    }
}

/** The magic pot broken in the air (MM's ObjTsubo_MagicPotBreak1): more, bigger shards. */
static void NvMmTsubo_MagicBreak(NvMmTsubo* this, PlayState* play) {
    NvMmTsuboData* typeData = &sPotTypeData[OBJ_TSUBO_GET_TYPE(&this->actor)];
    Vec3f pos;
    Vec3f velocity;
    s16 rot;
    s32 i;

    for (i = 0, rot = 0; i < 20; rot += 0x4E20, i++) {
        f32 r;
        s32 arg5;

        pos.x = Math_SinS(rot) * 10.0f;
        pos.y = Rand_ZeroOne() * 20.0f + 2.0f;
        pos.z = Math_CosS(rot) * 10.0f;
        velocity.x = pos.x * 0.3f;
        velocity.y = Rand_ZeroOne() * 10.0f + 2.0f;
        velocity.z = pos.z * 0.3f;
        Math_Vec3f_Sum(&pos, &this->actor.world.pos, &pos);
        r = Rand_ZeroOne();
        arg5 = (r < 0.2f) ? 0xE0 : (r < 0.6f) ? 0xC0 : 0xA0;
        EffectSsKakera_Spawn(play, &pos, &velocity, &this->actor.world.pos, -340, arg5, 20, 0, 0,
                             Rand_ZeroOne() * 160.0f + 15.0f, 0, 0, 50, KAKERA_COLOR_NONE, OBJECT_GAMEPLAY_KEEP,
                             typeData->shardDL);
    }
    func_80033480(play, &this->actor.world.pos, 50.0f, 2, 40, 50, true);
    func_80033480(play, &this->actor.world.pos, 50.0f, 2, 20, 80, true);
}

/** The magic pot landing in water (MM's ObjTsubo_MagicPotBreak2). */
static void NvMmTsubo_MagicWaterBreak(NvMmTsubo* this, PlayState* play) {
    NvMmTsuboData* typeData = &sPotTypeData[OBJ_TSUBO_GET_TYPE(&this->actor)];
    Vec3f* worldPos = &this->actor.world.pos;
    Vec3f pos;
    Vec3f velocity;
    s16 rot;
    s32 i;

    pos.y = worldPos->y + this->actor.yDistToWater;
    for (rot = 0, i = 0; i < 5; i++, rot += 0x10000 / 5) {
        pos.x = worldPos->x + Math_SinS((s32)(Rand_ZeroOne() * 6000) + rot) * 30.0f;
        pos.z = worldPos->z + Math_CosS((s32)(Rand_ZeroOne() * 6000) + rot) * 30.0f;
        EffectSsGSplash_Spawn(play, &pos, NULL, NULL, 0, 350);
    }
    pos.x = worldPos->x;
    pos.z = worldPos->z;
    EffectSsGRipple_Spawn(play, &pos, 300, 700, 0);
    for (i = 0, rot = 0; i < 15; rot += 0x4E20, i++) {
        pos.x = Math_SinS(rot) * 10.0f;
        pos.y = (Rand_ZeroOne() * 15.0f) + 2.0f;
        pos.z = Math_CosS(rot) * 10.0f;
        velocity.x = pos.x * 0.3f;
        velocity.y = (Rand_ZeroOne() * 4.0f) + 2.0f;
        velocity.z = pos.z * 0.3f;
        Math_Vec3f_Sum(&pos, worldPos, &pos);
        EffectSsKakera_Spawn(play, &pos, &velocity, worldPos, -170, (Rand_ZeroOne() < 0.2f) ? 0xC0 : 0xA0, 50, 5, 0,
                             (s16)((Rand_ZeroOne() * 150.0f) + 10.0f), 0, 0, 70, KAKERA_COLOR_NONE,
                             OBJECT_GAMEPLAY_KEEP, typeData->shardDL);
    }
}

/** The magic pot under water: MM's ObjTsubo_MagicPotBreak3 does nothing. */
static void NvMmTsubo_MagicUnderwaterBreak(NvMmTsubo* this, PlayState* play) {
}

void NvMmTsubo_Init(Actor* thisx, PlayState* play) {
    NvMmTsubo* this = (NvMmTsubo*)thisx;
    s32 type = OBJ_TSUBO_GET_TYPE(&this->actor);

    this->actor.gravity = -2.0f;
    this->actor.minVelocityY = -20.0f;
    this->actor.uncullZoneForward = 4000.0f;
    this->actor.uncullZoneScale = 100.0f;
    this->actor.uncullZoneDownward = 100.0f;
    Actor_SetScale(&this->actor, sPotTypeData[type].scale);
    this->actor.shape.rot.z = 0;
    this->actor.world.rot.z = 0;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    this->collider.dim.radius = sPotTypeData[type].radius;
    this->collider.dim.height = sPotTypeData[type].height;
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    this->actor.shape.shadowScale = 1.8f;
    this->homeRoom = this->actor.room;
    // (MM waits for its object to load; this draws by path at once)
    this->actor.draw = NvMmTsubo_Draw;
    this->onStillGround = false;
    this->func = NvMmTsubo_Idle;
}

void NvMmTsubo_Destroy(Actor* thisx, PlayState* play) {
    NvMmTsubo* this = (NvMmTsubo*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static void NvMmTsubo_Idle(NvMmTsubo* this, PlayState* play) {
    s32 type = OBJ_TSUBO_GET_TYPE(&this->actor);
    s32 acHit = (this->collider.base.acFlags & AC_HIT) != 0;

    if (acHit) {
        this->collider.base.acFlags &= ~AC_HIT;
    }
    if (this->collider.base.ocFlags1 & OC1_HIT) {
        this->collider.base.ocFlags1 &= ~OC1_HIT;
    }
    if (Actor_HasParent(&this->actor, play)) {
        this->actor.room = -1;
        this->actor.flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
        if ((type != OBJ_TSUBO_TYPE_3) && NvMm_DropOnLift(OBJ_TSUBO_P003F(&this->actor))) {
            NvMmTsubo_DropCollectible(this, play);
        }
        Player_PlaySfx(&GET_PLAYER(play)->actor, NA_SE_PL_PULL_UP_POT);
        this->func = NvMmTsubo_Held;
    } else if (acHit && (this->collider.info.acHitInfo->toucher.dmgFlags & 0x4FC1FFFC)) {
        if ((this->actor.bgCheckFlags & BGCHECKFLAG_WATER) && (this->actor.yDistToWater > 15.0f)) {
            sPotTypeData[type].breakPot3(this, play);
        } else {
            sPotTypeData[type].breakPot1(this, play);
        }
        NvMmTsubo_Drop(this, play);
        SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EV_POT_BROKEN);
        Actor_Kill(&this->actor);
    } else {
        if (!this->onStillGround) {
            Actor_MoveXZGravity(&this->actor);
            Actor_UpdateBgCheckInfo(play, &this->actor, 15.0f, 15.0f, 0.0f, BGCHECK_HELD);
            if ((this->actor.bgCheckFlags & BGCHECKFLAG_GROUND) &&
                (DynaPoly_GetActor(&play->colCtx, this->actor.floorBgId) == NULL)) {
                this->onStillGround = true;
                this->actor.flags &= ~ACTOR_FLAG_UPDATE_CULLING_DISABLED;
            }
        }
        if (this->actor.xzDistToPlayer < 800.0f) {
            Collider_UpdateCylinder(&this->actor, &this->collider);
            CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
            if (this->actor.xzDistToPlayer < 150.0f) {
                CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
                if (this->actor.xzDistToPlayer < 100.0f) {
                    s16 yawDiff = this->actor.yawTowardsPlayer - GET_PLAYER(play)->actor.world.rot.y;

                    if (ABS(yawDiff) > (0x10000 / 3)) {
                        Actor_OfferGetItem(&this->actor, play, GI_NONE, 36.0f, 30.0f);
                    }
                }
            }
        }
    }
}

static void NvMmTsubo_SetupThrown(NvMmTsubo* this) {
    this->timer = 80;
    this->actor.colChkInfo.mass = 180;
    sSpinXTarget = (Rand_ZeroOne() - 0.78f) * 4500.0f;
    sSpinYTarget = (Rand_ZeroOne() - 0.5f) * 2000.0f;
    sSpinX = 0;
    sSpinY = 0;
    this->func = NvMmTsubo_Thrown;
}

static void NvMmTsubo_Held(NvMmTsubo* this, PlayState* play) {
    Vec3f pos;
    s32 bgId;

    NvMmTsubo_CheckRoom(this, play);
    if (Actor_HasNoParent(&this->actor, play)) {
        this->actor.room = play->roomCtx.curRoom.num;
        Actor_MoveXZGravity(&this->actor);
        this->actor.flags &= ~ACTOR_FLAG_CAN_PRESS_SWITCHES;
        Actor_UpdateBgCheckInfo(play, &this->actor, 15.0f, 15.0f, 0.0f, BGCHECK_THROWN);
        NvMmTsubo_SetupThrown(this);
    } else {
        pos.x = this->actor.world.pos.x;
        pos.y = this->actor.world.pos.y + 20.0f;
        pos.z = this->actor.world.pos.z;
        this->actor.floorHeight =
            BgCheck_EntityRaycastFloor5(play, &play->colCtx, &this->actor.floorPoly, &bgId, &this->actor, &pos);
    }
}

static void NvMmTsubo_Thrown(NvMmTsubo* this, PlayState* play) {
    s32 atHit = (this->collider.base.atFlags & AT_HIT) != 0;

    if (atHit) {
        this->collider.base.atFlags &= ~AT_HIT;
    }
    NvMmTsubo_CheckRoom(this, play);
    if (this->timer > 0) {
        this->timer--;
    }
    if ((this->actor.bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH | BGCHECKFLAG_WALL)) || atHit ||
        (this->timer <= 0)) {
        sPotTypeData[OBJ_TSUBO_GET_TYPE(&this->actor)].breakPot1(this, play);
        NvMmTsubo_Drop(this, play);
        SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EV_POT_BROKEN);
        Actor_Kill(&this->actor);
    } else if (this->actor.bgCheckFlags & BGCHECKFLAG_WATER_TOUCH) {
        sPotTypeData[OBJ_TSUBO_GET_TYPE(&this->actor)].breakPot2(this, play);
        NvMmTsubo_Drop(this, play);
        SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EV_POT_BROKEN);
        SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, NA_SE_EV_DIVE_INTO_WATER_L);
        Actor_Kill(&this->actor);
    } else {
        Actor_MoveXZGravity(&this->actor);
        Math_StepToS(&sSpinX, sSpinXTarget, 150);
        Math_StepToS(&sSpinY, sSpinYTarget, 150);
        this->actor.shape.rot.x += sSpinX;
        this->actor.shape.rot.y += sSpinY;
        Actor_UpdateBgCheckInfo(play, &this->actor, 15.0f, 15.0f, 0.0f, BGCHECK_THROWN);
        Collider_UpdateCylinder(&this->actor, &this->collider);
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
}

void NvMmTsubo_Update(Actor* thisx, PlayState* play) {
    NvMmTsubo* this = (NvMmTsubo*)thisx;

    this->func(this, play);
    if (this->actor.draw == NULL) {
        this->actor.shape.shadowDraw = NULL;
    } else if (this->actor.projectedPos.z < 811.0f) {
        if (this->actor.projectedPos.z > 300.0f) {
            this->actor.shape.shadowAlpha = (811 - (s32)this->actor.projectedPos.z) >> 1;
            this->actor.shape.shadowDraw = NvMmTsubo_CircleShadow;
        } else if (this->actor.projectedPos.z > -10.0f) {
            this->actor.shape.shadowAlpha = 255;
            this->actor.shape.shadowDraw = NvMmTsubo_CircleShadow;
        } else {
            this->actor.shape.shadowDraw = NULL;
        }
    } else {
        this->actor.shape.shadowDraw = NULL;
    }
}

static void NvMmTsubo_Draw(Actor* thisx, PlayState* play) {
    Gfx_DrawDListOpa(play, sPotTypeData[OBJ_TSUBO_GET_TYPE(thisx)].modelDL);
}
