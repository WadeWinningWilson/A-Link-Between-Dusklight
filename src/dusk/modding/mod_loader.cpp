#include "dusk/mod_loader.hpp"
#include "dusk/hook_system.hpp"
#include "dusk/logging.h"
#include "mod_loader.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>

#include "aurora/dvd.h"
#include "dusk/io.hpp"
#include "miniz.h"
#include "native_module.hpp"
#include "nlohmann/json.hpp"

static aurora::Module Log("dusk::modLoader");

using namespace dusk::modding;
using namespace std::string_literals;
using namespace std::string_view_literals;

#if defined(_M_ARM64) || defined(__aarch64__)
static constexpr std::string_view k_archSuffix = "_arm64"sv;
#elif defined(_M_X64) || defined(__x86_64__)
static constexpr std::string_view k_archSuffix = "_x64"sv;
#elif defined(_M_IX86) || defined(__i386__)
static constexpr std::string_view k_archSuffix = "_x86"sv;
#else
static constexpr std::string_view k_archSuffix = ""sv;
#endif

static dusk::ModLoader g_modLoader;

namespace dusk {

ModLoader& ModLoader::instance() {
    return g_modLoader;
}

static std::unique_ptr<ModBundle> loadBundle(const std::filesystem::path& modPath, bool fromDir) {
    if (fromDir) {
        return std::make_unique<ModBundleDisk>(modPath);
    } else {
        std::vector<u8> data = io::FileStream::ReadAllBytes(modPath);
        return std::make_unique<ModBundleZip>(std::move(data));
    }
}

struct DllLocateResult {
    std::string primary;
    std::string fallback;
};

static std::string_view getFileNameWithoutExtension(const std::string_view fileName) {
    return fileName.substr(0, fileName.find_last_of('.'));
}

static DllLocateResult LocateDllInBundle(ModBundle& bundle) {
    std::string dllEntry, dllFallback;
    for (const auto name : bundle.getFileNames()) {
        if (!name.ends_with(".dll"sv) && !name.ends_with(".dylib"sv) && !name.ends_with(".so"sv)) {
            continue;
        }

        if (!k_archSuffix.empty() && getFileNameWithoutExtension(name).ends_with(k_archSuffix)) {
            dllEntry = name;
        } else if (dllFallback.empty()) {
            dllFallback = name;
        }
    }

    return DllLocateResult{dllEntry, dllFallback};
}

class InvalidModDataException : public std::runtime_error {
public:
    explicit InvalidModDataException(const std::string& msg) : runtime_error(msg) {}
    explicit InvalidModDataException(const char* msg) : runtime_error(msg) {}
};

static ModMetadata loadMetadata(const std::filesystem::path& modPath, ModBundle& bundle) {
    const auto metaJson = bundle.readFile("mod.json");
    auto j = nlohmann::json::parse(metaJson);

    std::string metaId = j.value("id", "");
    std::string metaName = j.value("name", "");
    std::string metaVersion = j.value("version", "");
    std::string metaAuthor = j.value("author", "");
    std::string metaDescription = j.value("description", "");
    const bool hasCode = j.value("has_code", false);

    if (metaId.empty()) {
        throw InvalidModDataException("Missing ID value in mod metadata!");
    }
    if (metaName.empty()) {
        metaName = io::fs_path_to_string(modPath.stem());
    }
    if (metaVersion.empty()) {
        metaVersion = "?"s;
    }
    if (metaAuthor.empty()) {
        metaAuthor = "unknown"s;
    }

    return ModMetadata{
        std::move(metaId),
        std::move(metaName),
        std::move(metaVersion),
        std::move(metaAuthor),
        std::move(metaDescription),
        hasCode,
    };
}

static bool checkDuplicateMod(const ModMetadata& metadata, const std::vector<LoadedMod>& mods) {
    return std::ranges::any_of(mods, [&](const LoadedMod& mod) {
        return mod.metadata.id == metadata.id;
    });
}

bool ModLoader::tryLoadNativeMod(LoadedMod& mod) {
    if (!EnableCodeMods) {
        DuskLog.error("Code mods are not available in this build");
        return false;
    }

    namespace fs = std::filesystem;

    auto [dllEntry, dllFallback] = LocateDllInBundle(*mod.bundle);
    if (dllEntry.empty()) {
        dllEntry = dllFallback;
    }

    if (dllEntry.empty()) {
        DuskLog.error(
            "ModLoader: no *{} found in {} — skipping", NativeModule::LibraryExtension, mod.metadata.id);
        return false;
    }

    const fs::path cacheDir = m_modsDir / ".cache" / mod.metadata.id;
    std::error_code ec;
    fs::create_directories(cacheDir, ec);

    const fs::path dllCachePath = cacheDir / fs::path(dllEntry).filename();

    std::vector<u8> dllData;
    try {
        dllData = mod.bundle->readFile(dllEntry);
    } catch (const std::runtime_error& e) {
        DuskLog.error(
            "ModLoader: failed to extract {} from {}", dllEntry, mod.metadata.id);
        return false;
    }

    {
        std::ofstream out(dllCachePath, std::ios::binary | std::ios::out);
        if (!out) {
            DuskLog.error("ModLoader: failed to write {}", io::fs_path_to_string(dllCachePath));
            return false;
        }

        out.write(
            reinterpret_cast<const char*>(dllData.data()),
            static_cast<std::streamsize>(dllData.size()));
    }

    auto nativeMod = std::make_unique<NativeMod>();
    try {
        nativeMod->handle = std::make_unique<NativeModule>(dllCachePath);
    } catch (const std::runtime_error& e) {
        DuskLog.error("ModLoader: failed to open {}: {}", io::fs_path_to_string(dllCachePath), e.what());
        return false;
    }

    const auto mod_api_ver = nativeMod->handle->LookupSymbol<uint32_t*>("mod_api_version");
    if (mod_api_ver && *mod_api_ver != DUSK_MOD_API_VERSION) {
        DuskLog.error("ModLoader: {} expects API v{} but engine is v{}, skipping",
            io::fs_path_to_string(fs::path(dllEntry).filename()), *mod_api_ver, DUSK_MOD_API_VERSION);
        return false;
    }

    nativeMod->fn_init = nativeMod->handle->LookupSymbol<NativeMod::FnInit>("mod_init");
    nativeMod->fn_tick = nativeMod->handle->LookupSymbol<NativeMod::FnTick>("mod_tick");
    nativeMod->fn_cleanup = nativeMod->handle->LookupSymbol<NativeMod::FnCleanup>("mod_cleanup");

    if (!nativeMod->fn_init || !nativeMod->fn_tick) {
        DuskLog.error("ModLoader: {} missing mod_init or mod_tick — skipping",
            io::fs_path_to_string(fs::path(dllEntry).filename()));
        return false;
    }

    mod.dir = io::fs_path_to_string(fs::absolute(cacheDir));
    mod.native = std::move(nativeMod);
    return true;
}

void ModLoader::tryLoadDusk(const std::filesystem::path& modPath, bool fromDir) {
    namespace fs = std::filesystem;

    std::unique_ptr<ModBundle> bundle;
    try {
        bundle = loadBundle(modPath, fromDir);
    } catch (const std::runtime_error& e) {
        Log.error("Failed to open {} bundle: {}", io::fs_path_to_string(modPath.filename()), e.what());
        return;
    }

    ModMetadata metadata;
    try
    {
        metadata = loadMetadata(modPath, *bundle);
    }
    catch (const std::runtime_error& e) {
        Log.error(
            "ModLoader: bad mod.json in {}: {}", io::fs_path_to_string(modPath.filename()), e.what());
        return;
    }

    if (checkDuplicateMod(metadata, m_mods)) {
        Log.error(
            "ModLoader: mod with id '{}' already exists, not loading {}",
            metadata.id,
            io::fs_path_to_string(modPath.filename()));
        return;
    }

    LoadedMod mod;
    mod.mod_path = io::fs_path_to_string(fs::absolute(modPath));
    mod.metadata = std::move(metadata);
    mod.bundle = std::move(bundle);

    if (mod.metadata.hasCode && !tryLoadNativeMod(mod)) {
        return;
    }

    const auto& inserted = m_mods.emplace_back(std::move(mod));

    DuskLog.info(
        "ModLoader: found '{}' ('{}') v{} by {} ({})",
        inserted.metadata.name,
        inserted.metadata.id,
        inserted.metadata.version,
        inserted.metadata.author,
        io::fs_path_to_string(modPath.filename()));
}

void ModLoader::init() {
    if (m_initialized) {
        return;
    }
    m_initialized = true;

    namespace fs = std::filesystem;
    if (!fs::is_directory(m_modsDir)) {
        DuskLog.info(
            "ModLoader: mods directory '{}' not found — mod loading skipped", io::fs_path_to_string(m_modsDir));
        return;
    }

    std::error_code ec;
    std::vector<fs::directory_entry> entries;
    for (auto& e : fs::directory_iterator(m_modsDir, ec)) {
        if (e.is_directory() && std::filesystem::exists(e.path() / "mod.json")) {
            entries.push_back(e);
        } else if (e.is_regular_file() && e.path().extension() == ".dusk") {
            entries.push_back(e);
        }
    }
    std::sort(entries.begin(), entries.end(),
        [](const fs::directory_entry& a, const fs::directory_entry& b) {
            return a.path().filename() < b.path().filename();
        });

    m_mods.reserve(entries.size());
    for (auto& entry : entries) {
        tryLoadDusk(entry.path(), entry.is_directory());
    }

    if (m_mods.empty()) {
        DuskLog.info("ModLoader: no mods found");
        return;
    }

    initOverlayFiles();

    DuskLog.info("ModLoader: initializing {} mod(s)...", m_mods.size());
    for (auto& mod : m_mods) {
        if (mod.native) {
            buildAPI(mod);
        }
    }

    for (auto& mod : m_mods) {
        if (!mod.native) {
            continue;
        }

        ModGuard guard(&mod);
        try {
            mod.native->fn_init(&mod.native->api);
            if (!mod.load_failed) {
                mod.active = true;
                DuskLog.info("ModLoader: '{}' initialized", mod.metadata.id);
            } else {
                DuskLog.error("ModLoader: '{}' failed to load due to hook conflicts", mod.metadata.id);
            }
        } catch (const std::exception& e) {
            DuskLog.error("ModLoader: exception in {}.mod_init(): {}", mod.metadata.id, e.what());
        } catch (...) {
            DuskLog.error("ModLoader: unknown exception in {}.mod_init()", mod.metadata.id);
        }
    }

    auto active =
        std::count_if(m_mods.begin(), m_mods.end(), [](const LoadedMod& m) { return m.active; });
    DuskLog.info("ModLoader: {}/{} mod(s) active", active, m_mods.size());
}

void ModLoader::tick() {
    for (auto& mod : m_mods) {
        if (!mod.active || !mod.native) {
            continue;
        }
        ModGuard guard(&mod);
        try {
            mod.native->fn_tick(&mod.native->api);
        } catch (const std::exception& e) {
            DuskLog.error(
                "ModLoader: exception in {}.mod_tick(): {} — disabling", mod.metadata.id, e.what());
            mod.active = false;
        } catch (...) {
            DuskLog.error("ModLoader: unknown exception in {}.mod_tick() — disabling", mod.metadata.id);
            mod.active = false;
        }
    }
}

void ModLoader::shutdown() {
    for (auto& mod : m_mods) {
        hookClearMod(&mod);
        if (mod.native && mod.native->fn_cleanup) {
            ModGuard guard(&mod);
            try {
                mod.native->fn_cleanup(&mod.native->api);
            } catch (...) {
            }
        }
    }
    m_mods.clear();
    g_services.clear();
    DuskLog.info("ModLoader: all mods unloaded");
}

}  // namespace dusk
