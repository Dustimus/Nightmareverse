/*
 * Nightmareverse: the clock tower's own pieces, MM's, ported as they are (mm/README.md "Clock Town's
 * scenery"), MM's models and collision (object_ctower_rot, object_tokei_tobira, object_tokei_step):
 *   Bg_Ctower_Gear (0x1B6)    inside the tower: the ceiling cogs and the centre cog turning, the water
 *                             wheel turning (stood on) and splashing; the organ only shows in MM's
 *                             cutscene (not here: kept hidden, as MM's starts).
 *   Bg_Ctower_Rot (0x061)     the twisting corridor (it turns as Link walks along it, its floor stood on)
 *                             and the two stone doors behind it (they slide shut once he's through).
 *   Obj_Tokei_Tobira (0x1A2)  the tower's swinging double doors, pushed open as he walks into them.
 *   Obj_Tokei_Step (0x1B8)    the tower's front wall in South Clock Town: on the last night (day 3 from
 *                             midnight) the steps to the top. MM drops them in a cutscene at midnight;
 *                             here they drop the same way, bouncing, when the clock passes midnight
 *                             while he's there, and are down when he arrives after.
 * Adapted: MM's cutscene cameras (the stone door closing, the first time through the tower doors) are
 * left out; the sounds OoT lacks (MM's water wheel, wood gate, tower stairs) are OoT's nearest.
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>

#define ROT(name) ("__OTR__mm/objects/object_ctower_rot/" name)
#define TOBIRA(name) ("__OTR__mm/objects/object_tokei_tobira/" name)
#define STEP(name) ("__OTR__mm/objects/object_tokei_step/" name)

s32 NvMmClock_Day(void);
u16 NvMmClock_Time(void);

static void NvMmCt_Dl(PlayState* play, const char* dl, s32 xlu) {
    OPEN_DISPS(play->state.gfxCtx);
    if (xlu) {
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, (Gfx*)dl);
    } else {
        Gfx_SetupDL_25Opa(play->state.gfxCtx);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)dl);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

static void NvMmCt_Mesh(PlayState* play, DynaPolyActor* dyna, s32 flags, const char* col) {
    CollisionHeader* colHeader = NULL;

    DynaPolyActor_Init(dyna, flags);
    CollisionHeader_GetVirtual((void*)col, &colHeader);
    dyna->bgId = DynaPoly_SetBgActor(gPlayState, &gPlayState->colCtx.dyna, &dyna->actor, colHeader);
}

/** MM's Actor_WorldToActorCoords: a point in the actor's own frame (its shape's yaw). */
static void NvMmCt_ToActor(Actor* actor, Vec3f* out, Vec3f* pos) {
    f32 c = Math_CosS(actor->shape.rot.y);
    f32 s = Math_SinS(actor->shape.rot.y);
    f32 dx = pos->x - actor->world.pos.x;
    f32 dz = pos->z - actor->world.pos.z;

    out->x = (dx * c) - (dz * s);
    out->z = (dx * s) + (dz * c);
    out->y = pos->y - actor->world.pos.y;
}

/* ---- Bg_Ctower_Gear ---- */

enum { GEAR_CEILING_COG, GEAR_CENTER_COG, GEAR_WATER_WHEEL, GEAR_ORGAN };

typedef struct {
    DynaPolyActor dyna;
} NvMmCtGear;

const size_t gNvMmCtGearSize = sizeof(NvMmCtGear);

static Vec3f sExitSplashOffsets[] = {
    { -70.0f, -60.0f, 8.0f }, { -60.0f, -60.0f, -9.1f }, { -75.0f, -60.0f, -9.1f }, { -70.0f, -60.0f, -26.2f },
};
static Vec3f sEnterSplashOffsets[] = {
    { 85.0f, -60.0f, 8.0f }, { 80.0f, -60.0f, -9.1f }, { 85.0f, -60.0f, -26.2f },
};

