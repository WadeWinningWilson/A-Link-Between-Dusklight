#include "dusk/mod_loader.hpp"
#include "dusk/hook_system.hpp"
#include "dusk/logging.h"

#include <RmlUi/Core.h>


#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <string>
#include <unordered_map>

#include "miniz.h"
#include "nlohmann/json.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

static void* pl_dlopen(const std::filesystem::path& p) {
    return LoadLibraryW(p.wstring().c_str());
}
static void* pl_dlsym(void* h, const char* name) {
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(h), name));
}
static void pl_dlclose(void* h) {
    FreeLibrary(static_cast<HMODULE>(h));
}
static std::string pl_dlerror() {
    char buf[256]{};
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
        GetLastError(), 0, buf, sizeof(buf), nullptr);
    std::string s = buf;
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) {
        s.pop_back();
    }
    return s;
}
static constexpr const char* k_libExt = ".dll";

#else
#include <dlfcn.h>
static void* pl_dlopen(const std::filesystem::path& p) {
#if defined(__linux__)
    return dlopen(p.c_str(), RTLD_LAZY | RTLD_LOCAL | RTLD_DEEPBIND);
#else
    return dlopen(p.c_str(), RTLD_LAZY | RTLD_LOCAL);
#endif
}
static void* pl_dlsym(void* h, const char* name) {
    return dlsym(h, name);
}
static void pl_dlclose(void* h) {
    dlclose(h);
}
static std::string pl_dlerror() {
    const char* e = dlerror();
    return e ? e : "(unknown error)";
}
#if defined(__APPLE__)
static constexpr const char* k_libExt = ".dylib";
#else
static constexpr const char* k_libExt = ".so";
#endif
#endif

#if defined(_M_ARM64) || defined(__aarch64__)
static constexpr std::string_view k_archSuffix = "_arm64";
#elif defined(_M_X64) || defined(__x86_64__)
static constexpr std::string_view k_archSuffix = "_x64";
#elif defined(_M_IX86) || defined(__i386__)
static constexpr std::string_view k_archSuffix = "_x86";
#else
static constexpr std::string_view k_archSuffix = "";
#endif

static FILE* fs_fopen(const std::filesystem::path& p, const char* mode) {
#if defined(_WIN32)
    std::wstring wmode(mode, mode + strlen(mode));
    return _wfopen(p.wstring().c_str(), wmode.c_str());
#else
    return fopen(p.c_str(), mode);
#endif
}

static thread_local dusk::LoadedMod* g_currentMod = nullptr;
static std::unordered_map<std::string, void*> g_services;

namespace dusk {
thread_local void* g_dusk_hook_current_mod = nullptr;
}

struct ModGuard {
    explicit ModGuard(dusk::LoadedMod* m) {
        g_currentMod = m;
        dusk::g_dusk_hook_current_mod = m;
    }
    ~ModGuard() {
        g_currentMod = nullptr;
        dusk::g_dusk_hook_current_mod = nullptr;
    }
};

static const char* modName() {
    return g_currentMod ? g_currentMod->name.c_str() : "mod";
}

static void cb_log_info(const char* fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    std::string s(vsnprintf(nullptr, 0, fmt, ap2), '\0');
    va_end(ap2);
    vsnprintf(s.data(), s.size() + 1, fmt, ap);
    va_end(ap);
    DuskLog.info("[{}] {}", modName(), s);
}

static void cb_log_warn(const char* fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    std::string s(vsnprintf(nullptr, 0, fmt, ap2), '\0');
    va_end(ap2);
    vsnprintf(s.data(), s.size() + 1, fmt, ap);
    va_end(ap);
    DuskLog.warn("[{}] {}", modName(), s);
}

static void cb_log_error(const char* fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    std::string s(vsnprintf(nullptr, 0, fmt, ap2), '\0');
    va_end(ap2);
    vsnprintf(s.data(), s.size() + 1, fmt, ap);
    va_end(ap);
    DuskLog.error("[{}] {}", modName(), s);
}

