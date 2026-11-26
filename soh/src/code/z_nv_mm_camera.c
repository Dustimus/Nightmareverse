/*
 * Nightmareverse: MM's scene cameras in OoT's camera (mm/README.md "MM's camera settings").
 *
 * A room's collision gives each floor a camera setting. Torch keeps MM's lists in OoT's layout (the
 * same shape), but MM numbers its settings differently: read as OoT's, most land on something
 * unrelated (MM's DUNGEON3, all over Clock Town, is OoT's Bongo Bongo camera; MM's CIRCLE0 in the
 * Curiosity Shop is OoT's twisted-hallway cutscene camera, which crashed with no cutscene). Here MM's
 * setting becomes OoT's of the same original name where OoT has one (NORMAL0-2, DUNGEON0-2, FIXED0,
 * CIRCLE0, CIRCLE3), and OoT's NORMAL0 otherwise (MM's ROOM0/1, DUNGEON3, the inn's and the
 * minigames' own). Hooked where OoT reads the setting (z_bgcheck.c, one line); MM's scenes only.
 */
#include "global.h"

extern PlayState* gPlayState;

u16 NvMmCam_Setting(u16 mmSetting) {
    if ((gPlayState == NULL) || !NvMm_IsScene(gPlayState->sceneNum)) {
        return mmSetting;
    }
    switch (mmSetting) { // (MM's CameraSettingType values, by MM's original names)
        case 0x00:                  // NONE
            return CAM_SET_NONE;
        case 0x0C:                  // NORMAL1
            return CAM_SET_NORMAL1;
        case 0x4A:                  // NORMAL2
            return CAM_SET_NORMAL2;
        case 0x11:                  // DUNGEON0
            return CAM_SET_DUNGEON0;
        case 0x12:                  // DUNGEON1
            return CAM_SET_DUNGEON1;
        case 0x32:                  // DUNGEON2
            return CAM_SET_DUNGEON2;
        case 0x0F:                  // FIXED0 (the Milk Bar's stage...)
            return CAM_SET_MARKET_BALCONY; // (OoT's "FIXED0")
        case 0x0E:                  // CIRCLE0
            return CAM_SET_PIVOT_CRAWLSPACE; // (OoT's "CIRCLE0")
        case 0x25:                  // CIRCLE3
            return CAM_SET_PIVOT_IN_FRONT;   // (OoT's "CIRCLE3")
        default:                    // NORMAL0, ROOM0/1, DUNGEON3, FIXED2/3, RCIRC0, INBUSH, the minigames'
            return CAM_SET_NORMAL0;
    }
}
