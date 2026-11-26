/*
 * Nightmareverse: following the Bremen march (mm/README.md "The Bremen march and Kamaro's dance").
 *
 * In MM a dog (En_Dg) and Grog's chicks (En_Nwc) fall in behind Link while he marches with the Bremen
 * Mask (z_nv_mm_march.c). Here OoT's animals do too: a dog (En_Dog) or a cucco (En_Niw) near Link
 * when he starts marching (or as he passes) joins the line behind him, each following the one ahead
 * (Link for the first), walking while it's further than its gap and waiting when close; when the
 * march stops they go back to what they were doing. OoT's actors ask NvMarch_Follow each frame from
 * their update (a few lines each in z_en_dog.c and z_en_niw.c); MM's own followers can too.
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>

s32 NvMmMask_Marching(PlayState* play);

#define NV_MARCH_MAX 8
#define NV_MARCH_JOIN_DIST 300.0f

static Actor* sLine[NV_MARCH_MAX];
static s32 sLineLen;
static u32 sLineFrame; // the frame the line was last checked (a stale line from an ended march is dropped)

/** Whether `actor` is still one of the scene's actors (the line keeps pointers). */
static s32 NvMarch_Alive(PlayState* play, Actor* actor) {
    Actor* a = play->actorCtx.actorLists[actor->category].head;

    while (a != NULL) {
        if (a == actor) {
            return true;
        }
        a = a->next;
    }
    return false;
}

static void NvMarch_Clean(PlayState* play) {
    s32 i;
    s32 n = 0;

    if (!NvMmMask_Marching(play) || ((play->gameplayFrames - sLineFrame) > 2)) {
        sLineLen = 0;
    }
    for (i = 0; i < sLineLen; i++) {
        if (NvMarch_Alive(play, sLine[i])) {
            sLine[n++] = sLine[i];
        }
    }
    sLineLen = n;
    sLineFrame = play->gameplayFrames;
}

/**
 * `actor` following the march, `gap` behind the one ahead. Returns 0 when it isn't (it goes on as it
 * would), 1 walking (its speed and heading set), 2 close enough, waiting.
 */
s32 NvMarch_Follow(PlayState* play, Actor* actor, f32 gap) {
    Player* player = GET_PLAYER(play);
    Actor* leader;
    s32 i;
    f32 dist;
    f32 speed;

    NvMarch_Clean(play);
    if (!NvMmMask_Marching(play)) {
        return 0;
    }
    for (i = 0; i < sLineLen; i++) {
        if (sLine[i] == actor) {
            break;
        }
    }
    if (i == sLineLen) { // not in the line yet: near enough, it joins at the end
        if ((sLineLen >= NV_MARCH_MAX) || (actor->xzDistToPlayer > NV_MARCH_JOIN_DIST)) {
            return 0;
        }
        sLine[sLineLen++] = actor;
        if (getenv("NV_LOG") != NULL) {
            printf("NVMM march: actor %04X follows (%d in line)\n", actor->id, (int)sLineLen);
            fflush(stdout);
        }
    }
    leader = (i == 0) ? &player->actor : sLine[i - 1];
    dist = Math_Vec3f_DistXZ(&actor->world.pos, &leader->world.pos);
    if (dist <= gap) {
        Math_ApproachF(&actor->speedXZ, 0.0f, 0.5f, 1.0f);
        return 2;
    }
    speed = CLAMP((dist - gap) * 0.15f, 1.0f, 6.0f); // (keeping up with the march, faster when left behind)
    Math_ApproachF(&actor->speedXZ, speed, 0.6f, 1.0f);
    Math_SmoothStepToS(&actor->world.rot.y, Math_Vec3f_Yaw(&actor->world.pos, &leader->world.pos), 5, 0x800, 1);
    actor->shape.rot.y = actor->world.rot.y;
    return 1;
}
