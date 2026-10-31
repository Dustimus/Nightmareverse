/*
 * Nightmareverse: RE1's files (documents), the status screen's File tab (z_nv_status.c).
 *
 * The files Jill has read, in the order she found them (RE1's file_set_collected_flag: a document's
 * item id less 0x5F). Their pages come with the reader.
 */
#include "global.h"
#include <stdio.h>

#define NV_FILES_MAX 32

static u8 sNvFiles[NV_FILES_MAX];
static s32 sNvFileCount;

/** Files found so far. */
s32 NvFiles_Count(void) {
    return sNvFileCount;
}

/** A document picked up: RE1's file number (item id - 0x5F). False if it was already there. */
s32 NvFiles_Add(s32 file) {
    s32 k;

    for (k = 0; k < sNvFileCount; k++) {
        if (sNvFiles[k] == file) {
            return false;
        }
    }
    if (sNvFileCount < NV_FILES_MAX) {
        sNvFiles[sNvFileCount++] = file;
    }
    return true;
}

/** The k-th file's name in the list: our own plain label. */
const char* NvFiles_Name(s32 k) {
    static char sName[16];

    snprintf(sName, sizeof(sName), "File %d", (k >= 0 && k < sNvFileCount) ? sNvFiles[k] + 1 : 0);
    return sName;
}

void NvFiles_Open(PlayState* play, s32 k) {
}
