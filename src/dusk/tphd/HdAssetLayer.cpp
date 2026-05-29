#include "HdAssetLayer.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <list>
#include <mutex>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include <aurora/hd_texture.hpp>
#include <dolphin/dvd.h>

#include "JSystem/J3DGraphLoader/J3DModelLoader.h"
#include "JSystem/JKernel/JKRArchive.h"
#include "JSystem/JKernel/JKRDecomp.h"
#include "JSystem/JUtility/JUTTexture.h"
#include "dusk/endian.h"
#include "dusk/logging.h"
#include "AddrLib.hpp"
#include "GtxParser.hpp"
#include "TphdPack.hpp"
#include "tracy/Tracy.hpp"

static aurora::Module HdLog("dusk::tphd::hd");

namespace dusk::tphd {

namespace {

std::filesystem::path g_contentPath;
std::mutex g_cacheMutex;

// Heap-allocated, never freed — these must outlive g_dComIfG_gameInfo's
// static destructor which holds JKRArchives referencing these bytes.
std::list<std::vector<u8>>& g_mountBuffers() {
    static auto* p = new std::list<std::vector<u8>>{};
    return *p;
}
std::unordered_map<s32, const std::vector<u8>*>& g_entryNumToBytes() {
    static auto* p = new std::unordered_map<s32, const std::vector<u8>*>{};
    return *p;
}

bool endsWithSuffix(std::string_view s, std::string_view suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// On-disk Yaz0 file header.
struct Yaz0Header {
    /* 0x00 */ char magic[4];     // "Yaz0"
    /* 0x04 */ BE(u32) decompressedSize;
    /* 0x08 */ u8 pad[8];
};
static_assert(sizeof(Yaz0Header) == 0x10);

// If `bytes` is a Yaz0 stream, return the inflated payload; otherwise nullopt.
std::optional<std::vector<u8>> tryDecodeYaz0(std::span<const u8> bytes) {
    if (bytes.size() < sizeof(Yaz0Header) ||
        std::memcmp(bytes.data(), "Yaz0", 4) != 0) {
        return std::nullopt;
    }
    ZoneScoped;
    const auto* hdr = reinterpret_cast<const Yaz0Header*>(bytes.data());
    const u32 expandedSize = hdr->decompressedSize;
    std::vector<u8> decoded(expandedSize);
    JKRDecomp::decodeSZS(const_cast<u8*>(bytes.data()), decoded.data(),
                         expandedSize, 0);
    return decoded;
}

std::optional<std::vector<u8>> readWholeFile(const std::filesystem::path& path) {
    std::FILE* f = std::fopen(path.string().c_str(), "rb");
    if (!f) return std::nullopt;
    std::fseek(f, 0, SEEK_END);
    long len = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (len < 0) { std::fclose(f); return std::nullopt; }
    std::vector<u8> buf(static_cast<size_t>(len));
    size_t got = std::fread(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    if (got != buf.size()) return std::nullopt;
    return buf;
}

// Extract the path portion under "res/" from JSystem's absolute path.
// Example: "/arcName/res/Stage/D_SB10/R00_00.arc" -> "res/Stage/D_SB10/R00_00.arc"
std::string_view extractResPath(std::string_view gcPath) {
    auto p = gcPath.find("res/");
    if (p == std::string_view::npos) return {};
    return gcPath.substr(p);
}


// Case-insensitive ASCII suffix match — RARC archives lowercase filenames
// at build time, but our HD pack.gz preserves the original Wii-U authoring
// camelCase. Example: RARC has "coverbg.bti", pack has "coverBG.bti.gtx".
bool endsWithSuffixCI(std::string_view s, std::string_view suffix) {
    if (s.size() < suffix.size()) return false;
    auto toLower = [](unsigned char c) -> unsigned char {
        return (c >= 'A' && c <= 'Z') ? c + ('a' - 'A') : c;
    };
    const char* a = s.data() + (s.size() - suffix.size());
    for (size_t i = 0; i < suffix.size(); ++i) {
        if (toLower(a[i]) != toLower(suffix[i])) return false;
    }
    return true;
}

// Match an arc-relative path (e.g. "bmdr/model.bmd") against the Gfx2 entries
// in the HD pack, which look like "tex/.../<arc-rel>.gtx".
const TmpkEntry* findGtxBySuffix(const TphdPack& pack, std::string_view arcRelPath) {
    const std::string tail = "/" + std::string(arcRelPath) + ".gtx";
    for (const auto& e : pack.entries()) {
        if (e.data.size() < 4 || std::memcmp(e.data.data(), "Gfx2", 4) != 0) continue;
        if (endsWithSuffixCI(e.name, tail)) return &e;
    }
    return nullptr;
}

// Post-deswizzle CPU expansions to RGBA8. Used for formats whose HD layout
// can't be directly sampled with a GPU view swizzle (IA4 nibble unpack,
// RGB565 16-bit), and as a fallback if R8_PC/RG8_PC view swizzle isn't
// available. GC sampling semantics: I8 -> (I,I,I,I); IA4/IA8 -> (I,I,I,A).

std::vector<u8> expandR5G6B5toRgba8(std::span<const u8> in, u32 width, u32 height) {
    std::vector<u8> out(static_cast<size_t>(width) * height * 4);
    const size_t pixelCount = static_cast<size_t>(width) * height;
    for (size_t i = 0; i < pixelCount && (i * 2 + 1) < in.size(); ++i) {
        // GX2 stores RGB565 pixel data in GPU-native LE
        u16 px;
        std::memcpy(&px, &in[i * 2], sizeof(px));
        u8 b5 = static_cast<u8>((px >> 11) & 0x1F);
        u8 g6 = static_cast<u8>((px >> 5) & 0x3F);
        u8 r5 = static_cast<u8>(px & 0x1F);
        out[i * 4 + 0] = static_cast<u8>((r5 << 3) | (r5 >> 2));
        out[i * 4 + 1] = static_cast<u8>((g6 << 2) | (g6 >> 4));
        out[i * 4 + 2] = static_cast<u8>((b5 << 3) | (b5 >> 2));
        out[i * 4 + 3] = 0xFF;
    }
    return out;
}

// IA4: high nibble = A, low nibble = I (matches aurora's GC IA4 decoder).
std::vector<u8> expandIA4toRgba8(std::span<const u8> in, u32 width, u32 height) {
    std::vector<u8> out(static_cast<size_t>(width) * height * 4);
    const size_t pixelCount = static_cast<size_t>(width) * height;
    for (size_t i = 0; i < pixelCount && i < in.size(); ++i) {
        u8 b = in[i];
        u8 A = static_cast<u8>((b & 0xF0) | (b >> 4));
        u8 I = static_cast<u8>(((b & 0x0F) << 4) | (b & 0x0F));
        out[i * 4 + 0] = I; out[i * 4 + 1] = I; out[i * 4 + 2] = I; out[i * 4 + 3] = A;
    }
    return out;
}

enum class Expansion {
    None,
    R5G6B5_to_RGBA8,
    IA4_to_RGBA8,
};

struct Gx2FormatMapping {
    u32 gx2Format;       // GX2 surface format
    u8 newGxFormat;      // Aurora PC-target format
    u32 bpp;             // Deswizzle bits-per-pixel (per pixel, or per 4x4 block for BCn)
    bool isBcn;
    Expansion expansion; // Optional post-deswizzle CPU expansion
};

// I8/IA8 pass through as R8_PC/RG8_PC (aurora applies .rrrr/.rrrg view
// swizzle on the GPU side — half / quarter the VRAM of CPU-expanded RGBA8).
// IA4 + RGB565 need CPU expansion (nibble / 16-bit unpack). CMPR stays
// BC1_PC (compressed on the GPU).
constexpr Gx2FormatMapping kFormatMap[] = {
    // gx2 fmt          PC target            bpp  isBcn  expansion
    { 0x01 /* I8     */, 0x41 /* R8_PC    */,  8,  false, Expansion::None },
    { 0x02 /* IA4    */, 0x46 /* RGBA8_PC */,  8,  false, Expansion::IA4_to_RGBA8 },
    { 0x07 /* IA8    */, 0x43 /* RG8_PC   */, 16,  false, Expansion::None },
    { 0x08 /* RGB565 */, 0x46 /* RGBA8_PC */, 16,  false, Expansion::R5G6B5_to_RGBA8 },
    { 0x1A /* RGBA8  */, 0x46 /* RGBA8_PC */, 32,  false, Expansion::None },
    { 0x31 /* CMPR   */, 0x4E /* BC1_PC   */, 64,  true,  Expansion::None },
};

const Gx2FormatMapping* findFormatMapping(u32 gx2Format) {
    for (const auto& m : kFormatMap) {
        if (m.gx2Format == gx2Format) return &m;
    }
    return nullptr;
}

std::vector<u8> applyExpansion(Expansion exp, std::vector<u8> linear, u32 w, u32 h) {
    switch (exp) {
    case Expansion::R5G6B5_to_RGBA8: return expandR5G6B5toRgba8(linear, w, h);
    case Expansion::IA4_to_RGBA8:    return expandIA4toRgba8(linear, w, h);
    case Expansion::None: break;
    }
    return linear;
}

// Per-mip tile-mode + pitch. Demote rule mirrored from decaf-emu's
// R600AddrLib::ComputeSurfaceMipLevelTileMode (MIT, AMD-derived) — see
// AddrLib.cpp header for the full copyright notice.
//
// R700 macro-tile size: 32 × 16 elements (BCN element = 4×4 block).
// Mips below that are demoted to Tiled1DThin1 (microtile-only, 8-element
// align).
struct MipLevelDesc {
    u32 width;
    u32 height;
    u32 pitch;
    addrlib::TileMode tileMode;
};

MipLevelDesc mipLevelDesc(const GtxSurface& s, u32 level, bool isBcn, u32 bpp) {
    MipLevelDesc d{};
    d.width    = std::max(1u, s.width  >> level);
    d.height   = std::max(1u, s.height >> level);
    d.tileMode = static_cast<addrlib::TileMode>(s.tileMode);

    if (level == 0) {
        d.pitch = s.pitch;
        return d;
    }

    // Mirror decaf's widthAlignFactor: when one microtile is smaller than
    // the pipe interleave (256 B), the demote threshold scales up.
    const u32 microTileBytes = (bpp * 64u) / 8u;
    const u32 widthAlignFactor = (microTileBytes <= 256u) ? (256u / microTileBytes) : 1u;

    if (d.tileMode == addrlib::TileMode::Tiled2DThin1 ||
        d.tileMode == addrlib::TileMode::Tiled2BThin1) {
        const u32 demoteWidth  = widthAlignFactor * 32u;
        const u32 wElem = isBcn ? (d.width  + 3u) / 4u : d.width;
        const u32 hElem = isBcn ? (d.height + 3u) / 4u : d.height;
        if (wElem < demoteWidth || hElem < 16u) {
            d.tileMode = addrlib::TileMode::Tiled1DThin1;
        }
    }

    const bool is1D = (d.tileMode == addrlib::TileMode::Tiled1DThin1 ||
                       d.tileMode == addrlib::TileMode::Tiled1DThick);
    const u32 alignment = is1D ? (8u * widthAlignFactor) : 32u;

    const u32 pixelsPerBlock = isBcn ? 4u : 1u;
    const u32 widthInBlock  = (d.width + pixelsPerBlock - 1u) / pixelsPerBlock;
    u32 levelPitch = ((widthInBlock + alignment - 1u) / alignment) * alignment;
    d.pitch = std::max(1u, levelPitch);
    return d;
}

// Slice the bytes for a single mip level. Wii-U quirk: mipOffsets[0] is
// often image_size, not a mipData offset. Level 1
// always starts at 0 in mipData; level >= 2 uses mipOffsets[level - 1].
std::span<const u8> mipLevelData(const GtxSurface& s, u32 level) {
    if (level == 0) return s.baseData;
    if (level >= s.mipCount) return {};

    u32 start = 0;
    if (level >= 2 && level - 1 < s.mipOffsets.size()) {
        start = s.mipOffsets[level - 1];
    }
    if (start >= s.mipData.size()) return {};

    u32 end = static_cast<u32>(s.mipData.size());
    if (level + 1 < s.mipCount && level < s.mipOffsets.size()) {
        const u32 next = s.mipOffsets[level];
        if (next > start && next <= s.mipData.size()) end = next;
    }
    return s.mipData.subspan(start, end - start);
}

struct DeswizzleResult {
    std::vector<u8> bytes;
    u32 mipCount;
};

DeswizzleResult deswizzleAllMips(const Gx2FormatMapping& m, const GtxSurface& s) {
    ZoneScoped;
    DeswizzleResult out{};
    const u32 maxLevels = std::min(s.mipCount, 13u);
    for (u32 level = 0; level < maxLevels; ++level) {
        const std::span<const u8> slice = mipLevelData(s, level);
        if (slice.empty()) break;

        const MipLevelDesc lvl = mipLevelDesc(s, level, m.isBcn, m.bpp);
        const addrlib::SurfaceDesc desc{
            .width    = lvl.width,
            .height   = lvl.height,
            .pitch    = lvl.pitch,
            .bpp      = m.bpp,
            .tileMode = lvl.tileMode,
            .swizzle  = s.swizzle,
            .isBcn    = m.isBcn,
            .isDepth  = false,
        };

        auto linear = applyExpansion(m.expansion,
                                     addrlib::deswizzle(desc, slice),
                                     lvl.width, lvl.height);
        out.bytes.insert(out.bytes.end(), linear.begin(), linear.end());
        out.mipCount = level + 1;
    }
    return out;
}

void registerHdSurface(const Gx2FormatMapping& m, const GtxSurface& s,
                       const void* pixelPtr, std::string_view gtxName,
                       u32 surfaceIdx) {
    ZoneScoped;
    auto decoded = deswizzleAllMips(m, s);

    HdLog.info("HD reg: ptr={} fmt=0x{:02X} {}x{} mips={}/{} bytes={} gtx={}[{}]",
               pixelPtr, m.newGxFormat, s.width, s.height,
               decoded.mipCount, s.mipCount, decoded.bytes.size(),
               gtxName, surfaceIdx);

    aurora::gfx::HdReplacement r;
    r.bytes    = std::move(decoded.bytes);
    r.width    = s.width;
    r.height   = s.height;
    r.gxFormat = m.newGxFormat;
    r.mipCount = std::max(decoded.mipCount, 1u);
    aurora::gfx::hd_register_replacement(pixelPtr, std::move(r));
}

// Lightweight RARC walker that returns per-file offsets without copying
// arc bytes — we need absolute pointers into the cached HD arc bytes
// (stable address) to match what the game later passes to GXInitTexObj.
struct ArcFileInfo {
    std::string path;  // e.g. "bmdr/model.bmd"
    u32 dataOffset;    // absolute offset from arc base
    u32 dataSize;
};

std::vector<ArcFileInfo> parseRarcFiles(std::span<const u8> arc) {
    std::vector<ArcFileInfo> out;
    if (arc.size() < 0x40 || std::memcmp(arc.data(), "RARC", 4) != 0) return out;

    constexpr size_t kMetaBase = sizeof(SArcHeader);  // = 0x20
    if (arc.size() < kMetaBase + sizeof(SArcDataInfo)) return out;

    const auto* hdr = reinterpret_cast<const SArcHeader*>(arc.data());
    const auto* dataInfo = reinterpret_cast<const SArcDataInfo*>(arc.data() + kMetaBase);

    const u32 nodeCount   = dataInfo->num_nodes;
    const size_t nodeTbl  = dataInfo->node_offset + kMetaBase;
    const size_t fileTbl  = dataInfo->file_entry_offset + kMetaBase;
    const size_t strTbl   = dataInfo->string_table_offset + kMetaBase;
    const size_t dataBase = kMetaBase + hdr->file_data_offset;

    auto readStringAt = [&](u32 offset) -> std::string {
        const u8* start = arc.data() + strTbl + offset;
        const u8* bufferEnd = arc.data() + arc.size();
        if (start >= bufferEnd) return {};

        const void* nul = std::memchr(start, 0,
                                      static_cast<size_t>(bufferEnd - start));
        const u8* terminator = nul ? static_cast<const u8*>(nul) : bufferEnd;
        return std::string(reinterpret_cast<const char*>(start),
                           static_cast<size_t>(terminator - start));
    };

    const auto* nodes = reinterpret_cast<const JKRArchive::SDIDirEntry*>(
        arc.data() + nodeTbl);
    const auto* files = reinterpret_cast<const JKRArchive::SDIFileEntry*>(
        arc.data() + fileTbl);

    for (u32 ni = 0; ni < nodeCount; ++ni) {
        const auto& node = nodes[ni];
        const std::string dirName = readStringAt(node.name_offset);
        const u16 fc       = node.num_entries;
        const u32 firstIdx = node.first_file_index;
        const bool isRoot = (ni == 0);

        for (u32 fi = 0; fi < fc; ++fi) {
            const auto& entry = files[firstIdx + fi];
            const u32 typeFlagsAndName = entry.type_flags_and_name_offset;
            const u8  typeFlags = static_cast<u8>(typeFlagsAndName >> 24);
            // Bit 0x01 = file, 0x02 = directory. We only want files.
            if ((typeFlags & 0x03) != 0x01) continue;

            std::string fname = readStringAt(typeFlagsAndName & 0xFFFFFF);
            if (fname == "." || fname == "..") continue;

            out.push_back({
                (!isRoot && !dirName.empty())
                    ? dirName + "/" + fname
                    : std::move(fname),
                static_cast<u32>(dataBase + entry.data_offset),
                entry.data_size,
            });
        }
    }
    return out;
}

// Absolute offset of slot `slotIdx`'s BTI header within a BMD's TEX1 block.
// Returns 0 on failure (the TEX1 table never sits at offset 0, so 0 is a
// safe sentinel).
u32 bmdSlotBtiOffset(std::span<const u8> bmd, u32 slotIdx) {
    constexpr size_t kBlocksOffset = offsetof(J3DModelFileData, mBlocks);  // = 0x20
    if (bmd.size() < kBlocksOffset ||
        std::memcmp(bmd.data(), "J3D2", 4) != 0) return 0;

    const auto* fileData = reinterpret_cast<const J3DModelFileData*>(bmd.data());
    const u32 numSections = fileData->mBlockNum;
    size_t pos = kBlocksOffset;

    for (u32 i = 0; i < numSections && pos + sizeof(J3DModelBlock) <= bmd.size(); ++i) {
        const auto* blk = reinterpret_cast<const J3DModelBlock*>(bmd.data() + pos);
        const u32 blockSize = blk->mBlockSize;
        if (blk->mBlockType == 'TEX1') {
            const auto* tex1 = reinterpret_cast<const J3DTextureBlock*>(bmd.data() + pos);
            const u16 numTex = tex1->mTextureNum;
            if (slotIdx >= numTex) return 0;
            const size_t btiAbs = pos + static_cast<u32>(tex1->mpTextureRes) + slotIdx * 0x20;
            if (btiAbs + 0x20 > bmd.size()) return 0;
            return static_cast<u32>(btiAbs);
        }
        if (blockSize == 0) break;
        pos += blockSize;
    }
    return 0;
}

// Walk the HD arc, pair BMDs with their pack.gz GTX entries, deswizzle each
// HD surface, and register the decoded bytes with aurora under the absolute
// pointer that GXInitTexObj will later receive.
//
// arcBytes must be the STABLE cache vector — its data() must not move after
// this call, or aurora's pointer lookups will miss.
void registerHdTexturesForArc(std::vector<u8>& arcBytes,
                              const std::vector<ArcFileInfo>& files,
                              const TphdPack& pack,
                              std::string_view arcLabel) {
    ZoneScoped;
    ZoneText(arcLabel.data(), arcLabel.size());
    size_t bmdReg = 0;
    size_t btiReg = 0;

    // Phase A: per-slot textures inside BMD/BDL models.
    for (const auto& f : files) {
        if (!endsWithSuffix(f.path, ".bmd") && !endsWithSuffix(f.path, ".bdl")) continue;

        const TmpkEntry* gtx = findGtxBySuffix(pack, f.path);
        if (!gtx) continue;

        std::span<const u8> bmdBytes(arcBytes.data() + f.dataOffset, f.dataSize);
        auto surfaces = parseGtx(gtx->data);

        for (u32 i = 0; i < surfaces.size(); ++i) {
            const auto& s = surfaces[i];
            if (s.baseData.empty()) continue;

            const Gx2FormatMapping* m = findFormatMapping(s.format);
            if (!m) continue;

            // HD-stub BMDs collapse every BTI's imageOffset to the same
            // pixel address. Rewrite each to be slot-unique so our pointer
            // map doesn't overwrite.
            const u32 btiAbs = bmdSlotBtiOffset(bmdBytes, i);
            if (btiAbs == 0) continue;

            auto* timg = reinterpret_cast<ResTIMG*>(
                arcBytes.data() + f.dataOffset + btiAbs);
            if (timg->imageOffset == 0) {
                HdLog.debug("Skip cross-arc placeholder slot {} in {}: "
                            "imageOffset==0",
                            i, gtx->name);
                continue;
            }

            const u32 newImgOff = 0x20 + i * 0x20;
            timg->imageOffset = static_cast<s32>(newImgOff);
            const u8 hdMips = static_cast<u8>(std::clamp<u32>(s.mipCount, 1u, 13u));
            timg->mipmapEnabled = 1;
            timg->mipmapCount = hdMips;
            timg->maxLOD = static_cast<s8>((hdMips - 1) * 8);
            timg->maxAnisotropy = GX_ANISO_4;
            registerHdSurface(*m, s,
                              arcBytes.data() + f.dataOffset + btiAbs + newImgOff,
                              gtx->name, i);
            ++bmdReg;
        }
    }

    // Phase B: standalone .bti files. Each BTI is its own arc entry; the
    // game loads it via JUTTexture (or similar) which calls GXInitTexObj
    // with `(u8*)resTIMG + imageOffset`. Register that exact pointer.
    for (const auto& f : files) {
        if (!endsWithSuffix(f.path, ".bti")) continue;
        if (f.dataSize < 0x20) continue;

        const TmpkEntry* gtx = findGtxBySuffix(pack, f.path);
        if (!gtx) continue;

        auto surfaces = parseGtx(gtx->data);
        if (surfaces.empty()) continue;
        const auto& s = surfaces[0];
        if (s.baseData.empty()) continue;

        const Gx2FormatMapping* m = findFormatMapping(s.format);
        if (!m) continue;

        // HD-stub BTIs put garbage in imageOffset. Write 0x20 so BOTH
        // consumer paths land on the same address (JUTTexture::storeTIMG and
        // direct-access helpers like dKyr_set_btitex_common). Both compute
        // i_img + 0x20, matching where we register below.
        auto* timg = reinterpret_cast<ResTIMG*>(arcBytes.data() + f.dataOffset);
        timg->imageOffset = 0x20;
        const u8 hdMips = static_cast<u8>(std::clamp<u32>(s.mipCount, 1u, 13u));
        timg->mipmapEnabled = 1;
        timg->mipmapCount = hdMips;
        timg->maxLOD = static_cast<s8>((hdMips - 1) * 8);
        timg->maxAnisotropy = GX_ANISO_4;
        registerHdSurface(*m, s, arcBytes.data() + f.dataOffset + 0x20,
                          gtx->name, 0);
        ++btiReg;
    }

    HdLog.info("registerHdTextures[{}]: {} BMD-slot, {} standalone-BTI replacements",
               arcLabel, bmdReg, btiReg);
}

}

void setHdContentPath(std::filesystem::path contentPath) {
    g_contentPath = std::move(contentPath);
    std::lock_guard lk(g_cacheMutex);
    g_mountBuffers().clear();
    g_entryNumToBytes().clear();
    aurora::gfx::hd_clear_replacements();
    aurora::gfx::hd_clear_arc_ranges();
    HdLog.info("HD content path set to: {}",
               g_contentPath.empty() ? "(disabled)" : g_contentPath.string());
}

// HD arcs whose Wii-U layouts don't match the GC UI pipeline.
static constexpr std::string_view kHdSkipList[] = {
    "res/Object/LogoUs.arc",
    "res/Object/balloon2D.arc",
    "res/Object/Coach2D.arc",
    "res/Object/fileSel.arc",
    "res/Layout/button.arc",
    "res/Layout/Title2D.arc",
    "res/Layout/main2D.arc",
    "res/Layout/dmapres.arc",
    "res/Layout/fmapres.arc",
    "res/Layout/saveres.arc",
    "res/Layout/fishres.arc",
    "res/FieldMap/res-f.arc",
    "res/FieldMap/res-d.arc",
};

std::optional<std::vector<u8>*> tryLoadHdArchive(std::string_view gcPath) {
    if (g_contentPath.empty()) return std::nullopt;

    std::string_view resPath = extractResPath(gcPath);
    if (resPath.empty()) return std::nullopt;

    for (auto skip : kHdSkipList) {
        if (resPath == skip) return std::nullopt;
    }

    std::filesystem::path hdArcPath = g_contentPath / std::string(resPath);
    if (!std::filesystem::exists(hdArcPath)) {
        return std::nullopt;  // no HD override — vanilla GC path
    }
    ZoneScoped;
#ifdef TRACY_ENABLE
    {
        auto fn = hdArcPath.filename().string();
        ZoneText(fn.c_str(), fn.size());
    }
#endif

    auto hdBytesOpt = readWholeFile(hdArcPath);
    if (!hdBytesOpt) {
        HdLog.warn("HD arc read failed: {}", hdArcPath.string());
        return std::nullopt;
    }

    if (auto inflated = tryDecodeYaz0(*hdBytesOpt)) {
        HdLog.info("HD arc Yaz0-decompressed: {} -> {} bytes",
                   hdArcPath.filename().string(), inflated->size());
        hdBytesOpt = std::move(inflated);
    }

    auto hdFiles = parseRarcFiles(std::span<const u8>(
        hdBytesOpt->data(), hdBytesOpt->size()));

    // Sidecar pack.gz holds the HD textures.
    auto hdPackPath = hdArcPath;
    hdPackPath.replace_extension(".pack.gz");
    std::optional<TphdPack> hdPack;
    if (std::filesystem::exists(hdPackPath)) {
        hdPack = TphdPack::loadFromFile(hdPackPath);
        if (!hdPack) HdLog.warn("HD pack failed to load: {}", hdPackPath.string());
    }

    // std::list keeps element addresses stable for aurora's pointer map.
    std::vector<u8>* mountBytes;
    std::string filename = hdArcPath.filename().string();
    {
        std::lock_guard lk(g_cacheMutex);
        g_mountBuffers().emplace_back(std::move(*hdBytesOpt));
        mountBytes = &g_mountBuffers().back();
    }

    HdLog.info("HD arc mount buffer allocated: {} at {} ({} bytes, pack.gz={})",
               filename, static_cast<const void*>(mountBytes->data()),
               mountBytes->size(), hdPack ? "yes" : "no");

    aurora::gfx::hd_register_arc_range(mountBytes->data(), mountBytes->size(),
                                       filename);
    if (hdPack) {
        registerHdTexturesForArc(*mountBytes, hdFiles, *hdPack, filename);
    }

    return mountBytes;
}

void registerHdBytesForEntryNum(s32 entryNum, const std::vector<u8>* bytes) {
    if (entryNum < 0 || bytes == nullptr) return;
    std::lock_guard lk(g_cacheMutex);
    g_entryNumToBytes()[entryNum] = bytes;
}

const std::vector<u8>* getHdBytesForEntryNum(s32 entryNum) {
    if (entryNum < 0) return nullptr;
    std::lock_guard lk(g_cacheMutex);
    auto it = g_entryNumToBytes().find(entryNum);
    return (it != g_entryNumToBytes().end()) ? it->second : nullptr;
}

}
