/*
 * Nightmareverse: young Link in Termina is Majora's Mask's own model (MM's object_link_child, the
 * "human" Link: decided with the user, 2026-11-23). The two games' young Link skeletons are the same
 * (21 limbs, the same joints but the sheath's, 27 units lower in MM's), so OoT's animations and
 * player code drive MM's model as they are.
 *
 * Nothing of OoT's player code is changed: OoT reads young Link's skeleton, face and hands from its
 * own tables (z_player_lib.c gPlayerSkelHeaders, sEyeTextures, sMouthTextures and the hand, waist
 * tables), so on each scene's start (SoH's OnSceneInit, before Link is made: NvMmActors.cpp) those
 * entries are MM's in Termina and OoT's own everywhere else. Adult Link is never touched.
 *
 * What's MM's: the skeleton and every body part it draws (legs, waist, torso, collar, arms, head,
 * hat), the face (MM's eyes and mouths: OoT's eight eye expressions onto MM's eight, OoT's "shock"
 * as MM's wincing, OoT's two unnamed as MM's looking up and down), the open and closed hands, the
 * hand with the Ocarina of Time. What's still OoT's: parts holding or wearing something (the sword
 * in hand, the sword and shield on his back, the Deku Shield, the slingshot, the boomerang, the
 * bottle, the Fairy Ocarina): OoT bakes the item into the hand, MM draws the hand and the item apart,
 * so MM's way comes with MM's items in hand. They sit right on MM's body (the same skeleton).
 */
#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include <libultraship/bridge/resourcebridge.h>

#define MM_LINK(name) ("__OTR__mm/objects/object_link_child/" name)

extern FlexSkeletonHeader* gPlayerSkelHeaders[];
extern void* sEyeTextures[2][8];
extern void* sMouthTextures[2][4];
extern Gfx* gPlayerLeftHandOpenDLs[];
extern Gfx* gPlayerLeftHandClosedDLs[];
extern Gfx* sPlayerRightHandOpenDLs[];
extern Gfx* sPlayerRightHandClosedDLs[];
extern Gfx* sPlayerWaistDLs[];
extern Gfx* sPlayerRightHandOotDLs[];
extern Gfx* sPlayerRightHandHookshotDLs[];
extern Gfx* sPlayerLeftHandHammerDLs[];

typedef struct {
    void** slot;     // the table entry (young Link's)
    const char* mm;  // MM's part
    void* oot;       // OoT's, kept to put back
} NvMmLinkSwap;

