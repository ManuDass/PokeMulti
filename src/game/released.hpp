#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>
namespace fr::game {
// Individual, player-owned Pokemon data, in semantic fields. No ROM bytes/art.
struct ReleasedPokemon {
    uint32_t personality=0,trainerId=0,experience=0,ivs=0,ribbons=0;
    uint16_t species=0,item=0,origins=0;
    std::array<uint16_t,4> moves{};
    std::array<uint8_t,4> pp{};
    std::array<uint8_t,6> ev{},condition{};
    std::array<uint8_t,10> nickname{};
    std::array<uint8_t,7> trainerName{};
    uint8_t level=0,language=2,markings=0,ppBonuses=0,friendship=0,virus=0,metLocation=0;
    auto operator<=>(const ReleasedPokemon&)const=default;
};
struct ReleasePoint {
    uint8_t group=0,map=0,elevation=0;
    int16_t x=0,y=0;
    auto operator<=>(const ReleasePoint&)const=default;
};
enum class ReleasePhase:uint8_t { Queued,Hello,Leaving,Waiting,Goodbye,Running,Hidden,Battle,Caught };
struct ReleasedMon {
    std::string id,owner,claimant;
    ReleasedPokemon pokemon;
    ReleasePoint origin,position,entrance;
    ReleasePhase phase=ReleasePhase::Queued;
    uint32_t revision=0,tick=0;
    int16_t pixelX=0,pixelY=0;
    uint8_t facing=1,frame=0;
    bool ownerOutside=false;
    auto operator<=>(const ReleasedMon&)const=default;
};
// Native recapture assigns a new trainer; identity survives that change.
bool sameReleasedIndividual(const ReleasedPokemon&,const ReleasedPokemon&);
bool validReleasedPokemon(const ReleasedPokemon&);
bool validReleasedMon(const ReleasedMon&);
std::string encodeReleased(const ReleasedMon&);
ReleasedMon decodeReleased(const std::string&);
// Durable host authority. Captured IDs are tombstones; retrying an offer cannot
// resurrect a caught Pokemon. A battle claim remains reserved over disconnects.
class ReleaseBook {
    std::filesystem::path path;
    std::vector<ReleasedMon> entries;
    void persist()const;
public:
    void open(const std::filesystem::path&);
    const std::vector<ReleasedMon>& records()const{return entries;}
    const ReleasedMon* find(const std::string&)const;
    bool offer(const ReleasedMon&);
    bool advance(const ReleasedMon&,bool durable);
    bool claim(const std::string& id,const std::string& player,const ReleasePoint&);
    bool finish(const std::string& id,const std::string& player,bool caught);
    void flush()const{persist();}
};
struct ReleaseCell { uint8_t elevation=0;bool walk=false,grass=false;uint16_t behavior=0; };
struct ReleaseMap {
    int width=0,height=0;bool outdoors=false;
    std::vector<ReleaseCell> cells;
    std::map<std::pair<int,int>,ReleasePoint> exits;
    const ReleaseCell* cell(int x,int y)const;
};
using ReleaseMaps=std::function<const ReleaseMap*(uint8_t,uint8_t)>;
using ReleaseGoal=std::function<bool(const ReleasePoint&,const ReleaseMap&)>;
// Tile-distance BFS through native connections/doors, including other routes.
std::vector<ReleasePoint> releaseRoute(ReleasePoint start,const ReleaseMaps&,const ReleaseGoal&,
                                      const std::vector<ReleasePoint>& blocked={},size_t limit=200000);
std::optional<ReleasePoint> releaseCrowdPlace(ReleasePoint door,const ReleaseMaps&,
                                            const std::vector<ReleasePoint>& reserved,uint32_t seed);
}
