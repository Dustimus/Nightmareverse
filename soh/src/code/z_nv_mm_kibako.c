/*
 * Nightmareverse: Majora's Mask's small crates, from MM's Obj_Kibako (zeldaret/mm, ovl_Obj_Kibako),
 * ported closely (mm/README.md). In Termina, MM's own props are used, not OoT's look-alikes: the
 * crate is MM's, drawn by path: object_kibako's outdoors (params bit 15 set), MM's
 * gameplay_dangeon_keep's indoors. Its square shadow is MM's (gameplay_keep's gSquareShadowDL).
 *
 * Params as MM's: the drop (& 0x3F, MM's drop table, below), its collectible flag ((>> 8) & 0x7F),
 * bit 7: one a Bomber can hide under (the hide-and-seek game; not yet here), bit 15: which crate.
 *
 * Adapted from MM:
 * - MM's drop table (D_801AE194) gives MM's collectible types; here OoT's of the same thing.
 *   MM's compass and the mushroom spore have no OoT collectible (nothing drops). A collectible flag
 *   past 0x3F is cut to OoT's six bits, as for Termina's other collectibles (mm_actors.py);
 * - OoT's movement, ground-check and carry calls for MM's; the sounds are OoT's, the same ones;
 * - MM's lift sound call passed the crate as Link (a bug noted in the decomp); here it's Link's.
 */
#include "global.h"
#include "overlays/effects/ovl_Effect_Ss_Kakera/z_eff_ss_kakera.h"

#define KIBAKO_COLLECTIBLE_ID(thisx) ((thisx)->params & 0x3F)
#define KIBAKO_COLLECTIBLE_FLAG(thisx) (((thisx)->params >> 0x8) & 0x7F)
#define KIBAKO_BANK_INDEX(thisx) (((thisx)->params >> 0xF) & 1)
#define KIBAKO_BOMBER_CAN_HIDE_IN_BOX(thisx) (((thisx)->params >> 7) & 1)

#define BGCHECK_FLAGS (1 | 4 | 0x40) // walls, floor, water (MM's UPDBGCHECKINFO_FLAG_1 | 4 | 40)

static Gfx* sDisplayLists[] = {
    (Gfx*)"__OTR__mm/objects/gameplay_dangeon_keep/gameplay_dangeon_keep_DL_007890",
    (Gfx*)"__OTR__mm/objects/object_kibako/gSmallCrateDL",
};
static Gfx* sKakeraDisplayLists[] = {
    (Gfx*)"__OTR__mm/objects/gameplay_dangeon_keep/gameplay_dangeon_keep_DL_007980",
    (Gfx*)"__OTR__mm/objects/object_kibako/gSmallCrateFragmentDL",
};
#define DL_SQUARE_SHADOW ((Gfx*)"__OTR__mm/objects/gameplay_keep/gSquareShadowDL")

struct NvMmKibako;
typedef void (*NvMmKibakoFunc)(struct NvMmKibako*, PlayState*);

typedef struct NvMmKibako {
    Actor actor;
    NvMmKibakoFunc func;
    ColliderCylinder collider;
    s16 timer;
    s8 isDropCollected;
    s8 homeRoom;
} NvMmKibako;

const size_t gNvMmKibakoSize = sizeof(NvMmKibako);

static s16 sSpinTarget = 0;
static s16 sSpinX = 0;
static s16 sSpinYTarget = 0;
static s16 sSpinY = 0;

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_NONE, AT_ON | AT_TYPE_PLAYER, AC_ON | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_2,
      COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00400000, 0x00, 0x02 }, { 0x058BC79C, 0x00, 0x00 }, TOUCH_ON | TOUCH_SFX_NONE, BUMP_ON,
      OCELEM_ON },
    { 15, 30, 0, { 0, 0, 0 } },
};

