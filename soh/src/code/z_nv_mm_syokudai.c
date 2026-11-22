/*
 * Nightmareverse: Majora's Mask's torches, from MM's Obj_Syokudai (zeldaret/mm, ovl_Obj_Syokudai),
 * ported closely (mm/README.md: in Termina, MM's own props). The stand is MM's object_syokudai and
 * the flame MM's gEffFire1DL (MM's gameplay_keep), drawn by path.
 *
 * Params as MM's: type (>> 12: 0 a switch lights it, 1 lighting it sets a switch, 2 no switch),
 * lit from the start (0x800), group size ((>> 7) & 0xF: light them all in time), switch flag
 * (& 0x7F, 0x7F none). Lit by a Deku stick, a fire arrow or another flame; a lit one lights Link's
 * stick and turns an arrow shot through it into a fire arrow; water puts it out.
 *
 * Adapted from MM:
 * - MM's cutscene queue (a torch with a cutscene shows it when its switch is set) is OoT's
 *   OnePointCutscene_Attention, as OoT's torches do; the switch is set at once;
 * - the flame's damage flags are OoT's (OoT's weapons hit it: OoT's fire arrows and Din's Fire,
 *   0x20820, where MM has 0x820); MM's stick-flame timer (player->unk_B28) is OoT's unk_860;
 * - MM's shadow for the stand (func_800B4B50) is left out: OoT's torches have none either;
 * - switch flags are OoT's 6 bits (& 0x3F) for setting and checking; Clock Town's torches have none.
 */
#include "global.h"
#include "overlays/actors/ovl_En_Arrow/z_en_arrow.h"

#define OBJ_SYOKUDAI_GET_TYPE(thisx) ((u16)(thisx)->params >> 0xC)
#define OBJ_SYOKUDAI_GET_START_LIT(thisx) ((thisx)->params & 0x800)
#define OBJ_SYOKUDAI_GET_GROUP_SIZE(thisx) (((thisx)->params >> 7) & 0xF)
#define OBJ_SYOKUDAI_GET_SWITCH_FLAG(thisx) ((thisx)->params & 0x7F)
#define OBJ_SYOKUDAI_SWITCH_FLAG_NONE 0x7F

#define OBJ_SYOKUDAI_GLOW_HEIGHT 70.0f
#define OBJ_SYOKUDAI_FLAME_HEIGHT 52.0f
#define OBJ_SYOKUDAI_STICK_IGNITION_HEIGHT 67.0f
#define OBJ_SYOKUDAI_STICK_IGNITION_RADIUS 20.0f
#define OBJ_SYOKUDAI_SNUFF_TIMER_JUST_LIT_BONUS 10
#define OBJ_SYOKUDAI_LIGHT_RADIUS_MAX 250
#define OBJ_SYOKUDAI_SNUFF_TIMER_INITIAL(groupSize) ((groupSize * 50) + 100)

enum { TYPE_SWITCH_CAUSES_FLAME, TYPE_FLAME_CAUSES_SWITCH, TYPE_NO_SWITCH };
enum { SNUFF_NEVER = -1, SNUFF_OUT = 0, SNUFF_GROUP_BY_WATER = 1, SNUFF_DEFAULT = 20 };
enum { INTERACTION_STICK = -1, INTERACTION_NONE = 0, INTERACTION_ARROW_FA = 1 };

#define MM_SYOKUDAI(name) ((Gfx*)"__OTR__mm/objects/object_syokudai/" name)
static Gfx* sDLists[] = {
    MM_SYOKUDAI("gObjectSyokudaiTypeSwitchCausesFlameDL"),
    MM_SYOKUDAI("gObjectSyokudaiTypeFlameCausesSwitchDL"),
    MM_SYOKUDAI("gObjectSyokudaiTypeNoSwitchDL"),
};
#define DL_FIRE ((Gfx*)"__OTR__mm/objects/gameplay_keep/gEffFire1DL")

typedef struct NvMmSyokudai {
    Actor actor;
    ColliderCylinder standCollider;
    ColliderCylinder flameCollider;
    s16 snuffTimer;
    u8 flameTexScroll;
    LightNode* lightNode;
    LightInfo lightInfo;
} NvMmSyokudai;

const size_t gNvMmSyokudaiSize = sizeof(NvMmSyokudai);

