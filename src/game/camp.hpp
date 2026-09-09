#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>
namespace fr::game {
inline constexpr int CampWidth=4,CampHeight=3,CampRadius=5,CampSpan=CampRadius*2+1;
struct CampMon {uint16_t species=0;int16_t x=0,y=0;uint8_t facing=1,frame=0,mood=0;};
struct CampState {
    uint32_t id=0,sequence=0,sampleTime=0,tick=0;
    uint8_t group=0,map=0,elevation=0;
    int16_t x=0,y=0;
    std::vector<CampMon> party;
    std::array<uint16_t,CampSpan> ground{};
};
struct CampCell {int x=0,y=0;auto operator<=>(const CampCell&) const=default;};
using CampTileCheck=std::function<bool(int,int)>;
bool campContains(const CampState&,int x,int y);
bool campsOverlap(const CampState&,const CampState&);
bool validCamp(const CampState&);
bool campGrass(uint8_t mapType,bool cave,bool generalTileset,uint16_t tile,uint16_t behavior,uint8_t elevation);
bool campWalkable(const CampState&,int x,int y);
bool campHasEntrance(const CampState&);
void setCampGround(CampState&,const std::vector<CampCell>&);
std::vector<CampCell> campGround(const CampState&,const CampTileCheck&,std::optional<CampCell> anchor=std::nullopt);
struct CampEdge {CampCell cell;uint8_t direction=0;}; // down, up, left, right
std::vector<CampEdge> campPerimeter(const CampState&);
bool campFootprint(const CampState&,const CampTileCheck&);
class CampSimulation {
    struct Step {int x=0,y=0,tx=0,ty=0;uint32_t begin=0,end=0,pause=0;};
    std::array<Step,6> steps{};
    uint32_t seed=1;
    uint32_t random();
public:
    CampState state;
    bool start(CampState camp,const std::vector<uint16_t>& party,const std::vector<CampCell>& ground);
    void update(const CampTileCheck& free);
};
}
