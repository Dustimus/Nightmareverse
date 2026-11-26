/*
 * Nightmareverse: the Rosa sisters, Judo (red) and Marilla (blue) (MM's En_Rz, MM id 0x27B), ported as
 * they are (mm/README.md "The townsfolk"): MM's model (object_rz: one model, the sisters' colours its
 * animated materials at step 0 or 1), their eyes and blinking, MM's lines. params & 0x8000: which sister;
 * & 0xF the type; (& 0x7E00) >> 9 their path.
 *   type 0  by day (West Clock Town): walking their path, stopping to think, a line each time;
 *   type 2  at night: practising their dance together; talked to, MM's line;
 *   type 1  after Link's shown them the dance (77_04): sitting, their thanks.
 * The heart piece: MM gives it when Link dances Kamaro's dance in front of them (Kamaro's Mask's dance,
 * MM's PLAYER_STATE2_2000000: NvMmMask_Dancing); then they ask him to show them, applaud, and dance his
 * dance ever after (the dance: z_nv_mm_march.c). MM's cutscene (their
 * camera, kneeling and applauding on cue) is played as plain talk; their music (MM's sub-BGM) waits for
 * MM's audio; once they've learnt Link's dance MM draws them with Link's own dance animation, kept
 * as their own dance here.
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>

#define RZ(name) ("__OTR__mm/objects/object_rz/" name)
#define NV_MM_TEXT(id) (0xB000 + (id))
#define MM_WE(byte, bit) (((byte) << 8) | (bit))
#define WE_77_04 MM_WE(77, 0x04)                  // Link showed them Kamaro's dance
#define WE_RECEIVED_ROSA_SISTERS_HP MM_WE(75, 0x80)
#define NOTEBOOK_MET_ROSA_SISTERS 12 // (MM's notebook table: 0x0C)
#define NOTEBOOK_RECEIVED_ROSA_SISTERS_HP 39 // (0x27)
#define MM_GI_HEART_PIECE 0x0C
#define MM_PLAYER_MASK_KAMARO 0x0E

#define RZ_JUDO 0
#define RZ_SISTER(thisx) ((thisx)->params & 0x8000)
#define RZ_TYPE(thisx) ((thisx)->params & 0xF)
#define RZ_PATH(thisx) (((thisx)->params & 0x7E00) >> 9)
#define RZ_LIMB_03 3
#define RZ_LIMB_0B 11
#define RZ_LIMB_MAX 22

#define RZ_STATE_BACKWARD 1 // (walking the path back)
#define RZ_STATE_THINKING 2 // (type 0: their thinking eyes)

s32 NvMm_CheckWeekEvent(s32 flag);
void NvMm_SetWeekEvent(s32 flag);
void NvMm_NotebookEvent(u16 event);
s32 NvMmMask_Worn(void);
GetItemEntry NvMmGetItem_Entry(s32 mmGi);
void NvMmMatAnim_DrawStep(PlayState* play, const char* path, u32 step);
s32 NvMmActor_Id(s32 mmId);

s32 NvMmMask_Dancing(PlayState* play); // Kamaro's dance being danced (MM's PLAYER_STATE2_2000000: z_nv_mm_march.c)

enum { RZ_ANIM_THINKING, RZ_ANIM_STANDING, RZ_ANIM_WALKING, RZ_ANIM_SITTING, RZ_ANIM_APPLAUDING, RZ_ANIM_ON_KNEES,
       RZ_ANIM_DANCE, RZ_ANIM_MAX };

static const char* sJudoAnims[] = { RZ("gRosaSistersThinkingAnim"),   RZ("gRosaSistersStandingAnim"),
                                    RZ("gRosaSistersWalkingWhileThinkingAnim"), RZ("gRosaSistersSittingAnim"),
                                    RZ("gRosaSistersApplaudingAnim"), RZ("gRosaSistersOnKneesAnim"),
                                    RZ("gRosaSistersDancingAnim") };
static const char* sMarillaAnims[] = { RZ("gRosaSistersStandingAnim"),   RZ("gRosaSistersStandingAnim"),
                                       RZ("gRosaSistersWalkingAnim"),    RZ("gRosaSistersSittingAnim"),
                                       RZ("gRosaSistersApplaudingAnim"), RZ("gRosaSistersOnKneesAnim"),
                                       RZ("gRosaSistersDancingAnim") };
static const char* sEyes[] = { RZ("object_rz_Tex_00BC50"), RZ("object_rz_Tex_00C190"), RZ("object_rz_Tex_00C590"),
                               RZ("object_rz_Tex_00C990"), RZ("object_rz_Tex_00CD90") };

struct NvMmRz;
typedef void (*NvMmRzFunc)(struct NvMmRz*, PlayState*);

typedef struct NvMmRz {
    Actor actor;
    SkelAnime skelAnime;
    Vec3s jointTable[RZ_LIMB_MAX];
    Vec3s morphTable[RZ_LIMB_MAX];
    Path* path;
    s32 curPointIndex;
    ColliderCylinder collider;
    struct NvMmRz* sister;
    s16 eyeIndex;
    s16 blinkTimer;
    u16 stateFlags;
    s16 animIndex;
    s16 timer;
    NvMmRzFunc actionFunc;
} NvMmRz;

const size_t gNvMmRzSize = sizeof(NvMmRz);

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_NONE, AT_NONE, AC_ON | AC_TYPE_ENEMY, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK0, { 0x00000000, 0x00, 0x00 }, { 0xF7CFFFFF, 0x00, 0x00 }, TOUCH_NONE | TOUCH_SFX_NORMAL, BUMP_ON,
      OCELEM_ON },
    { 20, 40, 0, { 0, 0, 0 } },
};

static void NvMmRz_Walk(NvMmRz* this, PlayState* play);
static void NvMmRz_Dance(NvMmRz* this, PlayState* play);
static void NvMmRz_Sit(NvMmRz* this, PlayState* play);

static void NvMmRz_ChangeAnim(NvMmRz* this, s16 i, u8 mode, f32 morph) {
    const char** anims = (RZ_SISTER(&this->actor) == RZ_JUDO) ? sJudoAnims : sMarillaAnims;
    AnimationHeader* a;

    if ((i < 0) || (i >= RZ_ANIM_MAX) || ((i == this->animIndex) && (mode == ANIMMODE_LOOP))) {
        return;
    }
    a = (AnimationHeader*)anims[i];
    Animation_Change(&this->skelAnime, a, 1.0f, 0.0f, Animation_GetLastFrame(a), mode, morph);
    this->animIndex = i;
}

static void NvMmRz_Notebook(void) { // MM's func_80BFBDFC
    if (NvMm_CheckWeekEvent(WE_RECEIVED_ROSA_SISTERS_HP)) {
        NvMm_NotebookEvent(NOTEBOOK_RECEIVED_ROSA_SISTERS_HP);
    }
    NvMm_NotebookEvent(NOTEBOOK_MET_ROSA_SISTERS);
}

static void NvMmRz_SetupPath(NvMmRz* this, PlayState* play) {
    this->path = NULL;
    this->curPointIndex = 0;
    if ((RZ_PATH(&this->actor) != 0x3F) && (play->setupPathList != NULL)) {
        Vec3s* points;

        this->path = &play->setupPathList[RZ_PATH(&this->actor)];
        points = (Vec3s*)SEGMENTED_TO_VIRTUAL(this->path->points);
        if (SQ(points->x - this->actor.world.pos.x) + SQ(points->z - this->actor.world.pos.z) < SQ(10.0f)) {
            this->stateFlags &= ~RZ_STATE_BACKWARD;
        } else {
            this->curPointIndex = this->path->count - 1;
            this->stateFlags |= RZ_STATE_BACKWARD;
        }
    }
}

/** MM's EnRz_GetPathStatus: 0 on the way, 1 at a point, 2 the path's end. */
static s32 NvMmRz_PathStatus(NvMmRz* this) {
    Vec3s* p;
    f32 dx, dz;

    if (this->path == NULL) {
        return 1;
    }
    p = &((Vec3s*)SEGMENTED_TO_VIRTUAL(this->path->points))[this->curPointIndex];
    dx = p->x - this->actor.world.pos.x;
    dz = p->z - this->actor.world.pos.z;
    this->actor.world.rot.y = Math_Atan2S(dz, dx);
    Math_SmoothStepToS(&this->actor.shape.rot.y, this->actor.world.rot.y, 2, 0x7D0, 0xC8);
    if ((SQ(dx) + SQ(dz)) < SQ(10.0f)) {
        if (this->stateFlags & RZ_STATE_BACKWARD) {
            if (--this->curPointIndex < 0) {
                return 2;
            }
        } else if (++this->curPointIndex >= this->path->count) {
            return 2;
        }
        return 1;
    }
    return 0;
}

