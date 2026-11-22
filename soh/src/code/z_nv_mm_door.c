/*
 * Nightmareverse: Majora's Mask's wooden doors, from MM's En_Door (zeldaret/mm, ovl_En_Door), ported
 * closely (mm/README.md: in Termina, MM's own props). MM's door skeleton and Link-is-human opening
 * animations (MM's gameplay_keep), with the door face by where it is: the shop doors' own
 * (object_wdor01-05) for a door with a schedule, else MM's plain door (or its field keep's, where
 * the scene loads that keep), all drawn by path.
 *
 * MM's params: type ((>> 7) & 7) and its value (& 0x7F):
 *   0, 2, 3  closed on some days, dawns or nights (which: MM's D_801AED48, by the value's low 3
 *            bits), with a message (0x182D, 0x181D or 0x180D, plus the value's next 4 bits)
 *   1        locked by a small key (its switch flag)
 *   4        ajar: swings open, slams shut as Link nears
 *   5        a shop's door, by its schedule (mm/schedules/En_Door.schl): open, or closed with
 *            message 0x1800 + what the schedule returns ("Out delivering mail", ...)
 * A schedule actor (the Postman, ...) can open it by setting openTimer.
 *
 * Adapted from MM:
 * - Link's door code is OoT's: it takes this for one of OoT's doors (OoT's EnDoor's first fields,
 *   animStyle and playerIsOpening, are kept at the same place), and goes through a scene exit
 *   when the door's params say OoT's DOOR_SCENEEXIT. So MM's params are kept in mmParams, and
 *   the actor's are OoT's: a scene exit where both sides are the same room (every Clock Town
 *   door), else a room door;
 * - a closed door's message is the door offering to talk (OoT's player has no talking door type);
 * - the opening animation is the human one, left or right by OoT's animStyle (no forms yet);
 * - MM's framed doors (type 7) and the Milk Bar's member door (0x1821) are left out for now.
 */
#include "global.h"
#include "z_nv_mm_schedule.h"
#include "overlays/actors/ovl_En_Door/z_en_door.h" // (OoT's door types, for Link's door code)

#define ENDOOR_GET_TYPE(params) (((params) >> 7) & 7)
#define ENDOOR_GET_TYPE_VAR(params) ((params) & 0x7F)
#define HALFDAYCHECK_INDEX(v) ((v) & 7)
#define HALFDAYCHECK_TEXT(v) (((v) >> 3) & 0xF)

enum {
    ENDOOR_TYPE_WHOLE_DAY, ENDOOR_TYPE_LOCKED, ENDOOR_TYPE_DAY, ENDOOR_TYPE_NIGHT, ENDOOR_TYPE_AJAR,
    ENDOOR_TYPE_SCHEDULE, ENDOOR_TYPE_6, ENDOOR_TYPE_FRAMED
};

#define DOOR_LIMB_3 3
#define DOOR_LIMB_4 4
#define DOOR_LIMB_MAX 5
#define DOOR_AJAR_SLAM_RANGE 120.0f
#define DOOR_AJAR_OPEN_RANGE (2 * DOOR_AJAR_SLAM_RANGE)
#define NV_MM_TEXT(id) (0xB000 + (id))

#define MM_KEEP(name) ((void*)"__OTR__mm/objects/gameplay_keep/" name)
static const char sDoorSkel[] = "__OTR__mm/objects/gameplay_keep/gDoorSkel";
static AnimationHeader* sAnimations[] = { MM_KEEP("gDoorHumanOpenLeftAnim"), MM_KEEP("gDoorHumanOpenRightAnim") };
static u8 sAnimOpenFrames[] = { 25, 25 };   // (MM's, human Link)
static u8 sAnimCloseFrames[] = { 70, 70 };

enum {
    DOOR_DL_DEFAULT, DOOR_DL_LOTTERY_CURIOSITY_SHOP_MAYOR_HOUSE, DOOR_DL_POST_OFFICE_TRADING_POST,
    DOOR_DL_INN_SCHOOL, DOOR_DL_MILK_BAR, DOOR_DL_MUSIC_BOX, DOOR_DL_DEFAULT_FIELD_KEEP, DOOR_DL_MAX
};

