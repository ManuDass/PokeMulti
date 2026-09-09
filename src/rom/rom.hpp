#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>
namespace fr {
inline constexpr size_t FireRedRomSize = 16 * 1024 * 1024;
inline constexpr uint32_t RomBase = 0x08000000;
enum class FireRedRevision { Unsupported, FireRed_US_10, FireRed_US_11, LeafGreen_US_10, LeafGreen_US_11 };
struct RomReport {
    FireRedRevision revision = FireRedRevision::Unsupported;
    size_t size = 0;
    std::string title, gameCode, sha256, sha1, error;
    uint8_t headerVersion = 0;
    bool supported() const { return revision != FireRedRevision::Unsupported && error.empty(); }
};
std::string revisionName(FireRedRevision revision);
std::string digest(std::span<const uint8_t> bytes, bool sha256 = true);
std::vector<uint8_t> readRom(const std::filesystem::path& path);
RomReport inspectRom(std::span<const uint8_t> bytes);
FireRedRevision identifyRevision(std::string_view sha1, uint8_t headerVersion);
std::string describe(const RomReport& report);
}
