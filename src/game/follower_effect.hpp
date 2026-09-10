#pragma once
#include "game/world.hpp"
#include <array>
namespace fr::game {
// Supplied 192 px cells, five columns: top 0..3 send out; bottom 5..7 recall.
// Each tick selects one cell only; blank cells never extend either sequence.
// Playback is 20 animation frames/s.
inline int followerBallPattern(bool recall,unsigned frame){
    constexpr std::array<int,10> in{-1,5,6,6,7,7,7,7,7,7},out{-1,0,0,0,1,1,2,2,3,3};
    return frame<10?(recall?in[frame]:out[frame]):-1;
}
inline int followerEmotePattern(unsigned kind,unsigned frame){
    constexpr int patterns[3][2]{{9,8},{1,2},{5,0}};
    return kind>=1&&kind<=3&&frame<24?patterns[kind-1][(frame/4)%2]:-1;
}
class FollowerVisual {
public:
    enum Phase {Hidden,Arriving,Present,Leaving};
    Phase phase=Hidden;PlayerState pose;uint64_t began=0;
    void update(const PlayerState& p,bool wanted,uint64_t tick){
        if(!p.active)return;
        const bool same=pose.follower==p.follower&&pose.followerShiny==p.followerShiny&&pose.followerToken==p.followerToken;
        if(pose.active&&(pose.mapGroup!=p.mapGroup||pose.mapNumber!=p.mapNumber||pose.elevation!=p.elevation)){
            // A warp is hidden by the native transition. Re-anchor without an
            // artificial recall/send-out for the same Pokemon.
            if(same&&wanted&&phase!=Hidden){pose=p;}else phase=Hidden;
        }
        if(phase==Leaving){if(tick<began+30)return;phase=Hidden;}
        if(phase==Hidden){if(wanted){pose=p;phase=Arriving;began=tick;}return;}
        if(!wanted||!same){phase=Leaving;began=tick;return;}
        pose=p;if(phase==Arriving&&tick>=began+30)phase=Present;
    }
    unsigned frame(uint64_t tick)const{return unsigned((tick-began)/3);}
    bool sprite(uint64_t tick)const{return phase==Present||(phase==Arriving&&frame(tick)>=6)||(phase==Leaving&&frame(tick)<2);}
};
}