static Gfx* sDoorDLists[DOOR_DL_MAX][2] = {
    { MM_KEEP("gDoorLeftDL"), MM_KEEP("gDoorRightDL") },
    { (Gfx*)"__OTR__mm/objects/object_wdor01/gLotteryCuriosityShopMayorHouseDoorDL",
      (Gfx*)"__OTR__mm/objects/object_wdor01/gLotteryCuriosityShopMayorHouseDoorDL" },
    { (Gfx*)"__OTR__mm/objects/object_wdor02/gPostOfficeTradingPostDoorDL",
      (Gfx*)"__OTR__mm/objects/object_wdor02/gPostOfficeTradingPostDoorDL" },
    { (Gfx*)"__OTR__mm/objects/object_wdor03/gInnSchoolDoorDL",
      (Gfx*)"__OTR__mm/objects/object_wdor03/gInnSchoolDoorDL" },
    { (Gfx*)"__OTR__mm/objects/object_wdor04/gMilkBarDoorDL", (Gfx*)"__OTR__mm/objects/object_wdor04/gMilkBarDoorDL" },
    { (Gfx*)"__OTR__mm/objects/object_wdor05/gMusicBoxHouseDoorDL",
      (Gfx*)"__OTR__mm/objects/object_wdor05/gMusicBoxHouseDoorDL" },
    { (Gfx*)"__OTR__mm/objects/gameplay_field_keep/gFieldWoodDoorLeftDL",
      (Gfx*)"__OTR__mm/objects/gameplay_field_keep/gFieldWoodDoorRightDL" },
};

#include "nv_mm_schedules/En_Door.schl.inc"

/** MM's sDoorSchedules, by the door's schedule type, and the door face each uses (MM's sObjectInfo). */
static u8* sDoorSchedules[] = {
    sDoorSch_SwordsmansSchool, sDoorSch_PostOffice, sDoorSch_LotteryShop, sDoorSch_TradingPost,
    sDoorSch_CuriosityShop, sDoorSch_LaundryPool, sDoorSch_BombShop, sDoorSch_TownShootingGallery,
    sDoorSch_TreasureChestShop, sDoorSch_HoneyDarlingShop, sDoorSch_MilkBar, sDoorSch_InnMainEntrance,
    sDoorSch_InnUpperEntrance, sDoorSch_InnGrannys, sDoorSch_InnStaffRoom, sDoorSch_InnKnifeChamber,
    sDoorSch_InnLargeSuite, sDoorSch_MayorsResidenceMainEntrance, sDoorSch_MayorsResidenceMayorDotour,
    sDoorSch_MayorsResidenceMadameAroma, sDoorSch_MayorsResidenceBedroom, D_80867710,
    sDoorSch_RomaniRanchMamasHouse, sDoorSch_RomaniRanchBarn, sDoorSch_RomaniRanchCuccoShack,
    sDoorSch_RomaniRanchDoggyRacetrack, sDoorSch_RomaniRanchBedroom, sDoorSch_IkanaCanyonMusicBoxHouse,
    sDoorSch_DampesHouse, sDoorSch_MagicHagsPotionShop, D_80867780, sDoorSch_SwampShootingGallery,
};
#define F DOOR_DL_DEFAULT_FIELD_KEEP
static u8 sScheduleDoorDLists[] = {
    DOOR_DL_INN_SCHOOL, DOOR_DL_POST_OFFICE_TRADING_POST, DOOR_DL_LOTTERY_CURIOSITY_SHOP_MAYOR_HOUSE,
    DOOR_DL_POST_OFFICE_TRADING_POST, DOOR_DL_LOTTERY_CURIOSITY_SHOP_MAYOR_HOUSE, DOOR_DL_DEFAULT, DOOR_DL_DEFAULT,
    DOOR_DL_DEFAULT, DOOR_DL_DEFAULT, DOOR_DL_DEFAULT, DOOR_DL_MILK_BAR, DOOR_DL_INN_SCHOOL, DOOR_DL_INN_SCHOOL, F, F,
    F, F, DOOR_DL_LOTTERY_CURIOSITY_SHOP_MAYOR_HOUSE, DOOR_DL_DEFAULT, DOOR_DL_DEFAULT, DOOR_DL_DEFAULT,
    DOOR_DL_DEFAULT, F, F, F, F, DOOR_DL_DEFAULT, DOOR_DL_MUSIC_BOX, F,
    DOOR_DL_DEFAULT /* (MM: the potion shop's, object_dor04: not here yet) */, DOOR_DL_MILK_BAR,
    DOOR_DL_DEFAULT /* (MM: the swamp's, object_dor03: not here yet) */,
};
#undef F

