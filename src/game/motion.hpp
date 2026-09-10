#pragma once
#include "game/world.hpp"
#include "game/follower.hpp"
#include <algorithm>
#include <cmath>
#include <deque>
namespace fr::game {
inline bool sameScene(const PlayerState& a,const PlayerState& b){return a.active&&b.active&&a.mapGroup==b.mapGroup&&a.mapNumber==b.mapNumber&&a.elevation==b.elevation;}
// Render a short history, never extrapolate past a stop or cut across a warp.
// Sender timestamps remove packet-arrival jitter; position, pose and follower
// share one timeline. All coordinates remain exact integer GBA pixels.
class MotionTimeline {
    struct Sample {double time;PlayerState state;};
    std::deque<Sample> samples;
    uint32_t sequence=0,lastStamp=0;uint64_t identity=0;
    bool initialized=false;double senderTime=0,offset=0,lastArrival=0;
public:
    static constexpr double delayMs=80;
    void clear(){*this={};}
    bool push(const PlayerState& state,double arrival){
        if(initialized&&state.identity==identity&&int32_t(state.sequence-sequence)<=0)return false;
        const bool reset=!initialized||state.identity!=identity;
        const int32_t elapsed=reset?0:int32_t(state.sampleTime-lastStamp);
        if(reset||elapsed<0||elapsed>2000){senderTime=0;offset=arrival;samples.clear();}
        else {
            senderTime+=elapsed;
            // Slowly follow improvements in the minimum observed transport delay.
            // A delayed packet never pushes the playback clock backwards.
            offset-=std::clamp(offset-(arrival-senderTime),0.0,0.25);
        }
        const bool discontinuity=samples.empty()||!sameScene(samples.back().state,state)||
            std::abs(int(state.pixelX)-samples.back().state.pixelX)+std::abs(int(state.pixelY)-samples.back().state.pixelY)>64;
        if(discontinuity)samples.clear();
        samples.push_back({senderTime,state});while(samples.size()>64)samples.pop_front();
        identity=state.identity;sequence=state.sequence;lastStamp=state.sampleTime;lastArrival=arrival;initialized=true;return true;
    }
    PlayerState sample(double time) const{
        if(samples.empty())return {};
        // Stale clients disappear instead of lingering indefinitely during a stall.
        if(time-lastArrival>2000)return {};
        const double target=time-offset-delayMs;
        if(target<=samples.front().time)return samples.front().state;
        for(size_t i=1;i<samples.size();++i){const auto& a=samples[i-1];const auto& b=samples[i];
            if(target>=b.time)continue;
            auto out=a.state;
            const double t=std::clamp((target-a.time)/std::max(1.0,b.time-a.time),0.0,1.0);
            auto mix=[&](int x,int y){return int16_t(std::lround(x+(y-x)*t));};
            out.pixelX=mix(a.state.pixelX,b.state.pixelX);out.pixelY=mix(a.state.pixelY,b.state.pixelY);
            out.offsetX=int8_t(mix(a.state.offsetX,b.state.offsetX));out.offsetY=int8_t(mix(a.state.offsetY,b.state.offsetY));
            if(a.state.followerVisible&&b.state.followerVisible){out.followerX=mix(a.state.followerX,b.state.followerX);out.followerY=mix(a.state.followerY,b.state.followerY);out.followerOffsetY=int8_t(mix(a.state.followerOffsetY,b.state.followerOffsetY));
                out.followerFacing=movementFacing(b.state.followerX-a.state.followerX,b.state.followerY-a.state.followerY,a.state.followerFacing);}
            return out;
        }
        return samples.back().state;
    }
};
}
