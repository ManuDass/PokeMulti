#include "online/session.hpp"
#include <algorithm>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>
using namespace fr::online;
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
template<class F>void until(F f){for(unsigned i=0;i<500;++i){if(f())return;std::this_thread::sleep_for(std::chrono::milliseconds(10));}throw std::runtime_error("Retry socket operation timed out");}
std::vector<fr::game::StoryValue> baseline(){std::vector<fr::game::StoryValue> out;for(auto id:fr::game::storyKeys())out.push_back({id,uint16_t(id==0x4054?1:0),0});return out;}
void seed(WorldAuthority& w){auto all=baseline();for(size_t i=0;i<all.size();i+=128)w.story(0,{all.begin()+i,all.begin()+std::min(i+128,all.size())},true,i+128>=all.size());}
WorldReport field(int x=25){WorldReport r;r.group=3;r.map=41;r.field=r.started=r.wildEnabled=true;r.sequence=1;NpcState n;n.localId=1;n.pose.active=true;n.pose.mapGroup=3;n.pose.mapNumber=41;n.pose.x=int16_t(x);n.pose.y=5;n.oldX=n.pose.x;n.oldY=5;n.pose.pixelX=int16_t(x*16);n.pose.pixelY=80;n.pose.facing=4;n.pose.elevation=3;r.npcs.push_back(n);return r;}
const EncounterKey rival{3,41,EncounterKind::Story,0,0x4054,1};
const NpcState& actor(const SharedWorld& w){for(const auto& map:w.maps)if(map.group==3&&map.map==41)return map.npcs.at(0);throw std::runtime_error("Missing rival map");}
int main(){try{
 WorldAuthority w;seed(w);w.report(0,field(),100);w.report(1,field(),100);
 check(w.claim(0,rival,10,100),"First trainer starts the scene");auto staged=field(32);
 w.checkpoint(1,10,staged.npcs);check(actor(w.snapshot()).pose.x==25,"A bystander cannot checkpoint someone else's encounter");
 w.checkpoint(0,10,staged.npcs);check(actor(w.snapshot()).pose.x==32,"Capture the actual completed approach");
 auto battle=staged;battle.field=false;battle.npcs.clear();battle.sequence=2;w.report(0,battle,150);
 check(w.snapshot().leases.size()==1&&!w.claim(1,rival,11,150),"Battle screen retains exclusive ownership");
 w.release(1,10,true);check(w.snapshot().leases.size()==1,"A foreign release cannot abandon another trainer's event");
 w.release(0,10,true);check(w.snapshot().leases.empty()&&w.snapshot().retries.size()==1,"Blackout opens a retry without completing the objective");
 check(w.snapshot().retries[0].actors==std::vector<uint8_t>{1},"Retry identifies the staged actor");
 auto stale=field();stale.sequence=2;w.report(1,stale,200);stale.sequence=3;w.report(1,stale,216);
 check(actor(w.snapshot()).pose.x==32,"Fresh map reports cannot reset a waiting actor to spawn");
 check(w.claim(1,rival,12,216)&&!w.claim(0,rival,13,216),"Another trainer can acquire the failed encounter exclusively");
 w.release(0,10,true);check(w.snapshot().leases[0].owner==1,"A delayed first-attempt release cannot affect the new owner");
 w.checkpoint(1,12,staged.npcs);auto center=field();center.group=5;center.map=4;center.npcs.clear();center.sequence=4;w.report(1,center,250);
 check(w.snapshot().leases.empty()&&w.snapshot().retries.size()==1,"A center return also releases a missed blackout notification");
 w.report(0,staged,260); // stale sequence rejected; publish a current report next
 staged.sequence=3;w.report(0,staged,270);check(w.claim(0,rival,14,270),"The original trainer may also retry");
 w.checkpoint(0,14,staged.npcs);w.release(0,14,true);
 check(actor(w.snapshot()).pose.x==32,"Repeated failures do not accumulate relative movement");
 check(w.claim(0,rival,15,270),"Retry after repeated failures");w.checkpoint(0,15,staged.npcs);
 auto progress=*std::find_if(w.snapshot().story.begin(),w.snapshot().story.end(),[](auto v){return v.id==0x4054;});progress.value=2;w.story(0,{progress},false,true);w.release(0,15);
 check(w.snapshot().retries.empty()&&!w.claim(0,rival,16,270),"Successful story advancement clears the retry and its old trigger");
 WorldAuthority disconnected;seed(disconnected);disconnected.report(0,field(),10);disconnected.report(1,field(),10);check(disconnected.claim(0,rival,20,10),"Disconnect fixture claim");disconnected.checkpoint(0,20,field(32).npcs);disconnected.leave(0);check(disconnected.claim(1,rival,21,20),"Disconnect lets a friend take over");check(disconnected.snapshot().retries.size()==1,"Disconnect preserves the completed approach");
 Session host(randomId(),"Aster"),guest(randomId(),"Leaf");host.host(0,"retry-test");guest.join("127.0.0.1",host.status().port,"retry-test");until([&]{return guest.status().peers.size()==2;});host.updateStory(baseline(),true);until([&]{return guest.status().world.storyReady;});host.updateWorld(field());guest.updateWorld(field());until([&]{return !guest.status().world.maps.empty();});
 const auto first=host.claimEncounter(rival);check(first!=0,"Native-style host claim over the socket");host.checkpointEncounter(first,field(32).npcs);until([&]{return actor(guest.status().world).pose.x==32;});host.releaseEncounter(first,true);
 until([&]{return guest.status().world.leases.empty()&&guest.status().world.retries.size()==1;});
 const auto next=guest.claimEncounter(rival);check(next!=0,"Guest receives retry and acquires the encounter through real room packets");guest.checkpointEncounter(next,field(32).npcs);guest.releaseEncounter(next);
 until([&]{return host.status().world.retries.empty()&&host.status().world.leases.empty()&&guest.status().world.retries.empty();});guest.stop();host.stop();
 std::cout<<"Encounter failure handoff, staged poses, repeated retries, wrong/stale owners, center return, disconnect, completion and socket snapshots passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
