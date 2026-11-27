/*
 * Nightmareverse: MM's actor cutscenes and their cameras (mm/README.md "MM's cutscene cameras"),
 * from MM's z_eventmgr.c (CutsceneManager) and the camera functions its cameras use (z_camera.c),
 * zeldaret/mm.
 *
 * In MM an actor wanting the camera queues its cutscene (its csId from the scene's actor entry, or
 * one of the actor's additional ones), waits until it's next (one at a time, by priority), starts it
 * and stops it; the cutscene's entry (the scene's list, command 0x1B) names a camera from the
 * scene's list (command 0x02): a setting and its data, e.g. FIXED1 (a fixed shot: eye, direction,
 * fov) or CIRCLE8 (a fixed eye turning to follow the actor the camera targets). MM's manager makes a
 * sub-camera for it, and on the stop copies it back to the camera it came from.
 *
 * Here: the manager is MM's, line for line (NvMmCsMgr_*, the same names after the prefix); the scene's
 * lists come from mm_scene.py ("<scene>_scene_csmgr"), each placed actor's csId from "<room>_csids"
 * (set as z_nv_mm_halfday.c spawns it, read with NvMmCs_ActorCsId). The sub-camera is OoT's
 * (Play_CreateSubCamera, set to OoT's FREE0 so OoT's camera leaves it be) and driven each frame from
 * here with MM's own camera math for the setting: Camera_Fixed1 (FIXED1, FIXED2, RFIXED1) and
 * Camera_Fixed2 (CIRCLE2, CIRCLE3, CIRCLE8). Any other setting (FREE0 included) leaves the camera
 * where it was for the actor to place (Play_CameraSetAtEye), as MM's Camera_Unique6 does.
 * The entrance views too: a spawn's camera (START0/START2) given to the main camera as the scene
 * starts, with MM's Camera_Unique0.
 * Not yet: MM's scripted cutscenes (scriptIndex: MM's cutscene camera splines), spawns on other
 * kinds of camera, MM's own camera collision check (Camera_BgCheck), the camera roll.
 */
#include "global.h"
#include "soh/ObjectExtension/ActorListIndex.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define CS_ID_NONE -1
#define CS_ID_GLOBAL_78 120
#define CS_ID_GLOBAL_TALK 124
#define CS_ID_GLOBAL_DOOR 125
#define CS_ID_GLOBAL_RETURN_TO_CAM 126
#define CS_ID_GLOBAL_END 127
#define CS_SCRIPT_ID_NONE -1
#define CS_CAM_ID_NONE -1
#define SUB_CAM_ID_DONE 0

// MM's camera settings (z64camera.h) the scenes' actor cutscene cameras use
#define MM_CAM_SET_FREE0 0x0A
#define MM_CAM_SET_FIXED1 0x1F
#define MM_CAM_SET_FIXED2 0x20
#define MM_CAM_SET_CIRCLE2 0x24
#define MM_CAM_SET_CIRCLE3 0x25
#define MM_CAM_SET_RFIXED1 0x38
#define MM_CAM_SET_CIRCLE8 0x3B
#define MM_CAM_SET_START0 0x2C
#define MM_CAM_SET_START2 0x2D

#define UNIQUE0_FLAG_0 (1 << 0)
#define UNIQUE0_FLAG_1 (1 << 1)
#define UNIQUE0_FLAG_4 (1 << 4)

#define FIXED1_FLAG_4 (1 << 4)
#define FIXED2_FLAG_0 (1 << 0)
#define FIXED2_FLAG_3 (1 << 3)
#define FIXED2_FLAG_5 (1 << 5)

typedef struct {
    s16 priority; // lower number, higher priority; -1 always next
    s16 length;   // frames, -1 until stopped
    s16 csCamId;
    s16 scriptIndex;
    s16 additionalCsId;
    u8 endSfx;
    u8 customValue;
    s16 hudVisibility;
    u8 endCam;
    u8 letterboxSize;
} NvMmActorCutscene; // MM's ActorCutscene, 16 bytes

typedef struct {
    s16 setting;
    s16 count;
    s32 first; // its first Vec3s in the data
} NvMmCsCam;

static const NvMmActorCutscene sGlobalCutsceneList[] = {
    { -100, -1, CS_CAM_ID_NONE, CS_SCRIPT_ID_NONE, CS_ID_NONE, 0xFF, 255, -1, 255, 255 },   // 0x78
    { -100, -1, CS_CAM_ID_NONE, CS_SCRIPT_ID_NONE, CS_ID_NONE, 0xFF, 255, -1, 255, 255 },   // 0x79
    { -100, -1, CS_CAM_ID_NONE, CS_SCRIPT_ID_NONE, CS_ID_NONE, 0xFF, 255, -1, 255, 255 },   // 0x7A
    { 2, -1, -25, CS_SCRIPT_ID_NONE, CS_ID_NONE, 0xFF, 255, 0, 0, 32 },                     // ELEGY
    { 32765, -1, CS_CAM_ID_NONE, CS_SCRIPT_ID_NONE, CS_ID_NONE, 0xFF, 255, -1, 0, 255 },    // TALK
    { 32764, -1, CS_CAM_ID_NONE, CS_SCRIPT_ID_NONE, CS_ID_NONE, 0xFF, 255, -1, 0, 255 },    // DOOR
    { 32766, -2, -2, CS_SCRIPT_ID_NONE, CS_ID_NONE, 0, 255, -1, 0, 32 },                    // RETURN_TO_CAM
    { 0, -1, CS_CAM_ID_NONE, CS_SCRIPT_ID_NONE, CS_ID_NONE, 0, 255, -1, 0, 32 },            // END
};

// the scene's lists ("MCSM", mm_scene.py)
static const NvMmActorCutscene* sSceneCutsceneList;
static s16 sSceneCutsceneCount;
static const NvMmCsCam* sCsCams;
static s32 sCsCamCount;
static const Vec3s* sCsCamData;
static const u8* sSpawnCams; // each spawn's camera (MM's player params & 0xFF; 0xFF none)
static s32 sSpawnCamCount;
static u32 sLastFrames;
static s16 sLoadedScene = -1;
static PlayState* sLoadedPlay;

