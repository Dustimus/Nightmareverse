/*
 * Nightmareverse: Majora's Mask's forms on Link (mm/README.md "The forms"). The first: the Deku form.
 *
 * Wearing the Deku Mask (its C button, as MM's) turns Link into Deku Link, and pressing it again
 * turns him back. He stays Deku through doors and loads, and in the save (as MM: the form is kept),
 * in Termina and in Hyrule alike (NV_GAMES.md: the forms are carried over).
 *
 * Nothing of OoT's player code is rewritten for this. Deku Link is OoT's young Link with:
 * - MM's Deku skeleton (object_link_nuts) under OoT's animations: MM's Deku Link plays the same
 *   animations as its human Link for walking, running, standing... (MM's D_8085BE84 has no form
 *   column), and the two skeletons have the same 21 joints in the same order;
 * - MM's Deku hands, waist and sheath (z_player_lib.c's limb hook asks NvMmForm_OverrideLimb),
 *   no sword, no shield (B is empty while Deku, as MM's: the sword is put back when he's human);
 * - MM's Deku size: the ceiling, wall, ledge and water heights of MM's sPlayerAgeProperties for
 *   PLAYER_FORM_DEKU, over OoT's young Link's (OoT's voice and animations kept).
 * - MM's people see the form: NvMmForm_Get, MM's GET_PLAYER_FORM (the message scripts' CHECK_DEKU).
 *
 * MM's transformation scene (Player_Action_86, then Player_Action_87 after the change), as MM's:
 * Link turns to the camera and puts the mask on (cl_setmask, then cl_setmaskend; the mask drawn on
 * his face from frame 12, squashing onto it, the fused mask from frame 51), or as Deku pulls it off
 * (pn_maskoffstart); the light darkens and a light flares by him (MM's D_8085D848, D_8085D910: the
 * ambient, fog and a point light, a lightning crack); then the screen goes white (45 a frame), he
 * changes, and it comes back (Action_87: as human, his idle from its start). It plays on every
 * change; into a form it can be skipped (A, B or a C button, from its fifth frame) only once that
 * form's scene has been watched to the end once (MM's week event flag, here a bit in the save),
 * back to human always. The camera is a close one of our own on his face (MM's is the scene's
 * cutscene camera, MM's CutsceneManager); the sounds are OoT's nearest where MM's are MM's own.
 *
 * His moves (the spin, the guard, the hops across water, sinking): z_nv_mm_deku.c.
 * Not yet: the bubble, the flower launch, the items he may use (only
 * masks and the ocarina, the pipes, for now), adult Link (forms don't depend on age: open), the
 * Goron, Zora and Fierce Deity forms.
 */
#include "global.h"
#include "soh/ResourceManagerHelpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MM_FORM_DEKU 3 // MM's PLAYER_FORM_*: Fierce Deity 0, Goron 1, Zora 2, Deku 3, human 4
#define MM_FORM_HUMAN 4
#define MM_ITEM_MASK_DEKU 0x32

#define DEKU(name) ("__OTR__mm/objects/object_link_nuts/" name)

typedef struct {
    s32 form;  // MM's PLAYER_FORM_* (4: human)
    s32 savedB; // the B button's item while a form has none (the sword), put back as human
    s32 savedSwordless; // OoT's "no sword" flag (infTable[29]: B left empty), put back as human
    s32 sceneSeen;      // per form (1 << form): its scene watched to the end (MM's gPlayerTransformMaskFlags)
} NvMmFormSave;

