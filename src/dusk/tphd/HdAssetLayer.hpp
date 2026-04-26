#ifndef DUSK_TPHD_HD_ASSET_LAYER_HPP
#define DUSK_TPHD_HD_ASSET_LAYER_HPP

#include <filesystem>
#include <optional>
#include <string_view>
#include <vector>

#include <dolphin/types.h>

namespace dusk::tphd {

// Configure the base directory for HD asset overrides. `contentPath` should
// point at a Wii-U `content/` directory (the parent of `res/`). Empty path
// disables HD overrides.
void setHdContentPath(std::filesystem::path contentPath);

// Returns a pointer to the cached HD archive bytes if an HD variant exists
// for the requested GC path, or std::nullopt otherwise. Caller must not
// outlive the next setHdContentPath() call.
std::optional<std::vector<u8>*> tryLoadHdArchive(std::string_view gcPath);

// HD bytes lookup by DVD entry number, used by JKRMemArchive's entryNum
// constructor to substitute HD content.
void registerHdBytesForEntryNum(s32 entryNum, const std::vector<u8>* bytes);
const std::vector<u8>* getHdBytesForEntryNum(s32 entryNum);

}

#endif
