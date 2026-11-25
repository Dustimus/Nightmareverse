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
void NvMmSyokudai_Init(Actor* thisx, PlayState* play);
void NvMmSyokudai_Destroy(Actor* thisx, PlayState* play);
void NvMmSyokudai_Update(Actor* thisx, PlayState* play);
void NvMmSyokudai_Draw(Actor* thisx, PlayState* play);
extern const size_t gNvMmSyokudaiSize;
void NvMmTsubo_Init(Actor* thisx, PlayState* play);
void NvMmTsubo_Destroy(Actor* thisx, PlayState* play);
void NvMmTsubo_Update(Actor* thisx, PlayState* play);
extern const size_t gNvMmTsuboSize;
void NvMmDoor_Init(Actor* thisx, PlayState* play);
void NvMmDoor_Destroy(Actor* thisx, PlayState* play);
void NvMmDoor_Update(Actor* thisx, PlayState* play);
void NvMmDoor_Draw(Actor* thisx, PlayState* play);
extern const size_t gNvMmDoorSize;
void NvMmPm_Init(Actor* thisx, PlayState* play);
void NvMmPm_Destroy(Actor* thisx, PlayState* play);
void NvMmPm_Update(Actor* thisx, PlayState* play);
void NvMmPm_Draw(Actor* thisx, PlayState* play);
extern const size_t gNvMmPmSize;
void NvMmPst_Init(Actor* thisx, PlayState* play);
void NvMmPst_Destroy(Actor* thisx, PlayState* play);
void NvMmPst_Update(Actor* thisx, PlayState* play);
void NvMmPst_Draw(Actor* thisx, PlayState* play);
extern const size_t gNvMmPstSize;
void NvMmAn_Init(Actor* thisx, PlayState* play);
void NvMmAn_Destroy(Actor* thisx, PlayState* play);
void NvMmAn_Update(Actor* thisx, PlayState* play);
void NvMmAn_Draw(Actor* thisx, PlayState* play);
extern const size_t gNvMmAnSize;
void NvMmAh_Init(Actor* thisx, PlayState* play);
void NvMmAh_Destroy(Actor* thisx, PlayState* play);
void NvMmAh_Update(Actor* thisx, PlayState* play);
void NvMmAh_Draw(Actor* thisx, PlayState* play);
extern const size_t gNvMmAhSize;
void NvMmLight_Init(Actor* thisx, PlayState* play);
void NvMmLight_Destroy(Actor* thisx, PlayState* play);
void NvMmLight_Update(Actor* thisx, PlayState* play);
void NvMmLight_Draw(Actor* thisx, PlayState* play);
extern const size_t gNvMmLightSize;
void NvMmYado_Init(Actor* thisx, PlayState* play);
void NvMmYado_Destroy(Actor* thisx, PlayState* play);
void NvMmYado_Update(Actor* thisx, PlayState* play);
void NvMmYado_Draw(Actor* thisx, PlayState* play);
extern const size_t gNvMmYadoSize;
void NvMmHariko_Init(Actor* thisx, PlayState* play);
void NvMmHariko_Destroy(Actor* thisx, PlayState* play);
void NvMmHariko_Update(Actor* thisx, PlayState* play);
void NvMmHariko_Draw(Actor* thisx, PlayState* play);
extern const size_t gNvMmHarikoSize;
#define NV_MM_SCENERY(Name) \
    void NvMm##Name##_Init(Actor* thisx, PlayState* play); \
    void NvMm##Name##_Destroy(Actor* thisx, PlayState* play); \
    void NvMm##Name##_Update(Actor* thisx, PlayState* play); \
    void NvMm##Name##_Draw(Actor* thisx, PlayState* play); \
    extern const size_t gNvMm##Name##Size;
