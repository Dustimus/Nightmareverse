// Nightmareverse: Termina's state (Majora's Mask) as the "termina" section of SoH's own save file.
//
// Decided with the user (2026-11-19, mm/README.md "Saving"): one file, so Hyrule and Termina are
// always saved and loaded together, and nothing can be taken from one and kept in an older copy of
// the other. Each game keeps to its own section (SaveManager: sections by name, a second register
// of the same name is refused). Two parts:
//   "cycle"     what the Song of Time resets: the day, the time, the speed;
//   "permanent" what it never resets: each Termina scene's collectibles and visited rooms, MM's own
//               items (those the Song of Time takes are cleared at the reset); later randomizer
//               checks taken.
//   Per-scene flags live in z_nv_mm_scene.c; "cycle" holds their chests, switches, cleared rooms.
#include <cstdint>
#include "soh/SaveManager.h"
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/game-interactor/GameInteractor.h"

extern "C" {
typedef struct {
    int32_t day;
    uint16_t time;
    int16_t speedOffset;
    int32_t inTermina;
    uint16_t hyruleTime;
} NvMmClockSave;
void NvMmClock_GetSave(NvMmClockSave* s);
void NvMmClock_SetSave(const NvMmClockSave* s);
void NvMmClock_NewGame(void);
#include "z64save.h"
int32_t NvMmScene_Count(void);
const char* NvMmScene_Name(int32_t i);
SavedSceneFlags* NvMmScene_At(int32_t i);
uint8_t* NvMm_WeekEventReg(void);
uint8_t* NvMmItem_Array(void);
typedef struct {
    int32_t form;
    int32_t savedB;
    int32_t savedSwordless;
    int32_t sceneSeen;
} NvMmFormSave;
NvMmFormSave* NvMmForm_SaveVars(void);
uint16_t* NvMmOwl_Activated(void);
int32_t NvMmOwl_SaveMark(void);
void NvMmOwl_SetSaveMark(int32_t owl);
void NvMmOwl_OnLoad(void);
uint8_t* NvMmMsg_BomberCode(void);
void NvMmMsg_NewBomberCode(void);
int32_t* NvMmBank_Rupees(void);
}

static bool sTerminaLoaded; // this load had a "termina" section

static void NvMmSave_Save(const SaveContext& saveContext, int sectionID, bool fullSave) {
    NvMmClockSave c;

    NvMmClock_GetSave(&c);
    SaveManager::Instance->SaveStruct("cycle", [&]() {
        SaveManager::Instance->SaveData("day", c.day);
        SaveManager::Instance->SaveData("time", c.time);
        SaveManager::Instance->SaveData("speedOffset", c.speedOffset);
        SaveManager::Instance->SaveData("inTermina", c.inTermina);
        SaveManager::Instance->SaveData("hyruleTime", c.hyruleTime);
        SaveManager::Instance->SaveStruct("scenes", []() { // chests, switches, cleared rooms
            for (int32_t i = 0; i < NvMmScene_Count(); i++) {
                SavedSceneFlags* f = NvMmScene_At(i);
                SaveManager::Instance->SaveStruct(NvMmScene_Name(i), [f]() {
                    SaveManager::Instance->SaveData("chest", f->chest);
                    SaveManager::Instance->SaveData("swch", f->swch);
                    SaveManager::Instance->SaveData("clear", f->clear);
                    SaveManager::Instance->SaveData("unk", f->unk);
                });
            }
        });
    });
    SaveManager::Instance->SaveStruct("cycleWeekEvents", []() { // MM's weekEventReg (cleared each cycle)
        uint8_t* w = NvMm_WeekEventReg();
        SaveManager::Instance->SaveArray("weekEventReg", 100, [w](size_t i) { SaveManager::Instance->SaveData("", w[i]); });
    });
    SaveManager::Instance->SaveStruct("permanent", []() {
        // MM's own items (z_nv_mm_getitem.c: masks, trade items...; the Song of Time clears its share)
        uint8_t* items = NvMmItem_Array();
        SaveManager::Instance->SaveArray("mmItems", 256, [items](size_t i) { SaveManager::Instance->SaveData("", items[i]); });
        NvMmFormSave* form = NvMmForm_SaveVars(); // Link's form (z_nv_mm_form.c: kept, as MM)
        SaveManager::Instance->SaveData("form", form->form);
        SaveManager::Instance->SaveData("formSavedB", form->savedB);
        SaveManager::Instance->SaveData("formSavedSwordless", form->savedSwordless);
        SaveManager::Instance->SaveData("formSceneSeen", form->sceneSeen);
        SaveManager::Instance->SaveData("owls", *NvMmOwl_Activated()); // (MM's owlActivationFlags)
        SaveManager::Instance->SaveData("owlSave", NvMmOwl_SaveMark()); // (a save made at an owl: resumes there)
        uint8_t* code = NvMmMsg_BomberCode(); // (MM's bomberCode: made with a new file)
        SaveManager::Instance->SaveArray("bomberCode", 5, [code](size_t i) { SaveManager::Instance->SaveData("", code[i]); });
        SaveManager::Instance->SaveData("bankRupees", *NvMmBank_Rupees()); // (MM's bank: kept through the cycles)
        SaveManager::Instance->SaveStruct("scenes", []() { // collectibles, visited rooms and floors
            for (int32_t i = 0; i < NvMmScene_Count(); i++) {
                SavedSceneFlags* f = NvMmScene_At(i);
                SaveManager::Instance->SaveStruct(NvMmScene_Name(i), [f]() {
                    SaveManager::Instance->SaveData("collect", f->collect);
                    SaveManager::Instance->SaveData("rooms", f->rooms);
                    SaveManager::Instance->SaveData("floors", f->floors);
                });
            }
        });
    });
}

