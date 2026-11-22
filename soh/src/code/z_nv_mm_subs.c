/*
 * Nightmareverse: Majora's Mask's shared helpers for its people (zeldaret/mm, z_sub_s.c), the parts
 * the ported actors use, ported closely (mm/README.md):
 * - MM's paths chain: each has an "additional path" index (the next of the same person's routes)
 *   and a custom value, which OoT's paths don't have. mm_scene.py writes them beside the scene's
 *   paths ("<scene>_pathinfo", "MPTH"); SubS_GetAdditionalPath follows the chain;
 * - MM's time pathing: someone due somewhere between two times walks a path so as to be at its
 *   end at the second: a B-spline (order 3) through its points, the progress by the time.
 */
#include "global.h"
#include "z_nv_mm_subs.h"
#include "soh/ResourceManagerHelpers.h"
#include <libultraship/bridge/resourcebridge.h>
#include <string.h>
#include <stdio.h>

#define ADDITIONAL_PATH_INDEX_NONE 0xFF

static s16 sInfoScene = -1;
static const u8* sInfo; // "MPTH", a count, then per path: additional index, pad, custom value
static s32 sInfoCount;

static void NvMmSubS_LoadPathInfo(PlayState* play) {
    char path[160];
    const char* file;
    size_t len;
    const u8* data;

    if (sInfoScene == play->sceneNum) {
        return;
    }
    sInfoScene = play->sceneNum;
    sInfo = NULL;
    sInfoCount = 0;
    if ((play->loadedScene == NULL) || ((file = play->loadedScene->sceneFile.fileName) == NULL)) {
        return;
    }
    len = strlen(file);
    snprintf(path, sizeof(path), "scenes/shared/%s/%s_pathinfo", file, file);
    if ((len == 0) || !ResourceMgr_FileExists(path)) {
        return;
    }
    data = (const u8*)ResourceGetDataByName(path);
    if ((data == NULL) || (memcmp(data, "MPTH", 4) != 0)) {
        return;
    }
    memcpy(&sInfoCount, data + 4, sizeof(sInfoCount));
    sInfo = data + 8;
}

/** MM's path->additionalPathIndex. */
u8 NvMmSubS_AdditionalPathIndex(PlayState* play, s32 pathIndex) {
    NvMmSubS_LoadPathInfo(play);
    return ((sInfo != NULL) && (pathIndex >= 0) && (pathIndex < sInfoCount)) ? sInfo[pathIndex * 4]
                                                                              : ADDITIONAL_PATH_INDEX_NONE;
}

/** MM's path->customValue. */
s16 NvMmSubS_PathCustomValue(PlayState* play, s32 pathIndex) {
    s16 v = 0;

    NvMmSubS_LoadPathInfo(play);
    if ((sInfo != NULL) && (pathIndex >= 0) && (pathIndex < sInfoCount)) {
        memcpy(&v, sInfo + pathIndex * 4 + 2, sizeof(v));
    }
    return v;
}

/** MM's SubS_GetAdditionalPath: the path pathIndex, then up to `limit` more along its chain. */
Path* NvMmSubS_GetAdditionalPath(PlayState* play, u8 pathIndex, s32 limit) {
    Path* path;
    s32 i = 0;

    if (play->setupPathList == NULL) {
        return NULL;
    }
    do {
        path = &play->setupPathList[pathIndex];
        if (i >= limit) {
            break;
        }
        pathIndex = NvMmSubS_AdditionalPathIndex(play, pathIndex);
        i++;
    } while (pathIndex != ADDITIONAL_PATH_INDEX_NONE);
    return path;
}

void NvMmSubS_TimePathing_FillKnots(f32 knots[], s32 order, s32 numPoints) {
    s32 i;
    f32 val = 0.0f;

    for (i = 0; i < numPoints; i++) {
        if ((i >= order) && (i < (numPoints - order + 1))) {
            val += 1.0f;
        }
        knots[i] = val;
    }
}

enum { PROGRESS_ERROR, PROGRESS_STILL_ON_PATH, PROGRESS_SHOULD_REACH_END };

static s32 NvMmSubS_TimePathing_ComputeProgress(f32* progress, s32 elapsedTime, s32 waypointTime, s32 totalTime,
                                                s32 pathCount, s32 order, f32 knots[]) {
    s32 i;
    s32 j;
    s32 k;
    f32 waypointTimeInv;

    *progress = 0.0f;
    if ((waypointTime <= 0) || (elapsedTime < 0)) {
        return PROGRESS_ERROR;
    }
    waypointTimeInv = 1.0f / waypointTime;
    k = 0;
    for (i = order - 1; i < pathCount; i++) {
        for (j = 0; j < waypointTime; j++) {
            if (k == elapsedTime) {
                break;
            }
            *progress += (knots[i + 1] - knots[i]) * waypointTimeInv;
            k++;
        }
    }
    return (elapsedTime == totalTime) ? PROGRESS_SHOULD_REACH_END : PROGRESS_STILL_ON_PATH;
}

