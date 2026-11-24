/*
 * Nightmareverse: Majora's Mask's forms on Link (mm/README.md "The forms"): Deku Link and Goron Link
 * so far (a table, sFormDefs: the Zora and the Fierce Deity slot in the same way).
 *
 * Wearing a form's mask (its C button, as MM's) turns Link into that form, and pressing it again
 * turns him back (another form's mask: straight into that one). He stays in his form through doors
 * and loads, and in the save (as MM: the form is kept), in Termina and in Hyrule alike (NV_GAMES.md:
 * the forms are carried over).
 *
 * Nothing of OoT's player code is rewritten for this. A form is OoT's young Link with:
 * - MM's skeleton for it (object_link_nuts, object_link_goron) under OoT's animations: MM's forms
 *   play the same animations as its human Link for walking, running, standing... (MM's D_8085BE84
 *   has no form column), and the skeletons have the same 21 joints in the same order;
 * - its hands, waist and sheath (z_player_lib.c's limb hook asks NvMmForm_OverrideLimb), its eyes
 *   (Goron Link's, in OoT's eye table while he's Goron), no sword, no shield (B is empty, as MM's:
 *   the sword is put back when he's human);
 * - its size (MM's sPlayerAgeProperties for the form over OoT's young Link's: OoT's voice and
 *   animations kept), its height for the camera, its speeds (MM's boot data);
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

#define MM_FORM_FD 0 // MM's PLAYER_FORM_*: Fierce Deity 0, Goron 1, Zora 2, Deku 3, human 4
#define MM_ITEM_MASK_FD 0x35
#define MM_FORM_GORON 1
#define MM_FORM_ZORA 2
#define MM_ITEM_MASK_ZORA 0x34
#define MM_FORM_DEKU 3
#define MM_FORM_HUMAN 4
#define MM_ITEM_MASK_DEKU 0x32
#define MM_ITEM_MASK_GORON 0x33

#define DEKU(name) ("__OTR__mm/objects/object_link_nuts/" name)
#define GORON(name) ("__OTR__mm/objects/object_link_goron/" name)
#define ZORA(name) ("__OTR__mm/objects/object_link_zora/" name)
#define BOY(name) ("__OTR__mm/objects/object_link_boy/" name)
#define KEEP(name) ("__OTR__mm/objects/gameplay_keep/" name)
#define MMANIM(name) ((LinkAnimationHeader*)("__OTR__mm/player_anim/" name))

/** A form, as MM has it: its model, its size (MM's sPlayerAgeProperties: the sizes; OoT's voice and
 * animations kept), its height (MM's Player_GetHeight), its speeds (MM's boot data, on OoT's movement
 * registers: OoT has no REG(39), MM's 9th left out), its mask on his face in the scene. */
typedef struct {
    s32 mmItem;          // its mask (MM's item id)
    const char* name;    // (the log)
    const char* skel;
    const char* handL[2]; // open, closed (OoT's choice: closed running)
    const char* handR[2];
    const char* waist;
    const char* sheath;
    const char* eyes[8]; // OoT's eight eye slots (NULL: the face has no eye texture)
    f32 sizes[17];       // ceilingCheckHeight ... unk_40
    Vec3s v44;
    Vec3s v4A[4];
    Vec3s v62[4];
    Vec3s v7A[4];        // (OoT's unk_7A[2] and unk_86[2]: MM's unk_7A[4])
    f32 height;
    s16 bootRegs[17];
    const char* maskDL;      // the mask going on (MM's D_801C0B20, the form's mask)
    const char* fusedMaskDL; // fused with his face (MM's D_801C0B20, + 4)
    const char* maskOffAnim; // pulling it off (MM's D_8085D160)
    const char* mouths[4];   // OoT's four mouth slots (NULL: the face has none)
} NvMmFormDef;

