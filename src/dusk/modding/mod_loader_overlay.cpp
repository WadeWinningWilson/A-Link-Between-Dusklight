#include "aurora/dvd.h"
#include "aurora/lib/logging.hpp"
#include "dusk/mod_loader.hpp"
#include "mod_loader.hpp"

#include <cstring>

using namespace std::string_literals;

namespace {

aurora::Module Log("dusk::modLoader::overlay");

struct OverlayFileData {
    std::string bundlePath;
    dusk::LoadedMod* mod; // TODO: is using a raw pointer a bad idea here?
};

std::vector<OverlayFileData> s_overlayFiles;

void findOverlayFiles(std::vector<AuroraOverlayFile>& files, dusk::LoadedMod& mod) {
    for (const auto& file : mod.bundle->getFileNames()) {
        if (!file.starts_with("overlay/")) {
            continue;
        }

        auto overlayPath = file.substr("overlay/"s.size());
        assert(!overlayPath.starts_with('/'));
        overlayPath.insert(0, "/");

        const auto size = mod.bundle->getFileSize(file);

        const auto index = s_overlayFiles.size();
        s_overlayFiles.emplace_back(file, &mod);
        files.emplace_back(strdup(overlayPath.c_str()), reinterpret_cast<void*>(index), size);
    }
}

struct OpenOverlayFile {
    std::vector<u8> data;
    size_t pos;
};

void* cbOpen(void* userdata) {
    const auto index = reinterpret_cast<size_t>(userdata);
    const auto& fileData = s_overlayFiles[index];
    auto fileContents = fileData.mod->bundle->readFile(fileData.bundlePath);

    return new OpenOverlayFile(std::move(fileContents), 0);
}

void cbClose(void* handle) {
    const auto openFile = static_cast<OpenOverlayFile*>(handle);
    delete openFile;
}

int64_t cbRead(void* handle, uint8_t *buf, const size_t len) {
    auto& openFile = *static_cast<OpenOverlayFile*>(handle);

    const auto remainingSpace = openFile.data.size() - openFile.pos;
    const auto toRead = std::min(remainingSpace, len);
    std::memcpy(buf, openFile.data.data() + openFile.pos, toRead);
    openFile.pos += toRead;
    return static_cast<int64_t>(toRead);
}

int64_t cbSeek(void* handle, int64_t offset, int32_t whence) {
    if (whence != 0) {
        Log.fatal("Invalid seek mode from aurora: {}", whence);
    }

    auto& openFile = *static_cast<OpenOverlayFile*>(handle);
    const auto posSigned = std::clamp(offset, static_cast<int64_t>(0), static_cast<int64_t>(openFile.data.size()));
    openFile.pos = static_cast<size_t>(posSigned);
    return posSigned;
}

constexpr AuroraOverlayCallbacks s_overlayCallbacks = {
    .open = cbOpen,
    .close = cbClose,
    .read = cbRead,
    .seek = cbSeek,
};

}

namespace dusk {

void ModLoader::initOverlayFiles() {
    Log.debug("Initializing overlay files...");

    aurora_dvd_overlay_callbacks(&s_overlayCallbacks);

    std::vector<AuroraOverlayFile> files;

    for (auto& mod : m_mods) {
        findOverlayFiles(files, mod);
    }

    Log.debug("Found {} overlay files.", files.size());
    aurora_dvd_overlay_files(files.data(), files.size());

    for (const auto& file : files) {
        std::free(const_cast<char*>(file.fileName));
    }
}

}  // namespace dusk
