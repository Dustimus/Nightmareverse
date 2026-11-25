/*
 * Nightmareverse: the jugglers (MM's En_Ja, MM id 0x244), the two brothers who stand by the Laundry
 * Pool's way in East Clock Town, ported closely (mm/README.md "The townsfolk"): MM's model (object_boj,
 * their colours set by the actor: params & 3 picks red or blue), MM's schedule (mm/schedules/En_Ja.schl:
 * days 1 and 2, by day in East Clock Town juggling to each other, at night at the inn practising card
 * tricks; four balls thrown between them on MM's arcs), MM's message scripts (nv_mm_msgscripts/En_Ja:
 * each brother speaks in turn, MM's FOCUS_TO_CHILD to the other). They're gone on day 3.
 * Adapted: their heads turning to Link while he talks (MM's SubS_UpdateLimb transform draw) are left
 * out, as for the inn's Goron; the card held at night is drawn as MM draws it.
 */
#include "global.h"
#include "z_nv_mm_schedule.h"
#include "z_nv_mm_msgevent.h"
#include <stdio.h>
#include <stdlib.h>

#define BOJ(name) ("__OTR__mm/objects/object_boj/" name)
#define BOJ_LIMB_0B 11
#define BOJ_LIMB_0E 14
#define BOJ_LIMB_0F 15
#define BOJ_LIMB_MAX 16
#define ENJA_BLUE(thisx) (((thisx)->params & 3) != 0)

#include "nv_mm_schedules/En_Ja.schl.inc"
#include "nv_mm_msgscripts/En_Ja.msg.inc"

s32 NvMmActor_Id(s32 mmId);

typedef struct {
    Vec3f pos;
    s16 color;
    s16 frame;
} NvMmJaBall;

struct NvMmJa;
typedef void (*NvMmJaFunc)(struct NvMmJa*, PlayState*);

typedef struct NvMmJa {
    Actor actor;
    SkelAnime skelAnime;
    NvMmJaFunc func;
    ColliderCylinder collider;
    u8 scheduleResult;
    s32 msgScriptPos;
    Vec3f handA0, handA1, handB0, handB1; // MM's unk_204/210 (his hands), unk_21C/228 (his brother's)
    NvMmJaBall balls[4];
    Vec3s jointTable[BOJ_LIMB_MAX];
    Vec3s morphTable[BOJ_LIMB_MAX];
    s32 offer;       // talking can start
    s32 holdCard;    // MM's unk_340 & 0x40: at night, a card in hand
    s16 eyeTimer;    // MM's unk_360 / unk_362: blinking
    s16 eyeIndex;
    s16 handBall0;   // MM's unk_364 / unk_366: the colours of the balls in his hands
    s16 handBall1;
    s32 animIndex;
} NvMmJa;

const size_t gNvMmJaSize = sizeof(NvMmJa);

static ColliderCylinderInit sCylinderInit = {
    { COLTYPE_HIT1, AT_NONE, AC_NONE, OC1_ON | OC1_TYPE_ALL, OC2_TYPE_1, COLSHAPE_CYLINDER },
    { ELEMTYPE_UNK1, { 0x00000000, 0x00, 0x00 }, { 0x00000000, 0x00, 0x00 }, TOUCH_NONE | TOUCH_SFX_NORMAL, BUMP_NONE,
      OCELEM_ON },
    { 12, 64, 0, { 0, 0, 0 } },
};

static struct {
    const char* anim;
    s8 morph;
} sAnims[] = {
    { BOJ("object_boj_Anim_002734"), 0 },  { BOJ("object_boj_Anim_0033B0"), 0 }, { BOJ("object_boj_Anim_002734"), -4 },
    { BOJ("object_boj_Anim_0033B0"), -4 }, { BOJ("object_boj_Anim_004078"), 0 }, { BOJ("object_boj_Anim_005CE4"), 0 },
};
static const char* sEyes[] = { BOJ("object_boj_Tex_0062B0"), BOJ("object_boj_Tex_0063B0"),
                               BOJ("object_boj_Tex_0064B0"), BOJ("object_boj_Tex_0063B0") };

static void NvMmJa_FollowSchedule(NvMmJa* this, PlayState* play);

static void NvMmJa_ChangeAnim(NvMmJa* this, s32 i) {
    AnimationHeader* a;

    if (this->animIndex == i) {
        return;
    }
    this->animIndex = i;
    a = (AnimationHeader*)sAnims[i].anim;
    Animation_Change(&this->skelAnime, a, 1.0f, 0.0f, Animation_GetLastFrame(a), ANIMMODE_LOOP, sAnims[i].morph);
}