static const NvMmFormDef sFormDefs[5] = {
    // the Fierce Deity, at adult Link's size: OoT's adult Link's sizes (sAgeProperties[0]) and speeds
    // (OoT's Kokiri boots, adult), height 68; in boss rooms MM's 1.5 (NvMmForm_FdSize: the decision, 2026-11-24)
    [MM_FORM_FD] = {
        MM_ITEM_MASK_FD, "Fierce Deity", BOY("gLinkFierceDeitySkel"),
        { BOY("gLinkFierceDeityLeftHandDL"), BOY("gLinkFierceDeityLeftHandDL") },
        { BOY("gLinkFierceDeityRightHandDL"), BOY("gLinkFierceDeityRightHandDL") },
        BOY("gLinkFierceDeityWaistDL"), NULL,
        { NULL },
        { 56.0f, 90.0f, 1.0f, 111.0f, 70.0f, 79.4f, 59.0f, 41.0f, 19.0f, 36.0f, 44.8f, 56.0f, 68.0f, 70.0f, 18.0f,
          15.0f, 70.0f },
        { 9, 4671, 359 },
        { { 8, 4694, 380 }, { 9, 6122, 359 }, { 8, 4694, 380 }, { 9, 6122, 359 } },
        { { 9, 6122, 359 }, { 9, 7693, 380 }, { 9, 6122, 359 }, { 9, 7693, 380 } },
        { { 8, 4694, 380 }, { 9, 6122, 359 }, { -1592, 4694, 380 }, { -1591, 6122, 359 } },
        68.0f,
        { 200, 1000, 300, 700, 550, 270, 600, 350, 800, 600, -100, 600, 590, 750, 125, 200, 130 },
        KEEP("gFierceDeityMaskDL"), "__OTR__mm/objects/object_mask_boy/object_mask_boy_DL_000900",
        "__OTR__mm/player_anim/gPlayerAnim_pz_maskoffstart",
        { NULL },
    },
    [MM_FORM_ZORA] = {
        MM_ITEM_MASK_ZORA, "Zora", ZORA("gLinkZoraSkel"),
        { ZORA("gLinkZoraLeftHandOpenDL"), ZORA("gLinkZoraLeftHandClosedDL") },
        { ZORA("gLinkZoraRightHandOpenDL"), ZORA("gLinkZoraRightHandClosedDL") },
        ZORA("gLinkZoraWaistDL"), ZORA("gLinkZoraSheathDL"),
        // (OoT's open, half, closed, roll left, roll right, shock, two unnamed: MM's, as young Link's)
        { ZORA("gLinkZoraEyesOpenTex"), ZORA("gLinkZoraEyesHalfTex"), ZORA("gLinkZoraEyesClosedTex"),
          ZORA("gLinkZoraEyesLeftTex"), ZORA("gLinkZoraEyesRightTex"), ZORA("gLinkZoraEyesWincingTex"),
          ZORA("gLinkZoraEyesUpTex"), ZORA("gLinkZoraEyesDownTex") },
        { 56.0f, 90.0f, 1.0f, 111.0f, 70.0f, 79.4f, 59.0f, 41.0f, 19.0f, 36.0f, 50.0f, 56.0f, 68.0f, 70.0f, 18.0f,
          23.0f, 70.0f },
        { 0x17, 0x1323, -0x6D },
        { { 0x17, 0x1323, -0x58 }, { 0x17, 0x18B7, -0x6D }, { 0x17, 0x1323, -0x58 }, { 0x17, 0x18B7, -0x6D } },
        { { 0x17, 0x18B7, -0x6D }, { 0x18, 0x1EDA, -0x58 }, { 0x17, 0x18B7, -0x6D }, { 0x18, 0x1EDA, -0x58 } },
        { { 8, 0x1256, 0x17C }, { 9, 0x17EA, 0x167 }, { -0x638, 0x1256, 0x17C }, { -0x637, 0x17EA, 0x167 } },
        68.0f,
        { 200, 1000, 300, 700, 550, 270, 700, 300, 800, 600, -100, 600, 590, 750, 125, 200, 130 },
        KEEP("gZoraMaskDL"), "__OTR__mm/objects/object_mask_zora/object_mask_zora_DL_000DB0",
        "__OTR__mm/player_anim/gPlayerAnim_pz_maskoffstart",
        { ZORA("gLinkZoraMouthClosedTex"), ZORA("gLinkZoraMouthHalfTex"), ZORA("gLinkZoraMouthOpenTex"),
          ZORA("gLinkZoraMouthSmileTex") },
    },
    [MM_FORM_GORON] = {
        MM_ITEM_MASK_GORON, "Goron", GORON("gLinkGoronSkel"),
        { GORON("gLinkGoronLeftHandOpenDL"), GORON("gLinkGoronLeftHandClosedDL") },
        { GORON("gLinkGoronRightHandOpenDL"), GORON("gLinkGoronRightHandClosedDL") },
        GORON("gLinkGoronWaistDL"), GORON("gLinkGoronSheathDL"),
        // (MM's: open, half, closed; its surprised for OoT's "shock"; the rest open)
        { GORON("gLinkGoronEyesOpenTex"), GORON("gLinkGoronEyesHalfTex"), GORON("gLinkGoronEyesClosedTex"),
          GORON("gLinkGoronEyesOpenTex"), GORON("gLinkGoronEyesOpenTex"), GORON("gLinkGoronEyesSurprisedTex"),
          GORON("gLinkGoronEyesOpenTex"), GORON("gLinkGoronEyesOpenTex") },
        { 70.0f, 90.0f, 0.74f, 111.0f, 70.0f, 79.4f, 59.0f, 41.0f, 19.0f, 36.0f, 50.0f, 56.0f, 68.0f, 70.0f, 19.5f,
          18.2f, 80.0f },
        { 0x17, 0xF3B, 0xDF },
        { { 0x18, 0xF3B, 0xDF }, { 0x17, 0x14CF, 0xDF }, { 0x18, 0xF3B, 0xDF }, { 0x17, 0x14CF, 0xDF } },
        { { 0x17, 0x14CF, 0xDF }, { 0x18, 0x1AF2, 0xDF }, { 0x17, 0x14CF, 0xDF }, { 0x18, 0x1AF2, 0xDF } },
        { { 8, 0x1256, 0x17C }, { 9, 0x17EA, 0x167 }, { -0x638, 0x1256, 0x17C }, { -0x637, 0x17EA, 0x167 } },
        80.0f,
        { 200, 1000, 300, 700, 550, 270, 700, 200, 800, 600, -140, 600, 590, 750, 125, 200, 130 },
        KEEP("gGoronMaskDL"), "__OTR__mm/objects/object_mask_goron/object_mask_goron_DL_0014A0",
        "__OTR__mm/player_anim/gPlayerAnim_pg_maskoffstart",
    },
    [MM_FORM_DEKU] = {
        MM_ITEM_MASK_DEKU, "Deku", DEKU("gLinkDekuSkel"),
        { DEKU("gLinkDekuLeftHandDL"), DEKU("gLinkDekuLeftHandDL") },
        { DEKU("gLinkDekuRightHandDL"), DEKU("gLinkDekuRightHandDL") },
        DEKU("gLinkDekuWaistDL"), DEKU("gLinkDekuSheathDL"),
        { NULL },
        { 35.0f, 50.0f, 0.3f, 71.0f, 50.0f, 49.0f, 39.0f, 27.0f, 19.0f, 8.0f, 13.6f, 24.0f, 24.0f, 70.0f, 14.0f, 12.0f,
          55.0f },
        { -0x18, 0xDED, 0x36C },
        { { -0x18, 0xD92, 0x35E }, { -0x18, 0x1371, 0x3A9 }, { 8, 0x1256, 0x17C }, { 9, 0x17EA, 0x167 } },
        { { -0x18, 0x1371, 0x3A9 }, { -0x18, 0x195F, 0x3A9 }, { 9, 0x17EA, 0x167 }, { 9, 0x1E0D, 0x17C } },
        { { 8, 0x1256, 0x17C }, { 9, 0x17EA, 0x167 }, { -0x638, 0x1256, 0x17C }, { -0x637, 0x17EA, 0x167 } },
        36.0f,
        { 200, 1000, 300, 700, 550, 270, 600, 1000, 800, 600, -100, 600, 590, 750, 125, 200, 130 },
        KEEP("gDekuMaskDL"), "__OTR__mm/objects/object_mask_nuts/object_mask_nuts_DL_001D90",
        "__OTR__mm/player_anim/gPlayerAnim_pn_maskoffstart",
    },
};

/* the Fierce Deity's size: adult Link's (1.0) but in boss rooms, MM's 1.5 there, grown and shrunk smoothly */
static f32 sFdSize = 1.0f; // 1.0 to 1.5
static const s16 sFdBigBootRegs[17] = { 200, 666, 200, 700, 366, 200, 600, 175, 800, 1000, -100, 600, 590, 800, 125, 300, 65 };

/** OoT's boss rooms (and MM's, when they're in: Majora's). */
static s32 NvMmForm_IsBossRoom(PlayState* play) {
    return ((play->sceneNum >= SCENE_DEKU_TREE_BOSS) && (play->sceneNum <= SCENE_GANONDORF_BOSS)) ||
           (play->sceneNum == SCENE_GANON_BOSS);
}