static void* cb_load_resource(const char* relative_path, size_t* out_size) {
    if (out_size) {
        *out_size = 0;
    }
    if (!g_currentMod || !relative_path) {
        DuskLog.error("load_resource: called outside mod context or with null path");
        return nullptr;
    }
    if (!g_currentMod->res_zip_open) {
        DuskLog.error("[{}] load_resource: zip not available", g_currentMod->name);
        return nullptr;
    }

    std::string entry = std::string("res/") + relative_path;
    size_t sz = 0;
    void* data = mz_zip_reader_extract_file_to_heap(&g_currentMod->res_zip, entry.c_str(), &sz, 0);
    if (!data) {
        DuskLog.error("[{}] load_resource: '{}' not found in zip", g_currentMod->name, entry);
        return nullptr;
    }
    if (out_size) {
        *out_size = sz;
    }
    return data;
}

static void cb_free_resource(void* data) {
    mz_free(data);
}

namespace {

class ModClickListener : public Rml::EventListener {
public:
    ModClickListener(void (*cb)(void*), void* ud) : m_cb(cb), m_ud(ud) {}
    void ProcessEvent(Rml::Event&) override { m_cb(m_ud); }
    void OnDetach(Rml::Element*) override { delete this; }
private:
    void (*m_cb)(void*);
    void* m_ud;
};

static std::string escape_rml(const char* text) {
    std::string out;
    for (const char* p = text; *p; ++p) {
        switch (*p) {
        case '&': out += "&amp;"; break;
        case '<': out += "&lt;";  break;
        case '>': out += "&gt;";  break;
        default:  out += *p;      break;
        }
    }
    return out;
}

}

static void cb_panel_add_section(DuskPanelHandle panel, const char* text) {
    auto* pane = static_cast<Rml::Element*>(panel);
    if (!pane || !text) {
        return;
    }
    auto el = pane->GetOwnerDocument()->CreateElement("div");
    el->SetClass("section-heading", true);
    el->SetInnerRML(escape_rml(text));
    pane->AppendChild(std::move(el));
}

static void cb_panel_add_button(DuskPanelHandle panel, const char* label,
                                void (*cb)(void*), void* userdata) {
    auto* pane = static_cast<Rml::Element*>(panel);
    if (!pane || !label || !cb) {
        return;
    }
    auto btn = pane->GetOwnerDocument()->CreateElement("button");
    btn->SetInnerRML(escape_rml(label));
    btn->AddEventListener(Rml::EventId::Click, new ModClickListener(cb, userdata));
    pane->AppendChild(std::move(btn));
}

static DuskElemHandle cb_panel_add_badge_row(DuskPanelHandle panel, const char* label, int ok) {
    auto* pane = static_cast<Rml::Element*>(panel);
    if (!pane || !label) {
        return nullptr;
    }
    auto* doc = pane->GetOwnerDocument();

    auto row = doc->CreateElement("div");
    row->SetClass("mod-info-row", true);

    auto badge = doc->CreateElement("span");
    badge->SetClass("achievement-badge", true);
    badge->SetClass(ok ? "unlocked" : "locked", true);
    badge->SetInnerRML(ok ? "PASS" : "WAIT");
    Rml::Element* badgePtr = row->AppendChild(std::move(badge));

    auto lbl = doc->CreateElement("span");
    lbl->SetClass("mod-info-value", true);
    lbl->SetInnerRML(escape_rml(label));
    row->AppendChild(std::move(lbl));

    pane->AppendChild(std::move(row));
    return static_cast<DuskElemHandle>(badgePtr);
}

static DuskElemHandle cb_panel_add_dyn_text(DuskPanelHandle panel, const char* text) {
    auto* pane = static_cast<Rml::Element*>(panel);
    if (!pane) {
        return nullptr;
    }
    auto el = pane->GetOwnerDocument()->CreateElement("div");
    el->SetInnerRML(text ? escape_rml(text) : std::string{});
    Rml::Element* ptr = pane->AppendChild(std::move(el));
    return static_cast<DuskElemHandle>(ptr);
}

static void cb_elem_set_badge(DuskElemHandle elem, int ok) {
    auto* el = static_cast<Rml::Element*>(elem);
    if (!el) {
        return;
    }
    el->SetClass("unlocked", ok != 0);
    el->SetClass("locked",   ok == 0);
    el->SetInnerRML(ok ? "PASS" : "WAIT");
}

static void cb_elem_set_text(DuskElemHandle elem, const char* text) {
    auto* el = static_cast<Rml::Element*>(elem);
    if (!el || !text) {
        return;
    }
    el->SetInnerRML(escape_rml(text));
}

