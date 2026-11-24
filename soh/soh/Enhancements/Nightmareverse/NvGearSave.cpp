// Nightmareverse: the gear page's own state (z_nv_gear.c) as the "gear" section of SoH's save file:
// the Giant's Knife and Biggoron's Sword owned apart (the user, 2026-11-24: two items, not OoT's one
// slot). OoT's slot still holds whichever is in hand, in OoT's own save; this keeps the other.
// A save from before this has no section: what OoT's slot holds is read back into it as it plays.
#include <cstdint>
#include "soh/SaveManager.h"
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

extern "C" {
typedef struct {
    int32_t knife;
    int32_t bgs;
    float knifeHealth;
    int32_t gfs;
} NvGearSave;
NvGearSave* NvGear_SaveVars(void);
void NvGear_NewGame(void);
}

static bool sGearLoaded;

static void NvGearSave_Save(const SaveContext& saveContext, int sectionID, bool fullSave) {
    NvGearSave* g = NvGear_SaveVars();

    SaveManager::Instance->SaveData("knife", g->knife);
    SaveManager::Instance->SaveData("bgs", g->bgs);
    SaveManager::Instance->SaveData("knifeHealth", g->knifeHealth);
    SaveManager::Instance->SaveData("gfs", g->gfs);
}

static void NvGearSave_Load() {
    NvGearSave* g = NvGear_SaveVars();

    NvGear_NewGame();
    SaveManager::Instance->LoadData("knife", g->knife, (int32_t)0);
    SaveManager::Instance->LoadData("bgs", g->bgs, (int32_t)0);
    SaveManager::Instance->LoadData("knifeHealth", g->knifeHealth, 0.0f);
    SaveManager::Instance->LoadData("gfs", g->gfs, (int32_t)0);
    sGearLoaded = true;
}

static void NvGearSave_Register() {
    SaveManager::Instance->AddInitFunction([](bool isDebug) { NvGear_NewGame(); });
    SaveManager::Instance->AddSaveFunction("gear", 1, NvGearSave_Save, true, SECTION_PARENT_NONE);
    SaveManager::Instance->AddLoadFunction("gear", 1, NvGearSave_Load);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t fileNum) {
        if (!sGearLoaded) {
            NvGear_NewGame(); // (not another file's)
        }
        sGearLoaded = false;
    });
}

static RegisterShipInitFunc sNvGearSaveInit(NvGearSave_Register);