static NvMmFormSave sForm = { MM_FORM_HUMAN, ITEM_NONE, 0, 0 };
static s32 sChangeTo = -1; // a change under way: to this form
static s32 sChangeFrame;     // MM's av1.actionVar1
static s32 sFillAlpha;       // MM's R_PLAY_FILL_SCREEN_ALPHA
static s32 sFillOn;          // MM's R_PLAY_FILL_SCREEN_ON: a frame's step (45 to white, then back)
static f32 sLightMix;        // MM's unk_B10[4]: the scene's light, 0 (as it was) to 3
static f32 sLightFlare;      // MM's unk_B10[5]: which point light, and how far it reaches
static f32 sMaskSquashZ;     // MM's unk_B10[2], unk_B10[3]: the mask squashing onto his face
static f32 sMaskSquashY;
static s32 sFromForm;        // the form the scene started in (MM's this->transformation then)
static s16 sCamId = -1;      // our camera on his face
static s16 sFaceYaw;
static LightInfo sLightInfo;
static LightNode* sLightNode;
static s16 sSavedAdj[10];    // OoT's light adjustments before the scene (MM's D_80862B50)
static PlayerAgeProperties sDekuProperties;
static const void* sAppliedLimbs; // the skeleton Link was last given
static PlayerAgeProperties* sOotProperties;

extern FlexSkeletonHeader* gPlayerSkelHeaders[];
s32 NvMmItem_Has(s32 item);
void NvMmDeku_Update(PlayState* play, Player* player);
void NvMmMask_TakeOff(void);

/** MM's GET_PLAYER_FORM: Link's form (MM's PLAYER_FORM_*). */
s32 NvMmForm_Get(void) {
    return sForm.form;
}

/** z_actor.c's Player_GetHeight (the camera's and others' Link height): MM's for the form, 0 as human. */
f32 NvMmForm_Height(void) {
    return (sForm.form == MM_FORM_DEKU) ? 36.0f : 0.0f;
}

NvMmFormSave* NvMmForm_SaveVars(void) {
    return &sForm;
}

void NvMmForm_NewGame(void) {
    sForm.form = MM_FORM_HUMAN;
    sForm.savedB = ITEM_NONE;
    sForm.savedSwordless = 0;
    sForm.sceneSeen = 0;
    sChangeTo = -1;
}

/** MM's sPlayerAgeProperties[PLAYER_FORM_DEKU] over OoT's young Link's: the sizes, not the animations. */
static void NvMmForm_MakeDekuProperties(PlayerAgeProperties* oot) {
    static const f32 sSizes[] = { 35.0f, 50.0f, 0.3f,  71.0f, 50.0f, 49.0f, 39.0f, 27.0f, 19.0f,
                                  8.0f,  13.6f, 24.0f, 24.0f, 70.0f, 14.0f, 12.0f, 55.0f };
    static const Vec3s s44 = { -0x18, 0xDED, 0x36C };
    static const Vec3s s4A[4] = { { -0x18, 0xD92, 0x35E }, { -0x18, 0x1371, 0x3A9 }, { 8, 0x1256, 0x17C },
                                  { 9, 0x17EA, 0x167 } };
    static const Vec3s s62[4] = { { -0x18, 0x1371, 0x3A9 }, { -0x18, 0x195F, 0x3A9 }, { 9, 0x17EA, 0x167 },
                                  { 9, 0x1E0D, 0x17C } };
    static const Vec3s s7A[4] = { { 8, 0x1256, 0x17C }, { 9, 0x17EA, 0x167 }, { -0x638, 0x1256, 0x17C },
                                  { -0x637, 0x17EA, 0x167 } };

    sDekuProperties = *oot; // (OoT's voice, sounds and animations)
    memcpy(&sDekuProperties, sSizes, sizeof(sSizes)); // ceilingCheckHeight ... unk_40
    sDekuProperties.unk_44 = s44;
    memcpy(sDekuProperties.unk_4A, s4A, sizeof(s4A));
    memcpy(sDekuProperties.unk_62, s62, sizeof(s62));
    memcpy(sDekuProperties.unk_7A, s7A, sizeof(s7A)); // (OoT's unk_7A[2] and unk_86[2]: MM's unk_7A[4])
}

