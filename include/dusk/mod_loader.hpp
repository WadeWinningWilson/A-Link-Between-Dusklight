#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "dusk/mod_api.h"
#include "miniz.h"

namespace dusk {

struct RmlTabContentCallback {
    void (*build_fn)(void* panel, void* userdata);
    void* userdata;
};

struct RmlTabUpdateCallback {
    void (*update_fn)(void* userdata);
    void* userdata;
};

struct LoadedMod {
    std::string name;
    std::string version;
    std::string author;
    std::string description;
    std::string mod_path;
    std::string dir;

    void* handle = nullptr;
    bool active = false;
    bool load_failed = false;

    using FnInit = void (*)(DuskModAPI*);
    using FnTick = void (*)(DuskModAPI*);
    using FnCleanup = void (*)(DuskModAPI*);

    FnInit fn_init = nullptr;
    FnTick fn_tick = nullptr;
    FnCleanup fn_cleanup = nullptr;

    DuskModAPI api{};

    std::vector<uint8_t> zip_data;
    mz_zip_archive res_zip{};
    bool res_zip_open = false;

    std::vector<RmlTabContentCallback> tab_content;
    std::vector<RmlTabUpdateCallback> tab_updates;
};

class ModLoader {
public:
    static ModLoader& instance();

    void setModsDir(std::filesystem::path dir) { m_modsDir = std::move(dir); }
    void init();
    void tick();
    void shutdown();

    const std::vector<LoadedMod>& mods() const { return m_mods; }

private:
    std::vector<LoadedMod> m_mods;
    std::filesystem::path m_modsDir;
    bool m_initialized = false;

    void tryLoadDusk(const std::filesystem::path& modPath);
    void buildAPI(LoadedMod& mod);
};

}  // namespace dusk
