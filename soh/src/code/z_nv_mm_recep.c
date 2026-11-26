/*
 * Nightmareverse: two small ones from Clock Town's buildings (mm/README.md "The Mayor's receptionist
 * and the Postman's letter").
 *
 * - En_Recepgirl (zeldaret/mm, ovl_En_Recepgirl): the Mayor's receptionist at her desk, MM's
 *   object_bg. Her lines, her switch flag (params: heard the directions), Kafei's Mask's line, her
 *   animations, blinking, her head turning to Link. MM turns the head in its transform draw; here
 *   OoT's limb override does (as the Postman, z_nv_mm_pm.c).
 * - En_Mm2 (ovl_En_Mm2): the Postman's letter to himself, on his table in the Post Office. Its model
 *   is in the actor's own overlay file: the textures and vertices come over as an object
 *   (mm/objects/ovl_En_Mm2) and its short display list, MM's sEnMm2DL, is written out below.
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include "soh/ResourceManagerHelpers.h"

#define NV_MM_TEXT(id) (0xB000 + (id))
#define MM_WE(byte, bit) (((byte) << 8) | (bit))
#define WEEKEVENTREG_RESOLVED_MAYOR_MEETING MM_WE(60, 0x08)
#define MM_PLAYER_MASK_KAFEIS_MASK 2

s32 NvMm_CheckWeekEvent(s32 flag);
s32 NvMmMask_Worn(void);

/* ---------------------------------------------------------------------------------------------- */
/* En_Recepgirl                                                                                   */

#define BG(name) ("__OTR__mm/objects/object_bg/" name)
#define OBJECT_BG_2_LIMB_05 5
#define OBJECT_BG_2_LIMB_MAX 0x18

static const char sRecepSkel[] = BG("object_bg_Skel_011B60");
static const char sAnimIdle[] = BG("object_bg_Anim_009890");
static const char sAnimTalkLoop[] = BG("object_bg_Anim_001384");
static const char sAnimBow[] = BG("object_bg_Anim_00AD98");
static const char sAnimStartTalk[] = BG("object_bg_Anim_00A280");
static const char sAnimPoint[] = BG("object_bg_Anim_000968");
static const char* sRecepEyes[] = { BG("object_bg_Tex_00F8F0"), BG("object_bg_Tex_00FCF0"), BG("object_bg_Tex_0100F0"),
                                    BG("object_bg_Tex_00FCF0") };

struct NvMmRecep;
typedef void (*NvMmRecepFunc)(struct NvMmRecep*, PlayState*);

typedef struct NvMmRecep {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[OBJECT_BG_2_LIMB_MAX];
    Vec3s morphTable[OBJECT_BG_2_LIMB_MAX];
    NvMmRecepFunc func;
    u8 eyeTexIndex;
    Vec3s headRot;
    u16 textId; // MM's (the actor's textId is OoT's: MM's + 0xB000)
} NvMmRecep;

size_t gNvMmRecepgirlSize = sizeof(NvMmRecep);

static void NvMmRecep_Wait(NvMmRecep* this, PlayState* play);
static void NvMmRecep_Talk(NvMmRecep* this, PlayState* play);

static void NvMmRecep_SetText(NvMmRecep* this, u16 id) {
    this->textId = id;
    this->actor.textId = NV_MM_TEXT(id);
}

void NvMmRecepgirl_Init(Actor* thisx, PlayState* play) {
    NvMmRecep* this = (NvMmRecep*)thisx;

    thisx->targetMode = 6;
    ActorShape_Init(&thisx->shape, -60.0f, NULL, 0.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)sRecepSkel, (AnimationHeader*)sAnimIdle,
                       this->jointTable, this->morphTable, OBJECT_BG_2_LIMB_MAX);
    this->eyeTexIndex = 2;
    NvMmRecep_SetText(this, Flags_GetSwitch(play, thisx->params) ? 0x2ADC : 0x2AD9); // directions again?; welcome
    this->func = NvMmRecep_Wait;
}

void NvMmRecepgirl_Destroy(Actor* thisx, PlayState* play) {
}

static void NvMmRecep_SetupWait(NvMmRecep* this) {
    if (this->skelAnime.animation == (AnimationHeader*)sAnimTalkLoop) {
        Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sAnimBow, 5.0f);
    }
    this->func = NvMmRecep_Wait;
}

/** MM's EnRecepgirl_Wait. */
static void NvMmRecep_Wait(NvMmRecep* this, PlayState* play) {
    if (SkelAnime_Update(&this->skelAnime)) {
        if (this->skelAnime.animation == (AnimationHeader*)sAnimStartTalk) {
            Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sAnimBow, 5.0f);
        } else {
            Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)sAnimIdle, -4.0f);
        }
    }
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sAnimStartTalk, -4.0f);
        this->func = NvMmRecep_Talk;
    } else if (Actor_IsFacingPlayer(&this->actor, 0x2000)) {
        if (NvMmMask_Worn() == MM_PLAYER_MASK_KAFEIS_MASK) {
            NvMmRecep_SetText(this, 0x2367);
        } else if (Flags_GetSwitch(play, this->actor.params)) {
            NvMmRecep_SetText(this, 0x2ADC);
        } else {
            NvMmRecep_SetText(this, 0x2AD9);
        }
        Actor_OfferTalk(&this->actor, play, 60.0f);
    }
}

