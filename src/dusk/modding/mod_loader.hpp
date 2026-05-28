#pragma once

#include <filesystem>
#include "miniz.h"

#include "dusk/mod_loader.hpp"

#if __APPLE__
#include <TargetConditionals.h>
#endif

namespace dusk::modding {

#if DUSK_CODE_MODS
constexpr bool EnableCodeMods = true;
#else
constexpr bool EnableCodeMods = false;
#endif

class ModBundle {
public:
    virtual ~ModBundle() = default;

    virtual std::vector<u8> readFile(const std::string& fileName) = 0;
    virtual std::vector<std::string> getFileNames() = 0;
    virtual size_t getFileSize(const std::string& fileName) = 0;
};

class ModBundleZip final : public ModBundle {
public:
    explicit ModBundleZip(std::vector<u8>&& data);
    ~ModBundleZip() override;
    std::vector<u8> readFile(const std::string& fileName) override;
    std::vector<std::string> getFileNames() override;
    size_t getFileSize(const std::string& fileName) override;

private:
    std::vector<uint8_t> zip_data;
    mz_zip_archive res_zip{};
    bool res_zip_open = false;
};

class ModBundleDisk final : public ModBundle {
public:
    explicit ModBundleDisk(std::filesystem::path root);
    ~ModBundleDisk() override = default;
    std::vector<u8> readFile(const std::string& fileName) override;
    std::vector<std::string> getFileNames() override;
    size_t getFileSize(const std::string& fileName) override;

private:
    [[nodiscard]] std::filesystem::path toRealPath(const std::string& fileName) const;
    std::filesystem::path root_path;
};


extern thread_local LoadedMod* g_currentMod;
extern std::unordered_map<std::string, void*> g_services;

extern thread_local void* g_dusk_hook_current_mod;

struct ModGuard {
    explicit ModGuard(dusk::LoadedMod* m) {
        g_currentMod = m;
        g_dusk_hook_current_mod = m;
    }
    ~ModGuard() {
        g_currentMod = nullptr;
        g_dusk_hook_current_mod = nullptr;
    }
};

inline const char* modName() {
    return g_currentMod ? g_currentMod->metadata.id.c_str() : "mod";
}

}  // namespace dusk::modding
