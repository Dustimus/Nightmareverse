/*
 * Nightmareverse (dev): scripted play for headless room checks, switched on by environment
 * variables (nothing happens without them):
 *
 *   NV_POS="x,z[,yaw]"        put Link there once the scene is up (OoT units, binang)
 *   NV_WALK="x,z;x,z;wait:N"  steer Link through waypoints with the control stick (camera-
 *                             relative, as a player would), up to NV_WALK_TIMEOUT frames each
 *                             (default 200); "wait:N" stands still N frames, "a" presses A,
 *                             "pos:x,z" puts Link there, "face:x,z" turns him to face a point,
 *                             "facerect:x0,z0,x1,z1" turns him (16 ways) to put the point in
 *                             front of him deepest into a rectangle (as a player faces a door), "entr:N" warps to entrance N (the
 *                             script goes on in the new scene), "mark:N" logs "NVMARK N" with
 *                             the scene, position and yaw (re1/tools/soh_check.py --doors)
 *   NV_TOUR="x,y,z;x,y,z"     put Link at each point in turn, NV_TOUR_HOLD frames each (default
 *                             40), printing "NVTOUR <i> ..." once he has settled there and
 *                             "NVTOUR done" after the last (re1/tools/soh_check.py screenshots
 *                             on those lines)
 *   NV_LOG=1                  log Link's position, floor and camera every 20 frames
 *
 * Always on: hold L and press D-pad down to note the spot Link stands on (scene, position,
 * facing, camera) in nv_spots.txt in the SoH folder, for marking places to change by hand.
 *
 * Called from Play_Update (z_play.c) before the actors update.
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NV_MAX_WAYPOINTS 512

typedef enum { NV_WP_WALK, NV_WP_WAIT, NV_WP_A, NV_WP_POS, NV_WP_ENTR, NV_WP_MARK, NV_WP_FACE, NV_WP_FACERECT } NvWaypointKind;

typedef struct {
    s16 kind;
    s16 wait; // NV_WP_WAIT: frames; NV_WP_ENTR, NV_WP_MARK: the number
    s16 x;
    s16 z;
    s16 x1; // NV_WP_FACERECT: the rectangle's far corner
    s16 z1;
} NvWaypoint;

static NvWaypoint sNvPath[NV_MAX_WAYPOINTS];
static s32 sNvPathLen = -1; // -1: not parsed yet
static s32 sNvPathIdx = 0;
static s32 sNvTimer = 0;
static s32 sNvTimeout = 200;
static s32 sNvPosDone = false;
static s32 sNvScene = -1;
static Vec3s sNvTour[NV_MAX_WAYPOINTS * 4];
static s32 sNvTourLen = -1;
static s32 sNvTourIdx = 0;
static s32 sNvTourTimer = 0;
static s32 sNvTourHold = 40;

static void NvDev_ParseTour(void) {
    const char* s = getenv("NV_TOUR");
    const char* h = getenv("NV_TOUR_HOLD");

    sNvTourLen = 0;
    if (h != NULL) {
        sNvTourHold = atoi(h);
    }
    while ((s != NULL) && (*s != '\0') && (sNvTourLen < ARRAY_COUNT(sNvTour))) {
        int x;
        int y;
        int z;

        if (sscanf(s, "%d,%d,%d", &x, &y, &z) == 3) {
            sNvTour[sNvTourLen].x = x;
            sNvTour[sNvTourLen].y = y;
            sNvTour[sNvTourLen].z = z;
            sNvTourLen++;
        }
        s = strchr(s, ';');
        if (s != NULL) {
            s++;
        }
    }
}

// one tour step: Link goes to the point at the start of it, and the camera gets the rest to settle
static void NvDev_Tour(PlayState* play, Player* player) {
    Vec3s* p;

    if (sNvTourLen < 0) {
        NvDev_ParseTour();
    }
    if ((sNvTourIdx > sNvTourLen) || (play->gameplayFrames < 30)) {
        return;
    }
    if (sNvTourIdx == sNvTourLen) {
        printf("NVTOUR done\n");
        fflush(stdout);
        sNvTourIdx++;
        return;
    }
    p = &sNvTour[sNvTourIdx];
    if (sNvTourTimer < 4) { // hold him there while the floor and camera catch up
        player->actor.world.pos.x = player->actor.home.pos.x = p->x;
        player->actor.world.pos.y = player->actor.home.pos.y = p->y + 10.0f;
        player->actor.world.pos.z = player->actor.home.pos.z = p->z;
        player->actor.velocity.y = 0.0f;
    }
    if (++sNvTourTimer == sNvTourHold) {
        printf("NVTOUR %d pos=%d,%d,%d floor=%d cam=%d\n", (int)sNvTourIdx, (int)player->actor.world.pos.x,
               (int)player->actor.world.pos.y, (int)player->actor.world.pos.z, player->actor.floorPoly != NULL,
               (int)GET_ACTIVE_CAM(play)->camDataIdx);
        fflush(stdout);
    } else if (sNvTourTimer >= sNvTourHold + 20) { // 20 frames for the screenshot
        sNvTourIdx++;
        sNvTourTimer = 0;
    }
}

static void NvDev_ParsePath(void) {
    const char* s = getenv("NV_WALK");
    const char* t = getenv("NV_WALK_TIMEOUT");

    sNvPathLen = 0;
    if (t != NULL) {
        sNvTimeout = atoi(t);
    }
    while ((s != NULL) && (*s != '\0') && (sNvPathLen < NV_MAX_WAYPOINTS)) {
        NvWaypoint* wp = &sNvPath[sNvPathLen];

        int x;
        int z;

        wp->x = wp->z = wp->wait = 0;
        if ((s[0] == 'a') && ((s[1] == ';') || (s[1] == '\0'))) {
            wp->kind = NV_WP_A;
            sNvPathLen++;
        } else if (strncmp(s, "wait:", 5) == 0) {
            wp->kind = NV_WP_WAIT;
            wp->wait = (s16)atoi(s + 5);
            sNvPathLen++;
        } else if (strncmp(s, "entr:", 5) == 0) {
            wp->kind = NV_WP_ENTR;
            wp->wait = (s16)strtol(s + 5, NULL, 0);
            sNvPathLen++;
        } else if (strncmp(s, "mark:", 5) == 0) {
            wp->kind = NV_WP_MARK;
            wp->wait = (s16)atoi(s + 5);
            sNvPathLen++;
        } else if ((strncmp(s, "pos:", 4) == 0) && (sscanf(s + 4, "%d,%d", &x, &z) == 2)) {
            wp->kind = NV_WP_POS;
            wp->x = x;
            wp->z = z;
            sNvPathLen++;
        } else if (strncmp(s, "facerect:", 9) == 0) {
            int x1;
            int z1;

            if (sscanf(s + 9, "%d,%d,%d,%d", &x, &z, &x1, &z1) == 4) {
                wp->kind = NV_WP_FACERECT;
                wp->x = x;
                wp->z = z;
                wp->x1 = x1;
                wp->z1 = z1;
                sNvPathLen++;
            }
        } else if ((strncmp(s, "face:", 5) == 0) && (sscanf(s + 5, "%d,%d", &x, &z) == 2)) {
            wp->kind = NV_WP_FACE;
            wp->x = x;
            wp->z = z;
            sNvPathLen++;
        } else if (sscanf(s, "%d,%d", &x, &z) == 2) {
            wp->kind = NV_WP_WALK;
            wp->x = x;
            wp->z = z;
            sNvPathLen++;
        }
        s = strchr(s, ';');
        if (s != NULL) {
            s++;
        }
    }
}

static void NvDev_MarkSpot(PlayState* play, Player* player) {
    FILE* f = fopen("nv_spots.txt", "a");
    const char* file = (play->loadedScene != NULL) ? play->loadedScene->sceneFile.fileName : NULL;

    if (f == NULL) {
        return;
    }
    fprintf(f, "%s pos=%d,%d,%d yaw=%d cam=%d\n", (file != NULL) ? file : "?", (int)player->actor.world.pos.x,
            (int)player->actor.world.pos.y, (int)player->actor.world.pos.z, (u16)player->actor.shape.rot.y,
            (int)GET_ACTIVE_CAM(play)->camDataIdx);
    fclose(f);
    Sfx_PlaySfxCentered(NA_SE_SY_DECIDE);
}

void NvDev_Update(PlayState* play) {
    Player* player = GET_PLAYER(play);
    Input* input = &play->state.input[0];
    const char* pos;

    if ((player != NULL) && CHECK_BTN_ALL(input->cur.button, BTN_L) &&
        CHECK_BTN_ALL(input->press.button, BTN_DDOWN)) {
        NvDev_MarkSpot(play, player);
    }

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

        if (wp->kind == NV_WP_POS) {
            player->actor.world.pos.x = player->actor.home.pos.x = wp->x;
            player->actor.world.pos.z = player->actor.home.pos.z = wp->z;
            player->actor.world.pos.y += 10.0f;
            sNvPathIdx++;
        } else if (wp->kind == NV_WP_FACE) {
            s16 yaw = Math_Atan2S(wp->z - player->actor.world.pos.z, wp->x - player->actor.world.pos.x);

            player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = yaw;
            sNvPathIdx++;
        } else if (wp->kind == NV_WP_FACERECT) {
            // the yaw whose points 13..22 in front of Link reach deepest into the rectangle
            f32 best = -1e9f;
            s16 bestYaw = player->actor.shape.rot.y;
            s32 k;

            for (k = 0; k < 16; k++) {
                s16 yaw = k * 0x1000;
                f32 d;
                f32 deepest = -1e9f;

                for (d = 13.0f; d <= 22.0f; d += 1.0f) {
                    f32 px = player->actor.world.pos.x + Math_SinS(yaw) * d;
                    f32 pz = player->actor.world.pos.z + Math_CosS(yaw) * d;
                    f32 depth = MIN(MIN(px - wp->x, wp->x1 - px), MIN(pz - wp->z, wp->z1 - pz));

                    deepest = MAX(deepest, depth);
                }
                if (deepest > best) {
                    best = deepest;
                    bestYaw = yaw;
                }
            }
            player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = bestYaw;
            sNvPathIdx++;
        } else if (wp->kind == NV_WP_ENTR) {
            if (play->transitionTrigger == TRANS_TRIGGER_OFF) {
                sNvPathIdx++;
                play->nextEntranceIndex = wp->wait;
                play->transitionTrigger = TRANS_TRIGGER_START;
                play->transitionType = TRANS_TYPE_INSTANT;
            }
        } else if (wp->kind == NV_WP_MARK) {
            printf("NVMARK %d scene=%d pos=%d,%d,%d yaw=%d\n", (int)wp->wait, (int)play->sceneNum,
                   (int)player->actor.world.pos.x, (int)player->actor.world.pos.y, (int)player->actor.world.pos.z,
                   (u16)player->actor.shape.rot.y);
            fflush(stdout);
            sNvPathIdx++;
        } else if (wp->kind == NV_WP_A) {
            input->press.button |= BTN_A;
            input->cur.button |= BTN_A;
            printf("NVWALK A pos=%d,%d yaw=%d f=%d\n", (int)player->actor.world.pos.x,
                   (int)player->actor.world.pos.z, (u16)player->actor.shape.rot.y, (int)play->gameplayFrames);
            sNvPathIdx++;
        } else if (wp->kind == NV_WP_WAIT) {
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
    if (getenv("NV_TOUR") != NULL) {
        NvDev_Tour(play, player);
    }
    if ((getenv("NV_LOG") != NULL) && ((play->gameplayFrames % 20) == 0)) {
        printf("NVPOS scene=%d f=%d pos=%d,%d,%d yaw=%d floor=%d cam=%d\n", (int)play->sceneNum,
               (int)play->gameplayFrames, (int)player->actor.world.pos.x, (int)player->actor.world.pos.y,
               (int)player->actor.world.pos.z, (u16)player->actor.shape.rot.y, player->actor.floorPoly != NULL,
               (int)GET_ACTIVE_CAM(play)->camDataIdx);
        fflush(stdout);
    }
}