// MM's drop table (D_801AE194), MM's collectibles as OoT's; ITEM00_NONE where nothing drops
static u8 sDrops[32] = {
    ITEM00_NONE,         ITEM00_RUPEE_GREEN, ITEM00_RUPEE_BLUE, ITEM00_NONE,  ITEM00_RUPEE_RED,
    ITEM00_RUPEE_PURPLE, ITEM00_NONE,        ITEM00_RUPEE_ORANGE, ITEM00_NONE, ITEM00_NONE,
    ITEM00_HEART,        ITEM00_HEART /* MM: 3 hearts (OoT has no such drop) */, ITEM00_HEART_PIECE, ITEM00_HEART_CONTAINER,
    ITEM00_MAGIC_SMALL,  ITEM00_MAGIC_LARGE, ITEM00_FLEXIBLE,   ITEM00_NONE /* big fairy */, ITEM00_NONE,
    ITEM00_NUTS,         ITEM00_NONE,        ITEM00_BOMBS_A,    ITEM00_NONE,  ITEM00_NONE,
    ITEM00_NONE,         ITEM00_STICK,       ITEM00_NONE,       ITEM00_NONE,  ITEM00_NONE,
    ITEM00_NONE,         ITEM00_ARROWS_SMALL, ITEM00_ARROWS_MEDIUM,
};
// MM's D_801AE214: these drop as soon as the crate is lifted
static u8 sDropOnLift[32] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0,
                              0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

static void NvMmKibako_Draw(Actor* thisx, PlayState* play);
static void NvMmKibako_Idle(NvMmKibako* this, PlayState* play);
static void NvMmKibako_Held(NvMmKibako* this, PlayState* play);
static void NvMmKibako_Thrown(NvMmKibako* this, PlayState* play);

static void NvMmKibako_SquareShadow(Actor* actor, Lights* lights, PlayState* play) {
    ActorShadow_Draw(actor, lights, play, DL_SQUARE_SHADOW, NULL);
}

static void NvMmKibako_SpawnCollectible(NvMmKibako* this, PlayState* play) {
    s32 id = KIBAKO_COLLECTIBLE_ID(&this->actor);
    s32 drop;

    if (this->isDropCollected == 0) {
        drop = (id < 32) ? sDrops[id] : ITEM00_NONE;
        if (drop != ITEM00_NONE) {
            Item_DropCollectible(play, &this->actor.world.pos,
                                 drop | ((KIBAKO_COLLECTIBLE_FLAG(&this->actor) & 0x3F) << 8));
            this->isDropCollected = 1;
        }
    }
}

static void NvMmKibako_SetShadow(NvMmKibako* this) {
    if ((this->actor.projectedPos.z < 370.0f) && (this->actor.projectedPos.z > -10.0f)) {
        this->actor.shape.shadowDraw = NvMmKibako_SquareShadow;
        this->actor.shape.shadowScale = 1.4f;
        this->actor.shape.shadowAlpha =
            (this->actor.projectedPos.z < 200.0f) ? 100 : (400 - ((s32)this->actor.projectedPos.z)) >> 1;
    } else {
        this->actor.shape.shadowDraw = NULL;
    }
}

/** Turns it to the nearest quarter, as a pushed crate settles. */
static void NvMmKibako_Square(NvMmKibako* this) {
    s16 angle = this->actor.world.rot.y;

    if ((angle & 0x3FFF) != 0) {
        Math_ScaledStepToS(&this->actor.world.rot.y, (s16)(angle + 0x2000) & 0xC000, 0x640);
        this->actor.shape.rot.y = this->actor.world.rot.y;
    }
}

/** Link can pick it up when near and facing it. */
static void NvMmKibako_OfferCarry(NvMmKibako* this, PlayState* play) {
    s16 angle;

    if (this->actor.xzDistToPlayer < 100.0f) {
        angle = this->actor.yawTowardsPlayer - GET_PLAYER(play)->actor.world.rot.y;
        if (ABS(angle) > 0x5555) {
            Actor_OfferGetItem(&this->actor, play, GI_NONE, 36.0f, 30.0f);
        }
    }
}