static DuskElemHandle cb_panel_add_progress(DuskPanelHandle panel, float value) {
    auto* pane = static_cast<Rml::Element*>(panel);
    if (!pane) {
        return nullptr;
    }
    auto el = pane->GetOwnerDocument()->CreateElement("progress");
    el->SetClass("progress-health", true);
    el->SetAttribute("value", value);
    Rml::Element* ptr = pane->AppendChild(std::move(el));
    return static_cast<DuskElemHandle>(ptr);
}

static void cb_elem_set_progress(DuskElemHandle elem, float value) {
    auto* el = static_cast<Rml::Element*>(elem);
    if (!el) {
        return;
    }
    el->SetAttribute("value", value);
}

static void cb_register_tab_content(void (*build_fn)(void*, void*), void* userdata) {
    if (g_currentMod && build_fn) {
        g_currentMod->tab_content.push_back({build_fn, userdata});
    }
}

static void cb_register_tab_update(void (*update_fn)(void*), void* userdata) {
    if (g_currentMod && update_fn) {
        g_currentMod->tab_updates.push_back({update_fn, userdata});
    }
}

static void cb_service_publish(const char* name, void* ptr) {
    if (!name) {
        return;
    }
    if (g_services.count(name)) {
        DuskLog.error(
            "[{}] service_publish: '{}' already published by another mod", modName(), name);
    }
    g_services[name] = ptr;
}

static void* cb_service_get(const char* name) {
    if (!name) {
        return nullptr;
    }
    auto it = g_services.find(name);
    return it != g_services.end() ? it->second : nullptr;
}

static void api_hook_pre(void* addr, int32_t (*fn)(void* args)) {
    dusk::hookRegisterPre(addr, g_currentMod, fn);
}

static void api_hook_post(void* addr, void (*fn)(void* args, void* retval)) {
    dusk::hookRegisterPost(addr, g_currentMod, modName(), fn);
}

static void api_hook_replace(void* addr, void (*fn)(void* args, void* retval)) {
    if (!dusk::hookSetReplace(addr, g_currentMod, modName(), fn)) {
        if (g_currentMod) {
            g_currentMod->load_failed = true;
        }
    }
}

static dusk::ModLoader g_modLoader;

namespace dusk {

ModLoader& ModLoader::instance() {
    return g_modLoader;
}

void ModLoader::buildAPI(LoadedMod& mod) {
    mod.api.api_version = DUSK_MOD_API_VERSION;
    mod.api.mod_dir = mod.dir.c_str();
    mod.api.log_info = cb_log_info;
    mod.api.log_warn = cb_log_warn;
    mod.api.log_error = cb_log_error;
    mod.api.load_resource = cb_load_resource;
    mod.api.free_resource = cb_free_resource;
    mod.api.register_tab_content = cb_register_tab_content;
    mod.api.register_tab_update = cb_register_tab_update;
    mod.api.panel_add_section   = cb_panel_add_section;
    mod.api.panel_add_button    = cb_panel_add_button;
    mod.api.panel_add_badge_row = cb_panel_add_badge_row;
    mod.api.panel_add_dyn_text  = cb_panel_add_dyn_text;
    mod.api.elem_set_badge      = cb_elem_set_badge;
    mod.api.elem_set_text       = cb_elem_set_text;
    mod.api.panel_add_progress  = cb_panel_add_progress;
    mod.api.elem_set_progress   = cb_elem_set_progress;
    mod.api.hook_install = hookInstallByAddr;
    mod.api.hook_pre = api_hook_pre;
    mod.api.hook_post = api_hook_post;
    mod.api.hook_replace = api_hook_replace;
    mod.api.hook_dispatch_pre = hookDispatchPre;
    mod.api.hook_dispatch_post = hookDispatchPost;
    mod.api.service_publish = cb_service_publish;
    mod.api.service_get = cb_service_get;
}

void ModLoader::tryLoadDusk(const std::filesystem::path& modPath) {
    namespace fs = std::filesystem;

    std::vector<uint8_t> zipBytes;
    {
        FILE* f = fs_fopen(modPath, "rb");
        if (!f) {
            DuskLog.error("ModLoader: failed to open {}", modPath.filename().string());
            return;
        }
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);
        zipBytes.resize(static_cast<size_t>(fsize));
        fread(zipBytes.data(), 1, zipBytes.size(), f);
        fclose(f);
    }