static const NvMmFormDef* NvMmForm_Def(s32 form) {
    return ((form >= 0) && (form < MM_FORM_HUMAN) && (sFormDefs[form].skel != NULL)) ? &sFormDefs[form] : NULL;
}

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
static PlayerAgeProperties sFormProperties; // the current form's (OoT's young Link's with MM's sizes)
static s32 sFormPropertiesFor = -1;
static const void* sAppliedLimbs; // the skeleton Link was last given
static PlayerAgeProperties* sOotProperties;

extern FlexSkeletonHeader* gPlayerSkelHeaders[];
s32 NvMmItem_Has(s32 item);
void NvMmDeku_Update(PlayState* play, Player* player);
void NvMmMask_TakeOff(void);

s32 NvMmGoron_Curled(Player* player);

/** MM's GET_PLAYER_FORM: Link's form (MM's PLAYER_FORM_*). */
s32 NvMmForm_Get(void) {
    return sForm.form;
}

/** z_actor.c's Player_GetHeight (the camera's and others' Link height): MM's for the form, 0 as human. */
f32 NvMmForm_Height(void) {
    const NvMmFormDef* def = NvMmForm_Def(sForm.form);

    if ((def != NULL) && (gPlayState != NULL) && NvMmGoron_Curled(GET_PLAYER(gPlayState))) {
        return 34.0f; // (MM: Goron Link curled, PLAYER_STATE3_1000)
    }
    if (sForm.form == MM_FORM_FD) { // (68 at adult size, MM's 124 at 1.5)
        return 68.0f + (124.0f - 68.0f) * ((sFdSize - 1.0f) / 0.5f);
    }
    return (def != NULL) ? def->height : 0.0f;
}

/** The form's speeds on OoT's movement registers (z_nv_mm_deku.c's Update, each frame): false as human. */
s32 NvMmForm_SetBootRegs(PlayState* play) {
    const NvMmFormDef* def = NvMmForm_Def(sForm.form);
    const s16* r;

    if (def == NULL) {
        return false;
    }
    r = ((sForm.form == MM_FORM_FD) && (sFdSize > 1.25f)) ? sFdBigBootRegs : def->bootRegs; // (MM's at 1.5)
    REG(27) = 2000;
    REG(48) = 370;
    REG(19) = r[0];
    REG(30) = r[1];
    REG(32) = r[2];
    REG(34) = r[3];
    REG(35) = r[4];
    REG(36) = r[5];
    REG(37) = r[6];
    REG(38) = r[7];
    REG(43) = r[8]; // (R_DECELERATE_RATE)
    REG(45) = r[9]; // (R_RUN_SPEED_LIMIT)
    REG(68) = r[10];
    REG(69) = r[11];
    IREG(66) = r[12];
    IREG(67) = r[13];
    IREG(68) = r[14];
    IREG(69) = r[15];
    MREG(95) = r[16];
    if (play->roomCtx.curRoom.behaviorType1 == ROOM_BEHAVIOR_TYPE1_2) { // (indoors, as OoT and MM)
        REG(45) = 500;
    }
    return true;
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

/** MM's sPlayerAgeProperties for the form over OoT's young Link's: the sizes, not the animations. */
static void NvMmForm_MakeProperties(PlayerAgeProperties* oot, const NvMmFormDef* def) {
    s32 i;

    sFormProperties = *oot; // (OoT's voice, sounds and animations)
    memcpy(&sFormProperties, def->sizes, sizeof(def->sizes)); // ceilingCheckHeight ... unk_40
    if ((def == &sFormDefs[MM_FORM_FD]) && (sFdSize != 1.0f)) { // (MM's Fierce Deity: adult Link's, times 1.5)
        f32* f = (f32*)&sFormProperties;

        for (i = 0; i < 17; i++) {
            if (i != 2) { // (unk_08, the root's scale: the actor's scale does it)
                f[i] = def->sizes[i] * sFdSize;
            }
        }
    }
    sFormProperties.unk_44 = def->v44;
    memcpy(sFormProperties.unk_4A, def->v4A, sizeof(def->v4A));
    memcpy(sFormProperties.unk_62, def->v62, sizeof(def->v62));
    memcpy(sFormProperties.unk_7A, def->v7A, sizeof(def->v7A));
}

/* the form's eyes in OoT's eye table (young Link's row; put back as human) */
extern void* sEyeTextures[2][8];
extern void* sMouthTextures[2][4];
static void* sEyesKept[8];
static void* sMouthsKept[4];

/** Whether a texture in OoT's eye or mouth tables is one of the forms' (put there by us). */
static s32 NvMmForm_IsFormFace(const void* tex) {
    s32 f;
    s32 i;

    for (f = 0; f < MM_FORM_HUMAN; f++) {
        for (i = 0; i < 8; i++) {
            if ((tex != NULL) && ((tex == (const void*)sFormDefs[f].eyes[i]) ||
                                  ((i < 4) && (tex == (const void*)sFormDefs[f].mouths[i])))) {
                return true;
            }
        }
    }
    return false;
}

/** The form's face in OoT's eye and mouth tables (young Link's row): the table's own entries kept
 * (only ever ones that aren't a form's: a scene's start may rewrite them, MM's human or OoT's) and put
 * back as human, or for a form without that part. */
static void NvMmForm_SetFaceRow(void** row, void** kept, s32 n, const char* const* form) {
    s32 i;

    for (i = 0; i < n; i++) {
        if (!NvMmForm_IsFormFace(row[i])) {
            kept[i] = row[i];
        }
        row[i] = ((form != NULL) && (form[0] != NULL)) ? (void*)form[i] : kept[i];
    }
}

static void NvMmForm_SetEyes(const NvMmFormDef* def) {
    NvMmForm_SetFaceRow(sEyeTextures[1], sEyesKept, 8, (def != NULL) ? def->eyes : NULL);
    NvMmForm_SetFaceRow(sMouthTextures[1], sMouthsKept, 4, (def != NULL) ? def->mouths : NULL);
}

/** Link's skeleton and size for his form (now, and on each new Link: a scene's start). */
static void NvMmForm_Apply(PlayState* play, Player* player) {
    FlexSkeletonHeader* header;
    const NvMmFormDef* def = NvMmForm_Def(sForm.form);
    const char* path = (def != NULL) ? def->skel : (const char*)gPlayerSkelHeaders[1];

    header = ResourceMgr_LoadSkeletonByName(path, &player->skelAnime);
    if (header == NULL) {
        return;
    }
    player->skelAnime.skeleton = SEGMENTED_TO_VIRTUAL(header->sh.segment);
    player->skelAnime.dListCount = header->dListCount;
    player->upperSkelAnime.skeleton = player->skelAnime.skeleton;
    player->upperSkelAnime.dListCount = header->dListCount;
    sAppliedLimbs = player->skelAnime.skeleton;

    if (def != NULL) {
        if ((player->ageProperties != &sFormProperties) || (sFormPropertiesFor != sForm.form)) {
            if (player->ageProperties != &sFormProperties) {
                sOotProperties = player->ageProperties;
            }
            NvMmForm_MakeProperties(sOotProperties, def);
            sFormPropertiesFor = sForm.form;
            player->ageProperties = &sFormProperties;
        }
    } else if ((player->ageProperties == &sFormProperties) && (sOotProperties != NULL)) {
        player->ageProperties = sOotProperties;
    }
    NvMmForm_SetEyes(def);
}

static s32 NvMmForm_FdShield(void);

static void NvMmForm_Change(PlayState* play, Player* player, s32 form) {
    // the sword on B: human Link's and the Fierce Deity's (he swings it: his own sword's ways later);
    // the other forms have none (B left empty, OoT's swordless B), the sword kept for when it's back
    s32 hadSword = (sForm.form == MM_FORM_HUMAN) || (sForm.form == MM_FORM_FD);
    s32 hasSword = (form == MM_FORM_HUMAN) || (form == MM_FORM_FD);

    if ((sForm.form == MM_FORM_FD) && (form != MM_FORM_FD) && LINK_IS_CHILD &&
        (CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) >= EQUIP_VALUE_SWORD_MASTER)) {
        // the Fierce Deity's adult sword put back: young Link's own again (the Kokiri Sword, if he has it)
        s32 kokiri = CHECK_OWNED_EQUIP(EQUIP_TYPE_SWORD, EQUIP_INV_SWORD_KOKIRI);

        Inventory_ChangeEquipment(EQUIP_TYPE_SWORD, kokiri ? EQUIP_VALUE_SWORD_KOKIRI : EQUIP_VALUE_SWORD_NONE);
        gSaveContext.equips.buttonItems[0] = kokiri ? ITEM_SWORD_KOKIRI : ITEM_NONE;
        if (!kokiri) {
            gSaveContext.infTable[29] = 1;
        }
    }
    if ((sForm.form == MM_FORM_FD) && (form != MM_FORM_FD) && LINK_IS_CHILD &&
        (CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD) == EQUIP_VALUE_SHIELD_MIRROR)) { // (OoT's Mirror put back)
        Inventory_ChangeEquipment(EQUIP_TYPE_SHIELD, CHECK_OWNED_EQUIP(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_HYLIAN) ? EQUIP_VALUE_SHIELD_HYLIAN
                                                     : CHECK_OWNED_EQUIP(EQUIP_TYPE_SHIELD, EQUIP_INV_SHIELD_DEKU) ? EQUIP_VALUE_SHIELD_DEKU
                                                                                                                  : EQUIP_VALUE_SHIELD_NONE);
    }
    if (hadSword && !hasSword) {
        sForm.savedB = gSaveContext.equips.buttonItems[0];
        sForm.savedSwordless = gSaveContext.infTable[29];
        gSaveContext.equips.buttonItems[0] = ITEM_NONE;
        gSaveContext.infTable[29] = 1;
    } else if (!hadSword && hasSword) {
        gSaveContext.equips.buttonItems[0] = sForm.savedB;
        gSaveContext.infTable[29] = sForm.savedSwordless;
        sForm.savedB = ITEM_NONE;
    }
    player->currentShield = (form == MM_FORM_HUMAN) ? CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD)
                            : (form == MM_FORM_FD)  ? NvMmForm_FdShield() // (adult Link's: his)
                                                    : PLAYER_SHIELD_NONE;
    Interface_LoadItemIcon1(play, 0);
    NvMmMask_TakeOff(); // (MM: a change takes the mask worn off)
    {
        void NvGear_FormChanged(PlayState * play, s32 form);

        NvGear_FormChanged(play, form); // (the Great Fairy's Sword: human Link's)
    }
    player->currentMask = PLAYER_MASK_NONE;
    sForm.form = form;
    NvMmForm_Apply(play, player);
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM form: %s\n", (NvMmForm_Def(form) != NULL) ? NvMmForm_Def(form)->name : "human");
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
    LinkAnimationHeader* anim =
        (sFromForm == MM_FORM_HUMAN) ? ANIM_SETMASK : (LinkAnimationHeader*)NvMmForm_Def(sFromForm)->maskOffAnim;

    LinkAnimation_Change(play, &player->skelAnime, anim, 1.0f, 0.0f, Animation_GetLastFrame(anim),
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
    gSPDisplayList(POLY_OPA_DISP++,
                   (Gfx*)(fused ? NvMmForm_Def(sChangeTo)->fusedMaskDL : NvMmForm_Def(sChangeTo)->maskDL));
    CLOSE_DISPS(play->state.gfxCtx);
    return true;
}

