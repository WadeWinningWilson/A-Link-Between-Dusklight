#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "dusk/mod_api.h"

namespace dusk::modding {
class ModBundle;
class NativeModule;
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
    bool hasCode;
};

struct NativeMod {
    std::unique_ptr<modding::NativeModule> handle;
    DuskModAPI api{};

    using FnInit = void (*)(DuskModAPI*);
    using FnTick = void (*)(DuskModAPI*);
    using FnCleanup = void (*)(DuskModAPI*);

    FnInit fn_init = nullptr;
    FnTick fn_tick = nullptr;
    FnCleanup fn_cleanup = nullptr;
};

enum class NativeModStatus : u8 {
    /**
     * Mod does not have native code included.
     */
    None,

    /**
     * Native code mod loaded successfully.
     *
     * Note that this only indicates load status of the native library. If the native lib throws in
     * its init function, it will still be disabled!
     */
    Loaded,

    /**
     * This build was compiled without native mod support!
     */
    BuildDisabled,

    /**
     * Mod does not have a native library suitable for this build's platform.
     */
    ModMissingPlatform,

    /**
     * Mod is built for a different API version than this build of the game.
     */
    ApiVersionMismatch,

    /**
     * Unknown error loading the native mod.
     */
    Unknown,
};

struct LoadedMod {
    ModMetadata metadata;
    std::string mod_path;
    std::string dir;

    bool active = false;
    bool load_failed = false;

    NativeModStatus native_status = NativeModStatus::None;
    std::unique_ptr<NativeMod> native;

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
    void tryLoadNativeMod(LoadedMod& mod);
    void buildAPI(LoadedMod& mod);
    void initOverlayFiles();
};

}  // namespace dusk