/** MM's BgCtowerGear_Splash: the water off the wheel's buckets. */
static void NvMmCtGear_Splash(NvMmCtGear* this, PlayState* play) {
    Actor* a = &this->dyna.actor;
    s32 near = (a->xzDistToPlayer < 1500.0f);
    s16 rotZ = a->shape.rot.z & 0x1FFF;
    Vec3f off, pos;
    s32 i, j;

    if (near && (rotZ < 0x1B58) && (rotZ >= 0x1388)) {
        Matrix_RotateY(a->home.rot.y * (M_PI / 0x8000), MTXMODE_NEW);
        Matrix_RotateX(a->home.rot.x * (M_PI / 0x8000), MTXMODE_APPLY);
        Matrix_RotateZ(a->home.rot.z * (M_PI / 0x8000), MTXMODE_APPLY);
        for (i = 0; i < 4; i++) {
            if (Rand_Next() >= 0x40000000) {
                off.x = sExitSplashOffsets[i].x - (Rand_ZeroOne() * 30.0f);
                off.y = sExitSplashOffsets[i].y;
                off.z = sExitSplashOffsets[i].z;
                Matrix_MultVec3f(&off, &pos);
                pos.x += a->world.pos.x + ((Rand_ZeroOne() * 20.0f) - 10.0f);
                pos.y += a->world.pos.y;
                pos.z += a->world.pos.z + ((Rand_ZeroOne() * 20.0f) - 10.0f);
                EffectSsGSplash_Spawn(play, &pos, NULL, NULL, 0, (Rand_Next() >> 25) + 340);
            }
        }
    }
    if ((rotZ < 0x1F4) && (rotZ >= 0)) {
        if (near) {
            Matrix_RotateY(a->home.rot.y * (M_PI / 0x8000), MTXMODE_NEW);
            Matrix_RotateX(a->home.rot.x * (M_PI / 0x8000), MTXMODE_APPLY);
            Matrix_RotateZ(a->home.rot.z * (M_PI / 0x8000), MTXMODE_APPLY);
            for (i = 0; i < 3; i++) {
                for (j = 0; j < 2; j++) {
                    off.x = sEnterSplashOffsets[i].x + (Rand_ZeroOne() * 10.0f);
                    off.y = sEnterSplashOffsets[i].y;
                    off.z = sEnterSplashOffsets[i].z;
                    Matrix_MultVec3f(&off, &pos);
                    pos.x += a->world.pos.x + ((Rand_ZeroOne() * 20.0f) - 10.0f);
                    pos.y += a->world.pos.y;
                    pos.z += a->world.pos.z + ((Rand_ZeroOne() * 20.0f) - 10.0f);
                    EffectSsGSplash_Spawn(play, &pos, NULL, NULL, 0, (Rand_Next() >> 25) + 280);
                }
            }
        }
        Audio_PlayActorSound2(a, NA_SE_EV_DIVE_INTO_WATER_L); // (MM's NA_SE_EV_WATERWHEEL_LEVEL: OoT's nearest)
    }
}

void NvMmCtGear_Init(Actor* thisx, PlayState* play) {
    NvMmCtGear* this = (NvMmCtGear*)thisx;
    s32 type = thisx->params & 3;

    Actor_SetScale(thisx, 0.1f);
    thisx->uncullZoneForward = 4000.0f;
    thisx->uncullZoneScale = (type == GEAR_CENTER_COG) ? 1500.0f : (type == GEAR_ORGAN) ? 420.0f : 400.0f;
    thisx->uncullZoneDownward = (type == GEAR_CENTER_COG) ? 2000.0f : (type == GEAR_ORGAN) ? 570.0f : 400.0f;
    this->dyna.bgId = BGACTOR_NEG_ONE;
    if (type == GEAR_WATER_WHEEL) {
        NvMmCt_Mesh(play, &this->dyna, DPM_PLAYER, ROT("gClockTowerWaterWheelCol"));
    } else if (type == GEAR_ORGAN) { // (MM's cutscene shows it; until then nothing)
        thisx->draw = NULL;
    }
}

void NvMmCtGear_Destroy(Actor* thisx, PlayState* play) {
    NvMmCtGear* this = (NvMmCtGear*)thisx;

    if (this->dyna.bgId != BGACTOR_NEG_ONE) {
        DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, this->dyna.bgId);
    }
}

void NvMmCtGear_Update(Actor* thisx, PlayState* play) {
    switch (thisx->params & 3) {
        case GEAR_CEILING_COG:
            thisx->shape.rot.x -= 0x1F4;
            break;
        case GEAR_CENTER_COG:
            thisx->shape.rot.y += 0x1F4;
            Actor_PlaySfx_Flagged(thisx, NA_SE_EV_WINDMILL_LEVEL - SFX_FLAG);
            break;
        case GEAR_WATER_WHEEL:
            thisx->shape.rot.z -= 0x1F4;
            NvMmCtGear_Splash((NvMmCtGear*)thisx, play);
            break;
    }
}