/** Link's skeleton and size for his form (now, and on each new Link: a scene's start). */
static void NvMmForm_Apply(PlayState* play, Player* player) {
    FlexSkeletonHeader* header;
    const char* path = (sForm.form == MM_FORM_DEKU) ? DEKU("gLinkDekuSkel") : (const char*)gPlayerSkelHeaders[1];

    header = ResourceMgr_LoadSkeletonByName(path, &player->skelAnime);
    if (header == NULL) {
        return;
    }
    player->skelAnime.skeleton = SEGMENTED_TO_VIRTUAL(header->sh.segment);
    player->skelAnime.dListCount = header->dListCount;
    player->upperSkelAnime.skeleton = player->skelAnime.skeleton;
    player->upperSkelAnime.dListCount = header->dListCount;
    sAppliedLimbs = player->skelAnime.skeleton;

    if (sForm.form == MM_FORM_DEKU) {
        if (player->ageProperties != &sDekuProperties) {
            sOotProperties = player->ageProperties;
            NvMmForm_MakeDekuProperties(sOotProperties);
            player->ageProperties = &sDekuProperties;
        }
    } else if ((player->ageProperties == &sDekuProperties) && (sOotProperties != NULL)) {
        player->ageProperties = sOotProperties;
    }
}

static void NvMmForm_Change(PlayState* play, Player* player, s32 form) {
    if (form == MM_FORM_DEKU) {
        if (sForm.form == MM_FORM_HUMAN) {
            sForm.savedB = gSaveContext.equips.buttonItems[0];
            sForm.savedSwordless = gSaveContext.infTable[29];
        }
        gSaveContext.equips.buttonItems[0] = ITEM_NONE; // (no sword: OoT's swordless B, left empty)
        gSaveContext.infTable[29] = 1;
        player->currentShield = PLAYER_SHIELD_NONE;
    } else {
        gSaveContext.equips.buttonItems[0] = sForm.savedB;
        gSaveContext.infTable[29] = sForm.savedSwordless;
        player->currentShield = CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);
        sForm.savedB = ITEM_NONE;
    }
    Interface_LoadItemIcon1(play, 0);
    NvMmMask_TakeOff(); // (MM: a change takes the mask worn off)
    player->currentMask = PLAYER_MASK_NONE;
    sForm.form = form;
    NvMmForm_Apply(play, player);
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM form: %s\n", (form == MM_FORM_DEKU) ? "Deku" : "human");
        fflush(stdout);
    }
}

/* ---- MM's transformation scene ---- */

#define MMANIM(name) ((LinkAnimationHeader*)("__OTR__mm/player_anim/" name))
#define ANIM_SETMASK MMANIM("gPlayerAnim_cl_setmask")
#define ANIM_SETMASKEND MMANIM("gPlayerAnim_cl_setmaskend")
#define ANIM_DEKU_MASKOFF MMANIM("gPlayerAnim_pn_maskoffstart")

s32 Player_SetupAction(PlayState* play, Player* this, PlayerActionFunc actionFunc, s32 flags);
void func_80839F90(Player* this, PlayState* play);
LinkAnimationHeader* Player_GetIdleAnim(Player* this);
void Player_PlayVoiceSfx(Player* this, u16 sfxId);

typedef struct {
    s16 fogNear;
    u8 fogColor[3];
    u8 ambientColor[3];
} NvMmSceneEnv; // MM's struct_8085D848_unk_00

typedef struct {
    Vec3f pos;
    u8 color[3];
    s16 radius;
} NvMmSceneLight; // MM's struct_8085D848_unk_18

// MM's D_8085D848: [0] from human (into a form), [1] from a form (back to human)
static const NvMmSceneEnv sSceneEnv[2][3] = {
    { { 650, { 0, 0, 0 }, { 10, 0, 30 } }, { 300, { 200, 200, 255 }, { 0, 0, 0 } }, { 600, { 0, 0, 0 }, { 0, 0, 200 } } },
    { { 650, { 0, 0, 0 }, { 10, 0, 30 } }, { 300, { 200, 200, 255 }, { 0, 0, 0 } }, { 600, { 0, 0, 0 }, { 0, 0, 200 } } },
};
static const NvMmSceneLight sSceneLights[2][3] = {
    { { { -40.0f, 20.0f, -10.0f }, { 120, 200, 255 }, 1000 }, { { 0.0f, -10.0f, 0.0f }, { 255, 255, 255 }, 5000 },
      { { -10.0f, 4.0f, 3.0f }, { 200, 200, 255 }, 5000 } },
    { { { 0.0f, 0.0f, 5.0f }, { 155, 255, 255 }, 100 }, { { 0.0f, 0.0f, 5.0f }, { 155, 255, 255 }, 100 },
      { { 0.0f, 0.0f, 5.0f }, { 155, 255, 255 }, 100 } },
};

