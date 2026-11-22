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
void NvMmDaiku_Init(Actor* thisx, PlayState* play);
void NvMmDaiku_Destroy(Actor* thisx, PlayState* play);
void NvMmDaiku_Update(Actor* thisx, PlayState* play);
void NvMmDaiku_Draw(Actor* thisx, PlayState* play);
extern const size_t gNvMmDaikuSize;
void NvMmStopheishi_Init(Actor* thisx, PlayState* play);
void NvMmStopheishi_Destroy(Actor* thisx, PlayState* play);
void NvMmStopheishi_Update(Actor* thisx, PlayState* play);
void NvMmStopheishi_Draw(Actor* thisx, PlayState* play);
extern const size_t gNvMmStopheishiSize;
void NvMmBombers_Init(Actor* thisx, PlayState* play);
void NvMmBombers_Destroy(Actor* thisx, PlayState* play);
void NvMmBombers_Update(Actor* thisx, PlayState* play);
void NvMmBombers_Draw(Actor* thisx, PlayState* play);
extern const size_t gNvMmBombersSize;
void NvMmEntotu_Init(Actor* thisx, PlayState* play);
void NvMmEntotu_Destroy(Actor* thisx, PlayState* play);
void NvMmEntotu_Update(Actor* thisx, PlayState* play);
extern const size_t gNvMmEntotuSize;
void NvMmKibako_Init(Actor* thisx, PlayState* play);
void NvMmKibako_Destroy(Actor* thisx, PlayState* play);
void NvMmKibako_Update(Actor* thisx, PlayState* play);
extern const size_t gNvMmKibakoSize;
void NvMmEntotu_Draw(Actor* thisx, PlayState* play);
}

struct NvMmActorDef {
    const char* name;
    const char* desc;
    int category;
    u32 flags;
    const size_t* size;
    ActorFunc init, destroy, update;
    ActorFunc draw = nullptr;
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
    { 0x09C,
      { "Mm_En_Daiku", "MM: a carpenter", ACTORCAT_NPC, ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY,
        &gNvMmDaikuSize, NvMmDaiku_Init, NvMmDaiku_Destroy, NvMmDaiku_Update, NvMmDaiku_Draw } },
    { 0x1C7,
      { "Mm_En_Stop_heishi", "MM: a gate guard in Clock Town", ACTORCAT_NPC,
        ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY, &gNvMmStopheishiSize, NvMmStopheishi_Init,
        NvMmStopheishi_Destroy, NvMmStopheishi_Update, NvMmStopheishi_Draw } },
    { 0x280,
      { "Mm_En_Bombers", "MM: a Bomber (the blue-hatted kids)", ACTORCAT_NPC,
        ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY, &gNvMmBombersSize, NvMmBombers_Init, NvMmBombers_Destroy,
        NvMmBombers_Update, NvMmBombers_Draw } },
    { 0x14D,
      { "Mm_Obj_Entotu", "MM: a Clock Town chimney", ACTORCAT_PROP,
        ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED, &gNvMmEntotuSize, NvMmEntotu_Init,
        NvMmEntotu_Destroy, NvMmEntotu_Update, NvMmEntotu_Draw } },
    { 0x081,
      { "Mm_Obj_Kibako", "MM: a small crate", ACTORCAT_PROP,
        ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_CAN_PRESS_SWITCHES, &gNvMmKibakoSize, NvMmKibako_Init,
        NvMmKibako_Destroy, NvMmKibako_Update, NULL } },
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
    entry.draw = def->second.draw; // (or none: the actor sets its own in init)
    int id = ActorDB::Instance->AddEntry(entry).entry.id;
    ids[mmId] = id;
    return id;
}