    std::string metaName, metaVersion, metaAuthor, metaDescription;
    {
        mz_zip_archive zip{};
        if (mz_zip_reader_init_mem(&zip, zipBytes.data(), zipBytes.size(), 0)) {
            size_t jsonSize = 0;
            void* jsonData = mz_zip_reader_extract_file_to_heap(&zip, "mod.json", &jsonSize, 0);
            mz_zip_reader_end(&zip);
            if (jsonData) {
                try {
                    std::string jsonStr(static_cast<char*>(jsonData), jsonSize);
                    mz_free(jsonData);
                    jsonData = nullptr;
                    auto j = nlohmann::json::parse(jsonStr);
                    metaName = j.value("name", "");
                    metaVersion = j.value("version", "");
                    metaAuthor = j.value("author", "");
                    metaDescription = j.value("description", "");
                } catch (const std::exception& e) {
                    mz_free(jsonData);
                    DuskLog.warn(
                        "ModLoader: bad mod.json in {}: {}", modPath.filename().string(), e.what());
                }
            }
        }
    }

    mz_zip_archive zip{};
    if (!mz_zip_reader_init_mem(&zip, zipBytes.data(), zipBytes.size(), 0)) {
        DuskLog.error("ModLoader: failed to open {}", modPath.filename().string());
        return;
    }

    std::string dllEntry, dllFallback;
    for (mz_uint i = 0, n = mz_zip_reader_get_num_files(&zip); i < n; ++i) {
        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) {
            continue;
        }
        if (mz_zip_reader_is_file_a_directory(&zip, i)) {
            continue;
        }
        fs::path fname(stat.m_filename);
        if (fname.extension() == k_libExt) {
            if (!k_archSuffix.empty() && fname.stem().string().ends_with(k_archSuffix)) {
                dllEntry = stat.m_filename;
                break;
            } else if (dllFallback.empty()) {
                dllFallback = stat.m_filename;
            }
        }
    }
    if (dllEntry.empty()) {
        dllEntry = dllFallback;
    }

    if (dllEntry.empty()) {
        mz_zip_reader_end(&zip);
        DuskLog.warn(
            "ModLoader: no *{} found in {} — skipping", k_libExt, modPath.filename().string());
        return;
    }

    const fs::path cacheDir = m_modsDir / ".cache" / modPath.stem();
    std::error_code ec;
    fs::create_directories(cacheDir, ec);

    const fs::path dllCachePath = cacheDir / fs::path(dllEntry).filename();

    size_t dllSize = 0;
    void* dllData = mz_zip_reader_extract_file_to_heap(&zip, dllEntry.c_str(), &dllSize, 0);
    mz_zip_reader_end(&zip);

    if (!dllData) {
        DuskLog.error(
            "ModLoader: failed to extract {} from {}", dllEntry, modPath.filename().string());
        return;
    }
    {
        FILE* out = fs_fopen(dllCachePath, "wb");
        if (out) {
            fwrite(dllData, 1, dllSize, out);
            fclose(out);
        } else {
            mz_free(dllData);
            DuskLog.error("ModLoader: failed to write {}", dllCachePath.string());
            return;
        }
    }
    mz_free(dllData);

    void* handle = pl_dlopen(dllCachePath);
    if (!handle) {
        DuskLog.error("ModLoader: failed to open {}: {}", dllCachePath.string(), pl_dlerror());
        return;
    }

    LoadedMod mod;
    mod.mod_path = fs::absolute(modPath).string();
    mod.dir = fs::absolute(cacheDir).string();
    mod.handle = handle;
    auto* mod_api_ver = reinterpret_cast<uint32_t*>(pl_dlsym(handle, "mod_api_version"));
    if (mod_api_ver && *mod_api_ver != DUSK_MOD_API_VERSION) {
        DuskLog.error("ModLoader: {} expects API v{} but engine is v{}, skipping",
            fs::path(dllEntry).filename().string(), *mod_api_ver, DUSK_MOD_API_VERSION);
        pl_dlclose(handle);
        return;
    }

    mod.fn_init = reinterpret_cast<LoadedMod::FnInit>(pl_dlsym(handle, "mod_init"));
    mod.fn_tick = reinterpret_cast<LoadedMod::FnTick>(pl_dlsym(handle, "mod_tick"));
    mod.fn_cleanup = reinterpret_cast<LoadedMod::FnCleanup>(pl_dlsym(handle, "mod_cleanup"));

    if (!mod.fn_init || !mod.fn_tick) {
        DuskLog.error("ModLoader: {} missing mod_init or mod_tick — skipping",
            fs::path(dllEntry).filename().string());
        pl_dlclose(handle);
        return;
    }

    mod.name = metaName.empty() ? modPath.stem().string() : metaName;
    mod.version = metaVersion.empty() ? "?" : metaVersion;
    mod.author = metaAuthor.empty() ? "unknown" : metaAuthor;
    mod.description = metaDescription;

    mod.zip_data = std::move(zipBytes);
    m_mods.push_back(std::move(mod));
    {
        LoadedMod& stored = m_mods.back();
        if (mz_zip_reader_init_mem(&stored.res_zip, stored.zip_data.data(), stored.zip_data.size(), 0)) {
            stored.res_zip_open = true;
        }
    }
    DuskLog.info("ModLoader: found '{}' v{} by {} ({})", m_mods.back().name, m_mods.back().version,
        m_mods.back().author, modPath.filename().string());
}

