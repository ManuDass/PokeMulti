#pragma once
#include "game/unique.hpp"
#include "game/campaign.hpp"
#include "game/rewards.hpp"
#include <array>
#include <algorithm>
#include <cstdint>
#include <vector>
namespace fr::game {
// Explicit policy for the supported FireRed revisions. Unknown state stays local.
// IDs below 0x900 are flags; 0x4000..0x40ff are semantic script variables.
inline bool sharedStory(uint16_t id,uint8_t policy=DefaultRewardSharing) {
    if(personalStoryState(id))return true; // campaign proof; native trainer state stays personal
    if(uniqueFlag(id))return !(policy&ShareSpecialPokemon);
    for(const auto& reward:storyRewardRules)if(id==reward.claimed)return !shareReward(reward,policy);
    if(id>=0x500&&id<0x800)return true; // defeated trainers
    if(id>=0x820&&id<=0x827)return true; // gym badges, reward flags remain personal
    if(id>=0x27a&&id<=0x28d)return true; // Silph doors
    if(id>=0x4000){
        switch(id){
        case 0x4057:case 0x406c:case 0x4051:case 0x4052:case 0x4054:case 0x4059:case 0x405a:
        case 0x405b:case 0x405c:case 0x405d:case 0x405e:case 0x405f:case 0x4060:
        case 0x4062:case 0x4063:case 0x4064:case 0x4065:case 0x4066:case 0x4067:
        case 0x406b:case 0x4070:case 0x4071:case 0x4074:case 0x4075:
        case 0x4076:case 0x4078:case 0x4079:case 0x407b:case 0x407d:case 0x407e:
        case 0x407f:case 0x4080:case 0x4081:case 0x4085:case 0x4086:case 0x4088:
        case 0x4089:case 0x408a:return true;
        default:return false;
        }
    }
    switch(id){
    case 0x092:case 0x82f:case 0x02e:case 0x031:case 0x032:case 0x033:case 0x034:case 0x035:
    case 0x038:case 0x03b:case 0x03c:case 0x03d:case 0x03e:case 0x03f:
    case 0x040:case 0x041:case 0x042:case 0x043:case 0x044:case 0x045:
    case 0x046:case 0x047:case 0x048:case 0x049:case 0x04a:case 0x04b:
    case 0x04c:case 0x04d:case 0x04e:case 0x04f:case 0x050:case 0x051:
    case 0x053:case 0x054:case 0x055:case 0x058:case 0x059:
    case 0x05b:case 0x05c:case 0x05e:case 0x05f:case 0x062:case 0x06b:
    case 0x071:case 0x072:case 0x073:case 0x074:case 0x075:case 0x076:
    case 0x079:case 0x07a:case 0x07b:case 0x07c:case 0x07d:case 0x07e:
    case 0x080:case 0x083:case 0x084:case 0x088:case 0x089:case 0x08b:
    case 0x08c:case 0x08d:case 0x08e:case 0x090:case 0x091:case 0x097:
    case 0x098:case 0x09d:case 0x0a2:case 0x0ad:
    case 0x233:case 0x23c:case 0x253:case 0x264:case 0x265:case 0x267:
    case 0x268:case 0x269:case 0x26a:case 0x26b:case 0x26c:case 0x26d:
    case 0x291:case 0x29d:case 0x29e:case 0x29f:case 0x2a0:case 0x2a1:
    case 0x2a3:case 0x2a5:case 0x2d2:case 0x2d3:case 0x2d4:case 0x2d5:
    case 0x2d6:case 0x2e3:case 0x82c:case 0x844:case 0x845:case 0x846:
    case 0x849:return true;
    default:return false;
    }
}
inline const std::vector<uint16_t>& storyKeys(uint8_t policy=DefaultRewardSharing){
    static const auto keys=[] {std::array<std::vector<uint16_t>,8> out;for(unsigned p=0;p<8;++p)for(unsigned i=0;i<0x4100;++i)if(sharedStory(uint16_t(i),uint8_t(p)))out[p].push_back(uint16_t(i));return out;}();return keys.at(policy);
}
// A map object's personal hide flag must never become shared visibility.
inline bool personalObjectFlag(uint16_t id,uint8_t policy=DefaultRewardSharing){return id>=0x20&&!sharedStory(id,policy);}
struct StoryValue { uint16_t id=0,value=0;uint32_t revision=0; };
inline bool validStory(const StoryValue& v,uint8_t policy=DefaultRewardSharing){return sharedStory(v.id,policy)&&(v.id>=0x4000||v.value<=1);}
}
