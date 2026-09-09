#include "input/pokeball.hpp"
#include <algorithm>
#include <cmath>
#include <chrono>
namespace fr::input {
uint64_t ballTime(){return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
std::optional<BallSample> decodeBall(std::span<const uint8_t> b){
    if(b.size()<5||b.size()>512)return {};
    // Protocol facts independently decoded from the published PBP input capture.
    // X occupies the low nibble of byte 3 and high nibble of byte 2; Y is byte 4.
    const auto axis=[](float v,float lo,float hi){return std::clamp((v-lo)*2.f/(hi-lo)-1.f,-1.f,1.f);};
    return BallSample{axis(float(((b[3]&15)<<4)|(b[2]>>4)),32,192),axis(float(b[4]),36,180),bool(b[1]&1),bool(b[1]&2)};
}
void BallControls::reset(){horizontal_=vertical_=0;armed_=chord_=false;topAt_=stickAt_=0;topWas_=stickWas_=false;}
uint16_t BallControls::keys(const BallSample& raw,uint64_t now,bool usable,const BallMapping& m){
    if(!usable||!std::isfinite(raw.x)||!std::isfinite(raw.y)){reset();return 0x3ff;}
    BallSample s=raw;if(m.invertY)s.y=-s.y;
    const float enter=float(std::clamp(m.deadzone,10,60))/100.f,leave=enter*.7f;
    // Releasing the stick/buttons after reconnect or returning from chat avoids
    // a held control triggering a choice in the newly focused game. Any position
    // inside the movement dead zone is neutral; hysteresis only releases motion.
    if(!armed_){if(std::abs(s.x)<enter&&std::abs(s.y)<enter&&!s.top&&!s.stick)armed_=true;return 0x3ff;}
    auto direction=[&](float v,int old){if(v>=enter)return 1;if(v<=-enter)return -1;if(old>0&&v>leave)return 1;if(old<0&&v<-leave)return -1;return 0;};
    horizontal_=direction(s.x,horizontal_);vertical_=direction(s.y,vertical_);
    uint16_t pressed=0;
    if(horizontal_)pressed|=uint16_t(1<<(horizontal_>0?4:5));
    if(vertical_)pressed|=uint16_t(1<<(vertical_>0?7:6));
    if(s.top&&!topWas_)topAt_=now;
    if(s.stick&&!stickWas_)stickAt_=now;
    // A short chord window prevents A/B leaking before the Start combination.
    // Short taps still emit one poll of their button on release.
    if(s.top&&s.stick&&!chord_){chord_=true;pressed|=8;}
    if(!chord_){
        if((s.top&&now-topAt_>=90)||(!s.top&&topWas_&&now-topAt_<90))pressed|=m.swapButtons?1:2;
        if((s.stick&&now-stickAt_>=90)||(!s.stick&&stickWas_&&now-stickAt_<90))pressed|=m.swapButtons?2:1;
    }
    if(!s.top&&!s.stick)chord_=false;
    topWas_=s.top;stickWas_=s.stick;
    return uint16_t(0x3ff&~pressed);
}
}