static u8 sWaitingCutsceneList[16];
static u8 sNextCutsceneList[16];

static struct {
    s16 csId;
    s16 length;
    s16 endCsId;
    s16 subCamId;
    Actor* targetActor;
    s32 startMethod;
    PlayState* play;
    s16 retCamId;
} sCsMgr = { CS_ID_NONE, 0, CS_ID_NONE, SUB_CAM_ID_DONE, NULL, 0, NULL, CAM_ID_MAIN };

enum { CS_START_0, CS_START_1, CS_START_2 };

/* ---------------------------------------------------------------------------------------------- */
/* Each placed actor's csId                                                                        */

#define NV_CS_ACTORS 96
static struct {
    Actor* actor;
    s16 csId;
} sActorCsIds[NV_CS_ACTORS];

static const u8* sRoomCsIds;
static s32 sRoomCsIdCount;
static s32 sRoomCsIdRoom = -1;
static s16 sRoomCsIdScene = -1;

static s32 NvMmCs_ScenePath(PlayState* play, char* out, size_t size, const char* suffix, s32 room) {
    const char* file;
    size_t len;

    if ((play->loadedScene == NULL) || ((file = play->loadedScene->sceneFile.fileName) == NULL)) {
        return false;
    }
    len = strlen(file);
    if ((len <= 6) || (strcmp(file + len - 6, "_scene") != 0)) {
        return false;
    }
    if (room >= 0) {
        snprintf(out, size, "scenes/shared/%s/%.*s_room_%02d_%s", file, (int)(len - 6), file, (int)room, suffix);
    } else {
        snprintf(out, size, "scenes/shared/%s/%s_%s", file, file, suffix);
    }
    return ResourceMgr_FileExists(out);
}

/** The csId of room entry i (MM's actor entry rot.y & 0x7F), CS_ID_NONE if none. */
s32 NvMmCs_RoomCsId(PlayState* play, s32 i) {
    char path[256];
    s32 id;

    static PlayState* sRoomCsIdPlay;

    if ((sRoomCsIdScene != play->sceneNum) || (sRoomCsIdPlay != play)) { // a new scene: its actors anew
        memset(sActorCsIds, 0, sizeof(sActorCsIds));
        sRoomCsIdPlay = play;
        sRoomCsIdRoom = -1;
    }
    if ((sRoomCsIdRoom != play->roomCtx.curRoom.num) || (sRoomCsIdScene != play->sceneNum)) {
        const u8* data = NULL;

        sRoomCsIds = NULL;
        sRoomCsIdCount = 0;
        sRoomCsIdRoom = play->roomCtx.curRoom.num;
        sRoomCsIdScene = play->sceneNum;
        if (NvMmCs_ScenePath(play, path, sizeof(path), "csids", sRoomCsIdRoom)) {
            data = (const u8*)ResourceGetDataByName(path);
        }
        if ((data != NULL) && (memcmp(data, "MCSI", 4) == 0)) {
            memcpy(&sRoomCsIdCount, data + 4, sizeof(s32));
            sRoomCsIds = data + 8;
        }
    }
    id = ((sRoomCsIds != NULL) && (i >= 0) && (i < sRoomCsIdCount)) ? sRoomCsIds[i] : 0x7F;
    return (id == 0x7F) ? CS_ID_NONE : id;
}

/** MM's actor->csId, set: as an actor is spawned from its entry (z_nv_mm_halfday.c), or by its spawner. */
void NvMmCs_SetActorCsId(Actor* actor, s32 csId) {
    s32 i;
    s32 free = -1;

    for (i = 0; i < NV_CS_ACTORS; i++) {
        if (sActorCsIds[i].actor == actor) {
            sActorCsIds[i].csId = csId;
            return;
        }
        if ((free < 0) && (sActorCsIds[i].actor == NULL)) {
            free = i;
        }
    }
    if (csId == CS_ID_NONE) {
        return;
    }
    if (free < 0) {
        free = 0; // (full: the oldest goes)
    }
    sActorCsIds[free].actor = actor;
    sActorCsIds[free].csId = csId;
}

/** MM's actor->csId. */
s32 NvMmCs_ActorCsId(Actor* actor) {
    s32 i;

    for (i = 0; i < NV_CS_ACTORS; i++) {
        if (sActorCsIds[i].actor == actor) {
            return sActorCsIds[i].csId;
        }
    }
    return CS_ID_NONE;
}

/** An actor gone: its csId too (Actor_Delete's hook would be best; called from the ports' destroys). */
void NvMmCs_ForgetActor(Actor* actor) {
    NvMmCs_SetActorCsId(actor, CS_ID_NONE);
}

/* ---------------------------------------------------------------------------------------------- */
/* The manager (MM's z_eventmgr.c)                                                                */

static const NvMmActorCutscene* NvMmCsMgr_GetCutsceneEntryImpl(s16 csId) {
    static const NvMmActorCutscene sNone = { 0, -1, CS_CAM_ID_NONE, CS_SCRIPT_ID_NONE, CS_ID_NONE, 0, 255, -1, 0, 255 };

    if (csId < CS_ID_GLOBAL_78) {
        return ((sSceneCutsceneList != NULL) && (csId >= 0) && (csId < sSceneCutsceneCount)) ? &sSceneCutsceneList[csId]
                                                                                              : &sNone;
    }
    if (csId - CS_ID_GLOBAL_78 < ARRAY_COUNT(sGlobalCutsceneList)) {
        return &sGlobalCutsceneList[csId - CS_ID_GLOBAL_78];
    }
    return &sNone;
}

static void NvMmCsDriver_Begin(PlayState* play, s16 subCamId, s16 csCamId, Actor* target);
static void NvMmCsDriver_End(void);