// MM's D_801AED48 (z_actor.c): the nights a day-dependent door is open, by its half-day index
#define NIGHT(d) (1 << (8 - 2 * (d)))
static s16 sOpenHalfDays[] = {
    NIGHT(0) | NIGHT(4),
    NIGHT(0) | NIGHT(1) | NIGHT(4),
    NIGHT(0) | NIGHT(2) | NIGHT(4),
    NIGHT(0) | NIGHT(1) | NIGHT(2) | NIGHT(4),
    NIGHT(0) | NIGHT(3) | NIGHT(4),
    NIGHT(0) | NIGHT(1) | NIGHT(3) | NIGHT(4),
    NIGHT(0) | NIGHT(2) | NIGHT(3) | NIGHT(4),
    NIGHT(0) | NIGHT(1) | NIGHT(2) | NIGHT(3) | NIGHT(4),
};
#define HALFDAYBIT_DAWNS 0x2AA
#define HALFDAYBIT_NIGHTS 0x155

struct NvMmDoor;
typedef void (*NvMmDoorFunc)(struct NvMmDoor*, PlayState*);

typedef struct NvMmDoor {
    // (OoT's EnDoor's first fields, where OoT's player code looks)
    Actor actor;
    SkelAnime skelAnime;
    u8 animStyle;       // set by Link: OoT's KNOB_ANIM_* (0, 1 left; 2, 3 right)
    u8 playerIsOpening; // set by Link
    // MM's
    u8 animIndex; // 0 left, 1 right
    s8 dlIndex;
    s16 lockTimer;
    s8 openTimer; // set by a schedule actor: frames to stay open (its sign: which way)
    u8 doorType;
    u8 typeVar;
    u16 mmParams;
    Vec3s limbTable[DOOR_LIMB_MAX];
    NvMmDoorFunc func;
} NvMmDoor;

const size_t gNvMmDoorSize = sizeof(NvMmDoor);

u16 NvMmClock_HalfDayBit(void);

static void NvMmDoor_Idle(NvMmDoor* this, PlayState* play);
static void NvMmDoor_Open(NvMmDoor* this, PlayState* play);
static void NvMmDoor_OpenScheduleActor(NvMmDoor* this, PlayState* play);
static void NvMmDoor_AjarWait(NvMmDoor* this, PlayState* play);
static void NvMmDoor_AjarOpen(NvMmDoor* this, PlayState* play);
static void NvMmDoor_AjarClose(NvMmDoor* this, PlayState* play);

static TransitionActorEntry* NvMmDoor_Transition(NvMmDoor* this, PlayState* play) {
    return &play->transiActorCtx.list[GET_TRANSITION_ACTOR_INDEX(&this->actor)];
}

