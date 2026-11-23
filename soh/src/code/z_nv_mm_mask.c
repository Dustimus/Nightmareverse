/*
 * Nightmareverse: wearing Majora's Mask's masks (mm/README.md "Wearing MM's masks").
 *
 * MM's masks go on C buttons from MM's masks page (z_nv_mm_pause.c) and are worn by pressing the
 * button, as OoT's own masks: on, and again, off. One mask at a time: putting one on takes off
 * any other, OoT's too (OoT's player->currentMask), and an OoT mask put on takes MM's off.
 *
 * On the buttons an MM mask is an item id OoT doesn't use, NV_MM_MASK_ITEM_FIRST + (MM's item id -
 * ITEM_MASK_DEKU): its icon is MM's (OoT's gItemIcons table, grown to 256 entries, filled here),
 * young Link may use it (gItemAgeReqs). Pressing it reaches here first (Player_UseItem's hook,
 * z_player.c), not OoT's item actions.
 *
 * Worn, it's drawn on Link's head as MM draws it (MM's D_801C0B20: the mask's model in the head's
 * space, after the head; the same skeleton). MM's people see it: NvMmMask_Worn gives MM's
 * PLAYER_MASK_* for their checks (the message scripts' CHECK_WORN_MASK, the actors' Player_GetMask).
 *
 * The transformation masks' buttons go to z_nv_mm_form.c (the Deku form so far; the others give
 * OoT's error). Not yet: MM's put-on animation (the mask goes on at once, with OoT's sound), what masks
 * do beyond being seen (the Bunny Hood's speed, the Stone Mask, the Blast Mask's blast, the Great
 * Fairy's Mask, the Giant's Mask...), the moving parts some have (the Bunny Hood's ears, the Couple's
 * Mask, the Circus Leader's tears), adult Link (plain masks on adult: an open question).
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>

#define NV_MM_MASK_ITEM_FIRST 0xA0 // (0xA0-0xB7: OoT has nothing there)
#define MM_ITEM_MASK_DEKU 0x32
#define MM_ITEM_MASK_GIANT 0x49

#define MASK(obj, dl) ("__OTR__mm/objects/" obj "/" dl)

extern void* gItemIcons[];
extern u8 gItemAgeReqs[];

// MM's item id for each of MM's PLAYER_MASK_* (1-20: the masks that aren't forms), and its model
static const struct {
    u8 item;
    const char* dl;
} sMasks[] = {
    { 0x00, NULL },
    { 0x36, MASK("object_mask_truth", "object_mask_truth_DL_0001A0") },
    { 0x37, MASK("object_mask_kerfay", "gKafeisMaskDL") },
    { 0x38, MASK("object_mask_yofukasi", "object_mask_yofukasi_DL_000490") },
    { 0x39, MASK("object_mask_rabit", "object_mask_rabit_DL_000610") },
    { 0x3A, MASK("object_mask_ki_tan", "object_mask_ki_tan_DL_0004A0") },
    { 0x3B, MASK("object_mask_json", "object_mask_json_DL_0004C0") },
    { 0x3C, MASK("object_mask_romerny", "object_mask_romerny_DL_0007A0") },
    { 0x3D, MASK("object_mask_zacho", "object_mask_zacho_DL_000700") },
    { 0x3E, MASK("object_mask_posthat", "object_mask_posthat_DL_000290") },
    { 0x3F, MASK("object_mask_meoto", "object_mask_meoto_DL_0005A0") },
    { 0x40, MASK("object_mask_bigelf", "object_mask_bigelf_DL_0016F0") },
    { 0x41, MASK("object_mask_gibudo", "object_mask_gibudo_DL_000250") },
    { 0x42, MASK("object_mask_gero", "gDonGeroMaskDL") },
    { 0x43, MASK("object_mask_dancer", "object_mask_dancer_DL_000EF0") },
    { 0x44, MASK("object_mask_skj", "object_mask_skj_DL_0009F0") },
    { 0x45, MASK("object_mask_stone", "object_mask_stone_DL_000820") },
    { 0x46, MASK("object_mask_bree", "object_mask_bree_DL_0003C0") },
    { 0x47, MASK("object_mask_bakuretu", "object_mask_bakuretu_DL_0005C0") },
    { 0x48, MASK("object_mask_bu_san", "object_mask_bu_san_DL_000710") },
    { 0x49, MASK("object_mask_kyojin", "object_mask_kyojin_DL_000380") },
};

static s32 sWorn; // MM's PLAYER_MASK_* worn (0: none)

s32 NvMmItem_Has(s32 item);
s32 NvMmForm_UseMask(PlayState* play, Player* player, s32 mmItem);
s32 NvMmForm_BlocksItem(s32 item);
s32 NvMmForm_Get(void);

/** MM's mask item as the item id it has on a C button (0 if it isn't a mask). */
s32 NvMmMask_ButtonItem(s32 mmItem) {
    if ((mmItem < MM_ITEM_MASK_DEKU) || (mmItem > MM_ITEM_MASK_GIANT)) {
        return 0;
    }
    return NV_MM_MASK_ITEM_FIRST + (mmItem - MM_ITEM_MASK_DEKU);
}