/** MM's CutsceneManager_Init, as a scene loads. */
static void NvMmCs_EntranceView(PlayState* play);

static void NvMmCsMgr_Init(PlayState* play) {
    char path[256];
    const u8* data = NULL;

    sSceneCutsceneList = NULL;
    sSceneCutsceneCount = 0;
    sCsCams = NULL;
    sCsCamCount = 0;
    sCsCamData = NULL;
    sSpawnCams = NULL;
    sSpawnCamCount = 0;
    if (NvMmCs_ScenePath(play, path, sizeof(path), "csmgr", -1)) {
        data = (const u8*)ResourceGetDataByName(path);
    }
    if ((data != NULL) && (memcmp(data, "MCSM", 4) == 0)) {
        s32 n;
        s32 nc;

        memcpy(&n, data + 4, 4);
        sSceneCutsceneList = (const NvMmActorCutscene*)(data + 8);
        sSceneCutsceneCount = n;
        data += 8 + 16 * n;
        memcpy(&nc, data, 4);
        sCsCams = (const NvMmCsCam*)(data + 4);
        sCsCamCount = nc;
        data += 4 + sizeof(NvMmCsCam) * nc;
        sCsCamData = (const Vec3s*)(data + 4);
        memcpy(&nc, data, 4);
        data += 4 + sizeof(Vec3s) * nc;
        memcpy(&n, data, 4);
        sSpawnCams = data + 4;
        sSpawnCamCount = n;
    }
    memset(sWaitingCutsceneList, 0, sizeof(sWaitingCutsceneList));
    memset(sNextCutsceneList, 0, sizeof(sNextCutsceneList));
    sCsMgr.endCsId = CS_ID_NONE;
    sCsMgr.play = play;
    sCsMgr.length = -1;
    sCsMgr.targetActor = NULL;
    sCsMgr.subCamId = SUB_CAM_ID_DONE;
    sCsMgr.csId = CS_ID_NONE;
    sCsMgr.startMethod = CS_START_0;
    NvMmCsDriver_End();
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM cutscenes: %d in the scene, %d cameras\n", (int)sSceneCutsceneCount, (int)sCsCamCount);
        fflush(stdout);
    }
    NvMmCs_EntranceView(play);
}

static s16 NvMmCsMgr_MarkNextCutscenes(void) {
    s16 bit;
    s32 i;
    s32 j;
    s32 count = 0;
    s16 csIdMax = CS_ID_NONE;
    s16 priorityMax = SHRT_MAX;

    for (i = 0; i < ARRAY_COUNT(sNextCutsceneList); i++) {
        for (bit = 1, j = 0; j < 8; j++) {
            if (sWaitingCutsceneList[i] & bit) {
                s16 csId = (i << 3) | j;
                s16 priority = NvMmCsMgr_GetCutsceneEntryImpl(csId)->priority;

                if (priority == -1) {
                    sNextCutsceneList[i] |= bit;
                } else if ((priority < priorityMax) && (priority > 0)) {
                    csIdMax = csId;
                    priorityMax = priority;
                }
                count++;
            }
            bit <<= 1;
        }
    }
    if (csIdMax != CS_ID_NONE) {
        sNextCutsceneList[csIdMax >> 3] |= 1 << (csIdMax & 7);
    }
    return count;
}

s16 NvMmCsMgr_Stop(s16 csId);
s16 NvMmCsMgr_StartWithPlayerCs(s16 csId, Actor* actor);
s16 NvMmCsMgr_IsNext(s16 csId);
void NvMmCsMgr_Queue(s16 csId);

/** MM's CutsceneManager_End: back to the camera it came from. */
static void NvMmCsMgr_End(void) {
    PlayState* play = sCsMgr.play;
    const NvMmActorCutscene* csEntry;

    switch (sCsMgr.startMethod) {
        case CS_START_2:
            if (sCsMgr.targetActor != NULL) {
                sCsMgr.targetActor->flags &= ~ACTOR_FLAG_UPDATE_DURING_OCARINA; // (MM's FREEZE_EXCEPTION)
            }
            // fallthrough
        case CS_START_1:
            Player_SetCsActionWithHaltedActors(play, NULL, 7); // (PLAYER_CSACTION_END)
            sCsMgr.startMethod = CS_START_0;
            break;
        default:
            break;
    }
    csEntry = NvMmCsMgr_GetCutsceneEntryImpl(sCsMgr.csId);
    if (csEntry->endSfx == 1) {
        Sfx_PlaySfxCentered(NA_SE_SY_TRE_BOX_APPEAR);
    } else if (csEntry->endSfx == 2) {
        Sfx_PlaySfxCentered(NA_SE_SY_CORRECT_CHIME);
    }
    if (sCsMgr.subCamId != SUB_CAM_ID_DONE) {
        if (csEntry->endCam != 1) { // (MM's END_CAM_0 and SMOOTH: the camera picks up from the shot)
            Camera* ret = Play_GetCamera(play, sCsMgr.retCamId);
            s16 setting = (ret != NULL) ? ret->setting : -1;
            s16 dataIdx = (ret != NULL) ? ret->camDataIdx : -1;

            Play_CopyCamera(play, sCsMgr.retCamId, sCsMgr.subCamId);
            if ((ret != NULL) && (setting > CAM_SET_NONE)) { // (its own setting and data again, not the shot's FREE0)
                s16 Camera_ChangeSettingFlags(Camera * camera, s16 setting, s16 flags);

                Camera_ChangeSettingFlags(ret, setting, 1 | 4);
                ret->camDataIdx = dataIdx;
                ret->animState = 0; // (its own camera function starts over from where the shot left it)
            }
        }
        Play_ClearCamera(play, sCsMgr.subCamId);
        Play_ChangeCameraStatus(play, sCsMgr.retCamId, CAM_STAT_ACTIVE);
    }
    NvMmCsDriver_End();
    if ((csEntry->hudVisibility != -1) && (csEntry->hudVisibility != 1)) {
        Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_ALL);
    }
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM cutscene %d ends\n", (int)sCsMgr.csId);
        fflush(stdout);
    }
    sCsMgr.csId = CS_ID_NONE;
    sCsMgr.endCsId = CS_ID_NONE;
    sCsMgr.length = -1;
    sCsMgr.targetActor = NULL;
    sCsMgr.subCamId = SUB_CAM_ID_DONE;
}

