#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "dusk/mod_api.h"

namespace dusk::modding {
class ModBundle;
}

namespace dusk {

struct RmlTabContentCallback {
    void (*build_fn)(void* panel, void* userdata);
    void* userdata;
};

struct RmlTabUpdateCallback {
    void (*update_fn)(void* userdata);
    void* userdata;
};

struct ModMetadata {
    std::string id;
    std::string name;
    std::string version;
    std::string author;
    std::string description;
};

struct LoadedMod {
    ModMetadata metadata;
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
    std::unique_ptr<modding::ModBundle> bundle;

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

    void tryLoadDusk(const std::filesystem::path& modPath, bool fromDir);
    void buildAPI(LoadedMod& mod);
    void initOverlayFiles();
};

}  // namespace dusk