static s32 NvMmRz_CanTalk(NvMmRz* this, PlayState* play) {
    return (this->actor.xzDistToPlayer < 80.0f) && (fabsf(this->actor.yDistToPlayer) < 20.0f) &&
           Player_IsFacingActor(&this->actor, 0x3000, play);
}

static NvMmRz* NvMmRz_FindSister(NvMmRz* this, PlayState* play) {
    Actor* npc = play->actorCtx.actorLists[ACTORCAT_NPC].head;

    while (npc != NULL) {
        if ((npc->id == this->actor.id) && (npc != &this->actor) && (RZ_TYPE(npc) == RZ_TYPE(&this->actor))) {
            return (NvMmRz*)npc;
        }
        npc = npc->next;
    }
    return NULL;
}

void NvMmRz_Init(Actor* thisx, PlayState* play) {
    NvMmRz* this = (NvMmRz*)thisx;

    Actor_SetScale(thisx, 0.01f);
    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 20.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)RZ("object_rz_Skel_00D8D8"),
                       (AnimationHeader*)RZ("gRosaSistersStandingAnim"), this->jointTable, this->morphTable, RZ_LIMB_MAX);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sCylinderInit);
    thisx->colChkInfo.mass = MASS_IMMOVABLE;
    NvMmRz_SetupPath(this, play);
    this->animIndex = RZ_ANIM_MAX;
    thisx->targetMode = 0;
    thisx->minVelocityY = -9.0f;
    thisx->gravity = -1.0f;
    switch (RZ_TYPE(thisx)) {
        case 1:
            if (!NvMm_CheckWeekEvent(WE_77_04)) {
                Actor_Kill(thisx);
                return;
            }
            NvMmRz_ChangeAnim(this, RZ_ANIM_SITTING, ANIMMODE_LOOP, 0.0f);
            this->actionFunc = NvMmRz_Sit;
            thisx->shape.yOffset = -1500.0f;
            break;
        case 2:
            thisx->flags |= ACTOR_FLAG_UPDATE_CULLING_DISABLED;
            NvMmRz_ChangeAnim(this, RZ_ANIM_DANCE, ANIMMODE_LOOP, 0.0f);
            this->actionFunc = NvMmRz_Dance;
            this->sister = NvMmRz_FindSister(this, play);
            break;
        default:
            this->stateFlags |= RZ_STATE_THINKING;
            if (NvMm_CheckWeekEvent(WE_77_04)) {
                Actor_Kill(thisx);
                return;
            }
            NvMmRz_ChangeAnim(this, RZ_ANIM_WALKING, ANIMMODE_LOOP, 0.0f);
            this->actionFunc = NvMmRz_Walk;
            thisx->textId = NV_MM_TEXT((RZ_SISTER(thisx) == RZ_JUDO) ? 0x291C : 0x2920);
            break;
    }
}