static void NvMmCsDriver_Update(PlayState* play);

/** MM's CutsceneManager_Update, every frame in Termina (z_nv_mm.c). */
void NvMmCsMgr_Update(PlayState* play) {
    s16 state = 0;

    if ((sLoadedPlay != play) || (sLoadedScene != play->sceneNum) || (sCsMgr.play != play) ||
        (play->gameplayFrames < sLastFrames)) { // (a new scene, or the same one entered again)
        sLoadedPlay = play;
        sLoadedScene = play->sceneNum;
        NvMmCsMgr_Init(play);
    }
    sLastFrames = play->gameplayFrames;
    if (NvMmCsMgr_IsNext(CS_ID_GLOBAL_RETURN_TO_CAM)) {
        NvMmCsMgr_StartWithPlayerCs(CS_ID_GLOBAL_RETURN_TO_CAM, &GET_PLAYER(play)->actor);
    }
    if ((sCsMgr.endCsId == CS_ID_NONE) && (sCsMgr.csId != CS_ID_NONE)) {
        if (sCsMgr.length > 0) {
            sCsMgr.length--;
        }
        state = 1;
        if (sCsMgr.length == 0) {
            NvMmCsMgr_Stop(sCsMgr.csId);
        }
    }
    if (sCsMgr.endCsId != CS_ID_NONE) {
        NvMmCsMgr_End();
        state = 2;
    }
    memset(sNextCutsceneList, 0, sizeof(sNextCutsceneList));
    if (sCsMgr.csId == CS_ID_NONE) {
        if ((NvMmCsMgr_MarkNextCutscenes() == 0) && (state != 0)) {
            Letterbox_SetSizeTarget(0);
        }
    }
    memset(sWaitingCutsceneList, 0, sizeof(sWaitingCutsceneList)); // (MM's CutsceneManager_ClearWaiting, after)
    NvMmCsDriver_Update(play);
}

void NvMmCsMgr_Queue(s16 csId) {
    if (csId > CS_ID_NONE) {
        sWaitingCutsceneList[csId >> 3] |= 1 << (csId & 7);
    }
}

s16 NvMmCsMgr_IsNext(s16 csId) {
    if (csId == CS_ID_GLOBAL_END) {
        return (sCsMgr.csId == CS_ID_NONE) ? 0x7F : 0;
    }
    if (csId <= CS_ID_NONE) {
        return -1;
    }
    return (sNextCutsceneList[csId >> 3] & (1 << (csId & 7))) ? 1 : 0;
}

/** MM's CutsceneManager_Start: the camera handed to the cutscene's own. */
s16 NvMmCsMgr_Start(s16 csId, Actor* actor) {
    PlayState* play = sCsMgr.play;
    const NvMmActorCutscene* csEntry;
    s32 csType = 0;

    if ((csId <= CS_ID_NONE) || (sCsMgr.csId != CS_ID_NONE) || (play == NULL)) {
        return csId;
    }
    sCsMgr.startMethod = CS_START_0;
    csEntry = NvMmCsMgr_GetCutsceneEntryImpl(csId);
    if (csEntry->letterboxSize != 255) {
        Letterbox_SetSizeTarget(csEntry->letterboxSize);
    }
    if (csEntry->hudVisibility == 0) {
        Interface_ChangeHudVisibilityMode(HUD_VISIBILITY_NOTHING);
    }
    if ((csId == CS_ID_GLOBAL_END) || (csEntry->scriptIndex != CS_SCRIPT_ID_NONE)) {
        csType = 1; // (MM's scripted cutscenes: not yet; its camera stays put)
    } else if ((csId != CS_ID_GLOBAL_DOOR) && (csId != CS_ID_GLOBAL_TALK)) {
        csType = 2;
    }
    if (csType != 0) {
        Camera* subCam;

        sCsMgr.retCamId = play->activeCamera;
        sCsMgr.subCamId = Play_CreateSubCamera(play);
        Play_CopyCamera(play, sCsMgr.subCamId, sCsMgr.retCamId);
        Play_ChangeCameraStatus(play, sCsMgr.retCamId, CAM_STAT_WAIT);
        Play_ChangeCameraStatus(play, sCsMgr.subCamId, CAM_STAT_ACTIVE);
        Play_CameraChangeSetting(play, sCsMgr.subCamId, CAM_SET_FREE0);
        subCam = Play_GetCamera(play, sCsMgr.subCamId);
        if (subCam != NULL) {
            subCam->target = actor;
        }
        sCsMgr.targetActor = actor;
        sCsMgr.length = csEntry->length;
        NvMmCsDriver_Begin(play, sCsMgr.subCamId, (csType == 2) ? csEntry->csCamId : CS_CAM_ID_NONE, actor);
    }
    sCsMgr.csId = csId;
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM cutscene %d starts (camera %d)\n", (int)csId, (int)csEntry->csCamId);
        fflush(stdout);
    }
    return csId;
}

s16 NvMmCsMgr_StartWithPlayerCs(s16 csId, Actor* actor) {
    s16 startCsId = NvMmCsMgr_Start(csId, actor);

    if (startCsId > CS_ID_NONE) {
        Player_SetCsActionWithHaltedActors(sCsMgr.play, NULL, 1); // (PLAYER_CSACTION_WAIT)
        if (sCsMgr.length == 0) {
            NvMmCsMgr_Stop(sCsMgr.csId);
        }
        sCsMgr.startMethod = CS_START_1;
    }
    return startCsId;
}

