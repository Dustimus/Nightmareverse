// Nightmareverse: the RE1 run's one save slot (z_nv_save.c builds and reads its contents).
//
// It lives in the Ship save file it belongs to, in its own section, "mansion" (SaveManager's
// sections: each game reads and writes only its own; the name is RE1's, kept apart from OoT's "base"
// and the other games'). The section holds the run as z_nv_save.c wrote it at the last typewriter,
// with a version number for the layout. A typewriter writes the section alone (SaveSection: OoT's
// own progress isn't saved by it); OoT's saves write it again as it stands, unchanged, so a new
// file never carries another file's run.
//
// Before, the run was a file of its own next to the save (Save/nv_re1_<file>.sav). One still there
// is moved into the section the first time its file loads (and kept as .sav.migrated). The dev
// slot (the debug file, 255, which SoH never writes) still uses that file: the headless
// walkthrough's checkpoints are such files.
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>
#include <ship/Context.h>
#include "soh/SaveManager.h"
#include "soh/ShipInit.hpp"
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include <spdlog/spdlog.h>

#define NV_SAVE_SECTION "mansion"
#define NV_SAVE_SECTION_VERSION 1

static std::mutex sNvRunMutex;
static std::vector<uint8_t> sNvRun; // the loaded file's run (empty: none)
static int sNvSectionId = -1;

static std::filesystem::path NvSaveFile_Path(int32_t fileNum) {
    return std::filesystem::path(Ship::Context::GetPathRelativeToAppDirectory("Save")) /
           ("nv_re1_" + std::to_string(fileNum) + ".sav");
}

/** A Ship save slot (0-2): the run goes in its section. Otherwise (the debug file) its own file. */
static bool NvSaveFile_InSection(int32_t fileNum) {
    return (fileNum >= 0) && (fileNum < 3) && (sNvSectionId >= 0);
}

static const char sB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string NvSaveFile_Encode(const std::vector<uint8_t>& in) {
    std::string out;
    size_t i;

    out.reserve((in.size() + 2) / 3 * 4);
    for (i = 0; i + 2 < in.size(); i += 3) {
        uint32_t v = (in[i] << 16) | (in[i + 1] << 8) | in[i + 2];

        out += sB64[(v >> 18) & 63];
        out += sB64[(v >> 12) & 63];
        out += sB64[(v >> 6) & 63];
        out += sB64[v & 63];
    }
    if (i < in.size()) {
        uint32_t v = in[i] << 16;

        if (i + 1 < in.size()) {
            v |= in[i + 1] << 8;
        }
        out += sB64[(v >> 18) & 63];
        out += sB64[(v >> 12) & 63];
        out += (i + 1 < in.size()) ? sB64[(v >> 6) & 63] : '=';
        out += '=';
    }
    return out;
}

static std::vector<uint8_t> NvSaveFile_Decode(const std::string& in) {
    std::vector<uint8_t> out;
    uint32_t v = 0;
    int bits = 0;

    for (char c : in) {
        const char* p = (c != '\0') ? strchr(sB64, c) : nullptr;

        if (p == nullptr) {
            continue; // ('=' and anything else)
        }
        v = (v << 6) | (uint32_t)(p - sB64);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back((uint8_t)((v >> bits) & 0xFF));
        }
    }
    return out;
}

// ---- the section ----

static void NvSaveFile_SaveSection(const SaveContext& saveContext, int sectionID, bool fullSave) {
    std::lock_guard<std::mutex> lock(sNvRunMutex);

    // (always both: SaveManager keeps the file's sections in memory, and an unwritten key would keep
    // the run that was there before)
    SaveManager::Instance->SaveData("run", NvSaveFile_Encode(sNvRun));
    SaveManager::Instance->SaveData("size", (uint32_t)sNvRun.size());
}

static void NvSaveFile_LoadSectionV1() {
    std::string run;
    uint32_t size = 0;
    std::vector<uint8_t> bytes;

    SaveManager::Instance->LoadData("run", run, std::string());
    SaveManager::Instance->LoadData("size", size, (uint32_t)0);
    bytes = NvSaveFile_Decode(run);
    std::lock_guard<std::mutex> lock(sNvRunMutex);
    sNvRun = (bytes.size() == size) ? bytes : std::vector<uint8_t>();
    if (bytes.size() != size) {
        SPDLOG_ERROR("Nightmareverse: the mansion's save section is damaged ({} of {} bytes); a new run", bytes.size(),
                     size);
    }
}

