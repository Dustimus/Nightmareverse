/*
 * Nightmareverse: Majora's Mask's schedule scripts, from MM's z_schedule.c (zeldaret/mm), ported
 * closely (mm/README.md). MM's people and doors ask a small script where they should be, or
 * whether they're open, by the day, the time, the scene and week events. The scripts are MM's
 * bytecode, compiled from the decomp's .schl files by mm/tools/mm_schedule.py into
 * soh/src/code/nv_mm_schedules/ (the ported actors include theirs).
 *
 * Commands (MM's numbers): 00/01 week event set, 02/03 time in range, 04 return a value (two
 * bytes, kept to one as in MM), 05 return none, 06 return as is, 07 nothing, 08 an item or mask
 * check, 09 return a value, 0A/0B not this scene, 0C/0D not this day, 0E return two times and a
 * value, 0F/10 before a time, 11/12 branch. The S forms branch by a byte, the L forms by two;
 * a branch is from the end of its command.
 *
 * Adapted from MM:
 * - the day, the time and week events are Termina's (z_nv_mm_clock.c, z_nv_mm_scene.c); the
 *   scene check compares OoT's scene numbers (the compiler turned MM's scenes into ours);
 * - command 08's checks (the Room Key, the Letter to Kafei, the Romani Mask) are false until those
 *   items are in.
 */
#include "global.h"
#include "z_nv_mm_schedule.h"

s32 NvMmClock_Day(void);
u16 NvMmClock_Time(void);
s32 NvMm_CheckWeekEvent(s32 flag);

#define PACK_16(h, l) (((h) << 8) | (l))

/** MM's SCRIPT_CALC_TIME: an hour and minute as the scripts compare times (0 at 6:00). */
u16 NvMmSchedule_Time(u8 hour, u8 minute) {
    f32 temp = hour * 60.0f;
    u16 dest;

    temp += minute;
    dest = temp * (0x10000 / 60 / 24.0f);
    return (u16)(dest - 0x10000 / 360 * 90);
}

/** MM's SCRIPT_TIME_NOW. */
u16 NvMmSchedule_Now(void) {
    return (u16)(NvMmClock_Time() - 0x10000 / 360 * 90);
}

static s32 Check(s32 holds, u8** script, s16 offset) {
    if (holds) {
        *script += offset;
    }
    return false;
}

static s32 TimeInRange(u8* cmd) {
    u16 start = NvMmSchedule_Time(cmd[1], cmd[2]);
    u16 end = NvMmSchedule_Time(cmd[3], cmd[4]);
    u16 now = NvMmSchedule_Now();

    end--;
    return (start <= now) && (now <= end);
}

static s32 BeforeTime(u8* cmd) {
    return NvMmSchedule_Now() < NvMmSchedule_Time(cmd[1], cmd[2]);
}

static s32 NotInScene(PlayState* play, u8* cmd) {
    return (s16)PACK_16(cmd[1], cmd[2]) != play->sceneNum;
}

static s32 NotInDay(u8* cmd) {
    return (s16)PACK_16(cmd[1], cmd[2]) != (s16)NvMmClock_Day();
}

static s32 Misc(PlayState* play, u8 which) {
    return false; // (MM: the Room Key, the Letter to Kafei, the Romani Mask; not in yet)
}

static u8 sCmdSizes[] = { 4, 5, 6, 7, 3, 1, 1, 4, 3, 2, 4, 5, 4, 5, 6, 4, 5, 2, 3 };

s32 NvMmSchedule_RunScript(PlayState* play, u8* script, NvMmScheduleOutput* output) {
    s32 stop;

    do {
        u8* cmd = script;
        u8 id = *cmd;

        stop = false;
        switch (id) {
            case 0x00:
                Check(NvMm_CheckWeekEvent(PACK_16(cmd[1], cmd[2])), &script, (s8)cmd[3]);
                break;
            case 0x01:
                Check(NvMm_CheckWeekEvent(PACK_16(cmd[1], cmd[2])), &script, (s16)PACK_16(cmd[3], cmd[4]));
                break;
            case 0x02:
                Check(TimeInRange(cmd), &script, (s8)cmd[5]);
                break;
            case 0x03:
                Check(TimeInRange(cmd), &script, (s16)PACK_16(cmd[5], cmd[6]));
                break;
            case 0x04: //! (MM's bug, kept: the result is one byte)
                output->result = PACK_16(cmd[1], cmd[2]);
                output->hasResult = true;
                stop = true;
                break;
            case 0x05:
                output->hasResult = false;
                stop = true;
                break;
            case 0x06:
                output->hasResult = true;
                stop = true;
                break;
            case 0x07:
                break;
            case 0x08:
                Check(Misc(play, cmd[1]), &script, (s8)cmd[2]);
                break;
            case 0x09:
                output->result = cmd[1];
                output->hasResult = true;
                stop = true;
                break;
            case 0x0A:
                Check(NotInScene(play, cmd), &script, (s8)cmd[3]);
                break;
            case 0x0B:
                Check(NotInScene(play, cmd), &script, (s16)PACK_16(cmd[3], cmd[4]));
                break;
            case 0x0C:
                Check(NotInDay(cmd), &script, (s8)cmd[3]);
                break;
            case 0x0D:
                Check(NotInDay(cmd), &script, (s16)PACK_16(cmd[3], cmd[4]));
                break;
            case 0x0E: {
                u16 time0 = NvMmSchedule_Time(cmd[1], cmd[2]);
                u16 time1 = NvMmSchedule_Time(cmd[3], cmd[4]);

                time1--;
                output->result = cmd[5];
                output->time0 = time0;
                output->time1 = time1;
                output->hasResult = true;
                stop = true;
                break;
            }
            case 0x0F:
                Check(BeforeTime(cmd), &script, (s8)cmd[3]);
                break;
            case 0x10:
                Check(BeforeTime(cmd), &script, (s16)PACK_16(cmd[3], cmd[4]));
                break;
            case 0x11:
                script += (s8)cmd[1];
                break;
            case 0x12:
                script += (s16)PACK_16(cmd[1], cmd[2]);
                break;
            default: // (not MM's: a bad script stops with nothing rather than running off)
                output->hasResult = false;
                return false;
        }
        script += sCmdSizes[id];
    } while (!stop);
    return output->hasResult;
}
