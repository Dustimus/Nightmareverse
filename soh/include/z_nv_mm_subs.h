#ifndef Z_NV_MM_SUBS_H
#define Z_NV_MM_SUBS_H

/* Nightmareverse: MM's shared helpers for its people (z_nv_mm_subs.c; MM's z_sub_s.c). */

#define NV_MM_TIME_PATHING_ORDER 3

struct PlayState;
struct Actor;

u8 NvMmSubS_AdditionalPathIndex(struct PlayState* play, s32 pathIndex);
s16 NvMmSubS_PathCustomValue(struct PlayState* play, s32 pathIndex);
Path* NvMmSubS_GetAdditionalPath(struct PlayState* play, u8 pathIndex, s32 limit);
void NvMmSubS_TimePathing_FillKnots(f32 knots[], s32 order, s32 numPoints);
s32 NvMmSubS_TimePathing_Update(Path* path, f32* progress, s32* elapsedTime, s32 waypointTime, s32 totalTime,
                                s32* waypoint, f32 knots[], Vec3f* targetPos, s32 timeSpeed);
void NvMmSubS_TimePathing_ComputeInitialY(struct PlayState* play, Path* path, s32 waypoint, Vec3f* targetPos);
struct Actor* NvMmSubS_FindActor(struct PlayState* play, struct Actor* from, u8 category, s16 id);

#endif
