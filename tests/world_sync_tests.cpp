#include "online/session.hpp"
#include "game/presentation.hpp"
#include "game/rewards.hpp"
#include <future>
#include <iostream>
#include <thread>
using namespace fr::online;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void until(F f){for(unsigned i=0;i<500;++i){if(f())return;std::this_thread::sleep_for(std::chrono::milliseconds(10));}throw std::runtime_error("World operation timed out");}
std::vector<fr::game::StoryValue> baseline(uint8_t policy=fr::game::DefaultRewardSharing){std::vector<fr::game::StoryValue> v;for(auto id:fr::game::storyKeys(policy))v.push_back({id,0,0});return v;}
void seed(WorldAuthority& a,uint8_t policy=fr::game::DefaultRewardSharing){auto v=baseline(policy);for(size_t i=0;i<v.size();i+=128)a.story(0,{v.begin()+i,v.begin()+std::min(v.size(),i+128)},true,i+128>=v.size());}
WorldReport report(unsigned sequence=1){WorldReport r;r.field=r.started=true;r.group=3;r.map=19;r.sequence=sequence;NpcState n;n.localId=1;n.pose.active=true;n.pose.mapGroup=3;n.pose.mapNumber=19;n.pose.x=10;n.pose.y=10;n.pose.pixelX=160;n.pose.pixelY=160;n.oldX=n.oldY=10;r.npcs.push_back(n);return r;}
int main(){try{
    using namespace fr::game;
    for(auto id:{0x154,0x028,0x02f,0x087,0x828,0x829,0x4031,0x4055,0x4069,0x406f})check(!sharedStory(uint16_t(id)),"Reward, onboarding, inventory or local interaction must remain personal");
    for(auto id:{0x53a,0x233,0x820,0x27a,0x4052})check(sharedStory(uint16_t(id)),"Missing shared story progression");
    check(questLogPlayback(2,0)&&questLogPlayback(3,3)&&questLogPlayback(0,1)&&!questLogPlayback(1,2)&&!questLogPlayback(0,0),"Quest Log overlays must hide only for playback");
    for(const auto& reward:storyRewardRules)check(sharedStory(reward.progress)&&(personalStoryState(reward.claimed)||!sharedStory(reward.claimed)),"Reward eligibility is shared but delivery must remain private");
    // No Pokedex/story baseline: Route 1 must still have one shared population.
    WorldAuthority early;
    auto earlyHost=report();earlyHost.started=false;earlyHost.wildEnabled=false;
    early.report(0,earlyHost,10);
    check(early.snapshot().maps[0].wildOwner==255&&early.snapshot().maps[0].npcs.empty(),"Before a starter there is no wild authority or shared NPC script");
    earlyHost.wildEnabled=true;early.report(0,earlyHost,20);
    earlyHost.wild.push_back({77,16,3,0,12,10,192,160});early.report(0,earlyHost,30);
    auto earlyGuest=earlyHost;earlyGuest.wild.clear();early.report(1,earlyGuest,40);
    check(!early.snapshot().storyReady&&early.snapshot().maps[0].wild.size()==1&&early.snapshot().maps[0].wildOwner==0,"Starter-only clients must publish wild spawns without a story baseline");
    check(!early.claim(1,{3,19,EncounterKind::Story,0},10,40)&&!early.claim(1,{3,19,EncounterKind::Npc,1},11,40),"Early wild access must not enable shared onboarding scripts");
    auto noParty=earlyGuest;noParty.wildEnabled=false;early.report(2,noParty,40);
    const EncounterKey earlyWild{3,19,EncounterKind::Wild,77};
    check(!early.claim(2,earlyWild,12,40),"A player without a usable Pokemon cannot claim a wild encounter");
    check(early.claim(1,earlyWild,13,40)&&!early.claim(0,earlyWild,14,40),"Before Pokedex, a guest can claim a shared spawn exactly once");
    early.report(0,earlyHost,50);check(early.snapshot().maps[0].wild.empty(),"Early consumed spawns must not return from stale reports");early.release(1,13);
    earlyHost.field=false;early.report(0,earlyHost,60);early.report(1,earlyGuest,70);
    check(early.snapshot().maps[0].wildOwner==1,"Early grass ownership transfers when the owner leaves the field");
    earlyGuest.wild.push_back({78,19,3,0,13,10,208,160});early.report(1,earlyGuest,80);
    check(early.snapshot().maps[0].wild.size()==1,"New early-game authority must continue spawning");
    // A busy NPC cannot make its neighboring coordinate barrier disappear.
    // Reserve the scene before either client enters the event tile.
    {
        WorldAuthority gates;seed(gates);auto first=report(),second=report();
        first.map=second.map=1;first.npcs[0].pose.mapNumber=1;second.npcs.clear();gates.report(0,first,100);gates.report(1,second,100);
        const EncounterKey oldMan{3,1,EncounterKind::Npc,1};
        const EncounterKey road{3,1,EncounterKind::Story,0,0x4051,0};
        check(gates.claim(0,oldMan,101,100),"First trainer can talk to road NPC");
        check(!gates.claim(1,road,102,100),"Story trigger stays blocked while another trainer talks");
        gates.release(1,101);
        check(!gates.claim(1,road,103,100),"Another trainer cannot release the NPC owner");
        second.map=2;gates.report(1,second,110);
        check(gates.claim(1,{3,2,EncounterKind::Story,0,0x406c,0},104,110),"Unrelated map remains playable during an NPC encounter");
        gates.release(1,104);second.map=1;gates.report(1,second,120);gates.release(0,101);
        check(gates.claim(1,road,105,120)&&!gates.claim(0,road,106,120),"Only one simultaneous approach may enter the scene");
        gates.leave(1);
        check(gates.claim(0,road,107,120),"Disconnect releases ownership without opening the story barrier");
        gates.release(0,107);gates.report(1,second,130);
        auto condition=*std::find_if(gates.snapshot().story.begin(),gates.snapshot().story.end(),[](auto v){return v.id==0x4051;});
        condition.value=1;gates.story(0,{condition},false,true);
        check(!gates.claim(1,road,108,130),"A stale closed scene cannot start after shared progress opens the route");
        check(gates.claim(1,{3,1,EncounterKind::Story,0,0x4051,1},109,130),"The newly active native story condition can start");
    }
    // The runtime must choose hidden-object records from the captured map,
    // even while its previous frame still refers to the city being left.
    {
        auto leaving=report();leaving.map=1;leaving.npcs[0].pose.mapNumber=1;leaving.npcs[0].visible=false;
        auto entering=leaving;entering.map=41;
        check(!validWorldReport(entering),"A Viridian hidden NPC cannot be published as Route 22 state");
        entering.npcs.clear();check(validWorldReport(entering),"An initial route snapshot without loaded NPCs is valid");
        WorldAuthority boundary;seed(boundary);boundary.report(0,leaving,100);boundary.report(0,entering,116);
        check(boundary.snapshot().maps.size()==2&&boundary.snapshot().maps[1].npcs.empty(),"Map transition must retain separate NPC records");
        bool rejected=false;try{entering.npcs=leaving.npcs;boundary.report(0,entering,132);}catch(const std::exception&){rejected=true;}
        check(rejected,"Host must retain cross-map validation after the runtime fix");
    }
    WorldAuthority authority;seed(authority);check(authority.snapshot().storyReady,"Complete host baseline");
    auto a=report(),b=report();authority.report(0,a,100);b.npcs[0].pose.x=20;authority.report(1,b,100);
    check(authority.snapshot().maps[0].npcs[0].pose.x==10&&authority.snapshot().maps[0].npcs[0].owner==0,"NPC wandering must have one authority");
    EncounterKey encounter{3,19,EncounterKind::Npc,1};check(authority.claim(1,encounter,1,100),"Triggering guest must own NPC encounter");check(!authority.claim(0,encounter,2,100),"Two players cannot trigger the same NPC");
    check(authority.snapshot().maps[0].npcs[0].owner==1,"NPC ownership transfers to triggering player");
    b.npcs[0].pose.x=11;authority.report(1,b,150);check(authority.snapshot().maps[0].npcs[0].pose.x==11,"Guest scripted movement must reach host");
    a.map=2;a.npcs.clear();authority.report(0,a,200);check(authority.snapshot().leases[0].owner==1,"Other players may leave the encounter map");
    authority.release(1,1);authority.leave(1);a=report(2);authority.report(0,a,250);check(authority.snapshot().maps[0].npcs[0].owner==0,"Disconnect must release NPC authority");
    a.wild.push_back({99,16,5,0,12,10,192,160});authority.report(0,a,300);authority.report(1,b,300);
    EncounterKey wild{3,19,EncounterKind::Wild,99};check(authority.claim(1,wild,3,300),"Guest wild contact");check(!authority.claim(0,wild,4,300),"Wild contact must be exclusive");authority.report(0,a,350);check(authority.snapshot().maps[0].wild.empty(),"Delayed owner snapshot must not resurrect consumed Pokemon");
    auto v=*std::find_if(authority.snapshot().story.begin(),authority.snapshot().story.end(),[](auto x){return x.id==0x820;});auto stale=v;v.value=1;authority.story(1,{v},false,true);authority.story(0,{stale},false,true);check(std::find_if(authority.snapshot().story.begin(),authority.snapshot().story.end(),[](auto x){return x.id==0x820;})->value==1,"Stale story commit cannot roll back another encounter");
    bool rejected=false;try{authority.story(0,{{0x828,1,0}},false,true);}catch(...){rejected=true;}check(rejected,"Host must reject personal reward synchronization");
    // Every host combination is a complete, independent semantic policy.
    for(uint8_t policy=0;policy<=AllRewardSharing;++policy){
        WorldAuthority room;room.rewardRules(policy);seed(room,policy);
        check(room.snapshot().storyReady,"Custom policy baseline is complete");
        for(const auto& reward:storyRewardRules){check(sharedStory(reward.progress,policy),"Story progress is always shared");check(personalStoryState(reward.claimed)||sharedStory(reward.claimed,policy)!=shareReward(reward,policy),"Unchecked reward claims are exclusive; checked rewards remain individually claimable");}
        for(const auto& mon:uniquePokemon)check(sharedStory(mon.claimed,policy)==!(policy&ShareSpecialPokemon),"Special Pokemon availability follows host policy");
        bool invalid=false;try{room.rewardRules(policy);}catch(...){invalid=true;}check(invalid,"Session rules cannot change after baseline");
    }
    WorldAuthority unique;seed(unique);unique.report(0,report(),100);unique.report(1,report(),100);
    const EncounterKey eevee{3,19,EncounterKind::Unique,0x263,0x263,0};
    check(unique.claim(1,eevee,50,100)&&!unique.claim(0,eevee,51,100),"Exactly one trainer may claim Eevee");
    check(!unique.claim(0,encounter,52,100),"Unique dialogue owns its map NPCs");
    auto claimValue=*std::find_if(unique.snapshot().story.begin(),unique.snapshot().story.end(),[](auto v){return v.id==0x263;});
    claimValue.value=1;unique.story(0,{claimValue},false,true);
    claimValue=*std::find_if(unique.snapshot().story.begin(),unique.snapshot().story.end(),[](auto v){return v.id==0x263;});check(!claimValue.value,"Only the encounter owner can commit its Pokemon claim");
    claimValue.value=1;unique.story(1,{claimValue},false,true);unique.release(1,50);
    check(!unique.claim(0,eevee,53,100),"Claim stays consumed after lease release");
    for(auto id:{0x263,0x57}){auto v=*std::find_if(unique.snapshot().story.begin(),unique.snapshot().story.end(),[&](auto x){return x.id==id;});check(v.value==1,"Claim hides gift object for every trainer");v.value=0;unique.story(0,{v},false,true);check(std::find_if(unique.snapshot().story.begin(),unique.snapshot().story.end(),[&](auto x){return x.id==id;})->value==1,"A stale player cannot restore a claimed Pokemon");}
    {Session policyHost(randomId(),"Policy host"),policyGuest(randomId(),"Policy guest");policyHost.host(0,"policy-room",ShareSpecialPokemon);policyGuest.join("127.0.0.1",policyHost.status().port,"policy-room");until([&]{return policyGuest.status().connected;});check(policyGuest.status().rewardPolicy==ShareSpecialPokemon,"Host checkbox selection reaches guests");policyHost.updateStory(baseline(ShareSpecialPokemon),true);until([&]{return policyGuest.status().world.storyReady;});policyGuest.stop();policyHost.stop();policyHost.host(0,"policy-room",0);check(policyHost.status().rewardPolicy==0,"A new session can use different rules");}
    Session host(randomId(),"Host"),guest(randomId(),"Guest");host.host(0,"world-test-room");guest.join("127.0.0.1",host.status().port,"world-test-room");until([&]{return guest.status().peers.size()==2;});auto socketEarly=report();socketEarly.started=false;
    host.updateWorld(socketEarly);guest.updateWorld(socketEarly);
    until([&]{return !host.status().world.maps.empty()&&!guest.status().world.maps.empty();});
    const auto earlyOwner=host.status().world.maps[0].wildOwner;
    socketEarly.sequence=2;socketEarly.wild.push_back({123,16,3,0,12,10,192,160,1,0,true});
    if(earlyOwner==0)host.updateWorld(socketEarly);else guest.updateWorld(socketEarly);
    until([&]{return !guest.status().world.maps[0].wild.empty()&&!host.status().world.maps[0].wild.empty();});
    check(!host.status().world.storyReady&&!guest.status().world.storyReady,"Early socket world must not seed story");
    host.setShinyRate(1);until([&]{return guest.status().shinyRate==1;});host.setShinyRate(8192);until([&]{return guest.status().shinyRate==8192;});
    check(host.status().world.maps[0].wild[0].shiny&&guest.status().world.maps[0].wild[0].shiny,"Existing spawn identity survives live rate changes on both clients");
    const EncounterKey socketWild{3,19,EncounterKind::Wild,123};
    auto earlyFirst=std::async(std::launch::async,[&]{return host.claimEncounter(socketWild);});
    auto earlySecond=std::async(std::launch::async,[&]{return guest.claimEncounter(socketWild);});
    const auto eh=earlyFirst.get(),eg=earlySecond.get();check(bool(eh)!=bool(eg),"Unseeded socket room has exactly one wild claim winner");
    if(eh)host.releaseEncounter(eh);else guest.releaseEncounter(eg);
    until([&]{return host.status().world.leases.empty()&&guest.status().world.leases.empty();});
    host.updateStory(baseline(),true);until([&]{return guest.status().world.storyReady;});
    // A host NPC snapshot does not acknowledge the guest's started=true report.
    // Include a guest-only actor and wait for it: otherwise the immediate claim
    // race can run before the guest's next 16 ms world-report transmission.
    auto startedGuest=report(3);auto guestActor=startedGuest.npcs.front();guestActor.localId=2;startedGuest.npcs.push_back(guestActor);
    host.updateWorld(report(3));guest.updateWorld(startedGuest);
    until([&]{return guest.status().world.maps[0].npcs.size()==2&&host.status().world.maps[0].npcs.size()==2;});
    auto first=std::async(std::launch::async,[&]{return host.claimEncounter(encounter);});auto second=std::async(std::launch::async,[&]{return guest.claimEncounter(encounter);});const auto h=first.get(),g=second.get();check(bool(h)!=bool(g),"Exactly one native client claim may win");if(h)host.releaseEncounter(h);else guest.releaseEncounter(g);until([&]{return host.status().world.leases.empty()&&guest.status().world.leases.empty();});
    const auto owned=guest.claimEncounter(encounter);check(owned!=0,"Guest can acquire released NPC");guest.stop();until([&]{return host.status().world.leases.empty()&&host.status().peers.size()==1;});
    host.stop();check(host.status().world.maps.empty(),"Leaving must clear shared world cache");
    std::cout<<"Shared NPC authority, encounter race, disconnect, wild consumption, story conflicts, personal rewards and recap gate passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