/** NvMmMask_UseItem (z_nv_mm_mask.c): a form's mask pressed (MM's item id); true if handled. */
s32 NvMmForm_UseMask(PlayState* play, Player* player, s32 mmItem) {
    s32 form = -1;
    s32 i;

    for (i = 0; i < MM_FORM_HUMAN; i++) { // (the form whose mask it is)
        if ((NvMmForm_Def(i) != NULL) && (NvMmForm_Def(i)->mmItem == mmItem)) {
            form = i;
        }
    }

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
    // the forms: the ocarina (MM's pipes and drums) and the masks; their other items wait for MM's items
    if ((item == ITEM_OCARINA_FAIRY) || (item == ITEM_OCARINA_TIME) || (item == ITEM_NONE)) {
        return false;
    }
    if ((sForm.form == MM_FORM_FD) && (item >= ITEM_SWORD_KOKIRI) && (item <= ITEM_SWORD_BGS)) {
        return false; // (the Fierce Deity's B: his sword, swung as Link's)
    }
    return true;
}

/**
 * The Fierce Deity: fireproof and breathing underwater, both tunics at once (the user, 2026-11-24: his
 * tunic can't change colour, so he has both). Read where OoT reads SoH's Super Tunic cheat (the hot
 * rooms' timer, the drowning timer, burning floors, the "it's hot" text).
 */
s32 NvMmForm_SuperTunic(void) {
    return sForm.form == MM_FORM_FD;
}

/** The Fierce Deity (young Link underneath): OoT's child-with-the-Hylian rules (the shield kept on
 * his back, held over his head) don't apply to him (z_player_lib.c's hook). */
s32 NvMmForm_IsFierceDeity(void) {
    return sForm.form == MM_FORM_FD;
}

/**
 * The Equipment screen's age check (z_kaleido_scope.h's hook): the Fierce Deity takes adult Link's swords,
 * the Master Sword and Biggoron's (NV_GAMES.md, the user 2026-11-20), as well as his own in the Kokiri
 * Sword's place. Nothing else of adult Link's: his tunic, boots and shield stay as the forms have them.
 */