s16 NvMmCsMgr_StartWithPlayerCsAndSetFlag(s16 csId, Actor* actor) {
    s16 startCsId = NvMmCsMgr_Start(csId, actor);

    if (startCsId > CS_ID_NONE) {
        Player_SetCsActionWithHaltedActors(sCsMgr.play, NULL, 1);
        if (sCsMgr.length == 0) {
            NvMmCsMgr_Stop(sCsMgr.csId);
        }
        if (actor != NULL) {
            actor->flags |= ACTOR_FLAG_UPDATE_DURING_OCARINA;
            sCsMgr.startMethod = CS_START_2;
        } else {
            sCsMgr.startMethod = CS_START_1;
        }
    }
    return startCsId;
}

s16 NvMmCsMgr_Stop(s16 csId) {
    const NvMmActorCutscene* csEntry;

    if (csId <= CS_ID_NONE) {
        return csId;
    }
    csEntry = NvMmCsMgr_GetCutsceneEntryImpl(sCsMgr.csId);
    if ((sCsMgr.length > 0) && (csEntry->scriptIndex == CS_SCRIPT_ID_NONE)) {
        return -2;
    }
    if ((csId != CS_ID_GLOBAL_END) && (csEntry->scriptIndex != CS_SCRIPT_ID_NONE)) {
        return -3;
    }
    if (csId == CS_ID_GLOBAL_END) {
        csId = sCsMgr.csId;
    }
    if (csId == sCsMgr.csId) {
        sCsMgr.endCsId = sCsMgr.csId;
        return sCsMgr.endCsId;
    }
    return -1;
}

s16 NvMmCsMgr_GetCurrentCsId(void) {
    return sCsMgr.csId;
}

s16 NvMmCsMgr_GetAdditionalCsId(s16 csId) {
    return (csId <= CS_ID_NONE) ? CS_ID_NONE : NvMmCsMgr_GetCutsceneEntryImpl(csId)->additionalCsId;
}

s16 NvMmCsMgr_GetLength(s16 csId) {
    return (csId <= CS_ID_NONE) ? -1 : NvMmCsMgr_GetCutsceneEntryImpl(csId)->length;
}

s16 NvMmCsMgr_GetCutsceneCustomValue(s16 csId) {
    return (csId <= CS_ID_NONE) ? -1 : NvMmCsMgr_GetCutsceneEntryImpl(csId)->customValue;
}

s16 NvMmCsMgr_GetCurrentSubCamId(s16 csId) {
    return sCsMgr.subCamId;
}

/* ---------------------------------------------------------------------------------------------- */
/* The camera driver: MM's camera functions for the cutscene's camera                              */

typedef struct NvMmCsDrv {
    s32 active;
    s16 camId;
    s16 setting;
    const Vec3s* data; // the camera's BgCamFuncData (MM's: pos, rot, fov, unk_0E, unk_10)
    Actor* target;
    s32 animState; // MM's: 0 new, 20 its target changed, 1 running
    Vec3f eye;
    Vec3f at;
    Vec3f eyeNext;
    f32 fov;
    Vec3f atOffset; // MM's focalActorAtOffset
    // Fixed1
    Vec3f eyeTarget;
    Vec3s rot;
    s32 dataFov;
    f32 step;  // MM's roData->unk_04
    f32 roFov; // MM's roData->fov
    s32 flags;
    // Fixed2
    f32 yOffset;
    f32 minDist;
    f32 maxDist;
    f32 atStep;  // MM's rwData->unk_14
    f32 eyeStep; // MM's rwData->unk_18
    f32 fov2;    // MM's rwData->unk_1C (fov * 100)
    Vec3f eyePoint;
    s16 prevSetting; // (the main camera: its OoT setting before)
    s16 prevDataIdx; // (...and its scene camera data)
    u32 leaseFrame;  // (the main camera: the frame it was last asked for)
    // Unique0 (the entrance views: the main camera, kept until it's done rather than leased)
    s32 entrance;
    s16 timer;
    s16 startYaw;  // MM's rwData->unk_3A
    Vec3f startPos; // MM's rwData->unk_00
    f32 rate;      // MM's rwData->unk_18
    f32 atRate;    // MM's camera->xz/yOffsetUpdateRate
    s32 done;      // MM's rwData->unk_3E
} NvMmCsDrv;

static NvMmCsDrv sCsDrv;   // the cutscene's sub-camera
static NvMmCsDrv sMainDrv; // the main camera, while an actor keeps it on a cutscene camera
static NvMmCsDrv* drv = &sCsDrv;

static f32 Cam_StepF(f32 target, f32 cur, f32 scale, f32 minDiff) { // MM's Camera_ScaledStepToCeilF
    f32 diff = target - cur;

    return (fabsf(diff) >= minDiff) ? cur + diff * scale : target;
}

static s16 Cam_StepS(s16 target, s16 cur, f32 scale, s16 minDiff) { // MM's Camera_ScaledStepToCeilS
    s16 diff = target - cur;
    s16 step = diff * scale + 0.5f;

    return (ABS(diff) >= minDiff) ? cur + step : target;
}

static void Cam_StepVec3f(Vec3f* target, Vec3f* cur, f32 xz, f32 y, f32 minDiff) {
    cur->x = Cam_StepF(target->x, cur->x, xz, minDiff);
    cur->y = Cam_StepF(target->y, cur->y, y, minDiff);
    cur->z = Cam_StepF(target->z, cur->z, xz, minDiff);
}

static VecSph Cam_Geo(Vec3f* from, Vec3f* to) { // MM's OLib_Vec3fDiffToVecGeo
    VecSph g;

    OLib_Vec3fDiffToVecSphGeo(&g, from, to);
    return g;
}

static Vec3f Cam_AddGeo(Vec3f* a, VecSph* g) { // MM's OLib_AddVecGeoToVec3f
    Vec3f v;

    OLib_VecSphGeoToVec3f(&v, g);
    v.x += a->x;
    v.y += a->y;
    v.z += a->z;
    return v;
}

