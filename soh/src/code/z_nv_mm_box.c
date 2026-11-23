/*
 * Nightmareverse: Termina's chests in MM's own model (MM's object_box). The chest itself stays OoT's
 * En_Box (Link's chest opening is tied to it; MM's chest types are numbered as OoT's), its params'
 * get-item field holding MM's get-item there (z_nv_mm_getitem.c); here only its look, MM's
 * EnBox_PostLimbDraw choice: the small chest plain, the big one gilded, the boss key's ornate. (MM
 * gilds a small chest holding a small key: none in Clock Town; left plain until dungeons come.)
 */
#include "global.h"

#define BOX(name) ((Gfx*)("__OTR__mm/objects/object_box/" name))


/** z_en_box.c's hook (EnBox_UpdateTexture): MM's chest model in Termina; false elsewhere. */
s32 NvMmBox_ChestDLs(PlayState* play, s32 type, Gfx** body, Gfx** lid) {
    if ((play == NULL) || !NvMm_IsScene(play->sceneNum)) {
        return false;
    }
    switch (type) {
        case 2: // ENBOX_TYPE_BIG_ORNATE (OoT's DECORATED_BIG)
            *body = BOX("gBoxChestBaseOrnateDL");
            *lid = BOX("gBoxChestLidOrnateDL");
            break;
        case 5: // the small ones
        case 6:
        case 7:
        case 8:
        case 12:
            *body = BOX("gBoxChestBaseDL");
            *lid = BOX("gBoxChestLidDL");
            break;
        default:
            *body = BOX("gBoxChestBaseGildedDL");
            *lid = BOX("gBoxChestLidGildedDL");
            break;
    }
    return true;
}