/** MM's func_80BC1AE0: his brother, the nearest juggler, faced. */
static Actor* NvMmJa_FindBrother(NvMmJa* this, PlayState* play) {
    Actor* best = NULL;
    Actor* a = play->actorCtx.actorLists[ACTORCAT_NPC].head;
    f32 bestDist = 1e9f;

    for (; a != NULL; a = a->next) {
        if ((a != &this->actor) && (a->id == this->actor.id)) {
            f32 d = Actor_WorldDistXYZToActor(&this->actor, a);

            if (d < bestDist) {
                bestDist = d;
                best = a;
            }
        }
    }
    this->actor.child = best;
    if (best != NULL) {
        this->actor.world.rot.y = Math_Vec3f_Yaw(&this->actor.world.pos, &best->world.pos);
    }
    return best;
}

/** MM's func_80BC2EA4: the balls' throws, hand to hand. */
static void NvMmJa_SetupBalls(NvMmJa* this) {
    static Vec3f sHandA0 = { -10.0f, 56.0f, 25.0f };
    static Vec3f sHandA1 = { 0.0f, 60.0f, 20.0f };
    static Vec3f sHandB0 = { 10.0f, 60.0f, 25.0f };
    static Vec3f sHandB1 = { 0.0f, 58.0f, 20.0f };
    f32 s0 = Math_SinS(this->actor.shape.rot.y), c0 = Math_CosS(this->actor.shape.rot.y);
    f32 s1 = Math_SinS(this->actor.child->shape.rot.y), c1 = Math_CosS(this->actor.child->shape.rot.y);
    s32 i;

    for (i = 0; i < 4; i++) {
        this->balls[i].frame = i * 0x30;
        this->balls[i].color = i;
    }
    this->handA0 = this->handA1 = this->actor.world.pos;
    this->handB0 = this->handB1 = this->actor.child->world.pos;
    this->handA0.x += (sHandA0.z * s0) + (sHandA0.x * c0);
    this->handA0.y += sHandA0.y;
    this->handA0.z += (sHandA0.z * c0) - (sHandA0.x * s0);
    this->handA1.x += (sHandA1.z * s0) + (sHandA1.x * c0);
    this->handA1.y += sHandA1.y;
    this->handA1.z += (sHandA1.z * c0) - (sHandA1.x * s0);
    this->handB0.x += (sHandB0.z * s1) + (sHandB0.x * c1);
    this->handB0.y += sHandB0.y;
    this->handB0.z += (sHandB0.z * c1) - (sHandB0.x * s1);
    this->handB1.x += (sHandB1.z * s1) + (sHandB1.x * c1);
    this->handB1.y += sHandB1.y;
    this->handB1.z += (sHandB1.z * c1) - (sHandB1.x * s1);
    this->handBall0 = 2;
    this->handBall1 = 0;
}

/** MM's func_80BC3154 and func_80BC32D8: each ball along its arc, the hands' balls on the beat. */
static void NvMmJa_UpdateBalls(NvMmJa* this) {
    s32 i;

    for (i = 0; i < 4; i++) {
        NvMmJaBall* b = &this->balls[i];

        if ((b->frame >= 18) && (b->frame < 83)) {
            f32 t = b->frame - 18;

            b->pos.x = (((65.0f - t) * this->handA0.x) + (t * this->handB1.x)) / 65.0f;
            b->pos.y = (((65.0f - t) * this->handA0.y) + (t * this->handB1.y)) / 65.0f;
            b->pos.z = (((65.0f - t) * this->handA0.z) + (t * this->handB1.z)) / 65.0f;
            t -= 32.5f;
            b->pos.y += -0.11360947f * SQ(t) + 120.0f;
        } else if ((b->frame >= 102) && (b->frame < 191)) {
            f32 t = b->frame - 102;

            b->pos.x = (((89.0f - t) * this->handB0.x) + (t * this->handA1.x)) / 89.0f;
            b->pos.y = (((89.0f - t) * this->handB0.y) + (t * this->handA1.y)) / 89.0f;
            b->pos.z = (((89.0f - t) * this->handB0.z) + (t * this->handA1.z)) / 89.0f;
            t -= 44.5f;
            b->pos.y += -0.08079788f * SQ(t) + 160.0f;
        }
        b->frame = (b->frame != 191) ? (b->frame + 1) : 0;
    }
    if (Animation_OnFrame(&this->skelAnime, this->skelAnime.endFrame)) {
        this->handBall1 = (this->handBall1 > 0) ? (this->handBall1 - 1) : 3;
    } else if (Animation_OnFrame(&this->skelAnime, 34.0f)) {
        this->handBall0 = (this->handBall0 > 0) ? (this->handBall0 - 1) : 3;
    }
}

static void NvMmJa_BallColor(Gfx** gfx, s32 c) {
    static const u8 sColors[][3] = { { 0, 0, 255 }, { 255, 0, 0 }, { 0, 0, 255 }, { 255, 255, 0 } };
    const u8* k = ((c >= 0) && (c < 4)) ? sColors[c] : (const u8*)"\xFF\xFF\xFF";

    gDPSetPrimColor((*gfx)++, 0, 0, k[0], k[1], k[2], 255);
}