static void NvMmCsDriver_Begin(PlayState* play, s16 subCamId, s16 csCamId, Actor* target) {
    Camera* cam = Play_GetCamera(play, subCamId);

    drv = &sCsDrv;
    memset(drv, 0, sizeof(*drv));
    if ((cam == NULL) || (csCamId < 0) || (sCsCams == NULL) || (csCamId >= sCsCamCount)) {
        return; // (MM's FREE0, or a global camera: left where it is)
    }
    drv->active = true;
    drv->camId = subCamId;
    drv->setting = sCsCams[csCamId].setting;
    drv->data = &sCsCamData[sCsCams[csCamId].first];
    drv->target = target;
    drv->eye = cam->eye;
    drv->at = cam->at;
    drv->eyeNext = cam->eye;
    drv->fov = cam->fov;
    drv->atOffset.x = cam->at.x - GET_PLAYER(play)->actor.world.pos.x;
    drv->atOffset.y = cam->at.y - GET_PLAYER(play)->actor.world.pos.y;
    drv->atOffset.z = cam->at.z - GET_PLAYER(play)->actor.world.pos.z;
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM cutscene camera %d: setting %02X eye %d,%d,%d\n", (int)csCamId, (int)drv->setting,
               drv->data[0].x, drv->data[0].y, drv->data[0].z);
        fflush(stdout);
    }
}

static void NvMmCsDriver_End(void) {
    sCsDrv.active = false;
}

/** MM's Camera_SetTargetActor on the cutscene's camera. */
void NvMmCs_SetTargetActor(Actor* actor) {
    sCsDrv.target = actor;
    sCsDrv.animState = 20;
}

/**
 * MM's Camera_ChangeActorCsCamIndex on the main camera (e.g. the Swordsman's Expert Course: his
 * cutscene's camera, outside any cutscene), asked each frame it's wanted; a frame without lets it go
 * back to OoT's own camera for the place. `csId`: the cutscene whose camera it takes.
 */
void NvMmCs_MainCameraFromCutscene(PlayState* play, s16 csId, Actor* target) {
    const NvMmActorCutscene* entry = NvMmCsMgr_GetCutsceneEntryImpl(csId);
    Camera* cam = Play_GetCamera(play, CAM_ID_MAIN);
    s16 csCamId = entry->csCamId;

    if ((cam == NULL) || (csId <= CS_ID_NONE) || (csCamId < 0) || (sCsCams == NULL) || (csCamId >= sCsCamCount)) {
        return;
    }
    if (!sMainDrv.active || sMainDrv.entrance || (sMainDrv.data != &sCsCamData[sCsCams[csCamId].first])) {
        s16 prev = sMainDrv.active ? sMainDrv.prevSetting : cam->setting;
        s16 prevIdx = sMainDrv.active ? sMainDrv.prevDataIdx : cam->camDataIdx;

        memset(&sMainDrv, 0, sizeof(sMainDrv));
        sMainDrv.active = true;
        sMainDrv.camId = CAM_ID_MAIN;
        sMainDrv.setting = sCsCams[csCamId].setting;
        sMainDrv.data = &sCsCamData[sCsCams[csCamId].first];
        sMainDrv.eye = sMainDrv.eyeNext = cam->eye;
        sMainDrv.at = cam->at;
        sMainDrv.fov = cam->fov;
        sMainDrv.prevSetting = prev;
        sMainDrv.prevDataIdx = prevIdx;
        Play_CameraChangeSetting(play, CAM_ID_MAIN, CAM_SET_FREE0);
    }
    sMainDrv.target = target;
    sMainDrv.leaseFrame = play->gameplayFrames;
}

static void NvMmCsDriver_UpdateOne(PlayState* play);
static void NvMmCsDriver_Unique0(PlayState* play);

/** MM's Camera_Fixed1: a fixed shot (eye, direction, fov from the data; RFIXED1's eye turns with its actor). */
static void NvMmCsDriver_Fixed1(PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 height = Player_GetHeight(player);
    const Vec3s* d = drv->data;
    VecSph eyeAt;
    VecSph eyeOffset;
    Vec3f focus;

    if ((drv->animState == 0) || (drv->animState == 20)) {
        drv->eyeTarget.x = d[0].x;
        drv->eyeTarget.y = d[0].y;
        drv->eyeTarget.z = d[0].z;
        drv->rot = d[1];
        drv->dataFov = d[2].x;
        drv->step = ((drv->setting == MM_CAM_SET_FIXED1) ? 100 : 10) * 0.01f;
        drv->roFov = 60.0f;
        drv->flags = (drv->setting == MM_CAM_SET_RFIXED1) ? FIXED1_FLAG_4 : 0;
        if ((drv->flags & FIXED1_FLAG_4) && (drv->target != NULL)) {
            PosRot* home = &drv->target->home;
            PosRot* world = &drv->target->world;
            VecSph g = Cam_Geo(&home->pos, &drv->eyeTarget);

            g.yaw = world->rot.y + (s16)(g.yaw - home->rot.y);
            drv->eyeTarget = Cam_AddGeo(&world->pos, &g);
            drv->rot.y = world->rot.y + (s16)(drv->rot.y - home->rot.y);
        }
        if (drv->dataFov == -1) {
            drv->dataFov = drv->roFov * 100;
        } else if (drv->dataFov <= 360) {
            drv->dataFov *= 100;
        }
        if (drv->animState == 0) {
            drv->roFov = drv->dataFov * 0.01f;
            if (d[2].y != -1) {
                drv->step = d[2].y * 0.01f;
            }
        }
        drv->animState = 1;
    }
    eyeAt = Cam_Geo(&drv->eye, &drv->at);
    Cam_StepVec3f(&drv->eyeTarget, &drv->eye, drv->step, drv->step, 0.2f);
    focus = player->actor.world.pos;
    focus.y += height;
    eyeOffset.r = OLib_Vec3fDist(&focus, &drv->eye);
    eyeOffset.pitch = Cam_StepS(drv->rot.x * -1, eyeAt.pitch, drv->step, 5);
    eyeOffset.yaw = Cam_StepS(drv->rot.y, eyeAt.yaw, drv->step, 5);
    drv->at = Cam_AddGeo(&drv->eye, &eyeOffset);
    drv->fov = Cam_StepF(drv->roFov, drv->fov, drv->step, 0.1f);
}

