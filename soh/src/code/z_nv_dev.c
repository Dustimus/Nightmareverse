/*
 * Nightmareverse (dev): scripted play for headless room checks, switched on by environment
 * variables (nothing happens without them):
 *
 *   NV_POS="x,z[,yaw]"        put Link there once the scene is up (OoT units, binang)
 *   NV_WALK="x,z;x,z;wait:N"  steer Link through waypoints with the control stick (camera-
 *                             relative, as a player would), up to NV_WALK_TIMEOUT frames each
 *                             (default 200); "wait:N" stands still N frames
 *   NV_LOG=1                  log Link's position, floor and camera every 20 frames
 *
 * Called from Play_Update (z_play.c) before the actors update.
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NV_MAX_WAYPOINTS 64

typedef struct {
    s16 wait; // 0: a waypoint, else frames to stand still
    s16 x;
    s16 z;
} NvWaypoint;

static NvWaypoint sNvPath[NV_MAX_WAYPOINTS];
static s32 sNvPathLen = -1; // -1: not parsed yet
static s32 sNvPathIdx = 0;
static s32 sNvTimer = 0;
static s32 sNvTimeout = 200;
static s32 sNvPosDone = false;
static s32 sNvScene = -1;

static void NvDev_ParsePath(void) {
    const char* s = getenv("NV_WALK");
    const char* t = getenv("NV_WALK_TIMEOUT");

    sNvPathLen = 0;
    if (t != NULL) {
        sNvTimeout = atoi(t);
    }
    while ((s != NULL) && (*s != '\0') && (sNvPathLen < NV_MAX_WAYPOINTS)) {
        NvWaypoint* wp = &sNvPath[sNvPathLen];

        if (strncmp(s, "wait:", 5) == 0) {
            wp->wait = (s16)atoi(s + 5);
            wp->x = wp->z = 0;
            sNvPathLen++;
        } else {
            int x;
            int z;

            if (sscanf(s, "%d,%d", &x, &z) == 2) {
                wp->wait = 0;
                wp->x = x;
                wp->z = z;
                sNvPathLen++;
            }
        }
        s = strchr(s, ';');
        if (s != NULL) {
            s++;
        }
    }
}

void NvDev_Update(PlayState* play) {
    Player* player = GET_PLAYER(play);
    Input* input = &play->state.input[0];
    const char* pos;

    if ((player == NULL) || (play->gameplayFrames < 2)) {
        return;
    }
    if (play->sceneNum != sNvScene) { // a new scene: the start position applies again
        sNvScene = play->sceneNum;
        sNvPosDone = false;
    }
    pos = getenv("NV_POS");
    if (!sNvPosDone && (pos != NULL) && (play->gameplayFrames > 10)) {
        int x;
        int z;
        int yaw = player->actor.shape.rot.y;

        if (sscanf(pos, "%d,%d,%d", &x, &z, &yaw) >= 2) {
            player->actor.world.pos.x = player->actor.home.pos.x = x;
            player->actor.world.pos.z = player->actor.home.pos.z = z;
            player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = (s16)yaw;
        }
        sNvPosDone = true;
    }
    if (sNvPathLen < 0) {
        NvDev_ParsePath();
    }
    if ((play->gameplayFrames > 20) && (sNvPathIdx < sNvPathLen)) {
        NvWaypoint* wp = &sNvPath[sNvPathIdx];

        if (wp->wait > 0) {
            if (++sNvTimer > wp->wait) {
                sNvPathIdx++;
                sNvTimer = 0;
            }
        } else {
            f32 dx = wp->x - player->actor.world.pos.x;
            f32 dz = wp->z - player->actor.world.pos.z;

            if ((SQ(dx) + SQ(dz) < SQ(15.0f)) || (++sNvTimer > sNvTimeout)) {
                printf("NVWALK reached=%d wp=%d pos=%d,%d f=%d\n", sNvTimer <= sNvTimeout, (int)sNvPathIdx,
                       (int)player->actor.world.pos.x, (int)player->actor.world.pos.z, (int)play->gameplayFrames);
                sNvPathIdx++;
                sNvTimer = 0;
            } else {
                s16 rel = Math_Atan2S(dz, dx) - Camera_GetInputDirYaw(GET_ACTIVE_CAM(play));

                input->cur.stick_x = input->rel.stick_x = (s8)(-Math_SinS(rel) * 60.0f);
                input->cur.stick_y = input->rel.stick_y = (s8)(Math_CosS(rel) * 60.0f);
            }
        }
    }
    if ((getenv("NV_LOG") != NULL) && ((play->gameplayFrames % 20) == 0)) {
        printf("NVPOS scene=%d f=%d pos=%d,%d,%d yaw=%d floor=%d cam=%d\n", (int)play->sceneNum,
               (int)play->gameplayFrames, (int)player->actor.world.pos.x, (int)player->actor.world.pos.y,
               (int)player->actor.world.pos.z, (u16)player->actor.shape.rot.y, player->actor.floorPoly != NULL,
               (int)GET_ACTIVE_CAM(play)->camDataIdx);
        fflush(stdout);
    }
}
