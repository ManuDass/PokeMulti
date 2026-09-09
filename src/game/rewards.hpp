#pragma once
#include <cstdint>
#include "game/reward_policy.hpp"
namespace fr::game {
struct RewardRule {uint16_t progress,minimum,claimed,item;const char* name;bool essential=false;};
// Personal delivery when a shared story encounter has moved its giver away.
// Normal NPC scripts read the same private claim flag, preventing a second copy.
inline constexpr RewardRule storyRewardRules[]{
 {0x820,1,0x254,327,"TM39 / Rock Tomb"},{0x821,1,0x297,291,"TM03 / Water Pulse"},
 {0x822,1,0x231,322,"TM34 / Shock Wave"},{0x823,1,0x293,307,"TM19 / Giga Drain"},
 {0x824,1,0x259,294,"TM06 / Toxic"},{0x825,1,0x29a,292,"TM04 / Calm Mind"},
 {0x826,1,0x24e,326,"TM38 / Fire Blast"},{0x827,1,0x298,314,"TM26 / Earthquake"},
 {0x4052,1,0x29b,363,"Fame Checker"},{0x407d,1,0x23f,316,"TM28 / Dig"},
 {0x233,1,0x234,265,"S.S. Ticket",true},{0x23c,1,0x23d,350,"Poke Flute",true},
 {0x053,1,0x250,1,"Master Ball"},
 {0x237,1,0x237,339,"HM01 / Cut",true},{0x238,1,0x238,340,"HM02 / Fly",true},
 {0x239,1,0x239,341,"HM03 / Surf",true},{0x23a,1,0x23a,342,"HM04 / Strength",true},
 {0x23b,1,0x23b,343,"HM05 / Flash",true},{0x2ef,1,0x2ef,344,"HM06 / Rock Smash",true},
 {0x036,1,0x036,356,"Lift Key",true},{0x037,1,0x037,359,"Silph Scope",true},
 {0x189,1,0x189,353,"Gold Teeth",true},{0x192,1,0x192,355,"Card Key",true},
 {0x2a6,1,0x2a6,369,"Tea",true}
};
inline bool shareReward(const RewardRule& rule,uint8_t policy){return rule.essential||(policy&(rule.item>=289&&rule.item<=338?ShareTMs:ShareStoryItems))!=0;}
}