/** MM's Camera_Fixed2: a fixed eye turning to follow (the target's focus for CIRCLE8, else Link). */
static void NvMmCsDriver_Fixed2(PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 height = Player_GetHeight(player);
    const Vec3s* d = drv->data;
    Vec3f atTarget;
    Vec3f eyeTarget;
    VecSph g;

    if ((drv->animState == 0) || (drv->animState == 20)) {
        f32 data05 = (drv->setting == MM_CAM_SET_CIRCLE8) ? 20 : 80;

        drv->flags = (drv->setting == MM_CAM_SET_CIRCLE8) ? FIXED2_FLAG_3 : 0;
        drv->yOffset = -40 * 0.01f * height * (0.8f - ((68.0f / height) * -0.2f));
        drv->minDist = 40;
        drv->fov2 = 60 * 100.0f;
        drv->eyePoint.x = d[0].x;
        drv->eyePoint.y = d[0].y;
        drv->eyePoint.z = d[0].z;
        if (d[2].x != -1) {
            drv->fov2 = d[2].x;
        }
        drv->maxDist = (d[2].y != -1) ? d[2].y : 0x3200;
        if (d[2].z != -1) {
            drv->atStep = drv->eyeStep = d[2].z * 0.01f;
        } else {
            drv->atStep = data05 * 0.01f;
            drv->eyeStep = 100 * 0.01f;
        }
        if (drv->fov2 <= 360) {
            drv->fov2 *= 100;
        }
        if (drv->animState == 20) {
            drv->atStep = 0.2f;
        }
    }
    if ((drv->flags & FIXED2_FLAG_3) && (drv->target != NULL)) {
        atTarget = drv->target->focus.pos;
        drv->atOffset.x = atTarget.x - player->actor.world.pos.x;
        drv->atOffset.y = atTarget.y - player->actor.world.pos.y;
        drv->atOffset.z = atTarget.z - player->actor.world.pos.z;
    } else {
        Vec3f off = { 0.0f, drv->yOffset + height, 0.0f };

        Cam_StepVec3f(&off, &drv->atOffset, drv->atStep, drv->atStep, 0.1f);
        atTarget.x = player->actor.world.pos.x + drv->atOffset.x;
        atTarget.y = player->actor.world.pos.y + drv->atOffset.y;
        atTarget.z = player->actor.world.pos.z + drv->atOffset.z;
    }
    if ((drv->animState == 0) && !(drv->flags & FIXED2_FLAG_0)) {
        drv->at = atTarget;
        g = Cam_Geo(&drv->at, &drv->eyePoint);
        if ((drv->maxDist < g.r) || (drv->flags & FIXED2_FLAG_5)) {
            g.r = drv->maxDist;
            eyeTarget = Cam_AddGeo(&drv->at, &g);
        } else if (g.r < drv->minDist) {
            g.r = drv->minDist;
            eyeTarget = Cam_AddGeo(&drv->at, &g);
        } else {
            eyeTarget = drv->eyePoint;
        }
        drv->eyeNext = drv->eye = eyeTarget;
        drv->fov = drv->fov2 * 0.01f;
    }
    Cam_StepVec3f(&atTarget, &drv->at, drv->atStep, drv->atStep, 1.0f);
    g = Cam_Geo(&drv->at, &drv->eyePoint);
    if ((drv->maxDist < g.r) || (drv->flags & FIXED2_FLAG_5)) {
        g.r = drv->maxDist;
        eyeTarget = Cam_AddGeo(&drv->at, &g);
    } else if (g.r < drv->minDist) {
        g.r = drv->minDist;
        eyeTarget = Cam_AddGeo(&drv->at, &g);
    } else {
        eyeTarget = drv->eyePoint;
    }
    Cam_StepVec3f(&eyeTarget, &drv->eyeNext, drv->eyeStep, drv->eyeStep, 0.1f);
    drv->eye = drv->eyeNext;
    drv->fov = Cam_StepF(drv->fov2 * 0.01f, drv->fov, drv->atStep, 0.1f);
    drv->animState = 1;
}

/**
 * MM's Camera_Unique0 (START0, START2: the view of the place as Link comes in, from the spawn's
 * camera): the eye is the data's, looking along its direction at the point on that line closest to
 * Link (START2 then turning to him), until its time is up and Link moves or a button is pressed.
 */