/** MM's EnRecepgirl_Talk. */
static void NvMmRecep_Talk(NvMmRecep* this, PlayState* play) {
    u8 talkState;

    if (SkelAnime_Update(&this->skelAnime)) {
        if (this->skelAnime.animation == (AnimationHeader*)sAnimStartTalk) {
            Animation_PlayLoop(&this->skelAnime, (AnimationHeader*)sAnimTalkLoop);
        } else if (this->skelAnime.animation == (AnimationHeader*)sAnimBow) {
            if (this->textId == 0x2ADA) { // the Mayor's office on the left (the meeting going on)
                Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sAnimPoint, 10.0f);
            } else {
                Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)sAnimIdle, 10.0f);
            }
        } else if (this->textId == 0x2ADA) {
            Animation_MorphToLoop(&this->skelAnime, (AnimationHeader*)sAnimIdle, 10.0f);
        } else {
            Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sAnimStartTalk, -4.0f);
        }
    }
    talkState = Message_GetState(&play->msgCtx);
    if (talkState == TEXT_STATE_CLOSING) {
        NvMmRecep_SetText(this, 0x2ADC);
        NvMmRecep_SetupWait(this);
    } else if ((talkState == TEXT_STATE_EVENT) && Message_ShouldAdvance(play)) {
        if (this->textId == 0x2AD9) {
            Flags_SetSwitch(play, this->actor.params);
            Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sAnimBow, 10.0f);
            NvMmRecep_SetText(this, NvMm_CheckWeekEvent(WEEKEVENTREG_RESOLVED_MAYOR_MEETING) ? 0x2ADF : 0x2ADA);
        } else if (this->textId == 0x2ADC) {
            Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sAnimBow, 10.0f);
            NvMmRecep_SetText(this, 0x2ADD);
        } else {
            Animation_MorphToPlayOnce(&this->skelAnime, (AnimationHeader*)sAnimPoint, 10.0f);
            if (this->textId == 0x2ADD) {
                NvMmRecep_SetText(this, 0x2ADE); // office on the left, drawing room on the right
            } else if (this->textId == 0x2ADA) {
                NvMmRecep_SetText(this, 0x2ADB); // the drawing room on the right
            } else {
                NvMmRecep_SetText(this, 0x2AE0); // the drawing room; not without an appointment
            }
        }
        Message_ContinueTextbox(play, this->actor.textId);
    }
}

void NvMmRecepgirl_Update(Actor* thisx, PlayState* play) {
    NvMmRecep* this = (NvMmRecep*)thisx;
    Vec3s torsoRot;

    this->func(this, play);
    Actor_TrackPlayer(play, &this->actor, &this->headRot, &torsoRot, this->actor.focus.pos);
    if (this->eyeTexIndex != 0) { // MM's EnRecepgirl_UpdateEyes
        if (++this->eyeTexIndex == 4) {
            this->eyeTexIndex = 0;
        }
    } else if (Rand_ZeroOne() < 0.02f) {
        this->eyeTexIndex++;
    }
}

static s32 NvMmRecep_OverrideLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3f* pos, Vec3s* rot,
                                      void* thisx) {
    NvMmRecep* this = (NvMmRecep*)thisx;

    if (limbIndex == OBJECT_BG_2_LIMB_05) {
        rot->x += this->headRot.y;
        rot->y += 0x400 - this->headRot.x; // (MM's transform draw: Matrix_RotateYS after the limb's own)
    }
    return false;
}

static void NvMmRecep_PostLimbDraw(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    static Vec3f sFocusOffset = { 500.0f, 0.0f, 0.0f };
    NvMmRecep* this = (NvMmRecep*)thisx;

    if (limbIndex == OBJECT_BG_2_LIMB_05) {
        Matrix_MultVec3f(&sFocusOffset, &this->actor.focus.pos);
    }
}

void NvMmRecepgirl_Draw(Actor* thisx, PlayState* play) {
    NvMmRecep* this = (NvMmRecep*)thisx;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, SEGMENTED_TO_VIRTUAL(sRecepEyes[this->eyeTexIndex]));
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount,
                          NvMmRecep_OverrideLimbDraw, NvMmRecep_PostLimbDraw, &this->actor);
    CLOSE_DISPS(play->state.gfxCtx);
}

/* ---------------------------------------------------------------------------------------------- */
/* En_Mm2                                                                                         */

#define MM2(name) ("__OTR__mm/objects/ovl_En_Mm2/" name)