static void NvMmKibako_CheckRoom(NvMmKibako* this, PlayState* play) {
    if ((this->isDropCollected == 0) && (play->roomCtx.curRoom.num != this->homeRoom)) {
        this->isDropCollected = 1;
    }
}

static void NvMmKibako_AirBreak(NvMmKibako* this, PlayState* play) {
    s16 angle;
    s32 i;
    Vec3f* worldPos = &this->actor.world.pos;
    Vec3f pos;
    Vec3f velocity;

    for (i = 0, angle = 0; i < 12; i++, angle += 0x4E20) {
        f32 sn = Math_SinS(angle);
        f32 cs = Math_CosS(angle);
        f32 r;
        s16 arg5;

        pos.x = sn * 16.0f;
        pos.y = (Rand_ZeroOne() * 5.0f) + 2.0f;
        pos.z = cs * 16.0f;
        velocity.x = pos.x * 0.2f;
        velocity.y = (Rand_ZeroOne() * 6.0f) + 2.0f;
        velocity.z = pos.z * 0.2f;
        pos.x += worldPos->x;
        pos.y += worldPos->y;
        pos.z += worldPos->z;
        r = Rand_ZeroOne();
        arg5 = (r < 0.1f) ? 0x60 : (r < 0.7f) ? 0x40 : 0x20;
        EffectSsKakera_Spawn(play, &pos, &velocity, worldPos, -200, arg5, 20, 0, 0, (Rand_ZeroOne() * 38.0f) + 10.0f,
                             0, 0, 60, KAKERA_COLOR_NONE, OBJECT_GAMEPLAY_KEEP,
                             sKakeraDisplayLists[KIBAKO_BANK_INDEX(&this->actor)]);
    }
    func_80033480(play, worldPos, 40.0f, 3, 0x32, 0x8C, 1);
    func_80033480(play, worldPos, 40.0f, 2, 0x14, 0x50, 1);
}

static void NvMmKibako_WaterBreak(NvMmKibako* this, PlayState* play) {
    s16 angle;
    s32 i;
    Vec3f* worldPos = &this->actor.world.pos;
    Vec3f pos;
    Vec3f velocity;

    pos.y = worldPos->y + this->actor.yDistToWater;
    for (angle = 0, i = 0; i < 5; i++, angle += 0x3333) {
        pos.x = worldPos->x + (Math_SinS(((s32)(Rand_ZeroOne() * 6000.0f)) + angle) * 15.0f);
        pos.z = worldPos->z + (Math_CosS(((s32)(Rand_ZeroOne() * 6000.0f)) + angle) * 15.0f);
        EffectSsGSplash_Spawn(play, &pos, NULL, NULL, 0, 350);
    }
    pos.x = worldPos->x;
    pos.z = worldPos->z;
    EffectSsGRipple_Spawn(play, &pos, 200, 600, 0);
    for (i = 0, angle = 0; i < 12; i++, angle += 0x4E20) {
        f32 sn = Math_SinS(angle);
        f32 cs = Math_CosS(angle);

        pos.x = sn * 16.0f;
        pos.y = (Rand_ZeroOne() * 5.0f) + 2.0f;
        pos.z = cs * 16.0f;
        velocity.x = pos.x * 0.18f;
        velocity.y = (Rand_ZeroOne() * 4.0f) + 2.0f;
        velocity.z = pos.z * 0.18f;
        pos.x += worldPos->x;
        pos.y += worldPos->y;
        pos.z += worldPos->z;
        EffectSsKakera_Spawn(play, &pos, &velocity, worldPos, -180, (Rand_ZeroOne() < 0.2f) ? 0x40 : 0x20, 50, 5, 0,
                             (Rand_ZeroOne() * 35.0f) + 10.0f, 0, 0, 70, KAKERA_COLOR_NONE, OBJECT_GAMEPLAY_KEEP,
                             sKakeraDisplayLists[KIBAKO_BANK_INDEX(&this->actor)]);
    }
}