static void NvMmCsDriver_Unique0(PlayState* play) {
    Player* player = GET_PLAYER(play);
    f32 height = Player_GetHeight(player);
    const Vec3s* d = drv->data;
    s32 flags = (drv->setting == MM_CAM_SET_START2) ? (UNIQUE0_FLAG_4 | UNIQUE0_FLAG_0) : UNIQUE0_FLAG_0;
    f32 roUnk00 = 0.0f;
    f32 roUnk04 = (drv->setting == MM_CAM_SET_START2) ? 30.0f : 100.0f;
    Vec3f* pos = &player->actor.world.pos;
    Linef line;
    VecSph g;
    u16 press = play->state.input[0].press.button;

    if (drv->animState == 0) {
        drv->eye = drv->eyeNext = (Vec3f){ d[0].x, d[0].y, d[0].z };
        drv->rot = d[1];
        if (d[2].x != -1) {
            drv->fov = (d[2].x <= 360) ? d[2].x : d[2].x * 0.01f;
        }
        drv->timer = (d[2].y == -1) ? 60 : d[2].y;
        drv->rate = ((d[2].z != -1) ? d[2].z : roUnk04) * 0.01f;
        drv->startPos = *pos;
        if (flags & UNIQUE0_FLAG_4) {
            drv->at = (Vec3f){ pos->x, pos->y + height + roUnk00, pos->z };
        }
        drv->atRate = 0.05f;
        drv->startYaw = player->actor.world.rot.y;
        drv->done = 0;
        drv->animState = 1;
        if (getenv("NV_LOG") != NULL) {
            printf("NVMM Unique0: eye %d,%d,%d rot %d,%d fov %d time %d; Link %.0f,%.0f,%.0f\n", d[0].x, d[0].y,
                   d[0].z, d[1].x, d[1].y, d[2].x, d[2].y, pos->x, pos->y, pos->z);
            fflush(stdout);
        }
    }
    g.r = Math_Vec3f_DistXYZ(pos, &drv->eye);
    g.yaw = drv->rot.y;
    g.pitch = -drv->rot.x;
    line.a = drv->eye;
    OLib_VecSphGeoToVec3f(&line.b, &g);
    Math3D_LineClosestToPoint(&line, pos, &drv->at);
    if (player->stateFlags1 & PLAYER_STATE1_IN_CUTSCENE) {
        drv->startPos = *pos;
    }
    if (flags & UNIQUE0_FLAG_4) {
        Vec3f head = { pos->x, pos->y + height + roUnk00, pos->z };

        Cam_StepVec3f(&head, &drv->at, drv->atRate, drv->atRate, 0.1f);
        drv->atRate = Cam_StepF(drv->rate, drv->atRate, 0.1f, 0.0001f);
    }
    if (drv->timer > 0) {
        drv->timer--;
        drv->startPos = *pos;
    } else if (!(player->stateFlags1 & PLAYER_STATE1_IN_CUTSCENE) &&
               ((Math_Vec3f_DistXZ(pos, &drv->startPos) >= 10.0f) ||
                (press & (BTN_A | BTN_B | BTN_CUP | BTN_CDOWN | BTN_CLEFT | BTN_CRIGHT | BTN_Z | BTN_L | BTN_R)))) {
        drv->done = 1; // (back to the place's own camera)
    }
}

/** MM's Play_Init: a spawn with a camera gives it to the main camera (Camera_ChangeActorCsCamIndex). */
static void NvMmCs_EntranceView(PlayState* play) {
    Camera* cam = Play_GetCamera(play, CAM_ID_MAIN);
    s32 spawn = play->curSpawn;
    s32 csCamId;
    s16 setting;

    if ((cam == NULL) || (sSpawnCams == NULL) || (spawn < 0) || (spawn >= sSpawnCamCount) ||
        ((csCamId = sSpawnCams[spawn]) == 0xFF) || (csCamId >= sCsCamCount)) {
        return;
    }
    setting = sCsCams[csCamId].setting;
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM entrance view: spawn %d camera %d setting %02X\n", (int)spawn, (int)csCamId, (int)setting);
        fflush(stdout);
    }
    if ((setting != MM_CAM_SET_START0) && (setting != MM_CAM_SET_START2)) {
        return; // (not yet: a spawn on another kind of camera)
    }
    memset(&sMainDrv, 0, sizeof(sMainDrv));
    sMainDrv.active = true;
    sMainDrv.entrance = true;
    sMainDrv.camId = CAM_ID_MAIN;
    sMainDrv.setting = setting;
    sMainDrv.data = &sCsCamData[sCsCams[csCamId].first];
    sMainDrv.target = &GET_PLAYER(play)->actor;
    sMainDrv.eye = sMainDrv.eyeNext = cam->eye;
    sMainDrv.at = cam->at;
    sMainDrv.fov = cam->fov;
    sMainDrv.prevSetting = cam->setting;
    sMainDrv.prevDataIdx = cam->camDataIdx;
    sMainDrv.leaseFrame = play->gameplayFrames;
    Play_CameraChangeSetting(play, CAM_ID_MAIN, CAM_SET_FREE0);
}

static void NvMmCsDriver_Update(PlayState* play) {
    drv = &sCsDrv;
    NvMmCsDriver_UpdateOne(play);
    drv = &sMainDrv;
    if (sMainDrv.active && (sMainDrv.done || (!sMainDrv.entrance && ((play->gameplayFrames - sMainDrv.leaseFrame) > 1)))) {
        // let go: OoT's camera again
        Camera* cam = Play_GetCamera(play, CAM_ID_MAIN);

        sMainDrv.active = false;
        if (cam != NULL) { // (forced back to its setting; the floor's own camera picks up from there)
            s16 Camera_ChangeSettingFlags(Camera * camera, s16 setting, s16 flags);

            Camera_ChangeSettingFlags(cam, sMainDrv.prevSetting, 1 | 4);
            cam->camDataIdx = sMainDrv.prevDataIdx;
            cam->animState = 0;
        }
    }
    if (!sCsDrv.active || (sCsMgr.csId == CS_ID_NONE)) {
        NvMmCsDriver_UpdateOne(play);
    }
    drv = &sCsDrv;
}

static void NvMmCsDriver_UpdateOne(PlayState* play) {
    if (!drv->active || (Play_GetCamera(play, drv->camId) == NULL)) {
        return;
    }
    switch (drv->setting) {
        case MM_CAM_SET_FIXED1:
        case MM_CAM_SET_FIXED2:
        case MM_CAM_SET_RFIXED1:
            NvMmCsDriver_Fixed1(play);
            break;
        case MM_CAM_SET_CIRCLE2:
        case MM_CAM_SET_CIRCLE3:
        case MM_CAM_SET_CIRCLE8:
            NvMmCsDriver_Fixed2(play);
            break;
        case MM_CAM_SET_START0:
        case MM_CAM_SET_START2:
            NvMmCsDriver_Unique0(play);
            break;
        default:
            return; // (left for the actor to place)
    }
    Play_CameraSetAtEye(play, drv->camId, &drv->at, &drv->eye);
    Play_CameraSetFov(play, drv->camId, drv->fov);
}
