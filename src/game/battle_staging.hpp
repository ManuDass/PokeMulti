#pragma once
#include "game/battle_presence.hpp"
#include <deque>
#include <map>
namespace fr::game {
// Cosmetic walking only: the original game keeps its trainer/party/save state.
// Search a small joint state space so the trainer and partner cannot swap
// through each other, pass through the opponent, or cut through terrain.
class BattleStaging {
    struct Step {unsigned actor;int x,y;};
    std::deque<Step> steps;
    std::vector<Step> history;
    unsigned walked=0;
    bool returning=false;
    static int tile(int pixel){return (pixel+8)/16;}
    static BattleMon* partner(BattlePresence& b){for(auto& m:b.mons)if(m.position==0)return &m;return nullptr;}
    static BattleMon* enemy(BattlePresence& b){for(auto& m:b.mons)if(m.position==1)return &m;return nullptr;}
    static void face(BattlePresence& b){
        for(auto& m:b.mons){const auto other=std::find_if(b.mons.begin(),b.mons.end(),[&](auto n){return n.position==(m.position^1);});if(other!=b.mons.end()&&!m.walking)m.facing=movementFacing(other->x-m.x,other->y-m.y,m.facing);}
        if(auto* m=partner(b))standFacing(b.trainer,movementFacing(m->x-b.trainer.pixelX,m->y-b.trainer.pixelY,b.trainer.facing));
    }
public:
    void begin(BattlePresence& b,const std::function<bool(int,int)>& passable){
        *this={};b.settled=false;b.returning=false;
        auto* a=partner(b);auto* e=enemy(b);if(!a||!e){b.settled=true;return;}
        using State=std::array<int,4>;const State start{tile(b.trainer.pixelX),tile(b.trainer.pixelY),tile(a->x),tile(a->y)};
        const int ex=tile(e->x),ey=tile(e->y);
        auto goal=[&](State s){int dx=ex-s[2],dy=ey-s[3];const int d=std::abs(dx)+std::abs(dy);return d>=1&&d<=2&&(!dx||!dy)&&(s[0]!=ex||s[1]!=ey)&&(s[0]!=s[2]||s[1]!=s[3])&&(s[0]-s[2])*dx+(s[1]-s[3])*dy<=0;};
        // Already facing across clear ground: retain even sub-tile positions.
        if(goal(start)){b.settled=true;face(b);return;}
        struct Parent {State state;unsigned actor;};std::map<State,Parent> parent;std::deque<State> queue{start};parent[start]={start,0};State finish=start;bool found=false;
        constexpr int dx[]{0,0,-1,1},dy[]{1,-1,0,0};
        while(!queue.empty()&&parent.size()<10000){auto state=queue.front();queue.pop_front();if(goal(state)){finish=state;found=true;break;}
            for(unsigned actor:{0u,1u})for(unsigned d=0;d<4;++d){auto next=state;const auto i=actor*2,j=(1-actor)*2;next[i]+=dx[d];next[i+1]+=dy[d];
                const int x=next[i],y=next[i+1];if(std::abs(x-start[0])>3||std::abs(y-start[1])>3||!passable(x,y)||(x==ex&&y==ey)||(x==next[j]&&y==next[j+1])||parent.contains(next))continue;
                parent[next]={state,actor};queue.push_back(next);
            }
        }
        if(found)for(auto s=finish;s!=start;){const auto p=parent.at(s);steps.push_front({p.actor,s[p.actor*2]*16,s[p.actor*2+1]*16});s=p.state;}
        // A cramped corridor is preferable to teleporting onto blocked ground.
        b.settled=steps.empty();if(b.settled)face(b);
    }
    void returnHome(BattlePresence& b){returning=true;b.returning=true;b.settled=false;for(auto& m:b.mons)if(m.position&1)m.visible=false;steps.clear();for(auto i=history.rbegin();i!=history.rend();++i)steps.push_back(*i);history.clear();}
    bool update(BattlePresence& b){
        for(auto& m:b.mons)m.walking=false;
        if(steps.empty()){b.settled=!returning;face(b);return true;}
        auto* a=partner(b);if(!a){steps.clear();return true;}auto step=steps.front();auto& x=step.actor?a->x:b.trainer.pixelX;auto& y=step.actor?a->y:b.trainer.pixelY;
        const int oldX=x,oldY=y;
        if(x!=step.x&&(y==step.y||std::abs(step.x-x)>std::abs(step.y-y)))x+=x<step.x?1:-1;else if(y!=step.y)y+=y<step.y?1:-1;
        if(!returning&&(x!=oldX||y!=oldY))history.push_back({step.actor,oldX,oldY});
        const auto facing=movementFacing(x-oldX,y-oldY,step.actor?a->facing:b.trainer.facing);++walked;
        face(b);
        if(step.actor){a->walking=true;a->facing=facing;a->frame=uint8_t((walked/4)%4);}
        else{auto& p=b.trainer;standFacing(p,facing);p.spriteFrame=uint8_t(3+(facing==2?1:facing>=3?2:0)*2+(walked/8)%2);p.x=int16_t(tile(p.pixelX));p.y=int16_t(tile(p.pixelY));}
        if(x==step.x&&y==step.y)steps.pop_front();return false;
    }
};
}
