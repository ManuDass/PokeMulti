#include "game/camp.hpp"
#include <algorithm>
#include <cmath>
#include <deque>
#include <set>
namespace fr::game {
bool campContains(const CampState& c,int x,int y){return c.id&&x>=c.x&&x<c.x+CampWidth&&y>=c.y&&y<c.y+CampHeight;}
bool campsOverlap(const CampState& a,const CampState& b){return a.id&&b.id&&a.group==b.group&&a.map==b.map&&a.x<b.x+CampWidth&&b.x<a.x+CampWidth&&a.y<b.y+CampHeight&&b.y<a.y+CampHeight;}
bool campWalkable(const CampState& c,int x,int y){
    const int col=x-(c.x+1-CampRadius),row=y-(c.y+1-CampRadius);
    return c.id&&col>=0&&row>=0&&col<CampSpan&&row<CampSpan&&(c.ground[row]&(1u<<col));
}
bool campHasEntrance(const CampState& c){
    // The tent needs a usable front entrance, not a mandatory lane behind it.
    // The connected roaming mask can follow a clearing beside a ledge or trees.
    return campWalkable(c,c.x+1,c.y+CampHeight)&&campWalkable(c,c.x+2,c.y+CampHeight);
}

void setCampGround(CampState& c,const std::vector<CampCell>& ground){
    c.ground={};for(auto p:ground){const int col=p.x-(c.x+1-CampRadius),row=p.y-(c.y+1-CampRadius);if(col>=0&&row>=0&&col<CampSpan&&row<CampSpan)c.ground[row]|=uint16_t(1u<<col);}
}
std::vector<CampEdge> campPerimeter(const CampState& c){
    // Flood only the outside. Shared edges, enclosed holes and the tent interior
    // produce no lines; the tent footprint joins the enclosing ground boundary.
    const int left=c.x-CampRadius,top=c.y-CampRadius;
    auto inside=[&](int x,int y){return campWalkable(c,x,y)||campContains(c,x,y);};
    std::set<CampCell> outside{{left,top}};std::deque<CampCell> queue{{left,top}};
    constexpr int dx[]{0,0,-1,1},dy[]{1,-1,0,0};
    while(!queue.empty()){auto p=queue.front();queue.pop_front();for(int d=0;d<4;++d){CampCell q{p.x+dx[d],p.y+dy[d]};if(q.x<left||q.y<top||q.x>left+CampSpan+1||q.y>top+CampSpan+1||inside(q.x,q.y)||outside.contains(q))continue;outside.insert(q);queue.push_back(q);}}
    std::vector<CampEdge> edges;
    for(int y=top+1;y<=top+CampSpan;++y)for(int x=left+1;x<=left+CampSpan;++x)if(inside(x,y))for(unsigned d=0;d<4;++d)if(outside.contains({x+dx[d],y+dy[d]}))edges.push_back({{x,y},uint8_t(d)});
    return edges;
}
bool validCamp(const CampState& c){
    if(!c.id)return c.party.empty();
    if(c.x<0||c.y<0||c.x>508||c.y>509||c.elevation>15||c.party.empty()||c.party.size()>6)return false;
    unsigned cells=0;
    for(int row=0;row<CampSpan;++row){if(c.ground[row]>>CampSpan)return false;for(int col=0;col<CampSpan;++col)if(c.ground[row]&(1u<<col)){
        const int x=c.x+1-CampRadius+col,y=c.y+1-CampRadius+row,dx=x*16-(c.x*16+24),dy=y*16-(c.y*16+16);
        if(x<0||y<0||x>511||y>511||dx*dx+dy*dy>CampRadius*CampRadius*256||campContains(c,x,y))return false;++cells;
    }}
    if(!campHasEntrance(c)||cells<c.party.size()+2||campGround(c,[&](int x,int y){return campWalkable(c,x,y);}).size()!=cells)return false;
    for(const auto& p:c.party){const int dx=p.x-(c.x*16+24),dy=p.y-(c.y*16+16);
        if(!p.species||p.species>411||p.x<0||p.y<0||p.x>8176||p.y>8176||dx*dx+dy*dy>CampRadius*CampRadius*256||!campWalkable(c,p.x/16,p.y/16)||p.facing<1||p.facing>4||p.frame>3||p.mood>2)return false;
    }return true;
}
bool campGrass(uint8_t mapType,bool cave,bool general,uint16_t tile,uint16_t behavior,uint8_t elevation){
    // General lawn: full mowed tile and the four matching grass texture variants.
    // Verified against the supported ROM tiles; Plain_Grass (0x00d) is encounter grass.
    // MB_NORMAL alone would also admit paths, floors, and many special tiles.
    return (mapType==1||mapType==2||mapType==3)&&!cave&&general&&((tile&1023)==1||(tile&1023)==8||(tile&1023)==9||(tile&1023)==0x10||(tile&1023)==0x11)&&!(tile&0xc00)&&(tile>>12)==elevation&&behavior==0;
}
bool campFootprint(const CampState& c,const CampTileCheck& free){for(int y=c.y;y<c.y+CampHeight;++y)for(int x=c.x;x<c.x+CampWidth;++x)if(!free(x,y))return false;return true;}
std::vector<CampCell> campGround(const CampState& c,const CampTileCheck& free,std::optional<CampCell> anchor){
    auto eligible=[&](int x,int y){int dx=x*16-(c.x*16+24),dy=y*16-(c.y*16+16);return dx*dx+dy*dy<=CampRadius*CampRadius*256&&!campContains(c,x,y)&&free(x,y);};
    // Choose the largest connected lawn beside the tent, never across a wall.
    std::set<CampCell> seen;std::vector<CampCell> out,seeds;
    for(int x=c.x;x<c.x+CampWidth;++x){seeds.push_back({x,c.y+CampHeight});seeds.push_back({x,c.y-1});}
    for(int y=c.y;y<c.y+CampHeight;++y){seeds.push_back({c.x-1,y});seeds.push_back({c.x+CampWidth,y});}
    constexpr int dx[]{0,0,-1,1},dy[]{1,-1,0,0};
    for(const auto seed:seeds){if(seen.contains(seed)||!eligible(seed.x,seed.y))continue;std::deque<CampCell> queue{seed};std::vector<CampCell> component;seen.insert(seed);
        while(!queue.empty()){const auto p=queue.front();queue.pop_front();component.push_back(p);for(int d=0;d<4;++d){CampCell n{p.x+dx[d],p.y+dy[d]};if(!seen.contains(n)&&eligible(n.x,n.y)){seen.insert(n);queue.push_back(n);}}}
        if(anchor){if(std::find(component.begin(),component.end(),*anchor)!=component.end())return component;}
        else if(component.size()>out.size())out=std::move(component);
    }
    return out;
}
uint32_t CampSimulation::random(){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
bool CampSimulation::start(CampState camp,const std::vector<uint16_t>& species,const std::vector<CampCell>& ground){
    if(species.empty()||species.size()>6||ground.size()<species.size()+2)return false;
    setCampGround(camp,ground);camp.party.clear();camp.tick=0;seed=camp.id?camp.id:1;
    for(size_t i=0;i<species.size();++i){const auto p=ground[i];camp.party.push_back({species[i],int16_t(p.x*16),int16_t(p.y*16),1});steps[i]={p.x,p.y,p.x,p.y,0,0,uint32_t(15+i*12)};}
    if(!validCamp(camp))return false;state=std::move(camp);return true;
}
void CampSimulation::update(const CampTileCheck& free){
    if(!state.id)return;++state.tick;const auto tick=state.tick;
    auto available=[&](int x,int y,size_t self){const int dx=x*16-(state.x*16+24),dy=y*16-(state.y*16+16);if(dx*dx+dy*dy>CampRadius*CampRadius*256||!campWalkable(state,x,y)||!free(x,y))return false;for(size_t j=0;j<state.party.size();++j)if(j!=self&&((steps[j].x==x&&steps[j].y==y)||(steps[j].tx==x&&steps[j].ty==y)))return false;return true;};
    auto facing=[](int dx,int dy){return uint8_t(dx<0?3:dx>0?4:dy<0?2:1);};
    for(size_t i=0;i<state.party.size();++i){auto& s=steps[i];auto& p=state.party[i];
        if(tick<s.end){const double t=double(tick-s.begin)/double(s.end-s.begin);p.x=int16_t(std::lround((s.x+(s.tx-s.x)*t)*16));p.y=int16_t(std::lround((s.y+(s.ty-s.y)*t)*16));p.frame=uint8_t((tick/8)%4);continue;}
        s.x=s.tx;s.y=s.ty;p.x=int16_t(s.x*16);p.y=int16_t(s.y*16);p.frame=0;
        if(tick<s.pause)continue;p.mood=0;
        bool playing=false;
        for(size_t j=i+1;j<state.party.size();++j){auto& other=steps[j];if(tick<other.end||state.party[j].mood||std::abs(s.x-other.x)+std::abs(s.y-other.y)!=1||random()%3)continue;
            p.facing=facing(other.x-s.x,other.y-s.y);state.party[j].facing=facing(s.x-other.x,s.y-other.y);p.mood=state.party[j].mood=uint8_t(1+random()%2);s.pause=other.pause=tick+50;playing=true;break;}
        if(playing)continue;
        constexpr int dx[]{0,0,-1,1},dy[]{1,-1,0,0};const unsigned first=random()%4;bool moved=false;
        for(unsigned n=0;n<4;++n){const unsigned d=(first+n)%4;const int x=s.x+dx[d],y=s.y+dy[d];if(!available(x,y,i))continue;s.tx=x;s.ty=y;s.begin=tick;s.end=tick+32;s.pause=s.end+12+random()%55;p.facing=facing(dx[d],dy[d]);moved=true;break;}
        if(!moved)s.pause=tick+30;
    }
}
}
