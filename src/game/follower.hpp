#pragma once
#include "game/world.hpp"
#include <algorithm>
#include <cmath>
#include <deque>
namespace fr::game {
inline uint8_t movementFacing(int dx,int dy,uint8_t fallback){
    if(!dx&&!dy)return fallback;
    if(std::abs(dx)>std::abs(dy))return dx>0?4:3;
    return dy>0?1:2;
}
class FollowerPath {
    struct Point {float x,y,distance;};
    std::deque<Point> trail;
    float distance=0;
    int lastX=0,lastY=0,walked=0;
    bool visible=false;
    uint8_t lastFacing=1,lastFrame=0;int still=0;
public:
    void clear(){*this={};}
    void update(PlayerState& p,bool allowed){
        p.followerVisible=false;
        if(!p.active){clear();return;}
        if(trail.empty())trail.push_back({float(p.pixelX),float(p.pixelY),0});
        const auto last=trail.back();const float moved=std::hypot(p.pixelX-last.x,p.pixelY-last.y);
        if(moved>32){clear();trail.push_back({float(p.pixelX),float(p.pixelY),0});return;}
        if(moved>0){distance+=moved;trail.push_back({float(p.pixelX),float(p.pixelY),distance});}
        const float behind=distance-16;
        while(trail.size()>2&&trail[1].distance<=behind)trail.pop_front();
        if(trail.size()<2||behind<trail.front().distance)return;
        const auto& a=trail[0];const auto& b=trail[1];
        const float t=std::clamp((behind-a.distance)/std::max(1.f,b.distance-a.distance),0.f,1.f);
        p.followerX=int16_t(std::lround(a.x+(b.x-a.x)*t));p.followerY=int16_t(std::lround(a.y+(b.y-a.y)*t));
        const int dx=visible?p.followerX-lastX:0,dy=visible?p.followerY-lastY:0;
        // Facing follows the follower's own segment, not the trainer's newest
        // heading. This avoids walking backward during delayed turns/reversals.
        if(dx||dy)still=0;else ++still;
        p.followerFacing=movementFacing(dx,dy,visible&&still<3?lastFacing:p.facing);
        walked+=std::abs(dx)+std::abs(dy);
        p.followerFrame=uint8_t((dx||dy)?(walked/4)%4:still<3?lastFrame:0);
        lastFacing=p.followerFacing;lastFrame=p.followerFrame;
        lastX=p.followerX;lastY=p.followerY;visible=true;p.followerVisible=allowed;
    }
};
}