static void NvMmKibako_Break(NvMmKibako* this, PlayState* play, s32 water) {
    if (water) {
        NvMmKibako_WaterBreak(this, play);
    } else {
        NvMmKibako_AirBreak(this, play);
    }
    NvMmKibako_SpawnCollectible(this, play);
    SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 20, NA_SE_EV_WOODBOX_BREAK);
    if (water) {
        SoundSource_PlaySfxAtFixedWorldPos(play, &this->actor.world.pos, 40, NA_SE_EV_DIVE_INTO_WATER_L);
    }
    Actor_Kill(&this->actor);
}

void NvMmKibako_Init(Actor* thisx, PlayState* play) {
    NvMmKibako* this = (NvMmKibako*)thisx;

    this->actor.gravity = -1.5f;
    this->actor.minVelocityY = -18.0f;
    this->actor.uncullZoneScale = 60.0f;
    this->actor.uncullZoneDownward = 60.0f;
    Actor_SetScale(&this->actor, 0.15f);
    this->actor.uncullZoneForward = (KIBAKO_BANK_INDEX(thisx) == 0) ? 4000.0f : 800.0f;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, &this->actor, &sCylinderInit);
    Collider_UpdateCylinder(&this->actor, &this->collider);
    this->actor.colChkInfo.mass = MASS_IMMOVABLE;
    this->homeRoom = this->actor.room;
    this->actor.draw = NvMmKibako_Draw; // (MM waits for its object to load; this draws by path)
    this->func = NvMmKibako_Idle;
}

void NvMmKibako_Destroy(Actor* thisx, PlayState* play) {
    NvMmKibako* this = (NvMmKibako*)thisx;

    Collider_DestroyCylinder(play, &this->collider);
}

static void NvMmKibako_Idle(NvMmKibako* this, PlayState* play) {
    if (Actor_HasParent(&this->actor, play)) {
        this->func = NvMmKibako_Held;
        this->actor.room = -1;
        this->actor.colChkInfo.mass = 120;
        if (sDropOnLift[KIBAKO_COLLECTIBLE_ID(&this->actor) & 0x1F]) {
            NvMmKibako_SpawnCollectible(this, play);
        }
        Player_PlaySfx(&GET_PLAYER(play)->actor, NA_SE_PL_PULL_UP_WOODBOX);
    } else if ((this->actor.bgCheckFlags & BGCHECKFLAG_WATER) && (this->actor.yDistToWater > 19.0f)) {
        NvMmKibako_Break(this, play, true);
    } else if (this->collider.base.acFlags & AC_HIT) {
        NvMmKibako_Break(this, play, false);
    } else {
        Actor_MoveXZGravity(&this->actor);
        NvMmKibako_Square(this);
        Actor_UpdateBgCheckInfo(play, &this->actor, 18.0f, 15.0f, 0.0f, BGCHECK_FLAGS);
        if (!(this->collider.base.ocFlags1 & OC1_TYPE_PLAYER) && (this->actor.xzDistToPlayer > 28.0f)) {
            this->collider.base.ocFlags1 |= OC1_TYPE_PLAYER;
        }
        if ((this->actor.colChkInfo.mass != MASS_IMMOVABLE) &&
            (Math3D_Vec3fDistSq(&this->actor.world.pos, &this->actor.prevPos) < 0.01f)) {
            this->actor.colChkInfo.mass = MASS_IMMOVABLE;
        }
        this->collider.base.acFlags &= ~AC_HIT;
        if (KIBAKO_BOMBER_CAN_HIDE_IN_BOX(&this->actor)) {
            Collider_UpdateCylinder(&this->actor, &this->collider);
            CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
            if (this->actor.xzDistToPlayer < 800.0f) {
                CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
                NvMmKibako_OfferCarry(this, play);
            }
        } else if (this->actor.xzDistToPlayer < 800.0f) {
            Collider_UpdateCylinder(&this->actor, &this->collider);
            CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
            if (this->actor.xzDistToPlayer < 180.0f) {
                CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
                NvMmKibako_OfferCarry(this, play);
            }
        }
    }
}

