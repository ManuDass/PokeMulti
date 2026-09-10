#pragma once
#include <filesystem>
#include <optional>
#include <string>
namespace fr {
struct Profile {
    std::filesystem::path romPath;
    std::string romSha256;
    std::string playerName;
    bool operator==(const Profile&) const = default;
};
void validateProfile(const Profile& profile);
std::optional<Profile> loadProfile(const std::filesystem::path& file);
void saveProfile(const std::filesystem::path& file, const Profile& profile);
Profile updateProfileRom(const std::filesystem::path& file, const std::filesystem::path& rom, const std::string& hash);
Profile renameProfile(const std::filesystem::path& file, const std::string& name);
}