void ModLoader::init() {
    if (m_initialized) {
        return;
    }
    m_initialized = true;

    namespace fs = std::filesystem;
    if (!fs::is_directory(m_modsDir)) {
        DuskLog.info(
            "ModLoader: mods directory '{}' not found — mod loading skipped", m_modsDir.string());
        return;
    }

    std::error_code ec;
    std::vector<fs::directory_entry> entries;
    for (auto& e : fs::directory_iterator(m_modsDir, ec)) {
        if (e.is_regular_file() && e.path().extension() == ".dusk") {
            entries.push_back(e);
        }
    }
    std::sort(entries.begin(), entries.end(),
        [](const fs::directory_entry& a, const fs::directory_entry& b) {
            return a.path().filename() < b.path().filename();
        });

    m_mods.reserve(entries.size());
    for (auto& entry : entries) {
        tryLoadDusk(entry.path());
    }

    if (m_mods.empty()) {
        DuskLog.info("ModLoader: no mods found");
        return;
    }

    DuskLog.info("ModLoader: initializing {} mod(s)...", m_mods.size());
    for (auto& mod : m_mods) {
        buildAPI(mod);
    }

    for (auto& mod : m_mods) {
        ModGuard guard(&mod);
        try {
            mod.fn_init(&mod.api);
            if (!mod.load_failed) {
                mod.active = true;
                DuskLog.info("ModLoader: '{}' initialized", mod.name);
            } else {
                DuskLog.error("ModLoader: '{}' failed to load due to hook conflicts", mod.name);
            }
        } catch (const std::exception& e) {
            DuskLog.error("ModLoader: exception in {}.mod_init(): {}", mod.name, e.what());
        } catch (...) {
            DuskLog.error("ModLoader: unknown exception in {}.mod_init()", mod.name);
        }
    }

    auto active =
        std::count_if(m_mods.begin(), m_mods.end(), [](const LoadedMod& m) { return m.active; });
    DuskLog.info("ModLoader: {}/{} mod(s) active", active, m_mods.size());
}

void ModLoader::tick() {
    for (auto& mod : m_mods) {
        if (!mod.active) {
            continue;
        }
        ModGuard guard(&mod);
        try {
            mod.fn_tick(&mod.api);
        } catch (const std::exception& e) {
            DuskLog.error(
                "ModLoader: exception in {}.mod_tick(): {} — disabling", mod.name, e.what());
            mod.active = false;
        } catch (...) {
            DuskLog.error("ModLoader: unknown exception in {}.mod_tick() — disabling", mod.name);
            mod.active = false;
        }
    }
}

void ModLoader::shutdown() {
    for (auto& mod : m_mods) {
        hookClearMod(&mod);
        if (mod.fn_cleanup) {
            ModGuard guard(&mod);
            try {
                mod.fn_cleanup(&mod.api);
            } catch (...) {
            }
        }
        if (mod.res_zip_open) {
            mz_zip_reader_end(&mod.res_zip);
            mod.res_zip_open = false;
        }
        mod.zip_data.clear();
        if (mod.handle) {
            pl_dlclose(mod.handle);
            mod.handle = nullptr;
        }
    }
    m_mods.clear();
    g_services.clear();
    DuskLog.info("ModLoader: all mods unloaded");
}

}  // namespace dusk