s32 NvMmForm_EquipAgeOk(s32 row, s32 col) {
    return (sForm.form == MM_FORM_FD) && (((row == 0) && (col >= 1)) || ((row == 1) && (col == 3))); // (+ OoT's Mirror)
}

/**
 * The Fierce Deity's sword beam (MM's func_808332A0, from Player_ActionHandler_7's swing): a swing begun while
 * targeting, with magic, sends one (z_nv_mm_beam.c), pitched at what he's locked on to; 1 magic (MM's).
 */
static void NvMmForm_FdBeam(PlayState* play, Player* player) {
    static s8 sPrevWeaponState = 0;
    s32 NvMmActor_Id(s32 mmId);
    s32 begun = (sPrevWeaponState == 0) && (player->meleeWeaponState != 0);

    sPrevWeaponState = player->meleeWeaponState;
    if (!begun || (player->meleeWeaponAnimation >= PLAYER_MWA_SPIN_ATTACK_1H)) {
        return;
    }
    if ((player->focusActor == NULL) && !(player->stateFlags1 & PLAYER_STATE1_PARALLEL)) {
        return;
    }
    if (gSaveContext.magic <= 0) {
        return;
    }
    {
        Vec3f* pos = &player->bodyPartsPos[PLAYER_BODYPART_WAIST];
        s16 pitch = (player->focusActor != NULL) ? Math_Vec3f_Pitch(pos, &player->focusActor->focus.pos) : 0;

        if (Actor_Spawn(&play->actorCtx, play, NvMmActor_Id(0x035), pos->x, pos->y, pos->z, pitch, 0, 0, 0) != NULL) {
            gSaveContext.magic = MAX(gSaveContext.magic - 1, 0); // (OoT's meter drains 2 a step: an odd cost would empty it)
            if (getenv("NV_LOG") != NULL) {
                printf("NVMM fierce deity: sword beam (pitch %d, magic %d)\n", pitch, gSaveContext.magic);
                fflush(stdout);
            }
        }
    }
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
    if (sForm.form != MM_FORM_HUMAN) {
        if ((player->ageProperties != &sFormProperties) || (sFormPropertiesFor != sForm.form)) {
            NvMmForm_Apply(play, player);
        }
        player->currentShield = (sForm.form == MM_FORM_FD) ? NvMmForm_FdShield() : PLAYER_SHIELD_NONE;
    }
    if ((sForm.form == MM_FORM_FD) && (player->actor.category == ACTORCAT_PLAYER)) {
        // the Fierce Deity's size: MM's 1.5 in a boss room, adult Link's elsewhere, a step at a time
        f32 target = NvMmForm_IsBossRoom(play) ? 1.5f : 1.0f;

        if (sFdSize != target) {
            Math_StepToF(&sFdSize, target, 0.02f);
            NvMmForm_MakeProperties(sOotProperties, &sFormDefs[MM_FORM_FD]);
            if (getenv("NV_LOG") != NULL) {
                printf("NVMM fierce deity: size %.2f\n", sFdSize);
                fflush(stdout);
            }
        }
        Actor_SetScale(&player->actor, 0.01f * sFdSize);
        NvMmForm_FdBeam(play, player);
    } else if ((sFdSize != 1.0f) || (player->actor.scale.y != 0.01f)) {
        if ((player->actor.category == ACTORCAT_PLAYER) && (player->actor.scale.y > 0.0f)) {
            sFdSize = 1.0f;
            Actor_SetScale(&player->actor, 0.01f); // (not the Fierce Deity: his own size again)
        }
    }
    NvMmForm_SetEyes(NvMmForm_Def(sForm.form)); // (each frame: a scene's start may rewrite the tables)
    NvMmDeku_Update(play, player); // (Deku Link's moves: z_nv_mm_deku.c)
    {
        void NvMmZora_Update(PlayState * play, Player * this);

        NvMmZora_Update(play, player); // (Zora Link's: z_nv_mm_zora.c)
    }
}

/** libultraship's CRC64 of a resource path (as a display list's texture hashes), for C. */
static u64 NvMmForm_PathHash(const char* path) {
    static u64 sTable[256];
    u64 crc = ~0ULL;

    if (sTable[1] == 0) {
        s32 i, k;

        for (i = 0; i < 256; i++) {
            u64 c = (u64)i << 56;

            for (k = 0; k < 8; k++) {
                c = (c & (1ULL << 63)) ? ((c << 1) ^ 0x42F0E1EBA9EA3693ULL) : (c << 1);
            }
            sTable[i] = c;
        }
    }
    while (*path != '\0') {
        crc = sTable[(u8)(crc >> 56) ^ (u8)*path++] ^ (crc << 8);
    }
    return crc;
}

/**
 * The Fierce Deity holding one of OoT's adult swords (NV_GAMES.md: he equips them from the menu as adult
 * Link does): his own fist (MM's gLinkFierceDeityLeftHandDL, as MM's sword hand is his fist and then the
 * Double Helix) and then the sword half of OoT's adult hand-and-sword list. OoT's lists draw the sword
 * first and the hand from the gauntlet's texture on (the same in the Master Sword's, Biggoron's and the
 * broken knife's), so the copy stops at the sync before that texture. Built once per list, kept.
 */
static Gfx* NvMmForm_FdHoldingOotSword(const char* ootHandDL) {
    static struct {
        const char* name;
        Gfx* gfx;
    } sBuilt[4];
    static u64 sGauntletHash;
    Gfx* src;
    Gfx* out;
    Gfx* g;
    s32 i, n, cut = -1, lastSync = -1;

    for (i = 0; i < ARRAY_COUNT(sBuilt); i++) {
        if (sBuilt[i].name == ootHandDL) {
            return sBuilt[i].gfx;
        }
    }
    if (sGauntletHash == 0) {
        sGauntletHash = NvMmForm_PathHash("objects/object_link_boy/gLinkAdultDefaultGauntlet1Tex");
    }
    src = ResourceMgr_LoadGfxByName(ootHandDL);
    if (src == NULL) {
        return NULL;
    }
    for (i = 0; i < 1024; i++) {
        u8 op = (u8)(src[i].words.w0 >> 24);

        if (op == (u8)G_ENDDL) {
            break;
        }
        if (op == (u8)G_RDPPIPESYNC) {
            lastSync = i;
        }
        if (op == (u8)G_SETTIMG_OTR_HASH) {
            u64 hash = ((u64)src[i + 1].words.w0 << 32) | (u64)(u32)src[i + 1].words.w1;

            if (hash == sGauntletHash) {
                cut = lastSync;
                break;
            }
        }
        if ((op == (u8)G_SETTIMG_OTR_HASH) || (op == (u8)G_DL_OTR_HASH) || (op == (u8)G_VTX_OTR_HASH) ||
            (op == (u8)G_MARKER) || (op == (u8)G_MTX_OTR) || (op == (u8)G_MOVEMEM_OTR) ||
            (op == (u8)G_BRANCH_Z_OTR)) {
            i++; // (a two-word command)
        }
    }
    if (cut <= 0) {
        return NULL;
    }
    n = cut + 2;
    out = malloc(n * sizeof(Gfx));
    g = out;
    gSPDisplayList(g++, ResourceMgr_LoadGfxByName(BOY("gLinkFierceDeityLeftHandDL")));
    memcpy(g, src, cut * sizeof(Gfx));
    g += cut;
    gSPEndDisplayList(g++);
    for (i = 0; i < ARRAY_COUNT(sBuilt); i++) {
        if (sBuilt[i].name == NULL) {
            sBuilt[i].name = ootHandDL;
            sBuilt[i].gfx = out;
            break;
        }
    }
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM fierce deity: %s's sword, %d commands\n", ootHandDL, cut);
        fflush(stdout);
    }
    return out;
}

