#include "online/campaign.hpp"
#include "online/session.hpp"
#include "game/dialogue.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
using namespace fr::online;
using namespace fr::game;
void require(bool good,const char* what){if(!good)throw std::runtime_error(what);}
template<class F>void until(F f){for(unsigned i=0;i<500;++i){if(f())return;std::this_thread::sleep_for(std::chrono::milliseconds(10));}throw std::runtime_error("Campaign socket operation timed out");}
std::vector<StoryValue> blank(uint8_t policy=DefaultRewardSharing){std::vector<StoryValue> v;for(auto id:storyKeys(policy))v.push_back({id,0,0});return v;}
StoryValue value(const Status& s,uint16_t id){for(auto v:s.world.story)if(v.id==id)return v;throw std::runtime_error("Missing story key");}
int main(){try{
    const auto root=std::filesystem::temp_directory_path()/("pokemulti-campaign-test-"+randomId());
    CampaignBook book;book.open(root);const auto id=book.snapshot().id;require(!book.ready(),"New campaign must wait for a native baseline");
    auto story=blank();book.commit(story,"Earlier adventure");
    for(auto& v:story)if(v.id==0x4057)v.value=2;
    book.commit(story,"Leaf");require(book.snapshot().completed.size()==2,"Parcel has explicit collection and completion milestones");
    book.commit(story,"Leaf");require(book.snapshot().completed.size()==2,"Repeated reports cannot duplicate the journal");
    CampaignBook resumed;resumed.open(root);require(resumed.ready()&&resumed.snapshot().id==id&&resumed.snapshot().completed.size()==2,"Campaign must survive reopening independently of native saves");
    auto none=resumed.baseline(0);require(std::any_of(none.begin(),none.end(),[](auto v){return v.id==0x4057&&v.value==2;}),"Changing reward policy cannot reset narrative progress");
    CampaignBook fresh;fresh.open(root,true);require(!fresh.ready()&&fresh.snapshot().id!=id&&std::filesystem::exists(root/"campaigns"/(id+".cfg")),"Starting another campaign preserves the previous file");
    for(const auto& r:storyRewardRules)if(r.essential)for(unsigned p=0;p<8;++p)require(shareReward(r,uint8_t(p)),"Quest access must survive every optional reward setting");
    for(auto f:{0x820,0x827,0x82c,0x555,0x69e,0x238})require(personalStoryState(uint16_t(f)),"Native achievements and receipts must stay personal");
    require(!sharedStory(0x4068)&&!sharedStory(0x4bc)&&!sharedStory(0x4031),"League sequence, championship and starter choices stay private");
    auto text=dialogueText(std::string(200,'W')+"\nHello",[](uint8_t){return 8u;});
    unsigned line=0;bool page=false;
    for(auto c:text){if(c==0xff)break;if(c==0xfe||c==0xfb){require(line<=204,"Long messages must fit native text width");line=0;page|=c==0xfb;}else line+=8;}
    require(page&&text.back()==0xff,"Long messages need native pages and a terminator");
    const auto unsafe=dialogueText(std::string("Hi")+char(0xfc)+char(0xff),[](uint8_t){return 6u;});
    require(std::count(unsafe.begin(),unsafe.end(),0xff)==1&&std::find(unsafe.begin(),unsafe.end(),0xfc)==unsafe.end(),"Player names cannot inject text commands");
    const auto networkRoot=root/"network";const auto hostId=randomId(),guestId=randomId();
    Session host(hostId,"Aster",networkRoot),guest(guestId,"Leaf");host.host(0,"campaign-test");guest.join("127.0.0.1",host.status().port,"campaign-test");
    until([&]{return guest.status().connected;});host.updateStory(blank(),true);until([&]{return guest.status().world.storyReady;});
    auto parcel=value(guest.status(),0x4057);parcel.value=2;guest.updateStory({parcel});
    until([&]{return host.status().campaign.completed.size()==2&&guest.status().campaign.completed.size()==2;});
    require(host.status().campaign.completed.back().actor=="Leaf","Host records the authenticated completing trainer");
    const auto campaign=host.status().campaign.id;
    auto badge=value(host.status(),0x820);badge.value=1;host.updateStory({badge});until([&]{return value(guest.status(),0x820).value==1;});
    auto rollback=value(guest.status(),0x820);rollback.value=0;guest.updateStory({rollback});until([&]{return value(guest.status(),0x820).revision>rollback.revision;});
    require(value(guest.status(),0x820).value==1,"An unearned local badge cannot undo the shared clearance");
    guest.stop();host.stop();host.host(0,"campaign-test",0);guest.join("127.0.0.1",host.status().port,"campaign-test");
    until([&]{return guest.status().world.storyReady&&!guest.status().campaign.id.empty();});
    require(guest.status().campaign.id==campaign&&value(guest.status(),0x4057).value==2&&value(guest.status(),0x820).value==1,"A cold room restart must restore the campaign before new native reports");
    require(guest.status().rewardPolicy==0,"Session reward choices remain configurable when resuming");
    guest.stop();host.stop();
    std::cout<<"Campaign persistence, new-campaign isolation, objective deduplication, policy independence, personal achievements, native text bounds and socket restart passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