void NvMmJa_Init(Actor* thisx, PlayState* play) {
    NvMmJa* this = (NvMmJa*)thisx;

    ActorShape_Init(&thisx->shape, 0.0f, ActorShadow_DrawCircle, 18.0f);
    SkelAnime_InitFlex(play, &this->skelAnime, (FlexSkeletonHeader*)BOJ("object_boj_Skel_00C240"), NULL,
                       this->jointTable, this->morphTable, BOJ_LIMB_MAX);
    this->animIndex = -1;
    NvMmJa_ChangeAnim(this, 0);
    Collider_InitCylinder(play, &this->collider);
    Collider_SetCylinder(play, &this->collider, thisx, &sCylinderInit);
    thisx->colChkInfo.mass = MASS_IMMOVABLE;
    Actor_SetScale(thisx, 0.01f);
    thisx->targetMode = 0;
    thisx->uncullZoneForward = 800.0f;
    thisx->gravity = 0.0f;
    this->scheduleResult = 0;
    this->func = NvMmJa_FollowSchedule;
}

void NvMmJa_Destroy(Actor* thisx, PlayState* play) {
    Collider_DestroyCylinder(play, &((NvMmJa*)thisx)->collider);
}

/** MM's func_80BC20D0: what the schedule's result has him doing. */
static s32 NvMmJa_StartResult(NvMmJa* this, PlayState* play, u8 result) {
    this->offer = false;
    this->holdCard = false;
    if (NvMmJa_FindBrother(this, play) == NULL) {
        return false;
    }
    this->offer = true;
    if (result == 1) { // day: juggling together
        NvMmJa_ChangeAnim(this, 5);
        NvMmJa_SetupBalls(this);
    } else { // night: at the inn, a card in hand
        NvMmJa_ChangeAnim(this, ENJA_BLUE(&this->actor) ? 4 : 1);
        this->actor.shape.shadowDraw = NULL;
        this->holdCard = true;
    }
    return true;
}

static void NvMmJa_FollowSchedule(NvMmJa* this, PlayState* play) {
    NvMmScheduleOutput out;

    if (!NvMmSchedule_RunScript(play, D_80BC35F0, &out) ||
        ((this->scheduleResult != out.result) && !NvMmJa_StartResult(this, play, out.result))) {
        this->actor.shape.shadowDraw = NULL;
        this->actor.flags &= ~ACTOR_FLAG_ATTENTION_ENABLED;
        out.result = 0;
    } else {
        if (out.result != 2) {
            this->actor.shape.shadowDraw = ActorShadow_DrawCircle;
        }
        this->actor.flags |= ACTOR_FLAG_ATTENTION_ENABLED;
    }
    this->scheduleResult = out.result;
    Math_ApproachS(&this->actor.shape.rot.y, this->actor.world.rot.y, 4, 0x1554);
}

static MsgScript* NvMmJa_GetMsgScript(NvMmJa* this) {
    if (this->scheduleResult == 2) {
        return ENJA_BLUE(&this->actor) ? D_80BC366C : D_80BC362C;
    }
    return ENJA_BLUE(&this->actor) ? D_80BC364C : D_80BC360C;
}

static void NvMmJa_Talk(NvMmJa* this, PlayState* play) {
    if (NvMmMsgEvent_RunScript(&this->actor, play, NvMmJa_GetMsgScript(this), NULL, &this->msgScriptPos)) {
        this->offer = true;
        this->msgScriptPos = 0;
        this->actor.shape.rot.y = this->actor.world.rot.y;
        NvMmJa_FindBrother(this, play);
        this->func = NvMmJa_FollowSchedule;
    }
}

void NvMmJa_Update(Actor* thisx, PlayState* play) {
    NvMmJa* this = (NvMmJa*)thisx;

    if (this->offer && Actor_ProcessTalkRequest(thisx, play)) {
        this->offer = false;
        this->func = NvMmJa_Talk;
    }
    this->func(this, play);
    if (this->scheduleResult == 0) {
        return;
    }
    SkelAnime_Update(&this->skelAnime);
    if (DECR(this->eyeTimer) == 0) { // MM's func_80BC1A68
        if (++this->eyeIndex >= 4) {
            this->eyeTimer = Rand_S16Offset(30, 30);
            this->eyeIndex = 0;
        }
    }
    if (this->offer) {
        Actor_OfferTalk(thisx, play, this->collider.dim.radius + 30.0f);
    }
    Collider_UpdateCylinder(thisx, &this->collider);
    this->collider.dim.height = (s16)fabsf(thisx->focus.pos.y - thisx->world.pos.y) + 5;
    CollisionCheck_SetOC(play, &play->colChkCtx, &this->collider.base);
    if (this->scheduleResult == 1) {
        NvMmJa_UpdateBalls(this);
    }
}

