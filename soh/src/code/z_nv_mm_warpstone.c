/*
 * Nightmareverse: the owl statues (MM's Obj_Warpstone, MM id 0x223), ported as they are (mm/README.md
 * "Owl statues"): MM's model (object_sek, closed and open), its text (0xC00 "Ye who hold the sacred
 * sword...", 0xC01 "You can save your progress and quit here"), its opening (a sword hit: the light
 * flaring from its beak, MM's gameplay_keep flash, then open), its warp id (params & 0xF: MM's
 * OWL_WARP_*, 4 for Clock Town).
 *
 * Opened statues are kept for good (MM's owlActivationFlags: the Song of Time leaves them; the termina
 * section's "permanent"). Saving at one (MM: save and quit, the game resumes there) is a save of the one
 * file, as the pause menu's, marked as made at that owl: loading it puts Link at the owl's spawn (MM's
 * sOwlWarpEntrances) with Termina's clock as it was, not at OoT's respawn. Adapted from MM: the game
 * goes on after the save (MM quits to the title), and the owl's mark stays until another save (MM's owl
 * save is gone once loaded). Not yet: the Song of Soaring's warps, MM's opening camera.
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>

#define SEK(name) ((Gfx*)("__OTR__mm/objects/object_sek/" name))
#define KEEP(name) ((Gfx*)("__OTR__mm/objects/gameplay_keep/" name))
#define NV_MM_TEXT(id) (0xB000 + (id)) // (MM's text, NvMmText.cpp)

void Save_SaveFile(void);

typedef struct {
    DynaPolyActor dyna;
    ColliderCylinder collider;
    u8 isTalking;
    u8 openingTimer;
    u8 modelIndex; // 0 closed, 1 open
    u8 state;      // 0 closed, 1 opening, 2 open
} NvMmWarpstone;

const size_t gNvMmWarpstoneSize = sizeof(NvMmWarpstone);

#define OWL_ID(thisx) ((thisx)->params & 0xF)

static u16 sOwlsActivated;  // MM's owlActivationFlags (saved: NvMmSave.cpp)
static s32 sOwlSaveId = -1; // the owl the save was made at (-1: none)

u16* NvMmOwl_Activated(void) {
    return &sOwlsActivated;
}

static s32 sOwlSaving; // (a save now is the owl's: from its save until Link leaves it; SoH writes the file on a thread)

/** NvMmSave.cpp: what the file records: the owl, for a save made at one; -1 for any other save. */
s32 NvMmOwl_SaveMark(void) {
    return sOwlSaving ? sOwlSaveId : -1;
}

/** NvMmSave.cpp: the loaded file's mark. */
void NvMmOwl_SetSaveMark(s32 owl) {
    sOwlSaveId = owl;
}

/** MM's sOwlWarpEntrances (En_Test7), for the owls of the places that are here: the spawn there. */
static s32 NvMmOwl_Entrance(s32 owl) {
    switch (owl) {
        case 4: // OWL_WARP_CLOCK_TOWN: ENTRANCE(SOUTH_CLOCK_TOWN, 9)
            return ENTR_MM_Z2_CLOCKTOWER_9;
        default:
            return -1;
    }
}

/** NvMmSave.cpp's load hook: a save made at an owl resumes at the owl. */
void NvMmOwl_OnLoad(void) {
    s32 entrance = (sOwlSaveId >= 0) ? NvMmOwl_Entrance(sOwlSaveId) : -1;

    if (entrance >= 0) {
        gSaveContext.entranceIndex = entrance;
        if (getenv("NV_LOG") != NULL) {
            printf("NVMM owl: the save resumes at owl %d (entrance %04X)\n", (int)sOwlSaveId, (unsigned)entrance);
            fflush(stdout);
        }
    }
}

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_METAL, AT_NONE, AC_ON | AC_HARD | AC_TYPE_PLAYER, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_2, COLSHAPE_CYLINDER },
    // (what opens it: OoT's swords and the Fierce Deity's, as MM's sword hit; the hookshot sticks)
    { ELEMTYPE_UNK2, { 0x00100000, 0x00, 0x00 }, { DMG_SLASH | DMG_SPIN_ATTACK | DMG_JUMP_KOKIRI | DMG_JUMP_MASTER | DMG_JUMP_GIANT | DMG_HOOKSHOT, 0x00, 0x00 },
      TOUCH_NONE | TOUCH_SFX_NORMAL, BUMP_ON | BUMP_HOOKABLE, OCELEM_ON },
    { 20, 60, 0, { 0, 0, 0 } },
};

void NvMmWarpstone_Init(Actor* thisx, PlayState* play) {
    NvMmWarpstone* this = (NvMmWarpstone*)thisx;

    thisx->targetMode = 1;
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sCylinderInit);
    Actor_SetFocus(thisx, 40.0f);
    if (sOwlsActivated & (1 << OWL_ID(thisx))) {
        this->state = 2;
        this->modelIndex = 1;
    }
}

void NvMmWarpstone_Destroy(Actor* thisx, PlayState* play) {
    sOwlSaving = false;
    Collider_DestroyCylinder(play, &((NvMmWarpstone*)thisx)->collider);
}

/** MM's actions: closed (a sword hit opens it), opening (the light, then open), open. True while it
 * shouldn't be talked to. */