void NvMmCtGear_Draw(Actor* thisx, PlayState* play) {
    static const char* sDLs[] = { ROT("gClockTowerCeilingCogDL"), ROT("gClockTowerCenterCogDL"),
                                  ROT("gClockTowerWaterWheelDL") };

    if ((thisx->params & 3) < 3) {
        NvMmCt_Dl(play, sDLs[thisx->params & 3], false);
    }
}

/* ---- Bg_Ctower_Rot ---- */

enum { ROT_CORRIDOR, ROT_STONE_DOOR_MAIN, ROT_STONE_DOOR };

typedef struct {
    DynaPolyActor dyna;
    f32 timer;
    s32 state; // the doors: 0 still, 1 open (he's to come through), 2 closing
} NvMmCtRot;

const size_t gNvMmCtRotSize = sizeof(NvMmCtRot);

void NvMmCtRot_Init(Actor* thisx, PlayState* play) {
    NvMmCtRot* this = (NvMmCtRot*)thisx;
    Player* player = GET_PLAYER(play);
    Vec3f off;

    Actor_SetScale(thisx, 0.1f);
    if (thisx->params == ROT_CORRIDOR) {
        NvMmCt_Mesh(play, &this->dyna, DPM_PLAYER, ROT("gClockTowerCorridorCol"));
        return;
    }
    if (thisx->params == ROT_STONE_DOOR_MAIN) {
        NvMmCt_Mesh(play, &this->dyna, DPM_PLAYER, ROT("gClockTowerStoneDoorMainCol"));
        thisx->world.rot.y = thisx->shape.rot.y + 0x4000;
    } else {
        NvMmCt_Mesh(play, &this->dyna, DPM_PLAYER, ROT("gClockTowerStoneDoorCol"));
        thisx->world.rot.y = thisx->shape.rot.y - 0x4000;
    }
    NvMmCt_ToActor(thisx, &off, &player->actor.world.pos);
    if (off.z < 0.0f) { // (he's to come through: open, and shut behind him)
        thisx->world.pos.x = thisx->home.pos.x + (Math_SinS(thisx->world.rot.y) * 80.0f);
        thisx->world.pos.z = thisx->home.pos.z + (Math_CosS(thisx->world.rot.y) * 80.0f);
        this->timer = 80.0f;
        this->state = 1;
    }
}

void NvMmCtRot_Destroy(Actor* thisx, PlayState* play) {
    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, ((NvMmCtRot*)thisx)->dyna.bgId);
}

void NvMmCtRot_Update(Actor* thisx, PlayState* play) {
    NvMmCtRot* this = (NvMmCtRot*)thisx;
    Player* player = GET_PLAYER(play);
    Vec3f off;

    NvMmCt_ToActor(thisx, &off, &player->actor.world.pos);
    if (thisx->params == ROT_CORRIDOR) { // MM's BgCtowerRot_CorridorRotate: the twist follows him
        f32 rotZ = CLAMP(1100.0f - off.z, 0.0f, 1000.0f);

        Camera_RequestSetting(play->cameraPtrs[CAM_ID_MAIN], CAM_SET_DUNGEON0);
        thisx->shape.rot.z = rotZ * 16.384f;
        return;
    }
    if ((this->state == 1) && (off.z > 30.0f)) {
        this->state = 2;
    } else if (this->state == 2) { // MM's BgCtowerRot_DoorClose
        if (!Math_SmoothStepToF(&this->timer, 0.0f, 0.1f, 15.0f, 0.1f)) {
            if (thisx->params == ROT_STONE_DOOR_MAIN) {
                Audio_PlayActorSound2(thisx, NA_SE_EV_STONEDOOR_STOP);
            }
            this->state = 0;
        } else if (thisx->params == ROT_STONE_DOOR_MAIN) {
            Actor_PlaySfx_Flagged(thisx, NA_SE_EV_STONE_STATUE_OPEN - SFX_FLAG);
        }
        thisx->world.pos.x = thisx->home.pos.x + (Math_SinS(thisx->world.rot.y) * this->timer);
        thisx->world.pos.z = thisx->home.pos.z + (Math_CosS(thisx->world.rot.y) * this->timer);
    }
}

