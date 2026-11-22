#ifndef Z_NV_MM_SCHEDULE_H
#define Z_NV_MM_SCHEDULE_H

/* Nightmareverse: MM's schedule scripts (z_nv_mm_schedule.c; MM's z64schedule.h ScheduleOutput). */

#include <libultraship/libultra/types.h>

struct PlayState;

typedef struct NvMmScheduleOutput {
    u8 result;
    s32 time0; // (two times a return_time gives, in the scripts' time: 0 at 6:00)
    s32 time1;
    s32 hasResult;
} NvMmScheduleOutput;

s32 NvMmSchedule_RunScript(struct PlayState* play, u8* script, NvMmScheduleOutput* output);
u16 NvMmSchedule_Time(u8 hour, u8 minute);
u16 NvMmSchedule_Now(void);

#endif