void NvMmRz_Destroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((NvMmRz*)thisx)->collider);
}

/* ---- type 0: walking, thinking, a line each stop ---- */

static void NvMmRz_ThinkTalk(NvMmRz* this, PlayState* play);

static void NvMmRz_StopToThink(NvMmRz* this, PlayState* play);

static void NvMmRz_Think(NvMmRz* this, PlayState* play) { // MM's func_80BFC7E0
    SkelAnime_Update(&this->skelAnime);
    if (this->timer > 0) {
        this->timer--;
    } else {
        this->actionFunc = NvMmRz_Walk;
        NvMmRz_ChangeAnim(this, RZ_ANIM_WALKING, ANIMMODE_LOOP, -5.0f);
    }
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->actionFunc = NvMmRz_ThinkTalk;
        NvMmRz_ChangeAnim(this, RZ_ANIM_THINKING, ANIMMODE_LOOP, -5.0f);
        this->actor.speedXZ = 0.0f;
        NvMmRz_Notebook();
    } else if (NvMmRz_CanTalk(this, play)) {
        Actor_OfferTalk(&this->actor, play, 120.0f);
    }
}

static void NvMmRz_StopToThink(NvMmRz* this, PlayState* play) {
    this->timer = 100;
    this->actionFunc = NvMmRz_Think;
    this->actor.speedXZ = 0.0f;
    NvMmRz_ChangeAnim(this, RZ_ANIM_THINKING, ANIMMODE_LOOP, -5.0f);
}