/** MM's func_80854EFC: the scene's light, `mix` 0 (as the room has it) through the three steps and back. */
static void NvMmForm_SceneEnv(PlayState* play, f32 mix, const NvMmSceneEnv* steps) {
    EnvLightSettings* cur = &play->envCtx.lightSettings;
    NvMmSceneEnv now;
    const NvMmSceneEnv* to;
    const NvMmSceneEnv* from;
    static const u8 sBlack[3] = { 0, 0, 0 };
    const u8* lightTo;
    const u8* lightFrom;
    s32 i;

    now.fogNear = cur->fogNear;
    for (i = 0; i < 3; i++) {
        now.fogColor[i] = cur->fogColor[i];
        now.ambientColor[i] = cur->ambientColor[i];
    }
    if (mix <= 1.0f) {
        to = &steps[0], from = &now, lightTo = sBlack, lightFrom = cur->light1Color;
    } else if (mix <= 2.0f) {
        mix -= 1.0f, to = &steps[1], from = &steps[0], lightTo = sBlack, lightFrom = sBlack;
    } else if (mix <= 3.0f) {
        mix -= 2.0f, to = &steps[2], from = &steps[1], lightTo = sBlack, lightFrom = sBlack;
    } else {
        mix -= 3.0f, to = &now, from = &steps[2], lightTo = cur->light1Color, lightFrom = sBlack;
    }
    play->envCtx.adjFogNear = (s16)((to->fogNear - from->fogNear) * mix) + from->fogNear - cur->fogNear;
    for (i = 0; i < 3; i++) { // (MM's func_80854CD0)
        play->envCtx.adjFogColor[i] = (s32)((to->fogColor[i] - from->fogColor[i]) * mix) + from->fogColor[i] - cur->fogColor[i];
        play->envCtx.adjAmbientColor[i] =
            (s32)((to->ambientColor[i] - from->ambientColor[i]) * mix) + from->ambientColor[i] - cur->ambientColor[i];
        play->envCtx.adjLight1Color[i] = (s32)((lightTo[i] - lightFrom[i]) * mix) + lightFrom[i] - cur->light1Color[i];
    }
}

/** MM's func_808550D0: the light (env `mix`) and the point light (`flare`: which one, and its reach). */
static void NvMmForm_SceneLight(PlayState* play, Player* player, f32 mix, f32 flare, s32 set) {
    const NvMmSceneLight* light = &sSceneLights[set][0];
    Vec3f pos;
    f32 s = Math_SinS(player->actor.shape.rot.y);
    f32 c = Math_CosS(player->actor.shape.rot.y);

    NvMmForm_SceneEnv(play, mix, sSceneEnv[set]);
    if (flare > 2.0f) {
        flare -= 2.0f;
        light += 2;
    } else if (flare > 1.0f) {
        flare -= 1.0f;
        light++;
    }
    // (MM's Player_TranslateAndRotateY)
    pos.x = player->actor.world.pos.x + (light->pos.x * c) + (light->pos.z * s);
    pos.y = player->actor.world.pos.y + light->pos.y;
    pos.z = player->actor.world.pos.z - (light->pos.x * s) + (light->pos.z * c);
    Lights_PointNoGlowSetInfo(&sLightInfo, pos.x, pos.y, pos.z, light->color[0], light->color[1], light->color[2],
                              light->radius * flare);
}