static void NvMmSubS_TimePathing_ComputeWeights(s32 order, f32 progress, s32 waypoint, f32 knots[], f32 weights[]) {
    f32 weightsTemp[10][11];
    s32 i;
    s32 j;
    s32 k;

    for (i = 0; i < order; i++) {
        for (j = 0; j < order + 1; j++) {
            weightsTemp[i][j] = 0.0f;
        }
    }
    weightsTemp[0][order - 1] = 1.0f;
    for (i = 1; i < order; i++) {
        for (j = waypoint - i, k = (order - 1) - i; j <= waypoint; j++, k++) {
            if (knots[j + i] != knots[j]) {
                weightsTemp[i][k] = ((progress - knots[j]) / (knots[j + i] - knots[j])) * weightsTemp[i - 1][k];
            } else {
                weightsTemp[i][k] = 0.0f;
            }
            if (knots[j + i + 1] != knots[j + 1]) {
                weightsTemp[i][k] +=
                    ((knots[j + i + 1] - progress) / (knots[j + i + 1] - knots[j + 1])) * weightsTemp[i - 1][k + 1];
            }
        }
    }
    for (j = 0; j < order; j++) {
        weights[j] = weightsTemp[order - 1][j];
    }
}

static void NvMmSubS_TimePathing_ComputeTargetPosXZ(f32* x, f32* z, f32 progress, s32 order, s32 waypoint,
                                                    Vec3s points[], f32 knots[]) {
    f32 weights[11];
    f32 weightedX = 0.0f;
    f32 weightedZ = 0.0f;
    f32 weightedTotal = 0.0f;
    s32 i;

    NvMmSubS_TimePathing_ComputeWeights(order, progress, waypoint, knots, weights);
    for (i = 0; i < order; i++) {
        weightedX += weights[i] * points[waypoint - order + i + 1].x;
        weightedZ += weights[i] * points[waypoint - order + i + 1].z;
        weightedTotal += weights[i];
    }
    *x = weightedX / weightedTotal;
    *z = weightedZ / weightedTotal;
}

s32 NvMmSubS_TimePathing_Update(Path* path, f32* progress, s32* elapsedTime, s32 waypointTime, s32 totalTime,
                                s32* waypoint, f32 knots[], Vec3f* targetPos, s32 timeSpeed) {
    Vec3s* points = SEGMENTED_TO_VIRTUAL(path->points);
    s32 state;
    s32 reachedEnd = false;

    if (*waypoint >= path->count) {
        state = PROGRESS_SHOULD_REACH_END;
    } else {
        state = NvMmSubS_TimePathing_ComputeProgress(progress, *elapsedTime, waypointTime, totalTime, path->count,
                                                     NV_MM_TIME_PATHING_ORDER, knots);
    }
    switch (state) {
        case PROGRESS_STILL_ON_PATH:
            NvMmSubS_TimePathing_ComputeTargetPosXZ(&targetPos->x, &targetPos->z, *progress,
                                                    NV_MM_TIME_PATHING_ORDER, *waypoint, points, knots);
            break;
        case PROGRESS_SHOULD_REACH_END:
            targetPos->x = points[path->count - 1].x;
            targetPos->z = points[path->count - 1].z;
            reachedEnd = true;
            break;
    }
    *elapsedTime += timeSpeed;
    if (*elapsedTime >= totalTime) {
        *elapsedTime = totalTime;
    } else if (*elapsedTime < 0) {
        *elapsedTime = 0;
    }
    *waypoint = (*elapsedTime / waypointTime) + (NV_MM_TIME_PATHING_ORDER - 1);
    return reachedEnd;
}

/** The ground under a time path's target: a line down through its last few points' heights. */
void NvMmSubS_TimePathing_ComputeInitialY(PlayState* play, Path* path, s32 waypoint, Vec3f* targetPos) {
    Vec3s* points = SEGMENTED_TO_VIRTUAL(path->points);
    Vec3f posA;
    Vec3f posB;
    Vec3f posResult;
    s32 i = waypoint - (NV_MM_TIME_PATHING_ORDER - 1);
    s16 max = 0;
    s16 min = 0;
    s32 isSetup = false;
    CollisionPoly* outPoly = NULL;
    s32 bgId = 0;

    for (; i <= waypoint; i++) {
        if ((i < 0) || (i >= path->count)) {
            continue;
        }
        if (isSetup) {
            max = MAX(max, points[i].y);
            min = MIN(min, points[i].y);
        } else {
            max = min = points[i].y;
        }
        isSetup = true;
    }
    max += 30;
    min -= 30;
    posA = *targetPos;
    posB = *targetPos;
    posA.y = max;
    posB.y = min;
    if (BgCheck_EntityLineTest1(&play->colCtx, &posA, &posB, &posResult, &outPoly, true, true, true, true, &bgId)) {
        targetPos->y = posResult.y;
    }
}

/** MM's SubS_FindActor: the next actor of that id in the category after `from` (or the first). */
Actor* NvMmSubS_FindActor(PlayState* play, Actor* from, u8 category, s16 id) {
    Actor* a = (from == NULL) ? play->actorCtx.actorLists[category].head : from->next;

    while (a != NULL) {
        if (a->id == id) {
            return a;
        }
        a = a->next;
    }
    return NULL;
}
