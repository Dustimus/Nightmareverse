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
    }
    return id;
}