/** Our camera on his face (MM's is the scene's cutscene camera): in front, a little above. */
static void NvMmForm_SceneCamera(PlayState* play, Player* player) {
    Vec3f at = player->bodyPartsPos[PLAYER_BODYPART_HEAD];
    Vec3f eye;
    f32 dist = (sFromForm == MM_FORM_HUMAN) ? 70.0f : 90.0f;

    at.y -= 4.0f;
    eye.x = at.x + Math_SinS(sFaceYaw) * dist;
    eye.y = at.y + 8.0f;
    eye.z = at.z + Math_CosS(sFaceYaw) * dist;
    if (sCamId >= 0) {
        Play_CameraSetAtEye(play, sCamId, &at, &eye);
    }
}

static void NvMmForm_SceneEnd(Player* this, PlayState* play);

/** The scene, before the change (MM's Player_Action_86 and func_80855218). */
static void NvMmForm_Scene(Player* this, PlayState* play) {
    Input* input = &play->state.input[0];
    s32 skip = false;
    s32 human = (sFromForm == MM_FORM_HUMAN);

    this->actor.shape.rot.y = this->yaw = sFaceYaw;
    this->linearVelocity = 0.0f;
    NvMmForm_SceneCamera(play, this);

    // func_80855218: the animation, the mask squashing on, its sounds
    if (LinkAnimation_Update(play, &this->skelAnime) && (this->skelAnime.animation == ANIM_SETMASK)) {
        LinkAnimation_PlayLoop(play, &this->skelAnime, ANIM_SETMASKEND);
    } else if (human) {
        if (sChangeFrame >= 64) {
            Math_StepToF(&sMaskSquashZ, 0.0f, 0.015f);
        } else if (sChangeFrame >= 14) {
            Math_StepToF(&sMaskSquashZ, 0.3f, 0.3f);
        }
        if (sChangeFrame > 65) {
            Math_StepToF(&sMaskSquashY, 0.0f, 0.02f);
        } else if (sChangeFrame >= 16) {
            Math_StepToF(&sMaskSquashY, -0.1f, 0.1f);
        }
        if ((sFillOn == 0) && (this->skelAnime.animation == ANIM_SETMASK)) { // (MM's D_8085D8F0)
            if (LinkAnimation_OnFrame(&this->skelAnime, 2.0f)) {
                Player_PlaySfx(&this->actor, NA_SE_PL_PUT_OUT_ITEM);
            } else if (LinkAnimation_OnFrame(&this->skelAnime, 4.0f)) {
                Player_PlaySfx(&this->actor, NA_SE_PL_CHANGE_ARMS); // (MM: NA_SE_IT_SET_TRANSFORM_MASK)
            } else if (LinkAnimation_OnFrame(&this->skelAnime, 11.0f)) {
                Player_PlaySfx(&this->actor, NA_SE_PL_FREEZE_S);
            } else if (LinkAnimation_OnFrame(&this->skelAnime, 30.0f)) {
                Player_PlayVoiceSfx(this, NA_SE_VO_LI_TAKEN_AWAY); // (MM: NA_SE_PL_TRANSFORM_VOICE)
            }
        }
    } else if (sFillOn == 0) { // (MM's D_8085D904, and the face change at 15)
        if (LinkAnimation_OnFrame(&this->skelAnime, 8.0f)) {
            Player_PlaySfx(&this->actor, NA_SE_PL_CHANGE_ARMS);
        }
        if (sChangeFrame == 15) {
            Player_PlaySfx(&this->actor, NA_SE_PL_FACE_UP); // (MM: NA_SE_PL_FACE_CHANGE)
        }
    }

    if (sFillOn != 0) { // to white: then the change
        sFillAlpha += sFillOn;
        if (sFillAlpha > 255) {
            sFillAlpha = 255;
            NvMmForm_Change(play, this, sChangeTo);
            LinkAnimation_PlayOnce(play, &this->skelAnime, Player_GetIdleAnim(this));
            if (sCamId >= 0) { // (the screen's white: our camera goes, the room's comes back)
                Play_ClearCamera(play, sCamId);
                Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_ACTIVE);
                sCamId = -1;
            }
            sChangeFrame = 0;
            Player_SetupAction(play, this, NvMmForm_SceneEnd, 0);
        }
    } else if ((sChangeFrame++ > (human ? 0x53 : 0x37)) ||
               ((sChangeFrame >= 5) && (!human || (sForm.sceneSeen & (1 << sChangeTo))) &&
                (skip = CHECK_BTN_ANY(input->press.button, BTN_CRIGHT | BTN_CLEFT | BTN_CDOWN | BTN_CUP | BTN_B | BTN_A)))) {
        sFillOn = 45;
        sFillAlpha = 0;
        Sfx_PlaySfxCentered(NA_SE_SY_WHITE_OUT_T); // (MM: NA_SE_SY_TRANSFORM_MASK_FLASH)
        if (!skip) {
            sForm.sceneSeen |= 1 << sChangeTo; // (watched to the end: skippable from now on)
        }
    }

    // the light (MM's D_8085D910[0]: 0x10, 0xA, 0x3B, 0x3F)
    if (sChangeFrame >= 0x10) {
        if (sChangeFrame < 0x3B) {
            Math_StepToF(&sLightMix, 1.0f, 0.1f);
        } else if (sChangeFrame < 0x3F) {
            if (sChangeFrame == 0x3B) {
                Sfx_PlaySfxCentered(NA_SE_EV_LIGHTNING); // (MM: NA_SE_EV_LIGHTNING_HARD)
            }
            Math_StepToF(&sLightMix, 2.0f, 0.5f);
        } else {
            Math_StepToF(&sLightMix, 3.0f, 0.2f);
        }
        if (sChangeFrame < 0x40) {
            Math_StepToF(&sLightFlare, 1.0f, 0.2f);
        } else {
            Math_StepToF(&sLightFlare, 3.0f, 0.55f);
        }
    }
    NvMmForm_SceneLight(play, this, sLightMix, sLightFlare, human ? 0 : 1);
}

