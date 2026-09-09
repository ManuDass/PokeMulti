#pragma once
#include "game/world.hpp"
#include "game/follower.hpp"
#include <functional>
#include <set>
namespace fr::game {
struct BattleMon {uint8_t position=0;uint16_t species=0;int16_t x=0,y=0;uint8_t facing=1;bool visible=true;uint8_t frame=0;bool walking=false;};
struct BattlePresence {uint32_t id=0,tick=0;PlayerState trainer;std::vector<BattleMon> mons;bool settled=true,returning=false;};
inline int battleHop(uint32_t tick,unsigned phase=0){const unsigned t=(tick+phase)%48;return t<16?-int((t<8?t:16-t)/2):0;}
inline void standFacing(PlayerState& p,unsigned face){p.facing=uint8_t(face);p.spriteFrame=uint8_t(face==2?1:face>=3?2:0);p.flip=uint8_t(face==4?1:0);p.offsetX=p.offsetY=0;p.followerVisible=false;}
inline bool validBattle(const BattlePresence& b){
    if(!b.id)return b.mons.empty();const auto& p=b.trainer;
    if(!p.active||p.x<0||p.y<0||p.x>511||p.y>511||p.pixelX<0||p.pixelY<0||p.pixelX>8176||p.pixelY>8176||p.elevation>15||p.graphics>=152||p.facing<1||p.facing>4||p.spriteFrame>=64||p.flip>3||b.mons.size()>4)return false;
    std::set<unsigned> positions;
    for(const auto& m:b.mons)if(m.position>3||!positions.insert(m.position).second||(!m.species&&m.visible)||m.species>411||m.x<0||m.y<0||m.x>8176||m.y>8176||std::abs(int(m.x)-p.pixelX)>64||std::abs(int(m.y)-p.pixelY)>64||m.facing<1||m.facing>4||m.frame>3)return false;
    return true;
}
inline PlayerState battleMonPose(const BattlePresence& battle,const BattleMon& mon,unsigned slot){
    PlayerState p=battle.trainer;p.active=mon.visible;p.pixelX=mon.x;p.pixelY=mon.y;p.x=int16_t(mon.x/16);p.y=int16_t(mon.y/16);p.follower=mon.species;p.followerFacing=mon.facing;p.followerFrame=mon.walking?mon.frame:0;p.offsetX=0;p.offsetY=int8_t(battle.settled&&!battle.returning?battleHop(battle.tick,mon.position*12):0);p.identity=(uint64_t(battle.id)<<8)|slot;return p;
}
}
