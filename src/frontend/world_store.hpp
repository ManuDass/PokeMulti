#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include <cstdint>
#include <span>
namespace fr {
struct WorldInfo {
    std::string id,name,romHash;
    std::filesystem::path folder;
};
inline constexpr size_t WorldCheckpointLimit=524288;
bool worldIdValid(const std::string& id);
std::string worldRandomId();
std::vector<WorldInfo> listWorlds(const std::filesystem::path& root,const std::string& romHash);
WorldInfo loadWorld(const std::filesystem::path& folder);
WorldInfo createWorld(const std::filesystem::path& root,const std::string& name,const std::string& romHash);
void atomicWorldFile(const std::filesystem::path& file,std::span<const uint8_t> bytes);
std::vector<uint8_t> readWorldFile(const std::filesystem::path& file,size_t limit=WorldCheckpointLimit);
// One atomic checkpoint contains the flash save and its transaction sidecars.
std::vector<uint8_t> captureCheckpoint(const std::filesystem::path& runtime,std::span<const uint8_t> flash);
bool validCheckpoint(std::span<const uint8_t> bytes);
void restoreCheckpoint(const std::filesystem::path& runtime,std::span<const uint8_t> bytes);
class WorldPlayers {
    WorldInfo world_;
public:
    explicit WorldPlayers(WorldInfo world):world_(std::move(world)){}
    bool authenticate(const std::string& player,const std::string& secret);
    std::vector<uint8_t> load(const std::string& player) const;
    void commit(const std::string& player,std::span<const uint8_t> checkpoint);
    const WorldInfo& world() const {return world_;}
};
}