static void NvMmSave_Load() {
    NvMmClockSave c = { 1, 0x4000, 0, 0, 0 };

    SaveManager::Instance->LoadStruct("cycle", [&]() {
        SaveManager::Instance->LoadData("day", c.day, 1);
        SaveManager::Instance->LoadData("time", c.time, (uint16_t)0x4000);
        SaveManager::Instance->LoadData("speedOffset", c.speedOffset, (int16_t)0);
        SaveManager::Instance->LoadData("inTermina", c.inTermina, 0);
        SaveManager::Instance->LoadData("hyruleTime", c.hyruleTime, (uint16_t)0);
    });
    NvMmClock_NewGame(); // (clears the scenes' flags; the clock is set just below)
    SaveManager::Instance->LoadStruct("cycle", []() {
        SaveManager::Instance->LoadStruct("scenes", []() {
            for (int32_t i = 0; i < NvMmScene_Count(); i++) {
                SavedSceneFlags* f = NvMmScene_At(i);
                SaveManager::Instance->LoadStruct(NvMmScene_Name(i), [f]() {
                    SaveManager::Instance->LoadData("chest", f->chest, 0u);
                    SaveManager::Instance->LoadData("swch", f->swch, 0u);
                    SaveManager::Instance->LoadData("clear", f->clear, 0u);
                    SaveManager::Instance->LoadData("unk", f->unk, 0u);
                });
            }
        });
    });
    SaveManager::Instance->LoadStruct("cycleWeekEvents", []() {
        uint8_t* w = NvMm_WeekEventReg();
        SaveManager::Instance->LoadArray("weekEventReg", 100, [w](size_t i) { SaveManager::Instance->LoadData("", w[i], (uint8_t)0); });
    });
    SaveManager::Instance->LoadStruct("permanent", []() {
        uint8_t* items = NvMmItem_Array();
        SaveManager::Instance->LoadArray("mmItems", 256, [items](size_t i) { SaveManager::Instance->LoadData("", items[i], (uint8_t)0); });
        NvMmFormSave* form = NvMmForm_SaveVars();
        SaveManager::Instance->LoadData("form", form->form, (int32_t)4);
        SaveManager::Instance->LoadData("formSavedB", form->savedB, (int32_t)0xFF);
        SaveManager::Instance->LoadData("formSavedSwordless", form->savedSwordless, (int32_t)0);
        SaveManager::Instance->LoadData("formSceneSeen", form->sceneSeen, (int32_t)0);
        SaveManager::Instance->LoadData("owls", *NvMmOwl_Activated(), (uint16_t)0);
        int32_t owlSave = -1;
        SaveManager::Instance->LoadData("owlSave", owlSave, (int32_t)-1);
        NvMmOwl_SetSaveMark(owlSave);
        uint8_t* code = NvMmMsg_BomberCode();
        SaveManager::Instance->LoadArray("bomberCode", 5, [code](size_t i) { SaveManager::Instance->LoadData("", code[i], (uint8_t)0); });
        if (code[0] == 0) { // (a file from before the code)
            NvMmMsg_NewBomberCode();
        }
        SaveManager::Instance->LoadData("bankRupees", *NvMmBank_Rupees(), (int32_t)0);
        SaveManager::Instance->LoadStruct("scenes", []() {
            for (int32_t i = 0; i < NvMmScene_Count(); i++) {
                SavedSceneFlags* f = NvMmScene_At(i);
                SaveManager::Instance->LoadStruct(NvMmScene_Name(i), [f]() {
                    SaveManager::Instance->LoadData("collect", f->collect, 0u);
                    SaveManager::Instance->LoadData("rooms", f->rooms, 0u);
                    SaveManager::Instance->LoadData("floors", f->floors, 0u);
                });
            }
        });
    });
    NvMmClock_SetSave(&c);
    sTerminaLoaded = true;
}

static void NvMmSave_InitFile(bool isDebug) {
    NvMmClock_NewGame();
    *NvMmOwl_Activated() = 0;
    NvMmOwl_SetSaveMark(-1);
    NvMmMsg_NewBomberCode();
    *NvMmBank_Rupees() = 0;
}

static void NvMmSave_Register() {
    SaveManager::Instance->AddInitFunction(NvMmSave_InitFile);
    SaveManager::Instance->AddSaveFunction("termina", 1, NvMmSave_Save, true, SECTION_PARENT_NONE);
    SaveManager::Instance->AddLoadFunction("termina", 1, NvMmSave_Load);
    // a save from before Termina has no section: start it at the first dawn, not with whatever
    // the last file left behind
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadGame>([](int32_t fileNum) {
        if (!sTerminaLoaded) {
            NvMmClock_NewGame();
            *NvMmOwl_Activated() = 0;
            NvMmOwl_SetSaveMark(-1);
        } else {
            NvMmOwl_OnLoad(); // (a save made at an owl starts at the owl, not at OoT's respawn)
        }
        sTerminaLoaded = false;
    });
}

static RegisterShipInitFunc sNvMmSaveInit(NvMmSave_Register);