void NvMmDoor_Init(Actor* thisx, PlayState* play) {
    NvMmDoor* this = (NvMmDoor*)thisx;
    TransitionActorEntry* entry;

    this->actor.targetMode = 0;
    this->actor.uncullZoneForward = 4000.0f;
    this->actor.shape.rot.x = 0;
    this->actor.shape.rot.z = 0;
    this->mmParams = this->actor.params & 0x3FF;
    this->doorType = ENDOOR_GET_TYPE(this->mmParams);
    this->typeVar = ENDOOR_GET_TYPE_VAR(this->mmParams);
    SkelAnime_Init(play, &this->skelAnime, (SkeletonHeader*)sDoorSkel, sAnimations[0], this->limbTable,
                   this->limbTable, DOOR_LIMB_MAX);
    if (this->doorType == ENDOOR_TYPE_SCHEDULE) {
        this->dlIndex = sScheduleDoorDLists[this->typeVar % ARRAY_COUNT(sScheduleDoorDLists)];
    } else {
        // (MM: the scenes with their own doors first, none here yet; then the field keep's where loaded)
        this->dlIndex = (Object_GetIndex(&play->objectCtx, OBJECT_GAMEPLAY_FIELD_KEEP) >= 0)
                            ? DOOR_DL_DEFAULT_FIELD_KEEP
                            : DOOR_DL_DEFAULT;
    }
    // OoT's params for Link's door code: a scene exit, or a door between rooms
    entry = NvMmDoor_Transition(this, play);
    this->actor.params = (this->actor.params & 0xFC00) |
                         (((entry->sides[0].room == entry->sides[1].room) ? DOOR_SCENEEXIT : DOOR_ROOMLOAD) << 7);
    this->actor.world.rot.y = 0;
    this->func = NvMmDoor_Idle;
    if (this->doorType == ENDOOR_TYPE_LOCKED) {
        if (!Flags_GetSwitch(play, this->typeVar & 0x3F)) {
            this->lockTimer = 10;
        }
    } else if ((this->doorType == ENDOOR_TYPE_AJAR) &&
               (Actor_WorldDistXZToActor(&this->actor, &GET_PLAYER(play)->actor) > DOOR_AJAR_SLAM_RANGE)) {
        this->func = NvMmDoor_AjarWait;
        this->actor.world.rot.y = -0x1800;
    }
    Actor_SetFocus(&this->actor, 35.0f);
}

void NvMmDoor_Destroy(Actor* thisx, PlayState* play) {
    NvMmDoor* this = (NvMmDoor*)thisx;
    TransitionActorEntry* entry = NvMmDoor_Transition(this, play);

    if (entry->id < 0) {
        entry->id = -entry->id;
    }
}

/** Whether the door is shut now, and its message (MM's text id) if so. */
static s32 NvMmDoor_Closed(NvMmDoor* this, PlayState* play, Vec3f* playerPosRelToDoor, u16* textId) {
    if (this->doorType == ENDOOR_TYPE_AJAR) {
        *textId = 0x1800; // "It won't open"
        return true;
    }
    if ((this->doorType == ENDOOR_TYPE_WHOLE_DAY) || (this->doorType == ENDOOR_TYPE_DAY) ||
        (this->doorType == ENDOOR_TYPE_NIGHT)) {
        u16 halfDaysBit = NvMmClock_HalfDayBit();
        s32 dayBit = (halfDaysBit & HALFDAYBIT_DAWNS) >> 1;
        s32 nightBit = halfDaysBit & HALFDAYBIT_NIGHTS;
        s16 openBit = sOpenHalfDays[HALFDAYCHECK_INDEX(this->typeVar)];

        if (((this->doorType == ENDOOR_TYPE_WHOLE_DAY) && !((dayBit | nightBit) & openBit)) ||
            ((this->doorType == ENDOOR_TYPE_DAY) && !(nightBit & openBit)) ||
            ((this->doorType == ENDOOR_TYPE_NIGHT) && !(dayBit & openBit))) {
            //! (MM's bug, kept: 0x182D has no message)
            u16 base = (this->doorType == ENDOOR_TYPE_NIGHT) ? 0x180D
                       : (this->doorType == ENDOOR_TYPE_DAY) ? 0x181D
                                                             : 0x182D;

            *textId = base + HALFDAYCHECK_TEXT(this->typeVar);
            return true;
        }
        return false;
    }
    if ((this->doorType == ENDOOR_TYPE_SCHEDULE) && (playerPosRelToDoor->z > 0.0f)) {
        NvMmScheduleOutput out;

        if (NvMmSchedule_RunScript(play, sDoorSchedules[this->typeVar % ARRAY_COUNT(sDoorSchedules)], &out)) {
            *textId = out.result + 0x1800;
            return true;
        }
    }
    return false;
}