/** A new file or one about to load: no run until its section says otherwise. */
static void NvSaveFile_InitFile(bool isDebug) {
    std::lock_guard<std::mutex> lock(sNvRunMutex);
    sNvRun.clear();
}

static std::vector<uint8_t> NvSaveFile_ReadFile(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);

    if (!f) {
        return {};
    }
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
}

/** A file loaded with no run in it, but an older run's file beside it: that run moves in. */
static void NvSaveFile_OnLoadFile(int32_t fileNum) {
    std::filesystem::path old = NvSaveFile_Path(fileNum);
    std::vector<uint8_t> bytes;
    std::error_code ec;

    if (!NvSaveFile_InSection(fileNum) || !std::filesystem::exists(old, ec)) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(sNvRunMutex);
        if (!sNvRun.empty()) {
            return;
        }
        bytes = NvSaveFile_ReadFile(old);
        if (bytes.empty()) {
            return;
        }
        sNvRun = bytes;
    }
    SaveManager::Instance->SaveSection(fileNum, sNvSectionId, true);
    std::filesystem::path kept = old;
    kept += ".migrated";
    std::filesystem::rename(old, kept, ec);
    SPDLOG_INFO("Nightmareverse: file {}'s RE1 run moved into its save ({} bytes)", fileNum + 1, bytes.size());
}

/** A file erased (or copied over): an older run's file left beside it goes with it. */
static void NvSaveFile_OnDeleteFile(int32_t fileNum) {
    std::error_code ec;

    std::filesystem::remove(NvSaveFile_Path(fileNum), ec);
}

static void NvSaveFile_Register() {
    SaveManager::Instance->AddLoadFunction(NV_SAVE_SECTION, NV_SAVE_SECTION_VERSION, NvSaveFile_LoadSectionV1);
    sNvSectionId = SaveManager::Instance->AddSaveFunction(NV_SAVE_SECTION, NV_SAVE_SECTION_VERSION,
                                                          NvSaveFile_SaveSection, true, SECTION_PARENT_NONE);
    SaveManager::Instance->AddInitFunction(NvSaveFile_InitFile);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnLoadFile>(NvSaveFile_OnLoadFile);
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnDeleteFile>(NvSaveFile_OnDeleteFile);
}

static RegisterShipInitFunc sNvSaveFileInit(NvSaveFile_Register);

// ---- z_nv_save.c's three ----

extern "C" int32_t NvSaveFile_Write(int32_t fileNum, const void* data, uint32_t size) {
    if (NvSaveFile_InSection(fileNum)) {
        {
            std::lock_guard<std::mutex> lock(sNvRunMutex);
            sNvRun.assign((const uint8_t*)data, (const uint8_t*)data + size);
        }
        SaveManager::Instance->SaveSection(fileNum, sNvSectionId, true);
        return 1;
    }
    std::filesystem::path p = NvSaveFile_Path(fileNum);
    std::filesystem::path tmp = p;
    std::error_code ec;

    tmp += ".tmp";
    std::filesystem::create_directories(p.parent_path(), ec);
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) {
            return 0;
        }
        f.write((const char*)data, size);
        if (!f) {
            return 0;
        }
    }
    std::filesystem::rename(tmp, p, ec); // (whole or not at all)
    return ec ? 0 : 1;
}

/** The slot's bytes into buf (at most max); its size, or 0 when there's none. */
extern "C" uint32_t NvSaveFile_Read(int32_t fileNum, void* buf, uint32_t max) {
    if (NvSaveFile_InSection(fileNum)) {
        std::lock_guard<std::mutex> lock(sNvRunMutex);
        uint32_t n = (uint32_t)std::min<size_t>(sNvRun.size(), max);

        memcpy(buf, sNvRun.data(), n);
        return n;
    }
    std::ifstream f(NvSaveFile_Path(fileNum), std::ios::binary);
    if (!f) {
        return 0;
    }
    f.read((char*)buf, max);
    return (uint32_t)f.gcount();
}

extern "C" void NvSaveFile_Delete(int32_t fileNum) {
    std::error_code ec;

    if (NvSaveFile_InSection(fileNum)) {
        {
            std::lock_guard<std::mutex> lock(sNvRunMutex);
            sNvRun.clear();
        }
        SaveManager::Instance->SaveSection(fileNum, sNvSectionId, true);
    }
    std::filesystem::remove(NvSaveFile_Path(fileNum), ec);
}