/** A C button's item as MM's mask item (-1 if it isn't one of ours). */
static s32 NvMmMask_FromButton(s32 item) {
    if ((item < NV_MM_MASK_ITEM_FIRST) || (item > NV_MM_MASK_ITEM_FIRST + (MM_ITEM_MASK_GIANT - MM_ITEM_MASK_DEKU))) {
        return -1;
    }
    return MM_ITEM_MASK_DEKU + (item - NV_MM_MASK_ITEM_FIRST);
}

static s32 NvMmMask_PlayerMask(s32 mmItem) {
    s32 i;

    for (i = 1; i < ARRAY_COUNT(sMasks); i++) {
        if (sMasks[i].item == mmItem) {
            return i;
        }
    }
    return 0; // (a form)
}

/** The icons and age rules for MM's masks on the buttons (once; MM's icons from z_nv_mm_pause.c's). */
void NvMmMask_Init(void) {
    static s32 sDone;
    extern const char* NvMmPause_Icon(s32 mmItem);
    s32 item;

    if (sDone) {
        return;
    }
    sDone = true;
    for (item = MM_ITEM_MASK_DEKU; item <= MM_ITEM_MASK_GIANT; item++) {
        gItemIcons[NvMmMask_ButtonItem(item)] = (void*)NvMmPause_Icon(item);
        gItemAgeReqs[NvMmMask_ButtonItem(item)] = 1; // young Link (plain masks on adult: open)
    }
}

/** Player_UseItem's hook (z_player.c): an MM mask's button pressed; true if it was one. */
s32 NvMmMask_UseItem(PlayState* play, Player* player, s32 item) {
    s32 mmItem = NvMmMask_FromButton(item);
    s32 mask;

    if (mmItem < 0) {
        if (NvMmForm_BlocksItem(item)) { // (a form's hands: z_nv_mm_form.c)
            Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
            return true;
        }
        return false;
    }
    mask = NvMmMask_PlayerMask(mmItem);
    if (mask == 0) { // a form's mask: z_nv_mm_form.c
        return NvMmForm_UseMask(play, player, mmItem);
    }
    if (!NvMmItem_Has(mmItem) || (NvMmForm_Get() != 4)) { // (plain masks: human Link only, as MM)
        Sfx_PlaySfxCentered(NA_SE_SY_ERROR);
        return true;
    }
    sWorn = (sWorn == mask) ? 0 : mask;
    if (sWorn != 0) {
        player->currentMask = PLAYER_MASK_NONE; // (one mask at a time)
    }
    Audio_PlayActorSound2(&player->actor, NA_SE_PL_CHANGE_ARMS);
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM mask: %s 0x%02X\n", sWorn ? "on" : "off", mmItem);
        fflush(stdout);
    }
    return true;
}

/** MM's Player_GetMask for MM's people: the MM mask worn (MM's PLAYER_MASK_*), 0 for none. */
s32 NvMmMask_Worn(void) {
    return sWorn;
}

/*
 * The Great Fairy's Mask's hair: MM's Player_DrawGreatFairysMask, func_80127DA4 (the strands'
 * swing) and func_80128388 (their matrices), as they are; but the stray fairies' sparkle and float
 * (when a stray fairy is near: ACTORCTX_FLAG_3, not yet).
 */
typedef struct {
    Vec3f pos;
    Vec3f vel;
    s16 yaw;
    s16 pitch;
} NvMmHairNode; // MM's struct_801F58B0

typedef struct {
    f32 length;
    s16 yaw;
    s16 pitch;
    f32 unused;
    s16 yawLimit;
    s16 pitchLimit;
} NvMmHairLink; // MM's struct_80128388_arg1 (its unk_08/unk_14 unread)

static NvMmHairNode sHair[3][3];
static const NvMmHairLink sHairLinks[] = {
    { 0.0f, 0x0000, (s16)0x8000, 0.0f, 0x0000, 0x0000 },
    { 16.8f, 0x0000, 0x0000, 20.0f, 0x1388, 0x1388 },
    { 30.0f, 0x0000, 0x0000, 20.0f, 0x1F40, 0x2EE0 },
};
#define NV_ATAN2S_XY(x, y) Math_Atan2S((y), (x)) // (MM's Math_Atan2S_XY on OoT's Math_Atan2S)
#define NV_SQXZ(v) ((v).x * (v).x + (v).z * (v).z)

