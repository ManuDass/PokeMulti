#include "game/motion.hpp"
#include <iostream>
#include <stdexcept>
using namespace fr::game;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
PlayerState state(unsigned seq,unsigned stamp,int x,int y=0,unsigned face=4){PlayerState p;p.active=true;p.identity=1;p.sequence=seq;p.sampleTime=stamp;p.pixelX=int16_t(x);p.pixelY=int16_t(y);p.facing=uint8_t(face);p.spriteFrame=face==2?1:2;p.flip=face==4?1:0;p.followerVisible=true;p.followerX=int16_t(x-16);p.followerY=int16_t(y);return p;}
int main(){try{
    MotionTimeline m;
    // Uneven network arrival times must not change walking speed or destination.
    m.push(state(1,1000,32),5000);m.push(state(2,1020,34),5028);m.push(state(3,1040,36),5041);m.push(state(4,1060,38),5071);
    for(int i=0;i<=60;i+=5){const auto p=m.sample(5080+i);check(p.pixelX==int(std::lround(32+i/10.0)),"arrival jitter changed pixel velocity");check(p.followerX==p.pixelX-16,"follower left the shared timeline");}
    check(!m.push(state(3,1040,100),5080),"accepted stale position");check(!m.push(state(4,1060,100),5080),"accepted duplicate position");
    check(m.sample(5300).pixelX==38,"extrapolated through a stop");
    // Turns use the pose at playback time, not the newest facing packet.
    m.push(state(5,1080,38,0,2),5080);m.push(state(6,1100,38,-2,2),5100);
    check(m.sample(5140).facing==4&&m.sample(5140).flip==1,"future facing applied before turn");
    check(m.sample(5160).facing==2&&m.sample(5160).spriteFrame==1,"turn pose did not arrive with position");
    check(m.sample(5170).pixelX==38&&m.sample(5170).pixelY==-1,"corner cut during turn");
    auto warp=state(7,1120,240,240);warp.mapNumber=3;m.push(warp,5120);check(m.sample(5121).pixelX==240&&m.sample(5121).mapNumber==3,"interpolated across map warp");
    auto hidden=state(8,1140,240,240);hidden.active=false;m.push(hidden,5140);check(!m.sample(5140).active,"inactive player remained visible");
    auto replaced=state(1,0,80);replaced.identity=2;m.push(replaced,5160);check(m.sample(5160).pixelX==80,"slot reuse kept old player's sequence");
    check(!m.sample(8000).active,"stalled peer never expired");
    MotionTimeline wrap;wrap.push(state(0xfffffffe,0xfffffff0,32),100);wrap.push(state(0xffffffff,0,34),116);wrap.push(state(0,16,36),132);check(wrap.sample(204).pixelX==35,"timestamp/sequence wrap broke motion");
    std::cout<<"Motion: jitter, stops, turns, follower timing, stale packets, warps, identity and clock wrap passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
