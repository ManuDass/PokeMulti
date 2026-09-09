#pragma once
#include "game/world.hpp"
#include "game/camp.hpp"
#include "game/battle_presence.hpp"
#include "game/story.hpp"
#include <array>
#include <map>
#include <set>
#include <stdexcept>
namespace fr::online {
struct NpcState {
    uint8_t localId=0,owner=255,anim=0,command=0;
    int16_t oldX=0,oldY=0;
    bool visible=true;
    game::PlayerState pose{};
    uint32_t generation=0;
};
struct WildState {
    uint32_t id=0;uint16_t species=0;uint8_t level=0,elevation=0;
    int16_t x=0,y=0,pixelX=0,pixelY=0;
    uint8_t facing=1,frame=0;
};
struct WorldReport {
    game::CampState camp;
    game::BattlePresence battle;
    uint8_t group=0,map=0;bool field=false,started=false,wildEnabled=true;
    uint32_t sequence=0;
    std::vector<NpcState> npcs;
    std::vector<WildState> wild;
};
struct MapState {
    uint8_t group=0,map=0,wildOwner=255;
    uint32_t revision=0,sampleTime=0;
    std::vector<NpcState> npcs;
    std::vector<WildState> wild;
    std::vector<uint32_t> consumed;
};
enum class EncounterKind:uint8_t { Story=1,Npc=2,Wild=3,Unique=4 };
struct EncounterKey {
    uint8_t group=0,map=0;EncounterKind kind=EncounterKind::Story;uint32_t id=0;uint16_t condition=0,expected=0;
    auto operator<=>(const EncounterKey&) const=default;
};
struct EncounterLease { EncounterKey key;uint8_t owner=255;uint32_t token=0; };
struct EncounterRetry { EncounterKey key;std::vector<uint8_t> actors; };
struct SharedWorld {
    std::array<game::CampState,MaxRoomPlayers> camps{};
    std::array<game::BattlePresence,MaxRoomPlayers> battles{};
    std::array<uint32_t,MaxRoomPlayers> campDecisions{};
    bool storyReady=false;
    std::vector<game::StoryValue> story;
    std::vector<MapState> maps;
    std::vector<EncounterLease> leases;
    std::vector<EncounterRetry> retries;
};
bool validWorldReport(const WorldReport& report);
// Host-only arbiter. It consumes semantic state and authenticated room slots.
class WorldAuthority {
    struct Presence {WorldReport report;uint64_t time=0;};
    std::array<Presence,MaxRoomPlayers> present{};
    struct Attempt {uint32_t token=0;std::vector<uint8_t> actors;};
    std::array<Attempt,MaxRoomPlayers> attempts{};
    uint32_t revision=0;
    SharedWorld state;
    uint8_t rewardPolicy=game::DefaultRewardSharing;
    MapState& map(uint8_t group,uint8_t number);
    bool available(unsigned slot,uint8_t group,uint8_t number,uint64_t now,bool requireStarted=true) const;
public:
    void rewardRules(uint8_t policy){if(state.storyReady||!state.story.empty()||policy>game::AllRewardSharing)throw std::runtime_error("Reward rules are fixed when the room starts");rewardPolicy=policy;}
    const SharedWorld& snapshot() const{return state;}
    void report(unsigned slot,const WorldReport& update,uint64_t now);
    void leave(unsigned slot);
    void rejectCamp(unsigned slot,uint32_t id){if(slot<MaxRoomPlayers){state.campDecisions[slot]=id;state.camps[slot]={};}}
    bool claim(unsigned slot,const EncounterKey& key,uint32_t token,uint64_t now);
    void checkpoint(unsigned slot,uint32_t token,const std::vector<NpcState>& actors);
    void release(unsigned slot,uint32_t token,bool retry=false);
    // Initial host state, then per-key compare-and-set transactions. Stale
    // clients cannot restore flags that another encounter has already changed.
    void story(unsigned slot,const std::vector<game::StoryValue>& values,bool initial,bool final);
};
}