static const char sEnvelopeTex[] = MM2("sEnMm2EnvelopeTex");
static const char sLetterTex[] = MM2("sEnMm2LetterTex");
static char sMm2Vtx1[] = MM2("sEnMm21Vtx");
static char sMm2Vtx2[] = MM2("sEnMm22Vtx");

struct NvMmMm2;
typedef void (*NvMmMm2Func)(struct NvMmMm2*, PlayState*);

typedef struct NvMmMm2 {
    Actor actor;
    NvMmMm2Func func;
} NvMmMm2;

size_t gNvMmMm2Size = sizeof(NvMmMm2);

static void NvMmMm2_WaitForRead(NvMmMm2* this, PlayState* play);

void NvMmMm2_Init(Actor* thisx, PlayState* play) {
    NvMmMm2* this = (NvMmMm2*)thisx;

    Actor_SetScale(thisx, 0.015f);
    thisx->textId = NV_MM_TEXT(0x277B);
    this->func = NvMmMm2_WaitForRead;
}

void NvMmMm2_Destroy(Actor* thisx, PlayState* play) {
}

/** MM's EnMm2_Reading. */
static void NvMmMm2_Reading(NvMmMm2* this, PlayState* play) {
    switch (Message_GetState(&play->msgCtx)) {
        case TEXT_STATE_EVENT:
            if (Message_ShouldAdvance(play)) {
                Message_CloseTextbox(play);
                this->func = NvMmMm2_WaitForRead;
            }
            break;
        case TEXT_STATE_CLOSING:
        case TEXT_STATE_DONE:
            this->func = NvMmMm2_WaitForRead;
            break;
        default:
            break;
    }
}

/** MM's EnMm2_WaitForRead: "Check" when close and facing it. */
static void NvMmMm2_WaitForRead(NvMmMm2* this, PlayState* play) {
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->func = NvMmMm2_Reading;
    } else if ((this->actor.xzDistToPlayer < 60.0f) && Player_IsFacingActor(&this->actor, 0x3000, play)) {
        Actor_OfferTalk(&this->actor, play, 110.0f);
    }
}

void NvMmMm2_Update(Actor* thisx, PlayState* play) {
    NvMmMm2* this = (NvMmMm2*)thisx;

    this->func(this, play);
}

/** MM's sEnMm2DL ("mm_myletter_model"), as it is in the overlay. */
void NvMmMm2_Draw(Actor* thisx, PlayState* play) {
    Vtx* v1 = ResourceMgr_LoadVtxByName(sMm2Vtx1);
    Vtx* v2 = ResourceMgr_LoadVtxByName(sMm2Vtx2);
    Gfx* g;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    g = POLY_OPA_DISP;
    gSPTexture(g++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(g++);
    g->words.w0 = 0xFC30FE80; // (MM's combiner and render mode words, as they are)
    g->words.w1 = 0x5FFEF3F8;
    g++;
    gDPSetPrimColor(g++, 0, 0xFF, 0xFF, 0xFF, 0x9B, 0xFF);
    gDPSetEnvColor(g++, 0x87, 0x00, 0x00, 0xFF);
    g->words.w0 = 0xE200001C;
    g->words.w1 = 0xC8112078;
    g++;
    gDPLoadTextureBlock(g++, sEnvelopeTex, G_IM_FMT_I, G_IM_SIZ_8b, 32, 32, 0, G_TX_MIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
    gSPLoadGeometryMode(g++, 0x230005);
    gSPVertex(g++, v1, 10, 0);
    gSP2Triangles(g++, 0, 1, 2, 0, 2, 3, 0, 0);
    gSP2Triangles(g++, 4, 5, 6, 0, 7, 8, 6, 0);
    gSP2Triangles(g++, 6, 5, 7, 0, 6, 9, 4, 0);
    gDPPipeSync(g++);
    g->words.w0 = 0xFC127E60;
    g->words.w1 = 0xFFFFF3F8;
    g++;
    gDPLoadTextureBlock(g++, sLetterTex, G_IM_FMT_RGBA, G_IM_SIZ_16b, 16, 16, 0, G_TX_MIRROR | G_TX_CLAMP,
                        G_TX_MIRROR | G_TX_CLAMP, 4, 4, G_TX_NOLOD, G_TX_NOLOD);
    gDPSetPrimColor(g++, 0, 0x80, 0xFF, 0xFF, 0xFF, 0xFF);
    gSPVertex(g++, v2, 12, 0);
    gSP2Triangles(g++, 0, 1, 2, 0, 3, 1, 0, 0);
    gSP2Triangles(g++, 4, 5, 6, 0, 7, 5, 4, 0);
    gSP2Triangles(g++, 0, 8, 3, 0, 6, 9, 4, 0);
    gSP2Triangles(g++, 2, 10, 0, 0, 4, 11, 7, 0);
    POLY_OPA_DISP = g;
    CLOSE_DISPS(play->state.gfxCtx);
}
