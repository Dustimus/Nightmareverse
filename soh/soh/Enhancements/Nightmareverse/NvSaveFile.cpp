// Nightmareverse: the RE1 run's one save slot on disk (z_nv_save.c builds and reads its contents).
// One file per Ship of Harkinian save file, next to it: Save/nv_re1_<file>.sav.
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <ship/Context.h>

static std::filesystem::path NvSaveFile_Path(int32_t fileNum) {
    return std::filesystem::path(Ship::Context::GetPathRelativeToAppDirectory("Save")) /
           ("nv_re1_" + std::to_string(fileNum) + ".sav");
}

extern "C" int32_t NvSaveFile_Write(int32_t fileNum, const void* data, uint32_t size) {
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
    std::ifstream f(NvSaveFile_Path(fileNum), std::ios::binary);
    if (!f) {
        return 0;
    }
    f.read((char*)buf, max);
    return (uint32_t)f.gcount();
}

extern "C" void NvSaveFile_Delete(int32_t fileNum) {
    std::error_code ec;
    std::filesystem::remove(NvSaveFile_Path(fileNum), ec);
}
