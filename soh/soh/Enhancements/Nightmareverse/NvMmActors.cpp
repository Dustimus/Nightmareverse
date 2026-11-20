// Nightmareverse: actors ported from Majora's Mask, registered with SoH's ActorDB on first use (as
// NvActors.cpp does for RE1's). Termina's rooms name them by a marker id, 0x7000 + MM's own actor id
// (mm/tools/mm_actors.py); z_nv_mm_halfday.c swaps the marker for the registered id when the room's
// actors spawn.
#include <unordered_map>
#include "soh/ActorDB.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

extern "C" {
#include "z64.h"

void NvMmTokeidai_Init(Actor* thisx, PlayState* play);
void NvMmTokeidai_Destroy(Actor* thisx, PlayState* play);
void NvMmTokeidai_Update(Actor* thisx, PlayState* play);
extern const size_t gNvMmTokeidaiSize;
void NvMmFall_Init(Actor* thisx, PlayState* play);
void NvMmFall_Destroy(Actor* thisx, PlayState* play);
void NvMmFall_Update(Actor* thisx, PlayState* play);
extern const size_t gNvMmFallSize;
}

struct NvMmActorDef {
    const char* name;
    const char* desc;
    int category;
    u32 flags;
    const size_t* size;
    ActorFunc init, destroy, update;
};

// by MM's actor id
static const std::unordered_map<int, NvMmActorDef> sNvMmActors = {
    { 0x19C,
      { "Mm_Obj_Tokeidai", "MM: the clock tower and wall clocks", ACTORCAT_BG, 0, &gNvMmTokeidaiSize,
        NvMmTokeidai_Init, NvMmTokeidai_Destroy, NvMmTokeidai_Update } },
    { 0x17C,
      { "Mm_En_Fall", "MM: the moon", ACTORCAT_ITEMACTION,
        ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED, &gNvMmFallSize, NvMmFall_Init,
        NvMmFall_Destroy, NvMmFall_Update } },
};

/** The SoH actor id for one of MM's actor ids, or -1 if it isn't ported. */
extern "C" s32 NvMmActor_Id(s32 mmId) {
    static std::unordered_map<int, int> ids;
    auto it = ids.find(mmId);

    if (it != ids.end()) {
        return it->second;
    }
    auto def = sNvMmActors.find(mmId);
    if (def == sNvMmActors.end()) {
        return -1;
    }
    ActorDBInit entry;
    entry.name = def->second.name;
    entry.desc = def->second.desc;
    entry.category = def->second.category;
    entry.flags = def->second.flags;
    entry.objectId = OBJECT_GAMEPLAY_KEEP; // (MM's models are drawn by path: no object slot needed)
    entry.instanceSize = *def->second.size;
    entry.init = def->second.init;
    entry.destroy = def->second.destroy;
    entry.update = def->second.update;
    entry.draw = NULL; // (each piece sets its own in init)
    int id = ActorDB::Instance->AddEntry(entry).entry.id;
    ids[mmId] = id;
    return id;
}
