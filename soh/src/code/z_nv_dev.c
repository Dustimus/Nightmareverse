/*
 * Nightmareverse (dev): scripted play for headless room checks, switched on by environment
 * variables (nothing happens without them):
 *
 *   NV_POS="x,z[,yaw]"        put Link there once the scene is up (OoT units, binang)
 *   NV_WALK="x,z;x,z;wait:N"  steer Link through waypoints with the control stick (camera-
 *                             relative, as a player would), up to NV_WALK_TIMEOUT frames each
 *                             (default 200); "wait:N" stands still N frames, "a" presses A, "b" B,
 *                             "press:cu" (cd, cl, cr, a) holds a button 8 frames (an ocarina note),
 *                             "start" Start, "left"/"right"/"up"/"down" the D-pad, "cl:N" holds
 *                             C-Left N frames (RE1's gun's aim; letting go fires),
 *                             "pos:x,z" puts Link there, "face:x,z" turns him to face a point,
 *                             "facerect:x0,z0,x1,z1" turns him (16 ways) to put the point in
 *                             front of him deepest into a rectangle (as a player faces a door), "go:XXX" warps to RE1
 *                             room XXX (its first start), "entr:N" warps to entrance N (the
 *                             script goes on in the new scene), "mark:N" logs "NVMARK N" with
 *                             the scene, position and yaw (re1/tools/soh_check.py --doors),
 *                             "door:N" / "aot:N" put Link in the room's door N's / action entry
 *                             N's rectangle, facing into it, and press A (a story walkthrough),
 *                             "list" logs the room's doors and live action entries, "save"
 *                             writes the RE1 slot (a checkpoint, no ribbon), "use:XX" uses
 *                             item XX (hex) where he stands, as the status screen's Use
 *   NV_TOUR="x,y,z;x,y,z"     put Link at each point in turn, NV_TOUR_HOLD frames each (default
 *                             40), printing "NVTOUR <i> ..." once he has settled there and
 *                             "NVTOUR done" after the last (re1/tools/soh_check.py screenshots
 *                             on those lines)
 *   NV_LOG=1                  log Link's position, floor and camera every 20 frames
 *   NV_GODMODE=1              Link's health kept full (a story walkthrough, not its fights)
 *
 * Always on: hold L and press D-pad down to note the spot Link stands on (scene, position,
 * facing, camera) in nv_spots.txt in the SoH folder, for marking places to change by hand.
 *
 * Called from Play_Update (z_play.c) before the actors update.
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <math.h>

#define NV_MAX_WAYPOINTS 512

typedef enum { NV_WP_WALK, NV_WP_WAIT, NV_WP_A, NV_WP_POS, NV_WP_ENTR, NV_WP_MARK, NV_WP_FACE, NV_WP_FACERECT, NV_WP_B, NV_WP_START, NV_WP_DPAD, NV_WP_HOLDCL, NV_WP_HOLD, NV_WP_DOOR, NV_WP_AOT, NV_WP_LIST, NV_WP_SAVE, NV_WP_USE } NvWaypointKind;

typedef struct {
    s16 kind;
    s16 wait; // NV_WP_WAIT: frames; NV_WP_ENTR, NV_WP_MARK: the number
    s16 x;
    s16 z;
    s16 x1; // NV_WP_FACERECT: the rectangle's far corner
    s16 z1;
} NvWaypoint;

u32 gNvDevHeld; // a held button the ocarina's own pad read adds (NV_WALK's press:, 0 otherwise)

static NvWaypoint sNvPath[NV_MAX_WAYPOINTS];
static s32 sNvPathLen = -1; // -1: not parsed yet
static s32 sNvPathIdx = 0;
static s32 sNvTimer = 0;
static s32 sNvTimeout = 200;
static s32 sNvPosDone = false;
static s32 sNvScene = -1;
static s32 sNvWalkScene = -1; // the room a walk step started in: it ends if the room changes
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
        } else if ((s[0] == 'b') && ((s[1] == ';') || (s[1] == '\0'))) {
            wp->kind = NV_WP_B;
            sNvPathLen++;
        } else if ((strncmp(s, "start", 5) == 0) && ((s[5] == ';') || (s[5] == '\0'))) {
            wp->kind = NV_WP_START;
            sNvPathLen++;
        } else if ((strncmp(s, "left", 4) == 0) || (strncmp(s, "right", 5) == 0) || (strncmp(s, "up", 2) == 0) ||
                   (strncmp(s, "down", 4) == 0)) {
            wp->kind = NV_WP_DPAD;
            wp->wait = (s[0] == 'l') ? BTN_DLEFT : (s[0] == 'r') ? BTN_DRIGHT : (s[0] == 'u') ? BTN_DUP : BTN_DDOWN;
            sNvPathLen++;
        } else if (strncmp(s, "press:", 6) == 0) { // a note's press: press:cu / cd / cl / cr / a, held 8 frames
            wp->kind = NV_WP_HOLD;
            wp->x = (s[6] == 'a') ? BTN_A
                    : (s[7] == 'u') ? BTN_CUP : (s[7] == 'd') ? BTN_CDOWN : (s[7] == 'l') ? BTN_CLEFT : BTN_CRIGHT;
            wp->wait = 8;
            sNvPathLen++;
        } else if (strncmp(s, "cl:", 3) == 0) { // hold C-Left that many frames, then let go
            wp->kind = NV_WP_HOLDCL;
            wp->wait = (s16)atoi(s + 3);
            sNvPathLen++;
        } else if (strncmp(s, "wait:", 5) == 0) {
            wp->kind = NV_WP_WAIT;
            wp->wait = (s16)atoi(s + 5);
            sNvPathLen++;
        } else if (strncmp(s, "go:", 3) == 0) { // "go:XXX": warp to RE1 room XXX's first start
            s16 NvDoor_Entrance(const char* sceneFile, s32 spawn);
            char name[32];
            char room[8];

            snprintf(room, sizeof(room), "%.3s", s + 3);
            for (char* c = room; *c != '\0'; c++) { // (the scene files are lower case)
                *c = (char)tolower((unsigned char)*c);
            }
            snprintf(name, sizeof(name), "re1_%s_scene", room);
            wp->kind = NV_WP_ENTR;
            wp->wait = NvDoor_Entrance(name, 0);
            if (wp->wait >= 0) {
                sNvPathLen++;
            }
        } else if ((strncmp(s, "door:", 5) == 0) || (strncmp(s, "aot:", 4) == 0)) {
            // "door:N" / "aot:N": Link put in the room's door N's (or action entry N's) rectangle,
            // turned to face into it, and A pressed (a logic walkthrough: what's behind it, not the walk)
            wp->kind = (s[0] == 'd') ? NV_WP_DOOR : NV_WP_AOT;
            wp->wait = (s16)atoi(s + ((s[0] == 'd') ? 5 : 4));
            sNvPathLen++;
        } else if (strncmp(s, "use:", 4) == 0) { // the status screen's Use of item XX, where he stands
            wp->kind = NV_WP_USE;
            wp->wait = (s16)strtol(s + 4, NULL, 16);
            sNvPathLen++;
        } else if ((strncmp(s, "save", 4) == 0) && ((s[4] == ';') || (s[4] == '\0'))) {
            wp->kind = NV_WP_SAVE; // the RE1 slot written (a checkpoint: NV_BOOT_ENTRANCE=warp resumes it)
            sNvPathLen++;
        } else if ((strncmp(s, "list", 4) == 0) && ((s[4] == ';') || (s[4] == '\0'))) {
            wp->kind = NV_WP_LIST; // the room's doors and live action entries, logged
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
    static s32 sHealthSet;

    // dev: NV_HEALTH=n sets Link's health (in sixteenths of a heart) once, e.g. to see the enemies'
    // low-health kill moves
    if (getenv("NV_GODMODE") != NULL) { // (a story walkthrough: the route, not the fights)
        gSaveContext.health = gSaveContext.healthCapacity;
    }
    if (!sHealthSet && (getenv("NV_HEALTH") != NULL) && (play->gameplayFrames > 2)) {
        sHealthSet = true;
        gSaveContext.health = (s16)atoi(getenv("NV_HEALTH"));
    }

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

        // a walk that took Link into another room (stairs walked into) is over
        if ((wp->kind == NV_WP_WALK) && (sNvWalkScene >= 0) && (sNvWalkScene != play->sceneNum)) {
            sNvWalkScene = -1;
            sNvTimer = 0;
            sNvPathIdx++;
            return;
        }
        sNvWalkScene = (wp->kind == NV_WP_WALK) ? play->sceneNum : -1;

        if (wp->kind == NV_WP_POS) {
            // onto the floor there (an RE1 room's levels never lie over one another)
            Vec3f probe = { wp->x, 5000.0f, wp->z };
            CollisionPoly* poly;
            s32 bgId;
            f32 y = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &probe);

            player->actor.world.pos.x = player->actor.home.pos.x = wp->x;
            player->actor.world.pos.z = player->actor.home.pos.z = wp->z;
            player->actor.world.pos.y = (y > BGCHECK_Y_MIN ? y : player->actor.world.pos.y) + 10.0f;
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
        } else if ((wp->kind == NV_WP_DOOR) || (wp->kind == NV_WP_AOT)) {
            s32 NvDoor_DevRect(PlayState* play, s32 i, f32* r, const char** target);
            s32 NvAot_DevRect(s32 i, f32* r, s32* handler, s32* item);
            static s32 sPhase;
            static s32 sRetry;
            f32 r[4];
            const char* target = "";
            s32 handler = 1;
            s32 item = -1;
            s32 NvDoorScene_Covering(void);
            s32 NvMovie_Playing(void);
            s32 ok;

            if ((sPhase == 0) && ((play->transitionTrigger != TRANS_TRIGGER_OFF) ||
                                  (play->transitionMode != TRANS_MODE_OFF) || NvDoorScene_Covering() ||
                                  NvMovie_Playing() || (play->gameplayFrames < 20))) {
                return; // (the room isn't up yet: a door's scene, a movie, a load)
            }
            static f32 sRect[4];

            ok = (wp->kind == NV_WP_DOOR) ? NvDoor_DevRect(play, wp->wait, r, &target)
                                          : NvAot_DevRect(wp->wait, r, &handler, &item);
            if (sPhase == 0) {
                memcpy(sRect, r, sizeof(sRect));
            } else { // (started: its zone as it was, though a walk-in event may have used the entry up)
                memcpy(r, sRect, sizeof(sRect));
                ok = true;
            }
            if (!ok && (sPhase == 0) && (++sRetry < 600)) {
                return; // (not armed yet: the room's own scene may come first)
            }
            sRetry = 0;
            if (!ok) {
                printf("NVSTEP %s %d: none in scene %d\n", (wp->kind == NV_WP_DOOR) ? "door" : "aot", (int)wp->wait,
                       (int)play->sceneNum);
                fflush(stdout);
                sPhase = 0;
                sNvPathIdx++;
            } else if (sPhase == 0) { // onto the floor in the middle of it (a wall pushes him back out)
                // (a door's rectangle lies past the wall, where there's no floor: the nearest point
                // that has floor at his level, then a little further in)
                f32 cx = (r[0] + r[2]) / 2;
                f32 cz = (r[1] + r[3]) / 2;
                Vec3f probe = { cx, 5000.0f, cz };
                CollisionPoly* poly;
                s32 bgId;
                f32 y = BGCHECK_Y_MIN;
                f32 rad;
                s32 k;

                for (rad = 0.0f; rad <= 120.0f; rad += 4.0f) {
                    for (k = 0; k < ((rad == 0.0f) ? 1 : 24); k++) {
                        f32 a = k * (2.0f * M_PI / 24.0f);
                        Vec3f q = { cx + cosf(a) * rad, 5000.0f, cz + sinf(a) * rad };
                        f32 qy = BgCheck_EntityRaycastFloor3(&play->colCtx, &poly, &bgId, &q);

                        if ((qy > BGCHECK_Y_MIN) && (fabsf(qy - player->actor.world.pos.y) < 60.0f)) {
                            probe.x = cx + cosf(a) * (rad + ((rad > 0.0f) ? 14.0f : 0.0f));
                            probe.z = cz + sinf(a) * (rad + ((rad > 0.0f) ? 14.0f : 0.0f));
                            y = qy;
                            break;
                        }
                    }
                    if (y > BGCHECK_Y_MIN) {
                        break;
                    }
                }

                player->actor.world.pos.x = player->actor.home.pos.x = probe.x;
                player->actor.world.pos.z = player->actor.home.pos.z = probe.z;
                player->actor.world.pos.y = (y > BGCHECK_Y_MIN ? y : player->actor.world.pos.y) + 10.0f;
                player->linearVelocity = 0.0f;
                printf("NVSTEP %s %d -> %s (handler %d item %d) at %d,%d\n",
                       (wp->kind == NV_WP_DOOR) ? "door" : "aot", (int)wp->wait, target, (int)handler, (int)item,
                       (int)probe.x, (int)probe.z);
                fflush(stdout);
                sPhase++;
            } else if (sPhase < 8) { // settling, nudged toward its middle (walls stop him, as a player walking up)
                f32 dx = (r[0] + r[2]) / 2 - player->actor.world.pos.x;
                f32 dz = (r[1] + r[3]) / 2 - player->actor.world.pos.z;
                f32 d = sqrtf(SQ(dx) + SQ(dz));

                if ((sPhase >= 2) && (d > 4.0f)) {
                    player->actor.world.pos.x += dx / d * 3.0f;
                    player->actor.world.pos.z += dz / d * 3.0f;
                }
                sPhase++;
            } else if (sPhase == 8) { // facing into it, as a player at a door
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

                        deepest = MAX(deepest, MIN(MIN(px - r[0], r[2] - px), MIN(pz - r[1], r[3] - pz)));
                    }
                    if (deepest > best) {
                        best = deepest;
                        bestYaw = yaw;
                    }
                }
                player->actor.shape.rot.y = player->actor.world.rot.y = player->yaw = bestYaw;
                sPhase++;
            } else if (sPhase < 11) {
                sPhase++;
            } else if (sPhase == 11) { // A
                input->press.button |= BTN_A;
                input->cur.button |= BTN_A;
                sPhase = (wp->kind == NV_WP_DOOR) ? 12 : 0;
                if (wp->kind != NV_WP_DOOR) {
                    sNvPathIdx++;
                }
            } else { // a door: it opened, or again (an enemy holding him, a message in the way)
                static s32 sTries;

                if ((play->transitionTrigger != TRANS_TRIGGER_OFF) || NvDoorScene_Covering()) {
                    sPhase = 0;
                    sTries = 0;
                    sNvPathIdx++;
                } else if (++sPhase > 50) {
                    sPhase = 0;
                    if (++sTries >= 40) {
                        printf("NVSTEP door %d: didn't open\n", (int)wp->wait);
                        fflush(stdout);
                        sTries = 0;
                        sNvPathIdx++;
                    }
                }
            }
        } else if (wp->kind == NV_WP_USE) {
            extern s32 gNvDevUseItem;

            if (play->msgCtx.msgMode == MSGMODE_NONE) {
                gNvDevUseItem = wp->wait;
                sNvPathIdx++;
            }
        } else if (wp->kind == NV_WP_SAVE) {
            void NvSave_DevSave(PlayState* play);

            if ((play->transitionTrigger == TRANS_TRIGGER_OFF) && (play->msgCtx.msgMode == MSGMODE_NONE)) {
                NvSave_DevSave(play);
                sNvPathIdx++;
            }
        } else if (wp->kind == NV_WP_LIST) {
            s32 NvDoor_DevRect(PlayState* play, s32 i, f32* r, const char** target);
            void NvAot_DevList(void);
            f32 r[4];
            const char* target;
            s32 i;

            for (i = 0; NvDoor_DevRect(play, i, r, &target); i++) {
                printf("NVDOORLIST %d -> %s rect %d,%d..%d,%d\n", (int)i, target, (int)r[0], (int)r[1], (int)r[2],
                       (int)r[3]);
            }
            NvAot_DevList();
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
        } else if (wp->kind == NV_WP_B) {
            input->press.button |= BTN_B;
            input->cur.button |= BTN_B;
            sNvPathIdx++;
        } else if (wp->kind == NV_WP_START) {
            input->press.button |= BTN_START;
            input->cur.button |= BTN_START;
            sNvPathIdx++;
        } else if (wp->kind == NV_WP_DPAD) {
            input->press.button |= (u16)wp->wait;
            input->cur.button |= (u16)wp->wait;
            sNvPathIdx++;
        } else if (wp->kind == NV_WP_A) {
            input->press.button |= BTN_A;
            input->cur.button |= BTN_A;
            printf("NVWALK A pos=%d,%d yaw=%d f=%d\n", (int)player->actor.world.pos.x,
                   (int)player->actor.world.pos.z, (u16)player->actor.shape.rot.y, (int)play->gameplayFrames);
            sNvPathIdx++;
        } else if (wp->kind == NV_WP_HOLDCL) {
            if (sNvTimer == 0) {
                input->press.button |= BTN_CLEFT;
            }
            if (++sNvTimer > wp->wait) {
                sNvPathIdx++;
                sNvTimer = 0;
            } else {
                input->cur.button |= BTN_CLEFT;
            }
        } else if (wp->kind == NV_WP_HOLD) {
            if (sNvTimer == 0) {
                input->press.button |= (u16)wp->x;
            }
            if (++sNvTimer > wp->wait) {
                sNvPathIdx++;
                sNvTimer = 0;
                gNvDevHeld = 0;
            } else {
                input->cur.button |= (u16)wp->x;
                gNvDevHeld = (u16)wp->x; // the ocarina reads the pad itself (code_800EC960.c)
            }
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
        if (player->actor.wallPoly != NULL) {
            CollisionPoly* w = player->actor.wallPoly;
            printf("NVWALL n=%d,%d,%d d=%d bg=%x\n", w->normal.x, w->normal.y, w->normal.z, w->dist,
                   player->actor.bgCheckFlags);
        }
        printf("NVPOS scene=%d f=%d pos=%d,%d,%d yaw=%d floor=%d cam=%d\n", (int)play->sceneNum,
               (int)play->gameplayFrames, (int)player->actor.world.pos.x, (int)player->actor.world.pos.y,
               (int)player->actor.world.pos.z, (u16)player->actor.shape.rot.y, player->actor.floorPoly != NULL,
               (int)GET_ACTIVE_CAM(play)->camDataIdx);
        fflush(stdout);
    }
}