static NvMmLinkSwap sSwaps[] = {
    { (void**)&gPlayerSkelHeaders[1], MM_LINK("gLinkHumanSkel") },
    // the face: OoT's open, half, closed, roll left, roll right, shock, unknown 1 and 2
    { &sEyeTextures[1][0], MM_LINK("gLinkHumanEyesOpenTex") },
    { &sEyeTextures[1][1], MM_LINK("gLinkHumanEyesHalfTex") },
    { &sEyeTextures[1][2], MM_LINK("gLinkHumanEyesClosedTex") },
    { &sEyeTextures[1][3], MM_LINK("gLinkHumanEyesLeftTex") },
    { &sEyeTextures[1][4], MM_LINK("gLinkHumanEyesRightTex") },
    { &sEyeTextures[1][5], MM_LINK("gLinkHumanEyesWincingTex") },
    { &sEyeTextures[1][6], MM_LINK("gLinkHumanEyesUpTex") },
    { &sEyeTextures[1][7], MM_LINK("gLinkHumanEyesDownTex") },
    { &sMouthTextures[1][0], MM_LINK("gLinkHumanMouthClosedTex") },
    { &sMouthTextures[1][1], MM_LINK("gLinkHumanMouthHalfTex") },
    { &sMouthTextures[1][2], MM_LINK("gLinkHumanMouthOpenTex") },
    { &sMouthTextures[1][3], MM_LINK("gLinkHumanMouthSmileTex") },
    // the hands and the waist, near and far (MM's are the same both ways)
    { (void**)&gPlayerLeftHandOpenDLs[1], MM_LINK("gLinkHumanLeftHandOpenDL") },
    { (void**)&gPlayerLeftHandOpenDLs[3], MM_LINK("gLinkHumanLeftHandOpenDL") },
    { (void**)&gPlayerLeftHandClosedDLs[1], MM_LINK("gLinkHumanLeftHandClosedDL") },
    { (void**)&gPlayerLeftHandClosedDLs[3], MM_LINK("gLinkHumanLeftHandClosedDL") },
    { (void**)&sPlayerRightHandOpenDLs[1], MM_LINK("gLinkHumanRightHandOpenDL") },
    { (void**)&sPlayerRightHandOpenDLs[3], MM_LINK("gLinkHumanRightHandOpenDL") },
    { (void**)&sPlayerRightHandClosedDLs[1], MM_LINK("gLinkHumanRightHandClosedDL") },
    { (void**)&sPlayerRightHandClosedDLs[3], MM_LINK("gLinkHumanRightHandClosedDL") },
    { (void**)&sPlayerRightHandHookshotDLs[1], MM_LINK("gLinkHumanRightHandOpenDL") },
    { (void**)&sPlayerRightHandHookshotDLs[3], MM_LINK("gLinkHumanRightHandOpenDL") },
    { (void**)&sPlayerLeftHandHammerDLs[1], MM_LINK("gLinkHumanLeftHandOpenDL") },
    { (void**)&sPlayerLeftHandHammerDLs[3], MM_LINK("gLinkHumanLeftHandOpenDL") },
    { (void**)&sPlayerWaistDLs[1], MM_LINK("gLinkHumanWaistDL") },
    { (void**)&sPlayerWaistDLs[3], MM_LINK("gLinkHumanWaistDL") },
    { (void**)&sPlayerRightHandOotDLs[1], MM_LINK("gLinkHumanRightHandHoldingOcarinaDL") },
    { (void**)&sPlayerRightHandOotDLs[3], MM_LINK("gLinkHumanRightHandHoldingOcarinaDL") },
};

static s32 sSwapped;

/** SoH's OnSceneInit (before Link is made): MM's young Link in Termina, OoT's elsewhere. */
void NvMmMask_Init(void);

void NvMmLink_SceneInit(s16 sceneNum) {
    s32 want = NvMm_IsScene(sceneNum) && ResourceMgr_FileExists("mm/objects/object_link_child/gLinkHumanSkel") &&
               (getenv("NV_MM_OOT_LINK") == NULL); // (dev: OoT's model, to compare)
    s32 i;

    NvMmMask_Init(); // (MM's masks' icons on the C buttons: before the HUD draws one)
    if (want == sSwapped) {
        return;
    }
    for (i = 0; i < ARRAY_COUNT(sSwaps); i++) {
        if (want) {
            sSwaps[i].oot = *sSwaps[i].slot;
            *sSwaps[i].slot = (void*)sSwaps[i].mm;
        } else {
            *sSwaps[i].slot = sSwaps[i].oot;
        }
    }
    sSwapped = want;
    if (getenv("NV_LOG") != NULL) {
        printf("NVMM young Link: %s model (skeleton %s, eyes %s)\n", want ? "MM's" : "OoT's",
               (const char*)gPlayerSkelHeaders[1], (const char*)sEyeTextures[1][0]);
        fflush(stdout);
    }
}

/** dev (NV_LOG): which skeleton Link was made with (z_nv_mm.c, a scene's first frame). */
void NvMmLink_Check(PlayState* play) {
    Player* player = GET_PLAYER(play);
    FlexSkeletonHeader* mm = (FlexSkeletonHeader*)ResourceGetDataByName("mm/objects/object_link_child/gLinkHumanSkel");

    if ((getenv("NV_LOG") != NULL) && (player != NULL)) {
        FlexSkeletonHeader* oot = (FlexSkeletonHeader*)ResourceGetDataByName("objects/object_link_child/gLinkChildSkel");

        printf("NVMM young Link check: skeleton %p, MM's %p, OoT's %p; table %s, age %d\n",
               (void*)player->skelAnime.skeleton, (mm != NULL) ? (void*)mm->sh.segment : NULL,
               (oot != NULL) ? (void*)oot->sh.segment : NULL, (const char*)gPlayerSkelHeaders[1],
               (int)gSaveContext.linkAge);
        fflush(stdout);
    }
}
