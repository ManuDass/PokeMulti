#include "game/released.hpp"
#include "platform/atomic_file.hpp"
#include <algorithm>
#include <cmath>
#include <deque>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <limits>
#include <type_traits>
namespace fr::game {
namespace {
bool identity(const std::string& s){return s.size()==32&&std::all_of(s.begin(),s.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');});}
bool point(const ReleasePoint& p){return p.group<43&&p.map<128&&p.elevation<16&&p.x>=0&&p.y>=0&&p.x<512&&p.y<512;}
}
bool sameReleasedIndividual(const ReleasedPokemon& a,const ReleasedPokemon& b){return a.personality==b.personality&&a.species==b.species&&a.ivs==b.ivs;}
bool validReleasedPokemon(const ReleasedPokemon& p){return p.species>0&&p.species<=411&&p.level>0&&p.level<=100&&p.item<=374&&p.experience<=1640000&&!(p.ivs&0x40000000)&&p.language>0&&p.language<=7&&p.markings<=15&&std::all_of(p.moves.begin(),p.moves.end(),[](auto m){return m<=354;})&&std::all_of(p.pp.begin(),p.pp.end(),[](auto pp){return pp<=64;});}
bool validReleasedMon(const ReleasedMon& m){return identity(m.id)&&identity(m.owner)&&(m.claimant.empty()||identity(m.claimant))&&validReleasedPokemon(m.pokemon)&&point(m.origin)&&point(m.position)&&point(m.entrance)&&unsigned(m.phase)<=8&&m.pixelX>=0&&m.pixelY>=0&&m.pixelX<=8176&&m.pixelY<=8176&&m.facing>=1&&m.facing<=4&&m.frame<4&&m.tick<=3600000;}
std::string encodeReleased(const ReleasedMon& m){
    if(!validReleasedMon(m))throw std::runtime_error("Invalid released Pokemon");
    std::ostringstream o;auto n=[&](auto v){o<<' '<<uint32_t(v);};
    o<<m.id<<' '<<m.owner<<' '<<(m.claimant.empty()?"-":m.claimant);const auto& p=m.pokemon;
    n(p.personality);n(p.trainerId);n(p.experience);n(p.ivs);n(p.ribbons);n(p.species);n(p.item);n(p.origins);
    for(auto v:p.moves)n(v);for(auto v:p.pp)n(v);for(auto v:p.ev)n(v);for(auto v:p.condition)n(v);for(auto v:p.nickname)n(v);for(auto v:p.trainerName)n(v);
    n(p.level);n(p.language);n(p.markings);n(p.ppBonuses);n(p.friendship);n(p.virus);n(p.metLocation);
    for(const auto& v:{m.origin,m.position,m.entrance}){n(v.group);n(v.map);n(v.elevation);n(v.x);n(v.y);}
    n(m.phase);n(m.revision);n(m.tick);n(m.pixelX);n(m.pixelY);n(m.facing);n(m.frame);n(m.ownerOutside);return o.str();
}
ReleasedMon decodeReleased(const std::string& s){
    if(s.size()>1400)throw std::runtime_error("Released Pokemon record too large");
    ReleasedMon m;std::istringstream in(s);in>>m.id>>m.owner>>m.claimant;if(m.claimant=="-")m.claimant.clear();
    auto n=[&](auto& v){uint64_t value=0;in>>value;if(!in||value>uint64_t(std::numeric_limits<std::remove_reference_t<decltype(v)>>::max()))throw std::runtime_error("Invalid release field");v=std::remove_reference_t<decltype(v)>(value);};
    auto& p=m.pokemon;n(p.personality);n(p.trainerId);n(p.experience);n(p.ivs);n(p.ribbons);n(p.species);n(p.item);n(p.origins);
    for(auto& v:p.moves)n(v);for(auto& v:p.pp)n(v);for(auto& v:p.ev)n(v);for(auto& v:p.condition)n(v);for(auto& v:p.nickname)n(v);for(auto& v:p.trainerName)n(v);
    n(p.level);n(p.language);n(p.markings);n(p.ppBonuses);n(p.friendship);n(p.virus);n(p.metLocation);
    for(auto* v:{&m.origin,&m.position,&m.entrance}){n(v->group);n(v->map);n(v->elevation);n(v->x);n(v->y);}
    uint8_t phase=0,outside=0;n(phase);m.phase=ReleasePhase(phase);n(m.revision);n(m.tick);n(m.pixelX);n(m.pixelY);n(m.facing);n(m.frame);n(outside);m.ownerOutside=outside!=0;
    std::string trailing;if((in>>trailing)||outside>1||!validReleasedMon(m))throw std::runtime_error("Invalid release record");return m;
}
void ReleaseBook::persist(bool force)const{if(path.empty()||(manual&&!force))return;std::ostringstream out;out<<"PMRELEASE1\n";for(const auto& m:entries)out<<encodeReleased(m)<<'\n';fr::replaceText(path,out.str());}
void ReleaseBook::open(const std::filesystem::path& file){path=file;entries.clear();if(file.empty()||!std::filesystem::exists(file))return;std::ifstream in(file);std::string line;std::getline(in,line);if(line!="PMRELEASE1")throw std::runtime_error("Invalid released Pokemon history");while(std::getline(in,line)){if(line.empty())continue;auto m=decodeReleased(line);if(entries.size()>=4096||find(m.id))throw std::runtime_error("Invalid release history size/duplicate");entries.push_back(std::move(m));}}
const ReleasedMon* ReleaseBook::find(const std::string& id)const{for(const auto& m:entries)if(m.id==id)return &m;return nullptr;}
bool ReleaseBook::offer(const ReleasedMon& m){if(!validReleasedMon(m)||m.phase!=ReleasePhase::Queued||!m.claimant.empty())return false;if(const auto* old=find(m.id))return old->owner==m.owner&&old->pokemon==m.pokemon;if(entries.size()>=4096||std::count_if(entries.begin(),entries.end(),[](const auto& a){return a.phase!=ReleasePhase::Caught;})>=256)return false;auto next=m;next.revision=1;entries.push_back(next);try{persist();}catch(...){entries.pop_back();throw;}return true;}
bool ReleaseBook::advance(const ReleasedMon& m,bool durable){if(!validReleasedMon(m))return false;for(auto& old:entries)if(old.id==m.id){if(old.revision!=m.revision||old.phase==ReleasePhase::Battle||old.phase==ReleasePhase::Caught||m.phase==ReleasePhase::Battle||m.phase==ReleasePhase::Caught||m.owner!=old.owner||m.pokemon!=old.pokemon||m.origin!=old.origin||!m.claimant.empty())return false;const auto backup=old;old=m;++old.revision;try{if(durable)persist();}catch(...){old=backup;throw;}return true;}return false;}
bool ReleaseBook::claim(const std::string& id,const std::string& player,const ReleasePoint& where){if(!identity(player))return false;for(auto& m:entries)if(m.id==id){if(m.phase==ReleasePhase::Battle&&m.claimant==player)return true;if(m.phase!=ReleasePhase::Hidden||m.tick<180||m.position!=where)return false;auto backup=m;m.phase=ReleasePhase::Battle;m.claimant=player;++m.revision;try{persist();}catch(...){m=backup;throw;}return true;}return false;}
bool ReleaseBook::finish(const std::string& id,const std::string& player,bool caught){for(auto& m:entries)if(m.id==id){if(m.phase==ReleasePhase::Caught&&m.claimant==player)return caught;if(m.phase!=ReleasePhase::Battle||m.claimant!=player)return false;auto backup=m;m.phase=caught?ReleasePhase::Caught:ReleasePhase::Hidden;m.tick=0;m.frame=0;if(!caught)m.claimant.clear();++m.revision;try{persist();}catch(...){m=backup;throw;}return true;}return false;}
const ReleaseCell* ReleaseMap::cell(int x,int y)const{return x>=0&&y>=0&&x<width&&y<height&&cells.size()==size_t(width)*height?&cells[y*width+x]:nullptr;}
std::vector<ReleasePoint> releaseRoute(ReleasePoint start,const ReleaseMaps& maps,const ReleaseGoal& goal,const std::vector<ReleasePoint>& blocked,size_t limit){
    if(!point(start))return {};std::map<ReleasePoint,ReleasePoint> parent;std::deque<ReleasePoint> queue{start};parent[start]=start;
    constexpr int dx[]{0,0,-1,1},dy[]{1,-1,0,0};
    while(!queue.empty()&&parent.size()<limit){const auto p=queue.front();queue.pop_front();const auto* map=maps(p.group,p.map);if(!map)continue;
        if(goal(p,*map)){std::vector<ReleasePoint> route{p};while(route.back()!=start){route.push_back(parent.at(route.back()));if(route.size()>8192)return {};}std::reverse(route.begin(),route.end());return route;}
        auto visit=[&](ReleasePoint q,bool warp){const auto* target=maps(q.group,q.map);if(!target)return;const auto* c=target->cell(q.x,q.y);if(!c||!c->walk||(!warp&&c->elevation&&p.elevation&&c->elevation!=p.elevation)||std::find(blocked.begin(),blocked.end(),q)!=blocked.end())return;q.elevation=c->elevation;if(!parent.contains(q)){parent[q]=p;queue.push_back(q);}};
        for(unsigned d=0;d<4;++d){ReleasePoint q=p;q.x+=int16_t(dx[d]);q.y+=int16_t(dy[d]);if(const auto i=map->exits.find({q.x,q.y});i!=map->exits.end()&&(q.x<0||q.y<0||q.x>=map->width||q.y>=map->height)){visit(i->second,true);continue;}visit(q,false);}
        if(const auto i=map->exits.find({p.x,p.y});i!=map->exits.end())visit(i->second,true);
    }return {};
}
std::optional<ReleasePoint> releaseCrowdPlace(ReleasePoint door,const ReleaseMaps& maps,const std::vector<ReleasePoint>& reserved,uint32_t seed){
    const auto* map=maps(door.group,door.map);if(!map)return {};std::vector<std::pair<int,ReleasePoint>> candidates;
    for(int y=std::max(0,int(door.y)-2);y<std::min(map->height,int(door.y)+12);++y)for(int x=std::max(0,int(door.x)-12);x<std::min(map->width,int(door.x)+13);++x){const auto* c=map->cell(x,y);ReleasePoint p{door.group,door.map,c?c->elevation:uint8_t(0),int16_t(x),int16_t(y)};
        if(!c||!c->walk||c->grass||map->exits.contains({x,y})||std::abs(x-door.x)<=1||std::find(reserved.begin(),reserved.end(),p)!=reserved.end())continue;
        int nearest=6;for(const auto& r:reserved)if(r.group==p.group&&r.map==p.map)nearest=std::min(nearest,std::abs(x-r.x)+std::abs(y-r.y));
        const int distance=std::abs(x-door.x)+std::abs(y-door.y);const unsigned noise=(uint32_t(x)*73856093u)^(uint32_t(y)*19349663u)^seed;
        const int score=distance*12+std::abs(y-door.y-2)*4+(nearest<2?35:0)+int(noise%17);candidates.push_back({score,p});
    }
    std::stable_sort(candidates.begin(),candidates.end(),[](auto a,auto b){return a.first<b.first;});
    for(const auto& [score,p]:candidates){(void)score;auto path=releaseRoute(door,maps,[&](auto q,const auto&){return q==p;},reserved,12000);if(!path.empty())return p;}return {};
}
}