static void NvMmMask_HairSwing(NvMmHairNode* node, const NvMmHairLink* link, s32 count, Vec3f* root, Vec3f* toward) {
    NvMmHairNode* next = &node[1];
    Vec3f diff;
    Vec3f step;
    f32 f20, f22, f24, f28, f0;
    s32 i;
    s16 s0, s2;

    Math_Vec3f_Copy(&node->pos, root);
    Math_Vec3f_Diff(toward, root, &diff);
    node->yaw = NV_ATAN2S_XY(diff.z, diff.x);
    node->pitch = NV_ATAN2S_XY(sqrtf(NV_SQXZ(diff)), diff.y);
    link++;
    for (i = 1; i < count; i++) {
        Math_Vec3f_Sum(&next->pos, &next->vel, &next->pos);
        f0 = Math_Vec3f_DistXYZAndStoreDiff(&node->pos, &next->pos, &diff);
        f28 = f0 - link->length;
        if (f0 == 0.0f) {
            diff.x = 0.0f;
            diff.y = link->length;
            diff.z = 0.0f;
        }
        f20 = sqrtf(NV_SQXZ(diff));
        if (f20 > 4.0f) {
            next->yaw = NV_ATAN2S_XY(diff.z, diff.x);
            s2 = next->yaw - node->yaw;
            if (ABS(s2) > 0x4000) {
                next->yaw = (s16)(next->yaw + 0x8000);
                f20 = -f20;
            }
        }
        next->pitch = NV_ATAN2S_XY(f20, diff.y);
        s2 = next->yaw - node->yaw;
        s2 = CLAMP(s2, -link->yawLimit, link->yawLimit);
        next->yaw = node->yaw + s2;
        s0 = next->pitch - node->pitch;
        s0 = CLAMP(s0, -link->pitchLimit, link->pitchLimit);
        next->pitch = node->pitch + s0;

        f20 = Math_CosS(next->pitch) * link->length;
        step.x = Math_SinS(next->yaw) * f20;
        step.z = Math_CosS(next->yaw) * f20;
        step.y = Math_SinS(next->pitch) * link->length;
        Math_Vec3f_Sum(&node->pos, &step, &next->pos);
        next->vel.x *= 0.9f;
        next->vel.z *= 0.9f;

        f22 = Math_CosS(s0) * f28;
        f24 = Math_SinS(s0) * f28;
        next->vel.y += -1.0f;
        f20 = Math_SinS(node->pitch);
        next->vel.y += ((f22 * Math_CosS(node->pitch)) + (f24 * f20)) * 0.2f;
        next->vel.y = CLAMP(next->vel.y, -2.0f, 4.0f);

        f20 = (f24 * Math_CosS(node->pitch)) - (Math_SinS(node->pitch) * f22);
        f22 = Math_CosS(s2) * f20;
        f24 = Math_SinS(s2) * f20;
        f20 = Math_SinS(node->yaw);
        next->vel.x += ((f24 * Math_CosS(node->yaw)) - (f22 * f20)) * 0.1f;
        next->vel.x = CLAMP(next->vel.x, -4.0f, 4.0f);
        f20 = Math_SinS(node->yaw);
        next->vel.z += ((f22 * Math_CosS(node->yaw)) + (f24 * f20)) * -0.1f;
        next->vel.z = CLAMP(next->vel.z, -4.0f, 4.0f);

        node++;
        next++;
        link++;
    }
}

static void NvMmMask_HairMatrices(NvMmHairNode* node, const NvMmHairLink* link, s32 count, Mtx** mtx) {
    NvMmHairNode* next = &node[1];
    Vec3f pos = { 0.0f, 0.0f, 0.0f };
    Vec3s rot = { 0, 0, 0 };
    s32 i;

    for (i = 1; i < count; i++) {
        pos.x = link->length * 100.0f;
        rot.z = link->pitch + (s16)(next->pitch - node->pitch);
        rot.y = link->yaw + (s16)(next->yaw - node->yaw);
        Matrix_TranslateRotateZYX(&pos, &rot);
        MATRIX_TOMTX(*mtx);
        (*mtx)++;
        node++;
        next++;
        link++;
    }
}