/** After the change, from white (MM's Player_Action_87). */
static void NvMmForm_SceneEnd(Player* this, PlayState* play) {
    s32 toHuman = (sForm.form == MM_FORM_HUMAN);
    s32 i;

    if (sFillOn != 0) {
        sFillAlpha -= sFillOn;
        if (sFillAlpha < 0) {
            sFillOn = 0;
            sFillAlpha = 0;
        }
    }
    if (sChangeFrame++ < 4) {
        if (toHuman && (sChangeFrame == 4)) {
            LinkAnimation_Change(play, &this->skelAnime, Player_GetIdleAnim(this), 1.0f, 0.0f, 20.0f, ANIMMODE_ONCE,
                                 20.0f);
        }
    } else {
        f32 dist = sqrtf(SQ(play->state.input[0].rel.stick_x) + SQ(play->state.input[0].rel.stick_y));

        if (LinkAnimation_Update(play, &this->skelAnime) || ((sChangeFrame > 10) && (dist != 0.0f))) {
            if (sFillOn == 0) { // the scene's over
                sChangeTo = -1;
                this->stateFlags1 &= ~PLAYER_STATE1_IN_CUTSCENE;
                for (i = 0; i < 3; i++) {
                    play->envCtx.adjAmbientColor[i] = sSavedAdj[i];
                    play->envCtx.adjLight1Color[i] = sSavedAdj[3 + i];
                    play->envCtx.adjFogColor[i] = sSavedAdj[6 + i];
                }
                play->envCtx.adjFogNear = sSavedAdj[9];
                if (sLightNode != NULL) {
                    LightContext_RemoveLight(play, &play->lightCtx, sLightNode);
                    sLightNode = NULL;
                }
                play->envCtx.fillScreen = false;
                func_80839F90(this, play);
                return;
            }
        }
        Math_StepToF(&sLightFlare, 4.0f, 0.2f);
    }
    NvMmForm_SceneLight(play, this, 0.0f, sLightFlare, toHuman ? 0 : 1);
}

