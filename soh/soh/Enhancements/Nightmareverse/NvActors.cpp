// Nightmareverse: the actors it adds, registered with the ActorDB on first use (an id past
// OoT's own, as IvanCoop.cpp does for Ivan).
#include "soh/ActorDB.h"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

extern "C" {
#include "z64.h"
void NvEnemy_Init(Actor* thisx, PlayState* play);
void NvEnemy_Destroy(Actor* thisx, PlayState* play);
void NvEnemy_Update(Actor* thisx, PlayState* play);
void NvEnemy_Draw(Actor* thisx, PlayState* play);
extern const size_t gNvEnemySize;
void NvItemsActor_Init(Actor* thisx, PlayState* play);
void NvItemsActor_Destroy(Actor* thisx, PlayState* play);
void NvItemsActor_Update(Actor* thisx, PlayState* play);
void NvItemsActor_Draw(Actor* thisx, PlayState* play);
}

// RE1's enemies (soh/src/code/z_nv_enemy.c)
extern "C" s32 NvActor_EnemyId(void) {
    static int id = -1;

    if (id < 0) {
        ActorDBInit entry;
        entry.name = "Nv_Enemy";
        entry.desc = "RE1 enemy";
        entry.category = ACTORCAT_ENEMY;
        entry.flags = ACTOR_FLAG_ATTENTION_ENABLED | ACTOR_FLAG_HOSTILE | ACTOR_FLAG_UPDATE_CULLING_DISABLED |
                      ACTOR_FLAG_DRAW_CULLING_DISABLED;
        entry.objectId = OBJECT_GAMEPLAY_KEEP;
        entry.instanceSize = gNvEnemySize;
        entry.init = NvEnemy_Init;
        entry.destroy = NvEnemy_Destroy;
        entry.update = NvEnemy_Update;
        entry.draw = NvEnemy_Draw;
        id = ActorDB::Instance->AddEntry(entry).entry.id;
        // RE1's enemies don't start OoT's battle music: it would give away a zombie RE1 keeps quiet
        GameInteractor::Instance->RegisterGameHook<GameInteractor::OnVanillaBehavior>(
            [](GIVanillaBehavior flag, bool* should, va_list originalArgs) {
                if (flag != VB_DETECT_BGM_ENEMY) {
                    return;
                }
                va_list args;
                va_copy(args, originalArgs);
                Actor* actor = va_arg(args, Actor*);
                va_end(args);
                if ((actor != nullptr) && (actor->id == id)) {
                    *should = false;
                }
            });
    }
    return id;
}

// The items lying in an RE1 room (soh/src/code/z_nv_items.c): one actor draws them all
extern "C" s32 NvActor_ItemsId(void) {
    static int id = -1;

    if (id < 0) {
        ActorDBInit entry;
        entry.name = "Nv_Items";
        entry.desc = "RE1 room items";
        entry.category = ACTORCAT_PROP;
        entry.flags = ACTOR_FLAG_UPDATE_CULLING_DISABLED | ACTOR_FLAG_DRAW_CULLING_DISABLED;
        entry.objectId = OBJECT_GAMEPLAY_KEEP;
        entry.instanceSize = sizeof(Actor);
        entry.init = NvItemsActor_Init;
        entry.destroy = NvItemsActor_Destroy;
        entry.update = NvItemsActor_Update;
        entry.draw = NvItemsActor_Draw;
        id = ActorDB::Instance->AddEntry(entry).entry.id;
    }
    return id;
}
