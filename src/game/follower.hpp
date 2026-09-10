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
    struct Point {float x,y,distance;int8_t lift=0;};
    std::deque<Point> trail;
    float distance=0,followDistance=0,speed=1;
    int8_t lastLift=0;
    int lastX=0,lastY=0,walked=0;
    bool visible=false;
    uint8_t lastFacing=1,lastFrame=0;int still=0;
public:
    void clear(){*this={};}
    bool empty()const{return trail.empty();}
    void seed(const PlayerState& p,int x,int y){
        clear();const float gap=std::hypot(float(p.pixelX-x),float(p.pixelY-y));
        // Arrival can be partway through a native door step. Keep its true
        // distance and never invent a diagonal segment from a nearby tile.
        if(gap>32||(x!=p.pixelX&&y!=p.pixelY))return;
        distance=gap;trail.push_back({float(x),float(y),0});trail.push_back({float(p.pixelX),float(p.pixelY),distance});
        lastX=x;lastY=y;lastFacing=p.facing;
    }
    void translate(int dx,int dy){for(auto& p:trail){p.x+=dx;p.y+=dy;}lastX+=dx;lastY+=dy;}

    void update(PlayerState& p,bool allowed){
        p.followerVisible=false;p.followerOffsetY=0;
        if(!p.active)return; // Field fades do not discard the walking history.
        if(trail.empty())trail.push_back({float(p.pixelX),float(p.pixelY),0});
        const auto last=trail.back();const float moved=std::hypot(p.pixelX-last.x,p.pixelY-last.y);
        if(moved>32){clear();trail.push_back({float(p.pixelX),float(p.pixelY),0});return;}
        if(moved>0){distance+=moved;speed=std::clamp(moved,1.f,4.f);trail.push_back({float(p.pixelX),float(p.pixelY),distance,int8_t(std::min(0,int(p.offsetY)))});}
        else trail.back().lift=int8_t(std::min(0,int(p.offsetY)));
        float behind=std::max(distance-16,followDistance);
        // Once airborne, finish the recorded arc even if the trainer stops on
        // landing. Never rewind to restore the gap; walking opens it naturally.
        if(lastLift<0)behind=std::max(behind,std::min(distance,followDistance+speed));
        while(trail.size()>2&&trail[1].distance<=behind)trail.pop_front();
        if(trail.size()<2||behind<trail.front().distance)return;
        const auto& a=trail[0];const auto& b=trail[1];
        const float t=std::clamp((behind-a.distance)/std::max(1.f,b.distance-a.distance),0.f,1.f);
        p.followerX=int16_t(std::lround(a.x+(b.x-a.x)*t));p.followerY=int16_t(std::lround(a.y+(b.y-a.y)*t));
        p.followerOffsetY=int8_t(std::lround(a.lift+(b.lift-a.lift)*t));followDistance=behind;lastLift=p.followerOffsetY;
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