static void NvMmDoor_Idle(NvMmDoor* this, PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        return; // (a closed door's message: the textbox runs by itself)
    }
    if (this->playerIsOpening) {
        this->animIndex = (this->animStyle < 2) ? 0 : 1;
        this->func = NvMmDoor_Open;
        Animation_PlayOnceSetSpeed(&this->skelAnime, sAnimations[this->animIndex],
                                   (player->stateFlags1 & PLAYER_STATE1_IN_WATER) ? 0.75f : 1.5f);
        if (this->lockTimer != 0) {
            gSaveContext.inventory.dungeonKeys[gSaveContext.mapIndex]--;
            Flags_SetSwitch(play, this->typeVar & 0x3F);
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_CHAIN_KEY_UNLOCK);
        }
    } else if (this->openTimer != 0) {
        this->func = NvMmDoor_OpenScheduleActor;
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_DOOR_OPEN);
    } else if (!Player_InCsMode(play)) {
        Vec3f rel;

        Actor_WorldToActorCoords(&this->actor, &rel, &player->actor.world.pos);
        if ((fabsf(rel.y) < 20.0f) && (fabsf(rel.x) < 20.0f) && (fabsf(rel.z) < 50.0f)) {
            s16 yawDiff = player->actor.shape.rot.y - this->actor.shape.rot.y;
            u16 textId;

            if (rel.z > 0.0f) {
                yawDiff = 0x8000 - yawDiff;
            }
            if (ABS(yawDiff) < 0x3000) {
                if ((this->lockTimer != 0) && (gSaveContext.inventory.dungeonKeys[gSaveContext.mapIndex] <= 0)) {
                    this->actor.textId = NV_MM_TEXT(0x1802); // "Missing small key"
                    Actor_OfferTalk(&this->actor, play, 60.0f);
                } else if ((this->lockTimer == 0) && NvMmDoor_Closed(this, play, &rel, &textId)) {
                    this->actor.textId = NV_MM_TEXT(textId);
                    Actor_OfferTalk(&this->actor, play, 60.0f);
                } else {
                    if (this->lockTimer != 0) {
                        player->doorTimer = 10;
                    }
                    player->doorType = PLAYER_DOORTYPE_HANDLE;
                    player->doorDirection = (rel.z >= 0.0f) ? 1.0f : -1.0f;
                    player->doorActor = &this->actor;
                }
            }
        } else if ((this->doorType == ENDOOR_TYPE_AJAR) && (this->actor.xzDistToPlayer > DOOR_AJAR_OPEN_RANGE)) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_DOOR_OPEN);
            this->func = NvMmDoor_AjarOpen;
        }
    }
}

/** Opened for a schedule actor: openTimer frames, then it swings shut by itself. */
static void NvMmDoor_OpenScheduleActor(NvMmDoor* this, PlayState* play) {
    if (this->openTimer != 0) {
        s32 direction = (this->openTimer >= 0) ? 1 : -1;

        if (Math_ScaledStepToS(&this->actor.world.rot.y, direction * 0x3E80, 0x7D0)) {
            if (this->openTimer > 0) {
                this->openTimer--;
            } else {
                this->openTimer++;
            }
        }
    } else if (Math_ScaledStepToS(&this->actor.world.rot.y, 0, 0x7D0)) {
        this->func = NvMmDoor_Idle;
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_DOOR_CLOSE); // (MM's own swing-shut sound isn't in OoT's)
    }
}

static void NvMmDoor_AjarWait(NvMmDoor* this, PlayState* play) {
    if (this->actor.xzDistToPlayer < DOOR_AJAR_SLAM_RANGE) {
        this->func = NvMmDoor_AjarClose;
    }
}

static void NvMmDoor_AjarOpen(NvMmDoor* this, PlayState* play) {
    if (this->actor.xzDistToPlayer < DOOR_AJAR_SLAM_RANGE) {
        this->func = NvMmDoor_AjarClose;
    } else if (Math_ScaledStepToS(&this->actor.world.rot.y, -0x1800, 0x100)) {
        this->func = NvMmDoor_AjarWait;
    }
}