static void NvMmKibako_SetupThrown(NvMmKibako* this) {
    f32 r;

    sSpinTarget = 0;
    r = (Rand_ZeroOne() - 0.5f) * 1000.0f;
    sSpinYTarget = r;
    sSpinX = (Rand_ZeroOne() - 2.0f) * 1500.0f;
    sSpinY = r * 3.0f;
    this->timer = 80;
    this->func = NvMmKibako_Thrown;
}

static void NvMmKibako_Held(NvMmKibako* this, PlayState* play) {
    Vec3f pos;
    s32 bgId;

    NvMmKibako_CheckRoom(this, play);
    if (Actor_HasNoParent(&this->actor, play)) {
        this->actor.room = play->roomCtx.curRoom.num;
        if (fabsf(this->actor.speedXZ) < 0.1f) {
            this->func = NvMmKibako_Idle;
            this->collider.base.ocFlags1 &= ~OC1_TYPE_PLAYER;
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_PUT_DOWN_WOODBOX);
        } else {
            Actor_MoveXZGravity(&this->actor);
            NvMmKibako_SetupThrown(this);
            this->actor.flags &= ~ACTOR_FLAG_CAN_PRESS_SWITCHES;
        }
        Actor_UpdateBgCheckInfo(play, &this->actor, 18.0f, 15.0f, 0.0f, BGCHECK_FLAGS);
    } else {
        pos.x = this->actor.world.pos.x;
        pos.y = this->actor.world.pos.y + 20.0f;
        pos.z = this->actor.world.pos.z;
        this->actor.floorHeight =
            BgCheck_EntityRaycastFloor5(play, &play->colCtx, &this->actor.floorPoly, &bgId, &this->actor, &pos);
    }
}

static void NvMmKibako_Thrown(NvMmKibako* this, PlayState* play) {
    s32 atHit = (this->collider.base.atFlags & AT_HIT) != 0;

    if (atHit) {
        this->collider.base.atFlags &= ~AT_HIT;
    }
    NvMmKibako_CheckRoom(this, play);
    if (this->timer > 0) {
        this->timer--;
    }
    if ((this->actor.bgCheckFlags & (BGCHECKFLAG_GROUND | BGCHECKFLAG_GROUND_TOUCH | BGCHECKFLAG_WALL)) || atHit ||
        (this->timer <= 0)) {
        NvMmKibako_Break(this, play, false);
    } else if (this->actor.bgCheckFlags & BGCHECKFLAG_WATER_TOUCH) {
        NvMmKibako_Break(this, play, true);
    } else {
        if (this->actor.velocity.y < -0.05f) {
            this->actor.gravity = -2.3f;
        }
        Actor_MoveXZGravity(&this->actor);
        Math_StepToS(&sSpinX, sSpinTarget, 0xA0);
        Math_StepToS(&sSpinY, sSpinYTarget, 0xA0);
        this->actor.shape.rot.x = (s16)(this->actor.shape.rot.x + sSpinX);
        this->actor.shape.rot.y = (s16)(this->actor.shape.rot.y + sSpinY);
        Actor_UpdateBgCheckInfo(play, &this->actor, 18.0f, 15.0f, 0.0f, BGCHECK_FLAGS);
        Collider_UpdateCylinder(&this->actor, &this->collider);
        CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
        CollisionCheck_SetAT(play, &play->colChkCtx, &this->collider.base);
    }
}

void NvMmKibako_Update(Actor* thisx, PlayState* play) {
    NvMmKibako* this = (NvMmKibako*)thisx;

    this->func(this, play);
    NvMmKibako_SetShadow(this);
}

static void NvMmKibako_Draw(Actor* thisx, PlayState* play) {
    Gfx_DrawDListOpa(play, sDisplayLists[KIBAKO_BANK_INDEX(thisx)]);
}
