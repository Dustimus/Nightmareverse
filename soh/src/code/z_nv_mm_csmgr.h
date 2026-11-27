#ifndef Z_NV_MM_CSMGR_H
#define Z_NV_MM_CSMGR_H
/*
 * Nightmareverse: MM's actor cutscenes and their cameras (z_nv_mm_csmgr.c): MM's CutsceneManager_*
 * as NvMmCsMgr_*, and what MM keeps on the actor (actor->csId) or the camera.
 */
#include "global.h"

#define NV_MM_CS_ID_NONE -1
#define NV_MM_CS_ID_GLOBAL_TALK 124
#define NV_MM_CS_ID_GLOBAL_END 127

void NvMmCsMgr_Queue(s16 csId);
s16 NvMmCsMgr_IsNext(s16 csId);
s16 NvMmCsMgr_Start(s16 csId, Actor* actor);
s16 NvMmCsMgr_StartWithPlayerCs(s16 csId, Actor* actor);
s16 NvMmCsMgr_StartWithPlayerCsAndSetFlag(s16 csId, Actor* actor);
s16 NvMmCsMgr_Stop(s16 csId);
s16 NvMmCsMgr_GetCurrentCsId(void);
s16 NvMmCsMgr_GetAdditionalCsId(s16 csId);
s16 NvMmCsMgr_GetLength(s16 csId);
s16 NvMmCsMgr_GetCutsceneCustomValue(s16 csId);
s16 NvMmCsMgr_GetCurrentSubCamId(s16 csId);

s32 NvMmCs_ActorCsId(Actor* actor);              // MM's actor->csId
void NvMmCs_SetActorCsId(Actor* actor, s32 csId); // (an actor spawned with one, MM's Actor_SpawnAsChildAndCutscene)
void NvMmCs_SetTargetActor(Actor* actor);         // MM's Camera_SetTargetActor on the cutscene's camera
void NvMmCs_MainCameraFromCutscene(PlayState* play, s16 csId, Actor* target);

#endif