static void NvMmRz_ThinkTalk(NvMmRz* this, PlayState* play) { // MM's func_80BFC728: their lines in turn
    SkelAnime_Update(&this->skelAnime);
    if ((Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && Message_ShouldAdvance(play)) {
        u16 id = this->actor.textId - NV_MM_TEXT(0) + 1;

        Message_CloseTextbox(play);
        this->actionFunc = NvMmRz_Think;
        if (RZ_SISTER(&this->actor) == RZ_JUDO) {
            id = (id >= 0x2920) ? 0x291C : id;
        } else {
            id = (id >= 0x2924) ? 0x2920 : id;
        }
        this->actor.textId = NV_MM_TEXT(id);
    }
}

static void NvMmRz_Walk(NvMmRz* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    this->actor.speedXZ = 1.5f;
    switch (NvMmRz_PathStatus(this)) {
        case 2:
            NvMmRz_SetupPath(this, play);
            NvMmRz_StopToThink(this, play);
            break;
        case 1:
            NvMmRz_StopToThink(this, play);
            break;
    }
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->actionFunc = NvMmRz_ThinkTalk;
        NvMmRz_ChangeAnim(this, RZ_ANIM_THINKING, ANIMMODE_LOOP, -5.0f);
        this->actor.speedXZ = 0.0f;
        NvMmRz_Notebook();
    } else if (NvMmRz_CanTalk(this, play)) {
        Actor_OfferTalk(&this->actor, play, 120.0f);
    }
}

/* ---- type 2: dancing at night ---- */

static void NvMmRz_DanceTalk(NvMmRz* this, PlayState* play) { // MM's func_80BFC078
    SkelAnime_Update(&this->skelAnime);
    if ((Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && Message_ShouldAdvance(play)) {
        u16 cur = play->msgCtx.textId - NV_MM_TEXT(0);

        if ((cur == 0x2927) || (cur == 0x2928)) {
            Message_ContinueTextbox(play, NV_MM_TEXT(cur + 1));
            NvMm_SetWeekEvent(WE_77_04);
        } else {
            Message_CloseTextbox(play);
            this->actionFunc = NvMmRz_Dance;
            NvMmRz_ChangeAnim(this, RZ_ANIM_DANCE, ANIMMODE_LOOP, -5.0f);
            if (this->sister != NULL) {
                NvMmRz_ChangeAnim(this->sister, RZ_ANIM_DANCE, ANIMMODE_LOOP, -5.0f);
            }
        }
    }
}

static void NvMmRz_GiveHeartPiece(NvMmRz* this, PlayState* play) { // MM's func_80BFC270
    SkelAnime_Update(&this->skelAnime);
    if (Actor_HasParent(&this->actor, play)) {
        this->actor.parent = NULL;
        NvMmRz_Notebook();
        Message_StartTextbox(play, NV_MM_TEXT(0x2925), &this->actor);
        this->actionFunc = NvMmRz_DanceTalk;
    } else {
        GiveItemEntryFromActor(&this->actor, play, NvMmGetItem_Entry(MM_GI_HEART_PIECE), 2000.0f, 1000.0f);
    }
}

static void NvMmRz_Dance(NvMmRz* this, PlayState* play) { // MM's func_80BFC3F8
    SkelAnime_Update(&this->skelAnime);
    if (this->sister == NULL) {
        this->sister = NvMmRz_FindSister(this, play);
    }
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->actionFunc = NvMmRz_DanceTalk;
        Message_StartTextbox(play,
                             NV_MM_TEXT(((NvMmMask_Worn() == MM_PLAYER_MASK_KAMARO) && NvMm_CheckWeekEvent(WE_77_04))
                                            ? 0x2925
                                            : 0x2926),
                             &this->actor);
        NvMmRz_Notebook();
    } else if (NvMmRz_CanTalk(this, play) || ((this->actor.xzDistToPlayer < 100.0f) && NvMmMask_Dancing(play))) {
        if (NvMmMask_Dancing(play) && !NvMm_CheckWeekEvent(WE_77_04) && (this->sister != NULL)) {
            // Kamaro's dance shown them: they kneel, ask him, and the heart piece (once)
            NvMm_SetWeekEvent(WE_77_04);
            NvMmRz_ChangeAnim(this, RZ_ANIM_APPLAUDING, ANIMMODE_LOOP, -5.0f);
            if (!NvMm_CheckWeekEvent(WE_RECEIVED_ROSA_SISTERS_HP)) {
                NvMm_SetWeekEvent(WE_RECEIVED_ROSA_SISTERS_HP);
                this->actionFunc = NvMmRz_GiveHeartPiece;
            }
        } else {
            Actor_OfferTalk(&this->actor, play, 120.0f);
        }
    }
}

