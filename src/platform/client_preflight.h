#pragma once
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

// Validate files before creating a window or reserving the guest address space.
// This only checks the container/architecture, not game compatibility or trust.
namespace client {
inline bool arm64_elf(const std::filesystem::path& path) {
    unsigned char h[64] = {};
    std::ifstream f(path, std::ios::binary);
    return f.read(reinterpret_cast<char*>(h), sizeof(h)) &&
        h[0] == 0x7f && h[1] == 'E' && h[2] == 'L' && h[3] == 'F' &&
        h[4] == 2 && h[5] == 1 && h[6] == 1 && h[16] == 3 && h[17] == 0 &&
        h[18] == 183 && h[19] == 0;
}
inline std::vector<std::string> check_data(const std::string& library,
        const std::string& assets, const std::string& sre) {
    namespace fs = std::filesystem;
    std::vector<std::string> errors;
    if (!arm64_elf(fs::u8path(library)))
        errors.push_back("Missing or invalid ARM64 ELF game library: " + library);
    std::error_code ec;
    const auto root = fs::u8path(assets);
    if (!fs::is_directory(root, ec) ||
        (!fs::is_regular_file(root / "gamedata.gdata", ec) &&
         !fs::is_regular_file(root / "resources" / "gamedata.gdata", ec)))
        errors.push_back("Missing game assets (including gamedata.gdata): " + assets);
    if (!sre.empty() && !arm64_elf(fs::u8path(sre)))
        errors.push_back("Missing or invalid ARM64 ELF guest runtime: " + sre);
    return errors;
}
}