static void NvMmMask_DrawGreatFairysHair(PlayState* play, Player* player) {
    static const Vec3f sRoots[] = { { 174.0f, -1269.0f, -1.0f }, { 401.0f, -729.0f, -701.0f },
                                    { 401.0f, -729.0f, 699.0f } };
    static const Vec3f sToward[] = { { 74.0f, -1269.0f, -1.0f }, { 301.0f, -729.0f, -701.0f },
                                     { 301.0f, -729.0f, 699.0f } };
    Mtx* mtx = Graph_Alloc(play->state.gfxCtx, 6 * sizeof(Mtx));
    Vec3f root;
    Vec3f toward;
    s32 i;

    OPEN_DISPS(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x0B, mtx);
    for (i = 0; i < 3; i++) {
        Matrix_MultVec3f((Vec3f*)&sRoots[i], &root);
        Matrix_MultVec3f((Vec3f*)&sToward[i], &toward);
        NvMmMask_HairSwing(sHair[i], sHairLinks, 3, &root, &toward);
        Matrix_Push();
        Matrix_Translate(sRoots[i].x, sRoots[i].y, sRoots[i].z, MTXMODE_APPLY);
        NvMmMask_HairMatrices(sHair[i], sHairLinks, 3, &mtx);
        Matrix_Pop();
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

/** Player_PostLimbDrawGameplay's hook at the head (z_player_lib.c): the mask, in the head's space. */
void NvMmMask_DrawOnHead(PlayState* play, Player* player) {
    s32 NvMmForm_DrawSceneMask(PlayState * play, Player * player);

    if (NvMmForm_DrawSceneMask(play, player)) { // (a form's mask going on: z_nv_mm_form.c)
        return;
    }
    if (sWorn == 0) {
        return;
    }
    if (player->currentMask != PLAYER_MASK_NONE) { // an OoT mask went on: MM's comes off
        sWorn = 0;
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    if (sWorn == 4) { // the Bunny Hood: its ears' matrices (MM's Player_DrawBunnyHood, at rest)
        Mtx* mtx = Graph_Alloc(play->state.gfxCtx, 2 * sizeof(Mtx));
        Vec3s earRot;

        gSPSegment(POLY_OPA_DISP++, 0x0B, mtx);
        Matrix_Push();
        earRot = (Vec3s){ 0x3E2, 0xDBE, -0x348A };
        Matrix_SetTranslateRotateYXZ(97.0f, -1203.0f, -240.0f, &earRot);
        MATRIX_TOMTX(mtx++);
        earRot = (Vec3s){ -0x3E2, -0xDBE, -0x348A };
        Matrix_SetTranslateRotateYXZ(97.0f, -1203.0f, 240.0f, &earRot);
        MATRIX_TOMTX(mtx);
        Matrix_Pop();
    } else if (sWorn == 11) { // the Great Fairy's Mask: its three strands of hair, swinging (MM's)
        NvMmMask_DrawGreatFairysHair(play, player);
    } else if (sWorn == 18) { // the Blast Mask: its colour list, unlit (MM's D_801C0BC0)
        static Gfx sBlastColor[] = { gsDPSetEnvColor(0, 0, 0, 255), gsSPEndDisplayList() };

        gSPSegment(POLY_OPA_DISP++, 0x09, sBlastColor);
    }
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(POLY_OPA_DISP++, (Gfx*)sMasks[sWorn].dl);
    CLOSE_DISPS(play->state.gfxCtx);
}

/** MM's masks page (z_nv_mm_pause.c): the mask under the cursor to a C button (1-3). */
void NvMmMask_Equip(PlayState* play, s32 mmItem, s32 button) {
    s32 item = NvMmMask_ButtonItem(mmItem);
    s32 i;

    if (item == 0) {
        return;
    }
    NvMmMask_Init();
    for (i = 1; i <= 3; i++) { // (on one button only)
        if ((i != button) && (gSaveContext.equips.buttonItems[i] == item)) {
            gSaveContext.equips.buttonItems[i] = ITEM_NONE;
            gSaveContext.equips.cButtonSlots[i - 1] = SLOT_NONE;
        }
    }
    gSaveContext.equips.buttonItems[button] = item;
    gSaveContext.equips.cButtonSlots[button - 1] = SLOT_NONE;
    Audio_PlaySfxGeneral(NA_SE_SY_DECIDE, &gSfxDefaultPos, 4, &gSfxDefaultFreqAndVolScale, &gSfxDefaultFreqAndVolScale,
                         &gSfxDefaultReverb);
}

/** z_nv_mm_form.c: a change of form takes the mask off. */
void NvMmMask_TakeOff(void) {
    sWorn = 0;
}

void NvMmMask_NewGame(void) {
    sWorn = 0;
}