void NvMmCtRot_Draw(Actor* thisx, PlayState* play) {
    static const char* sDLs[] = { ROT("gClockTowerCorridorDL"), ROT("gClockTowerStoneDoorMainDL"),
                                  ROT("gClockTowerStoneDoorDL") };

    NvMmCt_Dl(play, sDLs[thisx->params % 3], false);
    if (thisx->params == ROT_CORRIDOR) {
        NvMmCt_Dl(play, ROT("gClockTowerCorridorFoliageDL"), true);
    }
}

/* ---- Obj_Tokei_Tobira ---- */

typedef struct {
    DynaPolyActor dyna;
    f32 vel;   // MM's unk160
    f32 angle; // MM's unk164
    s32 pushed; // MM's unk168
    s32 sfxTimer;
} NvMmTobira;

const size_t gNvMmTobiraSize = sizeof(NvMmTobira);

s32 NvMmActor_Id(s32 mmId);

void NvMmTobira_Init(Actor* thisx, PlayState* play) {
    NvMmTobira* this = (NvMmTobira*)thisx;
    s32 type = thisx->params & 1;
    Vec3f off = { 0.0f, 0.0f, 80.0f };
    Vec3f pos;

    thisx->uncullZoneForward = 4000.0f;
    thisx->uncullZoneScale = 300.0f;
    thisx->uncullZoneDownward = 300.0f;
    Actor_SetScale(thisx, 0.1f);
    NvMmCt_Mesh(play, &this->dyna, DPM_PLAYER | DPM_UNK3,
                type ? TOBIRA("object_tokei_tobira_Colheader_001590") : TOBIRA("object_tokei_tobira_Colheader_0012B0"));
    if (type == 0) { // (the left door brings its right one)
        Actor_SpawnAsChild(&play->actorCtx, thisx, play, NvMmActor_Id(0x1A2), thisx->world.pos.x, thisx->world.pos.y,
                           thisx->world.pos.z, thisx->shape.rot.x, thisx->shape.rot.y, thisx->shape.rot.z, 1);
    }
    Matrix_RotateY(((type ? 0x4000 : -0x4000) + thisx->shape.rot.y) * (M_PI / 0x8000), MTXMODE_NEW);
    Matrix_MultVec3f(&off, &pos);
    thisx->world.pos.x += pos.x;
    thisx->world.pos.y += pos.y;
    thisx->world.pos.z += pos.z;
}

void NvMmTobira_Destroy(Actor* thisx, PlayState* play) {
    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, ((NvMmTobira*)thisx)->dyna.bgId);
}

/** MM's ObjTokeiTobira_Update: pushed by him walking into it, swinging, settling. */
void NvMmTobira_Update(Actor* thisx, PlayState* play) {
    NvMmTobira* this = (NvMmTobira*)thisx;
    Player* player = GET_PLAYER(play);
    s32 type = thisx->params & 1;
    f32 side = type ? -1.0f : 1.0f;
    Vec3f off;

    if ((player->actor.bgCheckFlags & BGCHECKFLAG_PLAYER_WALL_INTERACT) &&
        (DynaPoly_GetActor(&play->colCtx, player->actor.wallBgId) == &this->dyna)) {
        f32 along;

        NvMmCt_ToActor(thisx, &off, &player->actor.world.pos);
        along = off.x * side;
        if (along > 20.0f) {
            if (off.z > 0.0f) {
                this->vel += (along - 20.0f) * side * (16.0f / 3.0f);
                this->pushed = 1;
                if (this->sfxTimer <= 0) {
                    Audio_PlayActorSound2(thisx, NA_SE_EV_WOODDOOR_OPEN); // (MM's NA_SE_OC_WOOD_GATE_OPEN: OoT's nearest)
                    this->sfxTimer = 80;
                }
            }
            if (along > 48.0f) {
                NvMmTobira* other = (NvMmTobira*)(type ? thisx->parent : thisx->child);

                if (other != NULL) {
                    other->pushed = 1;
                }
            }
        }
    } else if (this->pushed == 1) {
        NvMmCt_ToActor(thisx, &off, &player->actor.world.pos);
        if ((off.z > 0.0f) && (off.z < 30.0f)) {
            this->vel += side * 290.0f;
        }
    }
    if (this->sfxTimer > 0) {
        this->sfxTimer--;
    }
    this->pushed = 0;
    this->vel *= 0.87f;
    this->angle += this->vel;
    if ((this->angle * side) > 14336.0f) {
        this->vel *= 0.1f;
        this->angle = 14336.0f * side;
    } else if ((this->angle * side) < -1000.0f) {
        this->vel *= 0.1f;
        this->angle = -1000.0f * side;
    }
    thisx->shape.rot.y = (s32)this->angle + thisx->home.rot.y;
}

