#pragma once
#include "game/campaign.hpp"
#include "game/story.hpp"
#include <filesystem>
#include <map>
namespace fr::online {
class CampaignBook {
    std::filesystem::path path;
    game::CampaignState state;
    std::map<uint16_t,uint16_t> saved;
    void flush() const;
public:
    void open(const std::filesystem::path& folder,bool fresh=false);
    bool ready()const{return !saved.empty();}
    const game::CampaignState& snapshot()const{return state;}
    std::vector<game::StoryValue> baseline(uint8_t policy) const;
    void commit(const std::vector<game::StoryValue>& story,const std::string& actor);
};
}
