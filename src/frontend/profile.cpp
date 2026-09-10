#include "frontend/profile.hpp"
#include "platform/text.hpp"
#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
namespace fr {
void validateProfile(const Profile& profile) {
    if (!profile.romPath.is_absolute() || profile.romPath.native().size() > 32760)
        throw std::runtime_error("Profile ROM path must be an absolute local path.");
    const auto path = narrow(profile.romPath.native());
    if (path.find_first_of("\r\n") != std::string::npos || path.find('\0') != std::string::npos)
        throw std::runtime_error("Profile ROM path contains invalid characters.");
    if (profile.romSha256.size() != 64 || !std::all_of(profile.romSha256.begin(), profile.romSha256.end(), [](char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'); }))
        throw std::runtime_error("Profile ROM hash must be a lowercase SHA-256 value.");
    const auto name = widen(profile.playerName);
    if (name.empty() || name.size() > 24 || std::all_of(name.begin(), name.end(), [](wchar_t c) { return c == L' '; }))
        throw std::runtime_error("Choose a player name with 1-24 characters.");
    for (auto c : name) if (c < 32 || c == 127) throw std::runtime_error("Player name contains a control character.");
}
std::optional<Profile> loadProfile(const std::filesystem::path& file) {
    if (!std::filesystem::exists(file)) return std::nullopt;
    if (std::filesystem::file_size(file) > 16 * 1024) throw std::runtime_error("Profile file exceeds the size limit.");
    std::ifstream in(file, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot read the profile file.");
    Profile result;
    std::set<std::string> seen;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::istringstream fields(line);
        std::string key, value, extra;
        if (!(fields >> key >> std::quoted(value)) || (fields >> extra) || !seen.insert(key).second)
            throw std::runtime_error("Malformed or duplicate profile field.");
        if (key == "schema") { if (value != "1") throw std::runtime_error("Unsupported profile schema."); }
        else if (key == "rom_path") result.romPath = widen(value);
        else if (key == "rom_sha256") result.romSha256 = value;
        else if (key == "player_name") result.playerName = value;
        else throw std::runtime_error("Unknown profile field.");
    }
    if (in.bad() || seen != std::set<std::string>{"schema","rom_path","rom_sha256","player_name"})
        throw std::runtime_error("Incomplete profile file.");
    validateProfile(result);
    return result;
}
Profile updateProfileRom(const std::filesystem::path& file, const std::filesystem::path& rom, const std::string& hash) {
    auto profile=loadProfile(file);
    if(!profile)throw std::runtime_error("Set up your username before updating the ROM.");
    profile->romPath=rom;profile->romSha256=hash;
    saveProfile(file,*profile);return *profile;
}
Profile renameProfile(const std::filesystem::path& file, const std::string& name) {
    auto profile=loadProfile(file);
    if(!profile)throw std::runtime_error("Create a local profile before editing its username.");
    profile->playerName=name;
    saveProfile(file,*profile);
    return *profile;
}
void saveProfile(const std::filesystem::path& file, const Profile& profile) {
    validateProfile(profile);
    std::filesystem::create_directories(file.parent_path());
    auto temp = file; temp += ".tmp." + std::to_string(GetCurrentProcessId());
    try {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("Cannot create the local profile.");
        out << "schema \"1\"\nrom_path " << std::quoted(narrow(profile.romPath.native()))
            << "\nrom_sha256 " << std::quoted(profile.romSha256)
            << "\nplayer_name " << std::quoted(profile.playerName) << '\n';
        out.flush();
        if (!out) throw std::runtime_error("Could not write the complete profile.");
        out.close();
        if (!out) throw std::runtime_error("Could not close the complete profile.");
        if (!MoveFileExW(temp.c_str(), file.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Could not atomically replace the profile.");
    } catch (...) {
        std::error_code error; std::filesystem::remove(temp, error);
        throw;
    }
}
}