/* ---- type 1: sitting, after ---- */

static void NvMmRz_SitTalk(NvMmRz* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if ((Message_GetState(&play->msgCtx) == TEXT_STATE_EVENT) && Message_ShouldAdvance(play)) {
        Message_CloseTextbox(play);
        this->actionFunc = NvMmRz_Sit;
    }
}

static void NvMmRz_Sit(NvMmRz* this, PlayState* play) {
    SkelAnime_Update(&this->skelAnime);
    if (Actor_ProcessTalkRequest(&this->actor, play)) {
        this->actionFunc = NvMmRz_SitTalk;
        Message_StartTextbox(play, NV_MM_TEXT((NvMmMask_Worn() == MM_PLAYER_MASK_KAMARO) ? 0x2925 : 0x2924),
                             &this->actor);
    } else if (NvMmRz_CanTalk(this, play)) {
        Actor_OfferTalk(&this->actor, play, 120.0f);
    }
}

void NvMmRz_Update(Actor* thisx, PlayState* play) {
    NvMmRz* this = (NvMmRz*)thisx;

    Collider_UpdateCylinder(thisx, &this->collider);
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    Actor_MoveXZGravity(thisx);
    Actor_UpdateBgCheckInfo(play, thisx, 40.0f, 25.0f, 40.0f, 5);
    this->actionFunc(this, play);
    if (DECR(this->blinkTimer) == 0) {
        this->blinkTimer = Rand_S16Offset(60, 60);
    }
    this->eyeIndex = (this->blinkTimer > 2) ? 0 : this->blinkTimer;
}

static void NvMmRz_PostLimb(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    static Vec3f sFocusOffset = { 500.0f, -500.0f, 0.0f };

    if (limbIndex == RZ_LIMB_0B) {
        Matrix_MultVec3f(&sFocusOffset, &((Actor*)thisx)->focus.pos);
    }
}

void NvMmRz_Draw(Actor* thisx, PlayState* play) {
    NvMmRz* this = (NvMmRz*)thisx;
    s32 eye = (this->animIndex == RZ_ANIM_APPLAUDING) ? 4 : (this->stateFlags & RZ_STATE_THINKING) ? 3 : this->eyeIndex;

    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    // (their colours: the animated materials at step 0, Judo's red, or 1, Marilla's blue)
    NvMmMatAnim_DrawStep(play, "mm/objects/object_rz/object_rz_Matanimheader_00D768",
                         (RZ_SISTER(thisx) == RZ_JUDO) ? 0 : 1);
    gSPSegment(POLY_OPA_DISP++, 0x09, SEGMENTED_TO_VIRTUAL(sEyes[eye]));
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount, NULL,
                          NvMmRz_PostLimb, &this->actor);
    CLOSE_DISPS(play->state.gfxCtx);
}
