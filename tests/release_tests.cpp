#include "game/released.hpp"
#include "online/session.hpp"
#include <algorithm>
#include <chrono>
#include <future>
#include <iostream>
#include <set>
#include <thread>
using namespace fr::game;
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
template<class F>void until(F f){for(unsigned i=0;i<500;++i){if(f())return;std::this_thread::sleep_for(std::chrono::milliseconds(10));}throw std::runtime_error("Room release timed out");}
ReleasedMon mon(){ReleasedMon m;m.id=fr::online::randomId();m.owner=fr::online::randomId();m.pokemon.species=25;m.pokemon.level=10;m.pokemon.personality=0x12345678;m.pokemon.trainerId=0x87654321;m.pokemon.moves={33,45,0,0};m.pokemon.pp={35,40,0,0};m.pokemon.nickname.fill(255);m.pokemon.trainerName.fill(255);m.origin=m.position=m.entrance={3,1,3,4,4};m.pixelX=m.pixelY=64;return m;}
int main(){try{
    auto m=mon();check(decodeReleased(encodeReleased(m))==m,"Individual data roundtrip");
    auto recaptured=m.pokemon;recaptured.trainerId^=0xffffffff;recaptured.trainerName.fill(1);recaptured.origins^=0x8000;check(sameReleasedIndividual(m.pokemon,recaptured),"Native catcher changes trainer without changing individual");recaptured.ivs^=1;check(!sameReleasedIndividual(m.pokemon,recaptured),"Different individual cannot resolve capture receipt");
    auto bad=m;bad.pokemon.moves[0]=500;check(!validReleasedMon(bad),"Invalid native move");bad=m;bad.pokemon.ivs|=0x40000000;check(!validReleasedMon(bad),"Egg must not become wild");
    bool rejected=false;try{decodeReleased(encodeReleased(m)+" extra");}catch(...){rejected=true;}check(rejected,"Trailing record data rejected");
    auto dir=std::filesystem::temp_directory_path()/("pokemulti-release-"+fr::online::randomId());std::filesystem::create_directories(dir);ReleaseBook book;book.open(dir/"released.cfg");check(book.offer(m),"Offer accepted");check(book.offer(m)&&book.records().size()==1,"Offer retry is idempotent");
    auto live=*book.find(m.id);live.phase=ReleasePhase::Hidden;live.tick=180;check(book.advance(live,true),"Wild arrival committed");check(!book.advance(live,true),"Stale simulation rejected");auto claimant=fr::online::randomId();check(!book.claim(m.id,claimant,{3,1,3,5,4}),"Remote tile cannot claim");check(book.claim(m.id,claimant,m.position),"First encounter accepted");check(!book.claim(m.id,fr::online::randomId(),m.position),"Second player cannot claim same Pokemon");
    book.open(dir/"released.cfg");check(book.find(m.id)->phase==ReleasePhase::Battle,"Disconnect preserves exclusive claim");check(book.finish(m.id,claimant,false),"Run away returns released Pokemon");live=*book.find(m.id);live.tick=180;check(book.advance(live,true)&&book.claim(m.id,claimant,m.position),"Same individual can be encountered again");check(book.finish(m.id,claimant,true),"Capture removes wild individual");book.open(dir/"released.cfg");check(book.offer(m)&&book.find(m.id)->phase==ReleasePhase::Caught,"Old release retry must never duplicate caught Pokemon");
    std::map<unsigned,ReleaseMap> maps;auto make=[&](unsigned id,int w,int h,bool outdoors){auto& a=maps[id];a.width=w;a.height=h;a.outdoors=outdoors;a.cells.assign(size_t(w)*h,{3,true,false,0});};
    make(0x501,9,9,false);make(0x301,31,25,true);make(0x302,12,12,true);
    maps[0x501].exits[{4,8}]={3,1,3,15,8};maps[0x301].exits[{15,8}]={5,1,3,4,8};maps[0x301].exits[{31,10}]={3,2,3,0,5};maps[0x302].cells[5*12+5].grass=true;
    ReleaseMaps provider=[&](uint8_t g,uint8_t n)->const ReleaseMap*{auto i=maps.find((unsigned(g)<<8)|n);return i==maps.end()?nullptr:&i->second;};
    auto route=releaseRoute({5,1,3,4,3},provider,[](auto p,const auto& map){auto c=map.cell(p.x,p.y);return map.outdoors&&c&&c->grass;});check(!route.empty()&&route.back()==ReleasePoint{3,2,3,5,5},"Route crosses indoor door and outdoor map boundary");
    maps[0x302].cells[5*12+4].walk=false;maps[0x302].cells[5*12+6].walk=false;maps[0x302].cells[4*12+5].walk=false;maps[0x302].cells[6*12+5].walk=false;
    check(releaseRoute({5,1,3,4,3},provider,[](auto p,const auto& map){auto c=map.cell(p.x,p.y);return c&&c->grass;}).empty(),"Cannot run through walls to closer grass");
    std::vector<ReleasePoint> crowd;for(unsigned i=0;i<30;++i){auto p=releaseCrowdPlace({3,1,3,15,8},provider,crowd,0x1234+i);check(p.has_value(),"Crowd has room");check(std::abs(p->x-15)>1,"Door access remains clear");check(std::find(crowd.begin(),crowd.end(),*p)==crowd.end(),"Waiting Pokemon do not stack");crowd.push_back(*p);}
    fr::online::Session host(fr::online::randomId(),"Host",dir),one(fr::online::randomId(),"One"),two(fr::online::randomId(),"Two");host.host(0,"release-test-room");one.join("127.0.0.1",host.status().port,"release-test-room");two.join("127.0.0.1",host.status().port,"release-test-room");until([&]{return one.status().peers.size()==3&&two.status().peers.size()==3;});
    auto offered=mon();offered.owner=one.id();check(one.offerReleased(offered),"Guest release accepted by host");until([&]{return two.status().released.size()==1;});check(two.status().released[0].pokemon==offered.pokemon,"Room preserves the released individual");
    live=host.status().released[0];live.phase=ReleasePhase::Hidden;live.tick=180;host.advanceReleased({live});until([&]{return two.status().released[0].phase==ReleasePhase::Hidden;});
    PlayerState p;p.active=true;p.mapGroup=3;p.mapNumber=1;p.elevation=3;p.x=p.y=4;p.pixelX=p.pixelY=64;p.follower=1;one.update(p);two.update(p);until([&]{auto status=host.status();return status.peers[1].player.active&&status.peers[2].player.active;});
    auto a=std::async(std::launch::async,[&]{return one.claimReleased(offered.id);});auto b=std::async(std::launch::async,[&]{return two.claimReleased(offered.id);});const bool aa=a.get(),bb=b.get();check(aa!=bb,"Two network clients cannot catch the same released Pokemon");(aa?one:two).finishReleased(offered.id,true);until([&]{return host.status().released[0].phase==ReleasePhase::Caught;});
    one.stop();two.stop();host.stop();host.host(0,"release-test-room");check(host.status().released[0].phase==ReleasePhase::Caught,"Host restart preserves catch");host.stop();
    std::filesystem::remove_all(dir);std::cout<<"PASS release data, durable claims, cross-map paths, natural crowd, and network contention\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