/**
 * The Fierce Deity with one of OoT's adult shields (NV_GAMES.md: he takes adult Link's shields): his
 * own part first (`prefixDL`: his right fist, or nothing on his back), then OoT's list with the parts
 * that aren't the shield left out. OoT's shield-in-hand lists interleave the shield and the hand (the
 * Hylian's back is drawn after the hand) and its shield-on-back lists draw the sheath too, so the
 * parts go by texture: the triangles drawn with adult Link's hand or sheath textures are dropped,
 * everything else (the shield's faces, its handle, the render state) is kept. Built once, kept.
 */
static Gfx* NvMmForm_FdOotShield(const char* ootDL, const char* prefixDL) {
    static const char* const sDrop[] = {
        "objects/object_link_boy/gLinkAdultDefaultGauntlet1Tex", "objects/object_link_boy/gLinkAdultDefaultGauntlet2Tex",
        "objects/object_link_boy/gLinkAdultClosedHandThumbTex",  "objects/object_link_boy/gLinkAdultClosedHandSideTex",
        "objects/object_link_boy/gLinkAdultHandTex",             "objects/object_link_boy/gLinkAdultSheathTex",
        "objects/object_link_boy/gLinkAdultSheathBandTex",
    };
    static const char* const sKeep[] = {
        "objects/gameplay_keep/gHylianShieldDesignTex",        "objects/object_link_boy/gLinkAdultShieldHandleTex",
        "objects/object_link_boy/gLinkAdultHylianShieldBackTex",
        "objects/object_link_boy/gLinkAdultMirrorShieldLowerDesignTex",
        "objects/object_link_boy/gLinkAdultMirrorShieldUpperDesignTex", "objects/gameplay_keep/gHilite1Tex",
        "objects/gameplay_keep/gHilite2Tex",
    };
    static struct {
        const char* name;
        const char* prefix;
        Gfx* gfx;
    } sBuilt[6];
    static u64 sDropHash[ARRAY_COUNT(sDrop)], sKeepHash[ARRAY_COUNT(sKeep)];
    Gfx* src;
    Gfx* out;
    Gfx* g;
    s32 i, k, n, dropping = false;

    for (i = 0; i < ARRAY_COUNT(sBuilt); i++) {
        if ((sBuilt[i].name == ootDL) && (sBuilt[i].prefix == prefixDL)) {
            return sBuilt[i].gfx;
        }
    }
    if (sDropHash[0] == 0) {
        for (k = 0; k < ARRAY_COUNT(sDrop); k++) {
            sDropHash[k] = NvMmForm_PathHash(sDrop[k]);
        }
        for (k = 0; k < ARRAY_COUNT(sKeep); k++) {
            sKeepHash[k] = NvMmForm_PathHash(sKeep[k]);
        }
    }
    src = ResourceMgr_LoadGfxByName(ootDL);
    if (src == NULL) {
        return NULL;
    }
    for (n = 0; (n < 2048) && ((u8)(src[n].words.w0 >> 24) != (u8)G_ENDDL); n++) {
    }
    out = malloc((n + 3) * sizeof(Gfx));
    g = out;
    if (prefixDL != NULL) {
        gSPDisplayList(g++, ResourceMgr_LoadGfxByName(prefixDL));
    }
    for (i = 0; i < n; i++) {
        u8 op = (u8)(src[i].words.w0 >> 24);
        s32 two = (op == (u8)G_SETTIMG_OTR_HASH) || (op == (u8)G_DL_OTR_HASH) || (op == (u8)G_VTX_OTR_HASH) ||
                  (op == (u8)G_MARKER) || (op == (u8)G_MTX_OTR) || (op == (u8)G_MOVEMEM_OTR) ||
                  (op == (u8)G_BRANCH_Z_OTR);

        if (op == (u8)G_SETTIMG_OTR_HASH) { // (a texture: which part follows; a palette leaves it as it was)
            u64 hash = ((u64)src[i + 1].words.w0 << 32) | (u64)(u32)src[i + 1].words.w1;

            for (k = 0; k < ARRAY_COUNT(sDrop); k++) {
                if (hash == sDropHash[k]) {
                    dropping = true;
                }
            }
            for (k = 0; k < ARRAY_COUNT(sKeep); k++) {
                if (hash == sKeepHash[k]) {
                    dropping = false;
                }
            }
        }
        if (dropping && ((op == (u8)G_TRI1) || (op == (u8)G_TRI2) || (op == (u8)G_QUAD))) {
            continue;
        }
        *g++ = src[i];
        if (two) {
            *g++ = src[++i];
        }
    }
    gSPEndDisplayList(g++);
    for (i = 0; i < ARRAY_COUNT(sBuilt); i++) {
        if (sBuilt[i].name == NULL) {
            sBuilt[i].name = ootDL;
            sBuilt[i].prefix = prefixDL;
            sBuilt[i].gfx = out;
            break;
        }
    }
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM fierce deity: %s's shield, %d commands\n", ootDL, (int)(g - out));
        fflush(stdout);
    }
    return out;
}

/** The Fierce Deity's shield: OoT's adult Hylian or Mirror, worn (his others: none, as MM). */
static s32 NvMmForm_FdShield(void) {
    s32 shield = CUR_EQUIP_VALUE(EQUIP_TYPE_SHIELD);

    return ((shield == EQUIP_VALUE_SHIELD_HYLIAN) || (shield == EQUIP_VALUE_SHIELD_MIRROR)) ? shield : 0;
}

#define OOT_BOY(name) ("__OTR__objects/object_link_boy/" name)

