#include <utility>

#include "dusk/io.hpp"
#include "mod_loader.hpp"

namespace fs = std::filesystem;

namespace dusk::modding {
ModBundleDisk::ModBundleDisk(fs::path root) : root_path(std::move(root)) {}

std::vector<u8> ModBundleDisk::readFile(const std::string& fileName) {
    const fs::path filePath = reinterpret_cast<const char8_t*>(fileName.c_str());
    const auto finalPath = root_path / fileName;

    return io::FileStream::ReadAllBytes(finalPath);
}

std::vector<std::string> ModBundleDisk::getFileNames() {
    std::vector<std::string> files;

    std::error_code ec;
    for (fs::recursive_directory_iterator it(root_path,
             fs::directory_options::skip_permission_denied |
                 fs::directory_options::follow_directory_symlink,
             ec);
        it != fs::recursive_directory_iterator(); it.increment(ec))
    {
        if (ec) {
            break;
        }

        if (!it->is_regular_file()) {
            continue;
        }

        const auto& path = it->path();
        const auto relPath = fs::relative(path, root_path);
        auto string = io::fs_path_to_string(relPath);
        if constexpr (fs::path::preferred_separator != '/') {
            // Convert \ to / on Windows
            for (auto& chr : string) {
                if (chr == fs::path::preferred_separator) {
                    chr = '/';
                }
            }
        }

        files.emplace_back(std::move(string));
    }

    return files;
}

}  // namespace dusk::modding