void NvMmTobira_Draw(Actor* thisx, PlayState* play) {
    NvMmCt_Dl(play, (thisx->params & 1) ? TOBIRA("object_tokei_tobira_DL_0013E8") : TOBIRA("object_tokei_tobira_DL_001108"),
              false);
}

/* ---- Obj_Tokei_Step ---- */

typedef struct {
    Vec3f pos;
    f32 posChangeY;
    s16 startFallingTimer;
    s8 numBounces;
    s8 hasSoundPlayed;
} NvMmStepPanel;

typedef struct {
    DynaPolyActor dyna;
    NvMmStepPanel panels[7];
    s32 state; // 0 the wall, 1 dropping, 2 the steps
} NvMmStep;

const size_t gNvMmStepSize = sizeof(NvMmStep);

static f32 sPanelYOffsets[] = { -105.0f, -90.0f, -75.0f, -60.0f, -45.0f, -30.0f, -15.0f };
static f32 sDustXOffsets[] = { -60.0f, -40.0f, -20.0f, 0.0f, 20.0f, 40.0f, 60.0f };

/** The last night: day 3 from midnight (MM's CURRENT_DAY == 3 && CURRENT_TIME < 6:00). */
static s32 NvMmStep_LastNight(void) {
    return (NvMmClock_Day() == 3) && (NvMmClock_Time() < 0x4000);
}

static void NvMmStep_Panels(NvMmStep* this, s32 open) {
    Actor* a = &this->dyna.actor;
    s32 i;

    Matrix_SetTranslateRotateYXZ(a->world.pos.x, a->world.pos.y, a->world.pos.z, &a->shape.rot);
    for (i = 0; i < 7; i++) {
        Vec3f off = { 0.0f, open ? sPanelYOffsets[i] : 0.0f, i * -20.0f };

        Matrix_MultVec3f(&off, &this->panels[i].pos);
        this->panels[i].posChangeY = 0.0f;
        this->panels[i].numBounces = 0;
        this->panels[i].hasSoundPlayed = false;
        this->panels[i].startFallingTimer = (i == 0) ? 0 : 10;
    }
}

void NvMmStep_Init(Actor* thisx, PlayState* play) {
    NvMmStep* this = (NvMmStep*)thisx;

    thisx->uncullZoneForward = 4000.0f;
    thisx->uncullZoneScale = 300.0f;
    thisx->uncullZoneDownward = 300.0f;
    Actor_SetScale(thisx, 0.1f);
    NvMmCt_Mesh(play, &this->dyna, 0, STEP("gClocktowerPanelCol"));
    if (NvMmStep_LastNight()) { // (arriving after midnight: the steps are down)
        NvMmStep_Panels(this, true);
        func_8003EBF8(play, &play->colCtx.dyna, this->dyna.bgId);
        this->state = 2;
    } else {
        NvMmStep_Panels(this, false);
    }
}

void NvMmStep_Destroy(Actor* thisx, PlayState* play) {
    DynaPoly_DeleteBgActor(play, &play->colCtx.dyna, ((NvMmStep*)thisx)->dyna.bgId);
}

/** MM's ObjTokeiStep_OpenProcess: each panel drops in turn, bouncing three times, dust and a quake on
 * its first landing. True when all are down. */