NV_MM_SCENERY(Turret)
NV_MM_SCENERY(Tree)
NV_MM_SCENERY(MarketStep)
NV_MM_SCENERY(Kinoko)
NV_MM_SCENERY(Kusa)
NV_MM_SCENERY(Kanban)
NV_MM_SCENERY(Warpstone)
NV_MM_SCENERY(HitTag)
NV_MM_SCENERY(CtGear)
NV_MM_SCENERY(CtRot)
NV_MM_SCENERY(Tobira)
NV_MM_SCENERY(Step)
NV_MM_SCENERY(Guruguru)
NV_MM_SCENERY(Rz)
void NvMmEtcetera_Init(Actor* thisx, PlayState* play);
void NvMmEtcetera_Destroy(Actor* thisx, PlayState* play);
void NvMmEtcetera_Update(Actor* thisx, PlayState* play);
void NvMmEtcetera_Draw(Actor* thisx, PlayState* play);
extern const size_t gNvMmEtceteraSize;
void NvMmBubble_Init(Actor* thisx, PlayState* play);
void NvMmBubble_Destroy(Actor* thisx, PlayState* play);
void NvMmBubble_Update(Actor* thisx, PlayState* play);
void NvMmBubble_Draw(Actor* thisx, PlayState* play);
extern const size_t gNvMmBubbleSize;
void NvMmBeam_Init(Actor* thisx, PlayState* play);
void NvMmBeam_Destroy(Actor* thisx, PlayState* play);
void NvMmBeam_Update(Actor* thisx, PlayState* play);
void NvMmBeam_Draw(Actor* thisx, PlayState* play);
extern const size_t gNvMmBeamSize;
void NvMmTalk_Init(Actor* thisx, PlayState* play);
void NvMmTalk_Destroy(Actor* thisx, PlayState* play);
void NvMmTalk_Update(Actor* thisx, PlayState* play);
extern const size_t gNvMmTalkSize;
void NvMmIg_Init(Actor* thisx, PlayState* play);
void NvMmIg_Destroy(Actor* thisx, PlayState* play);
void NvMmIg_Update(Actor* thisx, PlayState* play);
void NvMmIg_Draw(Actor* thisx, PlayState* play);
extern const size_t gNvMmIgSize;
void NvMmNb_Init(Actor* thisx, PlayState* play);
void NvMmNb_Destroy(Actor* thisx, PlayState* play);
void NvMmNb_Update(Actor* thisx, PlayState* play);
void NvMmNb_Draw(Actor* thisx, PlayState* play);
extern const size_t gNvMmNbSize;
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
    { 0x039,
      { "Mm_Obj_Syokudai", "MM: a torch", ACTORCAT_PROP,
        ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_HOOKSHOT_PULLS_PLAYER, &gNvMmSyokudaiSize, NvMmSyokudai_Init,
        NvMmSyokudai_Destroy, NvMmSyokudai_Update, NvMmSyokudai_Draw } },
    { 0x082,
      { "Mm_Obj_Tsubo", "MM: a pot", ACTORCAT_PROP,
        ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_THROW_ONLY | ACTOR_FLAG_CAN_PRESS_SWITCHES, &gNvMmTsuboSize,
        NvMmTsubo_Init, NvMmTsubo_Destroy, NvMmTsubo_Update, NULL } },
    { 0x005,
      { "Mm_En_Door", "MM: a wooden door", ACTORCAT_DOOR, ACTOR_FLAG_UPDATE_CULLING_DISABLED, &gNvMmDoorSize,
        NvMmDoor_Init, NvMmDoor_Destroy, NvMmDoor_Update, NvMmDoor_Draw } },
    { 0x1D5,
      { "Mm_En_Pm", "MM: the Postman", ACTORCAT_NPC,
        ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
            ACTOR_FLAG_DRAW_CULLING_DISABLED,
        &gNvMmPmSize, NvMmPm_Init, NvMmPm_Destroy, NvMmPm_Update, NvMmPm_Draw } },
    { 0x1F2,
      { "Mm_En_Pst", "MM: a mailbox", ACTORCAT_PROP, ACTOR_FLAG_ATTENTION_ENABLED, &gNvMmPstSize, NvMmPst_Init,
        NvMmPst_Destroy, NvMmPst_Update, NvMmPst_Draw } },
    { 0x202,
      { "Mm_En_An", "MM: Anju", ACTORCAT_NPC,
        ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
            ACTOR_FLAG_DRAW_CULLING_DISABLED,
        &gNvMmAnSize, NvMmAn_Init, NvMmAn_Destroy, NvMmAn_Update, NvMmAn_Draw } },
    { 0x253,
      { "Mm_En_Ah", "MM: Anju's mother", ACTORCAT_NPC,
        ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
            ACTOR_FLAG_DRAW_CULLING_DISABLED,
        &gNvMmAhSize, NvMmAh_Init, NvMmAh_Destroy, NvMmAh_Update, NvMmAh_Draw } },
    { 0x243,
      { "Mm_En_Nb", "MM: Anju's grandmother", ACTORCAT_NPC,
        ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
            ACTOR_FLAG_DRAW_CULLING_DISABLED,
        &gNvMmNbSize, NvMmNb_Init, NvMmNb_Destroy, NvMmNb_Update, NvMmNb_Draw } },
    { 0x276,
      { "Mm_En_Ig", "MM: the Goron at the inn", ACTORCAT_NPC,
        ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
            ACTOR_FLAG_DRAW_CULLING_DISABLED,
        &gNvMmIgSize, NvMmIg_Init, NvMmIg_Destroy, NvMmIg_Update, NvMmIg_Draw } },
    { 0x004,
      { "Mm_En_Light", "MM: a flame or lamp", ACTORCAT_ITEMACTION, ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED, &gNvMmLightSize, NvMmLight_Init,
        NvMmLight_Destroy, NvMmLight_Update, NvMmLight_Draw } },
    { 0x294,
      { "Mm_Obj_Yado", "MM: the inn's upstairs window", ACTORCAT_BG, ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED, &gNvMmYadoSize, NvMmYado_Init,
        NvMmYado_Destroy, NvMmYado_Update, NvMmYado_Draw } },
    { 0x20A,
      { "Mm_Obj_Hariko", "MM: the bobblehead cow toy", ACTORCAT_PROP, 0, &gNvMmHarikoSize, NvMmHariko_Init,
        NvMmHariko_Destroy, NvMmHariko_Update, NvMmHariko_Draw } },
    { 0x221,
      { "Mm_Obj_Tokei_Turret", "MM: a carnival platform or flags", ACTORCAT_BG, 0, &gNvMmTurretSize, NvMmTurret_Init,
        NvMmTurret_Destroy, NvMmTurret_Update, NvMmTurret_Draw } },
    { 0x229,
      { "Mm_Obj_Tree", "MM: a branching tree", ACTORCAT_PROP, 0, &gNvMmTreeSize, NvMmTree_Init, NvMmTree_Destroy,
        NvMmTree_Update, NvMmTree_Draw } },
    { 0x1CC,
      { "Mm_Bg_Market_Step", "MM: West Clock Town's market stalls", ACTORCAT_BG,
        ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED, &gNvMmMarketStepSize,
        NvMmMarketStep_Init, NvMmMarketStep_Destroy, NvMmMarketStep_Update, NvMmMarketStep_Draw } },
    { 0x23B,
      { "Mm_Obj_Kinoko", "MM: a magic mushroom", ACTORCAT_ITEMACTION, ACTOR_FLAG_UPDATE_CULLING_DISABLED, &gNvMmKinokoSize, NvMmKinoko_Init,
        NvMmKinoko_Destroy, NvMmKinoko_Update, NULL } },
    { 0x090,
      { "Mm_En_Kusa", "MM: grass or a bush", ACTORCAT_PROP, ACTOR_FLAG_THROW_ONLY, &gNvMmKusaSize, NvMmKusa_Init,
        NvMmKusa_Destroy, NvMmKusa_Update, NULL } },
    { 0x0A8,
      { "Mm_En_Kanban", "MM: a square signpost", ACTORCAT_PROP,
        ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY | ACTOR_FLAG_UPDATE_CULLING_DISABLED, &gNvMmKanbanSize,
        NvMmKanban_Init, NvMmKanban_Destroy, NvMmKanban_Update, NvMmKanban_Draw } },
    { 0x223,
      { "Mm_Obj_Warpstone", "MM: an owl statue", ACTORCAT_ITEMACTION, ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY,
        &gNvMmWarpstoneSize, NvMmWarpstone_Init, NvMmWarpstone_Destroy, NvMmWarpstone_Update, NvMmWarpstone_Draw } },
    { 0x265,
      { "Mm_En_Hit_Tag", "MM: hidden rupees", ACTORCAT_ITEMACTION, ACTOR_FLAG_UPDATE_CULLING_DISABLED, &gNvMmHitTagSize,
        NvMmHitTag_Init, NvMmHitTag_Destroy, NvMmHitTag_Update, NULL } },
    { 0x1B6,
      { "Mm_Bg_Ctower_Gear", "MM: the clock tower's cogs", ACTORCAT_PROP, ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED,
        &gNvMmCtGearSize, NvMmCtGear_Init, NvMmCtGear_Destroy, NvMmCtGear_Update, NvMmCtGear_Draw } },
    { 0x061,
      { "Mm_Bg_Ctower_Rot", "MM: the twisting corridor", ACTORCAT_BG, ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED, &gNvMmCtRotSize,
        NvMmCtRot_Init, NvMmCtRot_Destroy, NvMmCtRot_Update, NvMmCtRot_Draw } },
    { 0x1A2,
      { "Mm_Obj_Tokei_Tobira", "MM: the clock tower's doors", ACTORCAT_BG, 0, &gNvMmTobiraSize, NvMmTobira_Init,
        NvMmTobira_Destroy, NvMmTobira_Update, NvMmTobira_Draw } },
    { 0x1B8,
      { "Mm_Obj_Tokei_Step", "MM: the clock tower's steps", ACTORCAT_BG, ACTOR_FLAG_UPDATE_CULLING_DISABLED,
        &gNvMmStepSize, NvMmStep_Init, NvMmStep_Destroy, NvMmStep_Update, NvMmStep_Draw } },
    { 0x248,
      { "Mm_En_Guruguru", "MM: Guru-Guru", ACTORCAT_NPC,
        ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY | ACTOR_FLAG_UPDATE_CULLING_DISABLED, &gNvMmGuruguruSize,
        NvMmGuruguru_Init, NvMmGuruguru_Destroy, NvMmGuruguru_Update, NvMmGuruguru_Draw } },
    { 0x27B,
      { "Mm_En_Rz", "MM: a Rosa sister", ACTORCAT_NPC, ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_FRIENDLY, &gNvMmRzSize,
        NvMmRz_Init, NvMmRz_Destroy, NvMmRz_Update, NvMmRz_Draw } },
    { 0x183,
      { "Mm_Obj_Etcetera", "MM: a Deku flower", ACTORCAT_BG, ACTOR_FLAG_UPDATE_CULLING_DISABLED, &gNvMmEtceteraSize,
        NvMmEtcetera_Init, NvMmEtcetera_Destroy, NvMmEtcetera_Update, NvMmEtcetera_Draw } },
    { 0x00F, // (MM's En_Arrow: here only its Deku bubble, spawned by Deku Link, never by a room)
      { "Mm_En_Arrow_Bubble", "MM: Deku Link's bubble", ACTORCAT_ITEMACTION, ACTOR_FLAG_UPDATE_CULLING_DISABLED, &gNvMmBubbleSize,
        NvMmBubble_Init, NvMmBubble_Destroy, NvMmBubble_Update, NvMmBubble_Draw } },
    { 0x035, // (MM's En_M_Thunder: here only the Fierce Deity's sword beam, spawned by him)
      { "Mm_En_M_Thunder_Beam", "MM: the Fierce Deity's sword beam", ACTORCAT_ITEMACTION, ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED,
        &gNvMmBeamSize, NvMmBeam_Init, NvMmBeam_Destroy, NvMmBeam_Update, NvMmBeam_Draw } },
    { 0x261,
      { "Mm_En_Talk", "MM: something to read or check", ACTORCAT_ITEMACTION, ACTOR_FLAG_ATTENTION_ENABLED, &gNvMmTalkSize, NvMmTalk_Init,
        NvMmTalk_Destroy, NvMmTalk_Update, NULL } },
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

/** A scene's transition actor (its doors) as SoH's: MM's ported ones are written 0x1000 + MM's id
 * in Termina's scenes (mm_actors.py doors()); anything else is as it was. Called from
 * Actor_SpawnTransitionActors (z_actor.c). */
extern "C" s16 NvMmActor_TransitionId(PlayState* play, s16 id) {
    if ((id & 0x1000) && NvMm_IsScene(play->sceneNum)) {
        s32 soh = NvMmActor_Id(id & 0xFFF);

        return (soh >= 0) ? soh : id;
    }
    return id;
}

// Termina's young Link is MM's model (z_nv_mm_link.c): its tables set on each scene's start, before
// Link is made.
#include "soh/ShipInit.hpp"
extern "C" void NvMmLink_SceneInit(s16 sceneNum);

static void NvMmLink_Register() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnSceneInit>(
        [](int16_t sceneNum) { NvMmLink_SceneInit(sceneNum); });
}

static RegisterShipInitFunc sNvMmLinkInit(NvMmLink_Register);