/** Two lists drawn one after the other (OoT's hand and MM's sword or shield in it), built once a pair. */
static Gfx* NvMmForm_Pair(const char* first, const char* second) {
    static struct {
        const char* a;
        const char* b;
        Gfx gfx[3];
    } sPairs[12];
    s32 i;

    for (i = 0; i < ARRAY_COUNT(sPairs); i++) {
        if ((sPairs[i].a == first) && (sPairs[i].b == second)) {
            return sPairs[i].gfx;
        }
        if (sPairs[i].a == NULL) {
            Gfx* g = sPairs[i].gfx;

            sPairs[i].a = first;
            sPairs[i].b = second;
            gSPDisplayList(g++, ResourceMgr_LoadGfxByName(first));
            gSPDisplayList(g++, ResourceMgr_LoadGfxByName(second));
            gSPEndDisplayList(g++);
            return sPairs[i].gfx;
        }
    }
    return NULL;
}

/**
 * MM's shield on young Link's back, on OoT's model (Hyrule; in Termina he's MM's own model and MM's own
 * lists are drawn as they are): OoT's sheath, then OoT's own placing of a shield on his back (the
 * matrix of gLinkChildDekuShieldWithMatrixDL, which OoT uses for the Deku Shield there), then MM's
 * shield. MM's shields are modelled in the hand's space; OoT's matrix takes the Deku Shield's own
 * space, which is the hand's with its axes turned: (x, y, z) -> (y, -z, -x - 100) (measured: every
 * vertex of OoT's held Deku Shield, texture coordinates too, lands on one of its plain model's). MM's own "with matrix" lists
 * place it from MM's sheath, which OoT's isn't. Built once per sheath and shield.
 */