static ColliderCylinderInit sStandColliderInit = {
    { COLTYPE_METAL, AT_NONE, AC_ON | AC_HARD | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_2, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2, { 0x00100000, 0x00, 0x00 }, { 0xEE01FFFF, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON | BUMP_HOOKABLE,
      OCELEM_ON },
    { 12, 45, 0, { 0, 0, 0 } },
};

static ColliderCylinderInit sFlameColliderInit = {
    { COLTYPE_NONE, AT_NONE, AC_ON | AC_TYPE_PLAYER, OC1_NONE, OC2_NONE, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK2, { 0x00000000, 0x00, 0x00 }, { 0x00020820, 0x00, 0x00 }, TOUCH_NONE, BUMP_ON, OCELEM_NONE },
    { 15, 45, 45, { 0, 0, 0 } },
};

static u8 sColTypes[] = { COLTYPE_METAL, COLTYPE_WOOD, COLTYPE_WOOD };

static s32 sNumLitTorchesInGroup;

static s32 NvMmSyokudai_Switch(NvMmSyokudai* this) {
    return OBJ_SYOKUDAI_GET_SWITCH_FLAG(&this->actor) & 0x3F;
}

void NvMmSyokudai_Init(Actor* thisx, PlayState* play) {
    NvMmSyokudai* this = (NvMmSyokudai*)thisx;
    s32 type = OBJ_SYOKUDAI_GET_TYPE(thisx);
    s32 switchFlag = OBJ_SYOKUDAI_GET_SWITCH_FLAG(thisx);

    Actor_SetScale(thisx, 1.0f);
    thisx->uncullZoneForward = 4000.0f;
    thisx->uncullZoneScale = 800.0f;
    thisx->uncullZoneDownward = 800.0f;
    ActorShape_Init(&thisx->shape, 0.0f, NULL, 1.0f);
    Collider_InitCylinder(play, &this->standCollider);
    Collider_SetCylinder(play, &this->standCollider, thisx, &sStandColliderInit);
    this->standCollider.base.colType = sColTypes[type % 3];
    Collider_InitCylinder(play, &this->flameCollider);
    Collider_SetCylinder(play, &this->flameCollider, thisx, &sFlameColliderInit);
    thisx->colChkInfo.mass = MASS_IMMOVABLE;
    Lights_PointGlowSetInfo(&this->lightInfo, thisx->world.pos.x, thisx->world.pos.y + OBJ_SYOKUDAI_GLOW_HEIGHT,
                            thisx->world.pos.z, 0xFF, 0xFF, 0xB4, -1);
    this->lightNode = LightContext_InsertLight(play, &play->lightCtx, &this->lightInfo);
    if (OBJ_SYOKUDAI_GET_START_LIT(thisx) ||
        (((type != TYPE_NO_SWITCH) || (switchFlag != OBJ_SYOKUDAI_SWITCH_FLAG_NONE)) &&
         Flags_GetSwitch(play, NvMmSyokudai_Switch(this)))) {
        s32 groupSize = OBJ_SYOKUDAI_GET_GROUP_SIZE(thisx);

        this->snuffTimer = SNUFF_NEVER;
        if (groupSize != 0) {
            sNumLitTorchesInGroup = groupSize;
        }
    } else {
        sNumLitTorchesInGroup = 0;
    }
    this->flameTexScroll = (u32)(Rand_ZeroOne() * SNUFF_DEFAULT);
    Actor_SetFocus(thisx, 60.0f);
}

void NvMmSyokudai_Destroy(Actor* thisx, PlayState* play) {
    NvMmSyokudai* this = (NvMmSyokudai*)thisx;

    Collider_DestroyCylinder(play, &this->standCollider);
    Collider_DestroyCylinder(play, &this->flameCollider);
    LightContext_RemoveLight(play, &play->lightCtx, this->lightNode);
}

/** Its switch set (MM queues the torch's cutscene first; OoT's torches look at it). */
static void NvMmSyokudai_SetSwitch(NvMmSyokudai* this, PlayState* play) {
    Flags_SetSwitch(play, NvMmSyokudai_Switch(this));
    OnePointCutscene_Attention(play, &this->actor);
    this->snuffTimer = SNUFF_NEVER;
}

void NvMmSyokudai_Update(Actor* thisx, PlayState* play) {
    NvMmSyokudai* this = (NvMmSyokudai*)thisx;
    s32 groupSize = OBJ_SYOKUDAI_GET_GROUP_SIZE(thisx);
    s32 switchFlag = NvMmSyokudai_Switch(this);
    s32 type = OBJ_SYOKUDAI_GET_TYPE(thisx);
    WaterBox* waterBox;
    f32 waterSurface;
    s32 lightRadius = -1;
    u8 lightIntensity = 0;
    Player* player;

    if (WaterBox_GetSurfaceImpl(play, &play->colCtx, thisx->world.pos.x, thisx->world.pos.z, &waterSurface,
                                &waterBox) &&
        ((waterSurface - thisx->world.pos.y) > OBJ_SYOKUDAI_FLAME_HEIGHT)) {
        this->snuffTimer = SNUFF_OUT;
        if (type == TYPE_FLAME_CAUSES_SWITCH) {
            Flags_UnsetSwitch(play, switchFlag);
            if (groupSize != 0) {
                this->snuffTimer = SNUFF_GROUP_BY_WATER;
            }
        }
    } else {
        s32 interaction = INTERACTION_NONE;
        u32 dmgFlags = 0;

        player = GET_PLAYER(play);
        if (OBJ_SYOKUDAI_GET_START_LIT(thisx)) {
            this->snuffTimer = SNUFF_NEVER;
        }
        if (groupSize != 0) {
            if (Flags_GetSwitch(play, switchFlag)) {
                if (this->snuffTimer == SNUFF_OUT) {
                    this->snuffTimer = SNUFF_NEVER;
                    if (type == TYPE_SWITCH_CAUSES_FLAME) {
                        OnePointCutscene_Attention(play, thisx);
                    }
                } else if (this->snuffTimer > SNUFF_OUT) {
                    this->snuffTimer = SNUFF_NEVER;
                }
            } else if (this->snuffTimer <= SNUFF_NEVER) {
                this->snuffTimer = SNUFF_DEFAULT;
            }
        }
        if (this->flameCollider.base.acFlags & AC_HIT) {
            dmgFlags = this->flameCollider.info.acHitInfo->toucher.dmgFlags;
            if (dmgFlags & 0x20820) {
                interaction = INTERACTION_ARROW_FA;
            }
        } else if (player->heldItemAction == PLAYER_IA_DEKU_STICK) {
            Vec3f tip;

            Math_Vec3f_Diff(&player->meleeWeaponInfo[0].tip, &thisx->world.pos, &tip);
            tip.y -= OBJ_SYOKUDAI_STICK_IGNITION_HEIGHT;
            if ((SQ(tip.x) + SQ(tip.y) + SQ(tip.z)) < SQ(OBJ_SYOKUDAI_STICK_IGNITION_RADIUS)) {
                interaction = INTERACTION_STICK;
            }
        }
        if (interaction != INTERACTION_NONE) {
            if (this->snuffTimer != SNUFF_OUT) { // lit: it lights the stick, or the arrow
                if (interaction <= INTERACTION_STICK) {
                    if (player->unk_860 == 0) {
                        player->unk_860 = 0xD2;
                        Audio_PlaySfxGeneral(NA_SE_EV_FLAME_IGNITION, &thisx->projectedPos, 4,
                                             &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                                             &gSfxDefaultReverb);
                    } else if (player->unk_860 < 0xC8) {
                        player->unk_860 = 0xC8;
                    }
                } else if (dmgFlags & 0x20) {
                    EnArrow* arrow = (EnArrow*)this->flameCollider.base.ac;

                    if ((arrow->actor.update != NULL) && (arrow->actor.id == ACTOR_EN_ARROW)) {
                        arrow->actor.params = ARROW_NORMAL_LIT;
                        arrow->collider.info.toucher.dmgFlags = 0x800;
                    }
                }
                if ((this->snuffTimer > SNUFF_NEVER) &&
                    (this->snuffTimer < OBJ_SYOKUDAI_SNUFF_TIMER_INITIAL(groupSize)) &&
                    (type != TYPE_SWITCH_CAUSES_FLAME)) {
                    this->snuffTimer = OBJ_SYOKUDAI_SNUFF_TIMER_INITIAL(groupSize);
                }
            } else if ((type != TYPE_SWITCH_CAUSES_FLAME) &&
                       (((interaction >= INTERACTION_ARROW_FA) && (dmgFlags & 0x20800)) ||
                        ((interaction <= INTERACTION_STICK) && (player->unk_860 != 0)))) { // out: lit by it
                if ((interaction < INTERACTION_NONE) && (player->unk_860 < 0xC8)) {
                    player->unk_860 = 0xC8;
                }
                if (groupSize == 0) {
                    if ((type == TYPE_NO_SWITCH) &&
                        (OBJ_SYOKUDAI_GET_SWITCH_FLAG(thisx) == OBJ_SYOKUDAI_SWITCH_FLAG_NONE)) {
                        this->snuffTimer = SNUFF_NEVER;
                    } else {
                        NvMmSyokudai_SetSwitch(this, play);
                    }
                } else {
                    sNumLitTorchesInGroup++;
                    if (sNumLitTorchesInGroup >= groupSize) {
                        NvMmSyokudai_SetSwitch(this, play);
                    } else {
                        this->snuffTimer =
                            OBJ_SYOKUDAI_SNUFF_TIMER_INITIAL(groupSize) + OBJ_SYOKUDAI_SNUFF_TIMER_JUST_LIT_BONUS;
                    }
                }
                Audio_PlaySfxGeneral(NA_SE_EV_FLAME_IGNITION, &thisx->projectedPos, 4, &gSfxDefaultFreqAndVolScale,
                                     &gSfxDefaultFreqAndVolScale, &gSfxDefaultReverb);
            }
        }
    }
    Collider_UpdateCylinder(thisx, &this->standCollider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->standCollider.base);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->standCollider.base);
    Collider_UpdateCylinder(thisx, &this->flameCollider);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->flameCollider.base);
    if (this->snuffTimer > SNUFF_OUT) {
        this->snuffTimer--;
        if ((this->snuffTimer == SNUFF_OUT) && (type != TYPE_SWITCH_CAUSES_FLAME)) {
            sNumLitTorchesInGroup--;
        }
    }
    if (this->snuffTimer != SNUFF_OUT) {
        if ((this->snuffTimer <= SNUFF_NEVER) || (this->snuffTimer >= SNUFF_DEFAULT)) {
            lightRadius = OBJ_SYOKUDAI_LIGHT_RADIUS_MAX;
        } else {
            lightRadius = (f32)this->snuffTimer * OBJ_SYOKUDAI_LIGHT_RADIUS_MAX / SNUFF_DEFAULT;
        }
        lightIntensity = Rand_ZeroOne() * 127;
        lightIntensity += 128;
        Actor_PlaySfx_Flagged(thisx, NA_SE_EV_TORCH - SFX_FLAG);
    }
    Lights_PointSetColorAndRadius(&this->lightInfo, lightIntensity, lightIntensity * 0.7f, 0, lightRadius);
    this->flameTexScroll++;
}

