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
 * MM's transformation scene (Player_Action_86) plays on every change, skippable with any button
 * once it has been watched to the end the first time (week event flags; changing back to human is
 * always skippable). Until MM's player animations are in (cl_setmask, pn_maskoffstart), the change
 * here is the scene's white flash alone (its 45-a-frame fill to white and back, its sounds).
 *
 * Not yet: Deku Link's own moves (the spin, the bubble, the hop across water, the flower launch, the
 * shell guard), MM's rule that he can't swim (deep water sends him back), the items he may use (only
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
} NvMmFormSave;

static NvMmFormSave sForm = { MM_FORM_HUMAN, ITEM_NONE, 0 };
static s32 sChangeTo = -1; // a change under way: to this form
static s32 sChangeFrame;
static s32 sFillAlpha;
static s32 sFillDir; // 1 to white, -1 back
static PlayerAgeProperties sDekuProperties;
static const void* sAppliedLimbs; // the skeleton Link was last given
static PlayerAgeProperties* sOotProperties;

extern FlexSkeletonHeader* gPlayerSkelHeaders[];
s32 NvMmItem_Has(s32 item);
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
    sChangeTo = (sForm.form == form) ? MM_FORM_HUMAN : form;
    sChangeFrame = 0;
    sFillAlpha = 0;
    sFillDir = 1;
    player->stateFlags1 |= PLAYER_STATE1_INPUT_DISABLED;
    player->actor.speedXZ = 0.0f;
    Audio_PlayActorSound2(&player->actor, NA_SE_PL_CHANGE_ARMS);
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
        sChangeFrame++;
        player->stateFlags1 |= PLAYER_STATE1_INPUT_DISABLED;
        player->actor.speedXZ = 0.0f;
        sFillAlpha += sFillDir * 45; // (MM's R_PLAY_FILL_SCREEN_ON 45)
        if ((sFillDir > 0) && (sFillAlpha >= 255)) {
            sFillAlpha = 255;
            sFillDir = -1;
            NvMmForm_Change(play, player, sChangeTo);
            Sfx_PlaySfxCentered(NA_SE_SY_TRE_BOX_APPEAR);
        } else if ((sFillDir < 0) && (sFillAlpha <= 0)) {
            sFillAlpha = 0;
            sChangeTo = -1;
            player->stateFlags1 &= ~PLAYER_STATE1_INPUT_DISABLED;
        }
        play->envCtx.fillScreen = (sFillAlpha > 0);
        play->envCtx.screenFillColor[0] = 220; // (MM's R_PLAY_FILL_SCREEN_R/G/B)
        play->envCtx.screenFillColor[1] = 220;
        play->envCtx.screenFillColor[2] = 220;
        play->envCtx.screenFillColor[3] = sFillAlpha;
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