static void NvMmJa_PostLimb(PlayState* play, s32 limbIndex, Gfx** dList, Vec3s* rot, void* thisx) {
    static Vec3f sFocus = { 600.0f, 0.0f, 0.0f };
    static Vec3f sCardPos = { -87.0f, 444.0f, -49.0f };
    static Vec3s sCardRot = { 0x7770, -0x4BC, -0x251C };
    static Vec3f sHand0 = { 400.0f, 0.0f, -400.0f };
    static Vec3f sHand1 = { 400.0f, 0.0f, 400.0f };
    NvMmJa* this = (NvMmJa*)thisx;
    f32 frame = this->skelAnime.curFrame;

    OPEN_DISPS(play->state.gfxCtx);
    if (limbIndex == BOJ_LIMB_0F) {
        Matrix_MultVec3f(&sFocus, &this->actor.focus.pos);
        this->actor.focus.rot = this->actor.world.rot;
    } else if (this->holdCard && (limbIndex == BOJ_LIMB_0B)) { // (the card, at night)
        Matrix_Push();
        Matrix_TranslateRotateZYX(&sCardPos, &sCardRot);
        gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(POLY_OPA_DISP++, (Gfx*)BOJ("object_boj_DL_00BA30"));
        Matrix_Pop();
    }
    if (this->scheduleResult == 1) { // the balls in his hands, on the frames he holds them
        s32 hand0 = (limbIndex == BOJ_LIMB_0B) && (((frame >= 0.0f) && (frame <= 6.0f)) || ((frame >= 35.0f) && (frame <= 47.0f)));
        s32 hand1 = (limbIndex == BOJ_LIMB_0E) && (frame >= 0.0f) && (frame <= 18.0f);

        if (hand0 || hand1) {
            Vec3f* p = hand0 ? &sHand0 : &sHand1;

            Matrix_Push();
            Matrix_Translate(p->x, p->y, p->z, MTXMODE_APPLY);
            Matrix_ReplaceRotation(&play->billboardMtxF);
            gDPPipeSync(POLY_OPA_DISP++);
            NvMmJa_BallColor(&POLY_OPA_DISP, hand0 ? this->handBall0 : this->handBall1);
            gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            gSPDisplayList(POLY_OPA_DISP++, (Gfx*)BOJ("object_boj_DL_00BCC8"));
            Matrix_Pop();
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);
}

void NvMmJa_Draw(Actor* thisx, PlayState* play) {
    static Color_RGBA8 sClothes[] = { { 200, 0, 80, 0 }, { 0, 130, 220, 0 } };
    NvMmJa* this = (NvMmJa*)thisx;
    s32 k = ENJA_BLUE(thisx) ? 1 : 0;
    s32 i;

    if (this->scheduleResult == 0) {
        return;
    }
    OPEN_DISPS(play->state.gfxCtx);
    Gfx_SetupDL_25Opa(play->state.gfxCtx);
    gSPSegment(POLY_OPA_DISP++, 0x08, Gfx_EnvColor(play->state.gfxCtx, sClothes[k].r, sClothes[k].g, sClothes[k].b, 0));
    gSPSegment(POLY_OPA_DISP++, 0x09, Gfx_EnvColor(play->state.gfxCtx, 255, 255, 255, 0));
    gSPSegment(POLY_OPA_DISP++, 0x0A, SEGMENTED_TO_VIRTUAL(sEyes[this->eyeIndex & 3]));
    gDPPipeSync(POLY_OPA_DISP++);
    SkelAnime_DrawFlexOpa(play, this->skelAnime.skeleton, this->skelAnime.jointTable, this->skelAnime.dListCount, NULL,
                          NvMmJa_PostLimb, thisx);
    if (this->scheduleResult == 1) { // MM's func_80BC3594: the balls in the air
        for (i = 0; i < 4; i++) {
            NvMmJaBall* b = &this->balls[i];

            if (((b->frame < 0) || (b->frame >= 19)) && ((b->frame < 83) || (b->frame >= 103))) {
                Matrix_Translate(b->pos.x, b->pos.y, b->pos.z, MTXMODE_NEW);
                Matrix_Mult(&play->billboardMtxF, MTXMODE_APPLY);
                Matrix_Scale(0.01f, 0.01f, 0.01f, MTXMODE_APPLY);
                NvMmJa_BallColor(&POLY_OPA_DISP, b->color);
                gSPMatrix(POLY_OPA_DISP++, MATRIX_NEWMTX(play->state.gfxCtx), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(POLY_OPA_DISP++, (Gfx*)BOJ("object_boj_DL_00BCC8"));
            }
        }
    }
    CLOSE_DISPS(play->state.gfxCtx);
}