static void NvMmDoor_AjarClose(NvMmDoor* this, PlayState* play) {
    if (Math_ScaledStepToS(&this->actor.world.rot.y, 0, 0x700)) {
        Audio_PlayActorSound2(&this->actor, NA_SE_EV_DOOR_CLOSE);
        this->func = NvMmDoor_Idle;
    }
}

static void NvMmDoor_Open(NvMmDoor* this, PlayState* play) {
    s32 i;

    if (DECR(this->lockTimer) == 0) {
        if (SkelAnime_Update(&this->skelAnime)) {
            this->func = NvMmDoor_Idle;
            this->playerIsOpening = false;
        } else if (Animation_OnFrame(&this->skelAnime, sAnimOpenFrames[this->animIndex])) {
            Audio_PlayActorSound2(&this->actor, NA_SE_OC_DOOR_OPEN);
            if (this->skelAnime.playSpeed < 1.5f) {
                s32 numEffects = (s32)(Rand_ZeroOne() * 30.0f) + 50;

                for (i = 0; i < numEffects; i++) {
                    EffectSsBubble_Spawn(play, &this->actor.world.pos, 60.0f, 100.0f, 50.0f, 0.15f);
                }
            }
        } else if (Animation_OnFrame(&this->skelAnime, sAnimCloseFrames[this->animIndex])) {
            Audio_PlayActorSound2(&this->actor, NA_SE_EV_DOOR_CLOSE);
        }
    }
}

void NvMmDoor_Update(Actor* thisx, PlayState* play) {
    NvMmDoor* this = (NvMmDoor*)thisx;

    this->func(this, play);
}

static s32 NvMmDoor_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                     void* thisx) {
    NvMmDoor* this = (NvMmDoor*)thisx;

    if (limbIndex == DOOR_LIMB_4) {
        Gfx** sides = sDoorDLists[this->dlIndex];
        TransitionActorEntry* entry = NvMmDoor_Transition(this, play);

        rot->z += this->actor.world.rot.y;
        if ((play->roomCtx.prevRoom.num >= 0) || (entry->sides[0].room == entry->sides[1].room)) {
            s16 yaw = (this->actor.shape.rot.y + this->skelAnime.jointTable[DOOR_LIMB_3].z + rot->z) -
                      Math_Vec3f_Yaw(&play->view.eye, &this->actor.world.pos);

            *dList = (ABS(yaw) < 0x4000) ? sides[0] : sides[1];
        } else {
            *dList = sides[(entry->sides[0].room != this->actor.room) ? 1 : 0];
        }
    }
    return false;
}

void NvMmDoor_Draw(Actor* thisx, PlayState* play) {
    NvMmDoor* this = (NvMmDoor*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    SkelAnime_DrawOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, NvMmDoor_OverrideLimbDraw, NULL,
                      &this->actor);
    if (this->actor.world.rot.y != 0) {
        gSPDisplayList(POLY_OPA_DISP++, (this->actor.world.rot.y > 0) ? sDoorDLists[DOOR_DL_DEFAULT][1]
                                                                      : sDoorDLists[DOOR_DL_DEFAULT][0]);
    }
    if (this->lockTimer) {
        Actor_DrawDoorLock(play, this->lockTimer, DOORLOCK_NORMAL);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

/** For MM's schedule actors (MM's SubS_FindScheduleDoor and openTimer): this door's schedule type,
 * or -1 if it isn't one of MM's schedule doors. */
s32 NvMmDoor_ScheduleType(Actor* thisx) {
    NvMmDoor* this = (NvMmDoor*)thisx;

    return (this->doorType == ENDOOR_TYPE_SCHEDULE) ? this->typeVar : -1;
}

/** Opens it for a schedule actor: `frames` long, its sign which way (MM's door->openTimer). */
void NvMmDoor_SetOpenTimer(Actor* thisx, s8 frames) {
    ((NvMmDoor*)thisx)->openTimer = frames;
}