/** MM's func_808388B8: the scene starts. */
static void NvMmForm_StartScene(PlayState* play, Player* player, s32 to) {
    Camera* mainCam = Play_GetCamera(play, CAM_ID_MAIN);
    s32 i;

    sChangeTo = to;
    sFromForm = sForm.form;
    sChangeFrame = 0;
    sFillOn = 0;
    sFillAlpha = 0;
    sLightMix = sLightFlare = 0.0f;
    sMaskSquashZ = sMaskSquashY = 0.0f;
    sFaceYaw = Camera_GetCamDirYaw(mainCam) + 0x8000; // (to the camera, as MM's)
    NvMmMask_TakeOff();
    player->currentMask = PLAYER_MASK_NONE;
    Player_SetupAction(play, player, NvMmForm_Scene, 0);
    player->stateFlags1 |= PLAYER_STATE1_IN_CUTSCENE;
    player->actor.velocity.y = 0.0f;
    LinkAnimation_Change(play, &player->skelAnime, (sFromForm == MM_FORM_HUMAN) ? ANIM_SETMASK : ANIM_DEKU_MASKOFF,
                         1.0f, 0.0f, Animation_GetLastFrame((sFromForm == MM_FORM_HUMAN) ? ANIM_SETMASK : ANIM_DEKU_MASKOFF),
                         ANIMMODE_ONCE, -6.0f);
    for (i = 0; i < 3; i++) {
        sSavedAdj[i] = play->envCtx.adjAmbientColor[i];
        sSavedAdj[3 + i] = play->envCtx.adjLight1Color[i];
        sSavedAdj[6 + i] = play->envCtx.adjFogColor[i];
    }
    sSavedAdj[9] = play->envCtx.adjFogNear;
    Lights_PointNoGlowSetInfo(&sLightInfo, player->actor.world.pos.x, player->actor.world.pos.y,
                              player->actor.world.pos.z, 0, 0, 0, 0);
    sLightNode = LightContext_InsertLight(play, &play->lightCtx, &sLightInfo);
    sCamId = Play_CreateSubCamera(play);
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM form scene: from %d to %d, camera %d\n", (int)sFromForm, (int)to, (int)sCamId);
        fflush(stdout);
    }
    Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_WAIT);
    Play_ChangeCameraStatus(play, sCamId, CAM_STAT_ACTIVE);
    NvMmForm_SceneCamera(play, player);
}

/** z_nv_mm_mask.c's head hook: the Deku Mask on his face while he puts it on (MM's draw for a form's
 * mask: from cl_setmask's frame 12, squashed onto him, the fused mask from frame 51); true if drawn. */
s32 NvMmForm_DrawSceneMask(PlayState* play, Player* player) {
    s32 fused;

    if ((sChangeTo < 0) || (sFromForm != MM_FORM_HUMAN) || (player->actionFunc != NvMmForm_Scene)) {
        return false;
    }
    if (!(((player->skelAnime.animation == ANIM_SETMASK) && (player->skelAnime.curFrame >= 12.0f)) ||
          (player->skelAnime.animation == ANIM_SETMASKEND))) {
        return false;
    }
    fused = ((player->skelAnime.animation == ANIM_SETMASK) && (player->skelAnime.curFrame >= 51.0f)) ||
            (player->skelAnime.animation == ANIM_SETMASKEND);
    OPEN_DISPS(play->state.gfxCtx);
    Matrix_Push();
    Matrix_Scale(1.0f, 1.0f - sMaskSquashY, 1.0f - sMaskSquashZ, MTXMODE_APPLY);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    Matrix_Pop();
    gSPDisplayList(POLY_OPA_DISP++, fused ? (Gfx*)"__OTR__mm/objects/object_mask_nuts/object_mask_nuts_DL_001D90"
                                          : (Gfx*)"__OTR__mm/objects/gameplay_keep/gDekuMaskDL");
    CLOSE_DISPS(play->state.gfxCtx);
    return true;
}