void NvMmSyokudai_Draw(Actor* thisx, PlayState* play) {
    NvMmSyokudai* this = (NvMmSyokudai*)thisx;
    s32 groupSize = OBJ_SYOKUDAI_GET_GROUP_SIZE(thisx);
    f32 flameScale;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, sDLists[OBJ_SYOKUDAI_GET_TYPE(thisx) % 3]);
    if (this->snuffTimer != SNUFF_OUT) {
        s32 snuffTimerInitial = OBJ_SYOKUDAI_SNUFF_TIMER_INITIAL(groupSize);

        flameScale = 1.0f;
        if (snuffTimerInitial < this->snuffTimer) {
            flameScale = (f32)(snuffTimerInitial - this->snuffTimer + OBJ_SYOKUDAI_SNUFF_TIMER_JUST_LIT_BONUS) /
                         OBJ_SYOKUDAI_SNUFF_TIMER_JUST_LIT_BONUS;
        } else if ((this->snuffTimer > SNUFF_OUT) && (this->snuffTimer < SNUFF_DEFAULT)) {
            flameScale = (f32)this->snuffTimer / SNUFF_DEFAULT;
        }
        flameScale *= 0.0027f;
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        gSPSegment(POLY_XLU_DISP++, 0x08,
                   Gfx_TwoTexScrollEx(play->state.gfxCtx, 0, 0, 0, 0x20, 0x40, 1, 0,
                                      (this->flameTexScroll * -SNUFF_DEFAULT) & 0x1FF, 0x20, 0x80, 0, 0, 0, -SNUFF_DEFAULT));
        gDPSetPrimColor(POLY_XLU_DISP++, 0x80, 0x80, 255, 255, 0, 255);
        gDPSetEnvColor(POLY_XLU_DISP++, 255, 0, 0, 0);
        Matrix_Translate(0.0f, OBJ_SYOKUDAI_FLAME_HEIGHT, 0.0f, MTXMODE_APPLY);
        Matrix_RotateY(BINANG_TO_RAD((s16)(Camera_GetCamDirYaw(GET_ACTIVE_CAM(play)) - thisx->shape.rot.y + 0x8000)),
                       MTXMODE_APPLY);
        Matrix_Scale(flameScale, flameScale, flameScale, MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, DL_FIRE);
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