static s32 NvMmWarpstone_Action(NvMmWarpstone* this, PlayState* play) {
    Actor* thisx = &this->dyna.actor;

    switch (this->state) {
        case 0:
            if (this->collider.base.acFlags & AC_HIT) {
                this->state = 1;
                Audio_PlayActorSound2(thisx, NA_SE_EV_TRE_BOX_APPEAR); // (MM's NA_SE_EV_OWL_WARP_SWITCH_ON: OoT has none; its chime)
                return true;
            }
            thisx->textId = NV_MM_TEXT(0xC00); // "Ye who hold the sacred sword, leave proof of our encounter."
            return false;
        case 1: // MM's ObjWarpstone_PlayOpeningCutscene (velocity.x: the light's size, home.rot.x: its alpha)
            if (this->openingTimer++ >= 66) {
                sOwlsActivated |= 1 << OWL_ID(thisx);
                this->state = 2;
                if (getenv("NV_LOG") != NULL) {
                    printf("NVMM owl: owl %d opened\n", (int)OWL_ID(thisx));
                    fflush(stdout);
                }
            } else if (this->openingTimer < 25) {
                Math_StepToF(&thisx->velocity.x, 0.01f, 0.001f);
                Math_StepToS(&thisx->home.rot.x, 0xFF, 0x12);
            } else {
                Math_StepToF(&thisx->velocity.x, 20.0f, 0.01f);
                if (thisx->velocity.x > 0.2f) {
                    this->modelIndex = 1;
                    Math_StepToS(&thisx->home.rot.x, 0, 0x14);
                }
            }
            return true;
        default:
            thisx->textId = NV_MM_TEXT(0xC01); // "You can save your progress and quit here."
            return false;
    }
}

/** The owl's save: the one file, marked as made here. */
static void NvMmWarpstone_Save(NvMmWarpstone* this, PlayState* play) {
    sOwlSaveId = OWL_ID(&this->dyna.actor);
    Play_SaveSceneFlags(play);
    gSaveContext.savedSceneNum = play->sceneNum;
    sOwlSaving = true;
    Save_SaveFile();
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM owl: saved at owl %d\n", (int)sOwlSaveId);
        fflush(stdout);
    }
}

void NvMmWarpstone_Update(Actor* thisx, PlayState* play) {
    NvMmWarpstone* this = (NvMmWarpstone*)thisx;

    if (sOwlSaving && (thisx->xzDistToPlayer > 150.0f)) {
        sOwlSaving = false; // (gone from the owl: a save now is an ordinary one)
    }
    if (this->isTalking) {
        if (Actor_TextboxIsClosing(thisx, play)) {
            this->isTalking = false;
        } else if ((Message_GetState(&play->msgCtx) == TEXT_STATE_CHOICE) && Message_ShouldAdvance(play)) {
            if (play->msgCtx.choiceIndex != 0) { // (MM: the second choice saves)
                Sfx_PlaySfxCentered(NA_SE_SY_PIECE_OF_HEART);
                NvMmWarpstone_Save(this, play);
            } else {
                Sfx_PlaySfxCentered(NA_SE_SY_MESSAGE_PASS);
            }
            Message_CloseTextbox(play);
        }
    } else if (Actor_ProcessTalkRequest(thisx, play)) {
        this->isTalking = true;
    } else if (!NvMmWarpstone_Action(this, play)) {
        Actor_OfferTalkNearColChkInfoCylinder(thisx, play);
    }
    this->collider.base.acFlags &= ~AC_HIT;
    Collider_UpdateCylinder(thisx, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    CollisionCheck_SetAC(play, &play->colChkCtx, &this->collider.base);
}

void NvMmWarpstone_Draw(Actor* thisx, PlayState* play) {
    NvMmWarpstone* this = (NvMmWarpstone*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, this->modelIndex ? SEK("gOwlStatueOpenedDL") : SEK("gOwlStatueClosedDL"));
    if (thisx->home.rot.x != 0) { // the light at its beak, opening: two flashes turning against each other
        Gfx_SetupDL_25Xlu(play->state.gfxCtx);
        Matrix_Translate(thisx->world.pos.x, thisx->world.pos.y + 34.0f, thisx->world.pos.z, MTXMODE_NEW);
        Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);
        Matrix_Translate(0.0f, 0.0f, 30.0f, MTXMODE_APPLY);
        Matrix_Scale(thisx->velocity.x, thisx->velocity.x, thisx->velocity.x, MTXMODE_APPLY);
        Matrix_Push();
        gDPPipeSync(POLY_XLU_DISP++);
        gDPSetPrimColor(POLY_XLU_DISP++, 128, 128, 255, 255, 200, thisx->home.rot.x);
        gDPSetEnvColor(POLY_XLU_DISP++, 100, 200, 0, 255);
        Matrix_RotateZ(((play->gameplayFrames * 1500) & 0xFFFF) * (M_PI / 0x8000), MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, KEEP("gEffFlash1DL"));
        Matrix_Pop();
        Matrix_RotateZ((~((play->gameplayFrames * 1200) & 0xFFFF) & 0xFFFF) * (M_PI / 0x8000), MTXMODE_APPLY);
        gSPMatrix(POLY_XLU_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_XLU_DISP++, KEEP("gEffFlash1DL"));
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