/** NvMmMask_UseItem (z_nv_mm_mask.c): a form's mask pressed (MM's item id); true if handled. */
s32 NvMmForm_UseMask(PlayState* play, Player* player, s32 mmItem) {
    s32 form = (mmItem == MM_ITEM_MASK_DEKU) ? MM_FORM_DEKU : -1;

    if ((form < 0) || !NvMmItem_Has(mmItem) || (sChangeTo >= 0) || !LINK_IS_CHILD) {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR); // (the other forms, and adult Link: not yet)
        return true;
    }
    if (!(player->actor.bgCheckFlags & BGCHECKFLAG_GROUND) || (player->stateFlags1 & PLAYER_STATE1_IN_WATER)) {
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
        return true;
    }
    NvMmForm_StartScene(play, player, (sForm.form == form) ? MM_FORM_HUMAN : form);
    return true;
}

/** Which items the form may use (z_nv_mm_mask.c, before OoT's item actions): true if blocked. */
s32 NvMmForm_BlocksItem(s32 item) {
    if (sForm.form == MM_FORM_HUMAN) {
        return false;
    }
    // Deku Link: the ocarina (MM's pipes) and the masks; MM's other Deku items wait for MM's items
    if ((item == ITEM_OCARINA_FAIRY) || (item == ITEM_OCARINA_TIME) || (item == ITEM_NONE)) {
        return false;
    }
    return true;
}

/** Each frame (z_nv_mm.c, in every scene): a change under way, and Link kept in his form. */
void NvMmForm_Update(PlayState* play) {
    Player* player = GET_PLAYER(play);

    if (player == NULL) {
        return;
    }
    if (sChangeTo >= 0) {
        play->envCtx.fillScreen = (sFillAlpha > 0); // (MM's R_PLAY_FILL_SCREEN: white, MM's 220)
        play->envCtx.screenFillColor[0] = play->envCtx.screenFillColor[1] = play->envCtx.screenFillColor[2] = 220;
        play->envCtx.screenFillColor[3] = sFillAlpha;
        if ((player->actionFunc != NvMmForm_Scene) && (player->actionFunc != NvMmForm_SceneEnd)) {
            sChangeTo = -1; // (the scene cut short: a load, a hit)
            play->envCtx.fillScreen = false;
            sLightNode = NULL; // (a new scene's lights: ours went with the old)
            if (sCamId >= 0) {
                Play_ClearCamera(play, sCamId);
                Play_ChangeCameraStatus(play, CAM_ID_MAIN, CAM_STAT_ACTIVE);
                sCamId = -1;
            }
        }
    }
    if ((sForm.form != MM_FORM_HUMAN) && (player->skelAnime.skeleton != sAppliedLimbs)) {
        NvMmForm_Apply(play, player); // (a new Link: a scene's start)
    }
    if (sForm.form == MM_FORM_DEKU) {
        if (player->ageProperties != &sDekuProperties) {
            NvMmForm_Apply(play, player);
        }
        player->currentShield = PLAYER_SHIELD_NONE;
    }
    NvMmDeku_Update(play, player); // (Deku Link's moves: z_nv_mm_deku.c)
}

/** z_player_lib.c's limb hook: Deku Link's own hands, waist and sheath (MM's form tables). */
void NvMmForm_OverrideLimb(s32 limbIndex, Gfx** dList) {
    const char* dl = NULL;

    if ((sForm.form != MM_FORM_DEKU)) {
        return;
    }
    switch (limbIndex) {
        case PLAYER_LIMB_L_HAND:
            dl = DEKU("gLinkDekuLeftHandDL");
            break;
        case PLAYER_LIMB_R_HAND:
            dl = DEKU("gLinkDekuRightHandDL");
            break;
        case PLAYER_LIMB_SHEATH:
            dl = DEKU("gLinkDekuSheathDL");
            break;
        case PLAYER_LIMB_WAIST:
            dl = DEKU("gLinkDekuWaistDL");
            break;
        default:
            return;
    }
    *dList = ResourceMgr_LoadGfxByName(dl);
}
