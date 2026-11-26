#ifndef Z_NV_MM_MEETING_H
#define Z_NV_MM_MEETING_H
/*
 * Nightmareverse: the Mayor's meeting (z_nv_mm_mayor.c). MM's En_Dt reaches into En_Muto and
 * En_Baisen to steer them (who looks at whom, when the talk starts and ends); here each keeps these
 * fields as MM's do, and the mayor reaches them through these.
 */
#include "global.h"

typedef struct NvMmMeetingRole {
    s32 cutsceneState;  // MM's: 1 the meeting's talk going on, 2 over
    Actor* targetActor; // the one speaking, looked at
    s16 textIdIndex;    // (MM's index into the actor's lines, set by the mayor for the Couple's Mask)
} NvMmMeetingRole;

NvMmMeetingRole* NvMmMuto_Role(Actor* muto);     // z_nv_mm_townprops.c
NvMmMeetingRole* NvMmBaisen_Role(Actor* baisen); // z_nv_mm_mayor.c

#endif