static s32 NvMmStep_Drop(NvMmStep* this, PlayState* play) {
    Actor* a = &this->dyna.actor;
    s32 isOpen = true;
    s32 prevBounced = true;
    s32 i;

    for (i = 0; i < 7; i++) {
        NvMmStepPanel* p = &this->panels[i];

        if (prevBounced && (p->startFallingTimer > 0)) {
            p->startFallingTimer--;
            isOpen = false;
        }
        if (prevBounced && (p->numBounces < 3) && (p->startFallingTimer <= 0)) {
            f32 finalY = sPanelYOffsets[i] + a->world.pos.y;

            if (!p->hasSoundPlayed) {
                Audio_PlayActorSound2(a, NA_SE_EV_STONE_STATUE_OPEN); // (MM's NA_SE_EV_CLOCK_TOWER_STAIR_MOVE)
                p->hasSoundPlayed = true;
            }
            p->posChangeY += -2.5f;
            p->posChangeY *= 0.83f;
            p->pos.y += p->posChangeY;
            isOpen = false;
            if (p->pos.y < finalY) {
                p->numBounces++;
                if (p->numBounces >= 3) {
                    p->pos.y = finalY;
                } else {
                    p->posChangeY *= -0.4f;
                    if (p->posChangeY > 4.0f) {
                        p->posChangeY = 4.0f;
                    }
                    p->pos.y = (p->pos.y - finalY) * -0.4f;
                    p->pos.y = (p->posChangeY < p->pos.y) ? (p->posChangeY + finalY) : (p->pos.y + finalY);
                    if (p->numBounces == 1) { // dust along it, and the ground shakes
                        static Vec3f sAccel = { 0.0f, 0.3f, 0.0f };
                        static Vec3f sZero = { 0.0f, 0.0f, 0.0f };
                        static Color_RGBA8 sPrim = { 170, 130, 90, 255 };
                        static Color_RGBA8 sEnv = { 100, 60, 20, 255 };
                        Vec3f off, pos;
                        s32 k;
                        s16 quake = Quake_Add(GET_ACTIVE_CAM(play), 3);

                        Matrix_RotateY(a->shape.rot.y * (M_PI / 0x8000), MTXMODE_NEW);
                        off.y = 115.0f;
                        off.z = -10.0f;
                        for (k = 0; k < 7; k++) {
                            off.x = sDustXOffsets[k];
                            Matrix_MultVec3f(&off, &pos);
                            pos.x += p->pos.x;
                            pos.y += p->pos.y;
                            pos.z += p->pos.z;
                            func_8002829C(play, &pos, &sZero, &sAccel, &sPrim, &sEnv,
                                          (s32)((Rand_ZeroOne() * 40.0f) + 80.0f), (s32)((Rand_ZeroOne() * 20.0f) + 50.0f));
                        }
                        Quake_SetSpeed(quake, 20000);
                        Quake_SetQuakeValues(quake, 1, 0, 0, 0);
                        Quake_SetCountdown(quake, 7);
                    }
                }
            }
        }
        prevBounced = p->numBounces > 0;
    }
    return isOpen;
}

void NvMmStep_Update(Actor* thisx, PlayState* play) {
    NvMmStep* this = (NvMmStep*)thisx;

    if ((this->state == 0) && NvMmStep_LastNight()) { // (midnight while he's here: MM's cutscene's drop)
        NvMmStep_Panels(this, false);
        this->state = 1;
        if (getenv("NV_LOG") != NULL) {
            printf("NVMM the clock tower's steps come down\n");
            fflush(stdout);
        }
    } else if ((this->state == 1) && NvMmStep_Drop(this, play)) {
        func_8003EBF8(play, &play->colCtx.dyna, this->dyna.bgId);
        this->state = 2;
    }
}

void NvMmStep_Draw(Actor* thisx, PlayState* play) {
    NvMmStep* this = (NvMmStep*)thisx;
    s32 i;

    if (this->state == 0) {
        NvMmCt_Dl(play, STEP("gClocktowerPanelDL"), false);
        return;
    }
    for (i = 0; i < 7; i++) { // (each panel at its own place, the actor's turn kept)
        Matrix_Translate(this->panels[i].pos.x, this->panels[i].pos.y, this->panels[i].pos.z, MTXMODE_NEW);
        Matrix_RotateZYX(thisx->shape.rot.x, thisx->shape.rot.y, thisx->shape.rot.z, MTXMODE_APPLY);
        Matrix_Scale(thisx->scale.x, thisx->scale.y, thisx->scale.z, MTXMODE_APPLY);
        NvMmCt_Dl(play, STEP("gClocktowerPanelDL"), false);
    }
}