static Gfx* NvMmForm_MmShieldOnBack(s32 swordIn, s32 hero) {
    static Gfx sBack[2][2][8];
    static Mtx sHandToShield;
    Gfx* g = sBack[swordIn][hero];

    if (g[0].words.w0 == 0) {
        Gfx* deku = ResourceMgr_LoadGfxByName("__OTR__objects/object_link_child/gLinkChildDekuShieldWithMatrixDL");
        s32 i;

        if (sHandToShield.m[0][0] == 0) {
            // (this hangs MM's shields as MM hangs them in Termina: the Hero's point down, its Triforce
            // under the bird, as the Hero's Shield is made (its icon); checked against MM, the user's
            // screenshot, 2026-11-24)
            MtxF mf = { { { 0, 0, -1, 0 }, { 1, 0, 0, 0 }, { 0, -1, 0, 0 }, { 0, 0, -100, 1 } } };

            Matrix_MtxFToMtx(&mf, &sHandToShield);
        }
        gSPDisplayList(g++, ResourceMgr_LoadGfxByName(swordIn ? "__OTR__objects/object_link_child/gLinkChildSwordAndSheathNearDL"
                                                               : "__OTR__objects/object_link_child/gLinkChildSheathNearDL"));
        for (i = 0; i < 8; i++) { // (OoT's matrix: its command, two words)
            if ((u8)(deku[i].words.w0 >> 24) == (u8)G_MTX_OTR) {
                *g++ = deku[i];
                *g++ = deku[i + 1];
                break;
            }
            if ((u8)(deku[i].words.w0 >> 24) == (u8)G_MARKER) {
                i++;
            }
        }
        gSPMatrix(g++, &sHandToShield, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
        gSPDisplayList(g++, ResourceMgr_LoadGfxByName(hero ? "__OTR__mm/objects/object_link_child/gLinkHumanHerosShieldDL"
                                                           : "__OTR__mm/objects/object_link_child/gLinkHumanMirrorShieldDL"));
        gSPEndDisplayList(g++);
    }
    return sBack[swordIn][hero];
}

/** z_player_lib.c's limb hook: the form's own hands, waist and sheath (MM's form tables; the hands
 * closed running, as OoT's). */
void NvMmForm_OverrideLimb(s32 limbIndex, Gfx** dList) {
    const NvMmFormDef* def = NvMmForm_Def(sForm.form);
    Player* player = (gPlayState != NULL) ? GET_PLAYER(gPlayState) : NULL;
    s32 closed = (player != NULL) && (player->actor.speedXZ > 2.0f);
    const char* dl;
    s32 NvGear_GfsOn(void);

    if ((def == NULL) && (limbIndex == PLAYER_LIMB_L_HAND) && (player != NULL) && NvGear_GfsOn() &&
        (player->leftHandType == PLAYER_MODELTYPE_LH_BGS)) {
        // the Great Fairy's Sword in human Link's hand (the gear page): OoT's fist and MM's sword, as MM's
        // own gLinkHumanLeftHandHoldingGreatFairysSwordDL is MM's fist and the sword
        s32 NvMmLink_IsMm(void);

        if (!LINK_IS_ADULT && NvMmLink_IsMm()) { // (MM's own model in Termina: MM's own hand holding it)
            *dList = ResourceMgr_LoadGfxByName("__OTR__mm/objects/object_link_child/gLinkHumanLeftHandHoldingGreatFairysSwordDL");
            return;
        }
        *dList = NvMmForm_Pair(LINK_IS_ADULT ? "__OTR__objects/object_link_boy/gLinkAdultLeftHandClosedNearDL"
                                             : "__OTR__objects/object_link_child/gLinkChildLeftFistNearDL",
                               "__OTR__mm/objects/object_link_child/gLinkHumanGreatFairysSwordDL");
        return;
    }
    if ((def == NULL) && (player != NULL) && (NvGear_MmShield() != 0)) {
        // MM's shield on young Link (the gear page), as MM draws it: in his fist (MM's own hand holding it
        // is its fist and the shield), or on his back with his sheath (MM's shield "with matrix", placed
        // from the sheath)
        s32 hero = (NvGear_MmShield() == 1);
        s32 NvMmLink_IsMm(void);
        s32 mmModel = NvMmLink_IsMm(); // (in Termina young Link is MM's own model: MM's lists as they are)

        if ((limbIndex == PLAYER_LIMB_R_HAND) && (player->rightHandType == PLAYER_MODELTYPE_RH_SHIELD)) {
            if (mmModel) {
                *dList = ResourceMgr_LoadGfxByName(hero ? "__OTR__mm/objects/object_link_child/gLinkHumanRightHandHoldingHerosShieldDL"
                                                        : "__OTR__mm/objects/object_link_child/gLinkHumanRightHandHoldingMirrorShieldDL");
                return;
            }
            *dList = NvMmForm_Pair("__OTR__objects/object_link_child/gLinkChildRightHandClosedNearDL",
                                   hero ? "__OTR__mm/objects/object_link_child/gLinkHumanHerosShieldDL"
                                        : "__OTR__mm/objects/object_link_child/gLinkHumanMirrorShieldDL");
            return;
        }
        if ((limbIndex == PLAYER_LIMB_SHEATH) && ((player->sheathType == PLAYER_MODELTYPE_SHEATH_18) ||
                                                  (player->sheathType == PLAYER_MODELTYPE_SHEATH_19))) {
            if (mmModel) { // (MM's own: its shield "with matrix", placed from MM's sheath, as MM draws it)
                *dList = NvMmForm_Pair((player->sheathType == PLAYER_MODELTYPE_SHEATH_18)
                                           ? "__OTR__objects/object_link_child/gLinkChildSwordAndSheathNearDL"
                                           : "__OTR__objects/object_link_child/gLinkChildSheathNearDL",
                                       hero ? "__OTR__mm/objects/object_link_child/gLinkHumanHerosShieldWithMtxDL"
                                            : "__OTR__mm/objects/object_link_child/gLinkHumanMirrorShieldWithMtxDL");
                return;
            }
            *dList = NvMmForm_MmShieldOnBack(player->sheathType == PLAYER_MODELTYPE_SHEATH_18, hero);
            return;
        }
    }
    if (def == NULL) {
        return;
    }
    if ((player != NULL) && NvMmGoron_Curled(player)) { // (curled: the ball's drawn instead, z_nv_mm_goron.c)
        *dList = NULL;
        return;
    }
    switch (limbIndex) {
        case PLAYER_LIMB_L_HAND:
            dl = def->handL[closed];
            if ((sForm.form == MM_FORM_FD) && (player != NULL) && ((player->leftHandType == PLAYER_MODELTYPE_LH_SWORD) ||
                                                                 (player->leftHandType == PLAYER_MODELTYPE_LH_BGS))) {
                const char* oot = NULL;
                Gfx* held;

                if (player->leftHandType == PLAYER_MODELTYPE_LH_BGS) { // (Biggoron's, or the broken knife)
                    oot = (gSaveContext.swordHealth <= 0.0f) ? "__OTR__objects/object_link_boy/gLinkAdultHandHoldingBrokenGiantsKnifeDL"
                                                            : "__OTR__objects/object_link_boy/gLinkAdultLeftHandHoldingBgsNearDL";
                } else if (CUR_EQUIP_VALUE(EQUIP_TYPE_SWORD) == EQUIP_VALUE_SWORD_MASTER) {
                    oot = "__OTR__objects/object_link_boy/gLinkAdultLeftHandHoldingMasterSwordNearDL";
                }
                // (the Kokiri Sword's place: his own Double Helix Sword, MM's)
                held = (oot != NULL) ? NvMmForm_FdHoldingOotSword(oot) : NULL;
                if (held != NULL) {
                    *dList = held;
                    return;
                }
                dl = BOY("gLinkFierceDeityLeftHandHoldingSwordDL");
            }
            break;
        case PLAYER_LIMB_R_HAND:
            dl = def->handR[closed];
            if ((sForm.form == MM_FORM_FD) && (player != NULL) && NvMmForm_FdShield() &&
                (player->rightHandType == PLAYER_MODELTYPE_RH_SHIELD)) { // (his shield in hand: OoT's, his fist)
                Gfx* held = NvMmForm_FdOotShield((NvMmForm_FdShield() == EQUIP_VALUE_SHIELD_MIRROR)
                                                     ? OOT_BOY("gLinkAdultRightHandHoldingMirrorShieldNearDL")
                                                     : OOT_BOY("gLinkAdultRightHandHoldingHylianShieldNearDL"),
                                                 BOY("gLinkFierceDeityRightHandDL"));

                if (held != NULL) {
                    *dList = held;
                    return;
                }
            }
            break;
        case PLAYER_LIMB_SHEATH:
            dl = def->sheath;
            if ((sForm.form == MM_FORM_FD) && (player != NULL) && NvMmForm_FdShield() &&
                ((player->sheathType == PLAYER_MODELTYPE_SHEATH_18) ||
                 (player->sheathType == PLAYER_MODELTYPE_SHEATH_19))) { // (his shield on his back, no sheath)
                Gfx* back = NvMmForm_FdOotShield((NvMmForm_FdShield() == EQUIP_VALUE_SHIELD_MIRROR)
                                                     ? OOT_BOY("gLinkAdultMirrorShieldAndSheathNearDL")
                                                     : OOT_BOY("gLinkAdultHylianShieldAndSheathNearDL"),
                                                 NULL);

                if (back != NULL) {
                    *dList = back;
                    return;
                }
            }
            break;
        case PLAYER_LIMB_WAIST:
            dl = def->waist;
            break;
        default:
            return;
    }
    *dList = (dl != NULL) ? ResourceMgr_LoadGfxByName(dl) : NULL;
}

/** z_player_lib.c's limb hook (each limb, after it's drawn): the forms' own extras there. Deku Link's
 * flowers in his hands flying (z_nv_mm_deku.c); Zora Link's fins on his forearms (MM's func_80126BD0:
 * folded, MM's 0.4, 0.6, 0.7, as when he isn't using them). */
void NvMmForm_PostLimb(PlayState* play, Player* player, s32 limbIndex) {
    void NvMmDeku_DrawHandFlower(PlayState* play, Player* this, s32 limbIndex);
    s32 NvMmZora_SwimFinsOut(Player* this);

    if ((limbIndex == PLAYER_LIMB_L_HAND) || (limbIndex == PLAYER_LIMB_R_HAND)) {
        NvMmDeku_DrawHandFlower(play, player, limbIndex);
    }
    if ((sForm.form == MM_FORM_ZORA) && (player->actor.category == ACTORCAT_PLAYER) &&
        ((limbIndex == PLAYER_LIMB_L_FOREARM) || (limbIndex == PLAYER_LIMB_R_FOREARM))) {
        OPEN_DISPS(play->state.gfxCtx);
        Matrix_Push();
        Matrix_Scale(0.4f, 0.6f, 0.7f, MTXMODE_APPLY);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)((limbIndex == PLAYER_LIMB_L_FOREARM) ? ZORA("gLinkZoraLeftFinDL")
                                                                                   : ZORA("gLinkZoraRightFinDL")));
        Matrix_Pop();
        if (NvMmZora_SwimFinsOut(player)) { // (gliding: his back fins out too, MM's D_801C0ABC)
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)((limbIndex == PLAYER_LIMB_L_FOREARM)
                                                       ? ZORA("gLinkZoraLeftSwimFinDL")
                                                       : ZORA("gLinkZoraRightSwimFinDL")));
        }
        CLOSE_DISPS(play->state.gfxCtx);
    }
}
