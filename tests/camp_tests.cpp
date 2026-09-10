#include "online/session.hpp"
#include <iostream>
#include <thread>
#include <set>
using namespace fr::game;using namespace fr::online;
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
template<class F>void until(F fn){for(unsigned i=0;i<600;++i){if(fn())return;std::this_thread::sleep_for(std::chrono::milliseconds(10));}throw std::runtime_error("Camp networking timed out");}
CampState tent(uint32_t id=1,int x=10,int y=10){CampState c;c.id=id;c.group=3;c.map=19;c.x=int16_t(x);c.y=int16_t(y);c.party={{1,int16_t(x*16),int16_t((y+3)*16),1}};setCampGround(c,campGround(c,[](int x,int y){return x>=0&&y>=0&&x<512&&y<512;}));return c;}
WorldReport report(CampState camp){WorldReport r;r.field=true;r.group=camp.group;r.map=camp.map;r.camp=camp;r.sequence=1;return r;}
int main(){try{
    for(unsigned type=0;type<10;++type)for(unsigned id=0;id<1024;++id){const bool expected=(type==1||type==2||type==3)&&(id==1||id==8||id==9||id==16||id==17);check(campGrass(uint8_t(type),false,true,uint16_t(0x3000|id),0,3)==expected,"Only full outdoor lawn tiles qualify");}
    check(!campGrass(3,true,true,0x3001,0,3)&&!campGrass(3,false,false,0x3001,0,3)&&!campGrass(3,false,true,0x3401,0,3)&&!campGrass(3,false,true,0x3001,0,2)&&!campGrass(3,false,true,0x3001,2,3),"Caves, wrong tilesets, collision, elevation and tall grass rejected");
    auto c=tent();auto free=[](int x,int y){return x>=0&&y>=0&&x<30&&y<30;};
    check(campFootprint(c,free)&&!campFootprint(c,[](int x,int y){return !(x==13&&y==12);}),"All twelve footprint tiles must be clear");
    auto ground=campGround(c,free);check(ground.size()>6,"Camp has roaming room");
    for(auto p:ground)check(campWalkable(c,p.x,p.y),"Campsite ground contains every party roaming tile");
    check(!campWalkable(c,10,10)&&!campWalkable(c,30,30),"Party roaming excludes the tent footprint and outside radius");
    check(campHasEntrance(c),"The front entrance opens onto campsite ground");
    auto oneSide=c;setCampGround(oneSide,{{14,10},{14,11},{14,12},{15,10},{15,11},{15,12},{16,10},{16,11},{16,12}});check(!campHasEntrance(oneSide)&&!validCamp(oneSide),"A patch disconnected from the front entrance cannot become a camp");
    // Route 1 clearing from the report: ledges enclose rows 11-14. A 4x3
    // tent fits at (4,11), with a connected front yard, but no back walking lane.
    constexpr const char* clearing[]{"#################","##.#....##...####","###..........####","##.#.........####","####......#######","#################"};
    auto lawn=[&](int x,int y){return x>=0&&x<17&&y>=10&&y<16&&clearing[y-10][x]=='.';};
    auto compact=tent(21,4,11);auto yard=campGround(compact,lawn,CampCell{8,13});
    check(campFootprint(compact,lawn),"Reported Route 1 clearing fits the native tent footprint");
    setCampGround(compact,yard);check(campHasEntrance(compact)&&!campWalkable(compact,4,10),"Camp can back onto a ledge with a reachable entrance");
    CampSimulation small;check(small.start(compact,{1,4,7,25,133,16},yard),"Full party can camp in the reported clearing");
    for(unsigned i=0;i<600;++i){small.update(lawn);check(validCamp(small.state),"Compact camp party stays connected and within grass");}
    auto sealed=compact;auto blocked=yard;std::erase(blocked,CampCell{5,14});setCampGround(sealed,blocked);check(!campHasEntrance(sealed),"Blocked front entrance is rejected");
    auto disconnected=campGround(compact,lawn,CampCell{1,1});check(disconnected.empty(),"Camper cannot reserve an unreachable yard");
    auto perimeter=campPerimeter(c);check(!perimeter.empty(),"Perimeter is visible");
    constexpr int dx[]{0,0,-1,1},dy[]{1,-1,0,0};
    for(auto e:perimeter){check(campWalkable(c,e.cell.x,e.cell.y)||campContains(c,e.cell.x,e.cell.y),"Outline stays on campsite ground");check(!campWalkable(c,e.cell.x+dx[e.direction],e.cell.y+dy[e.direction])&&!campContains(c,e.cell.x+dx[e.direction],e.cell.y+dy[e.direction]),"No internal grid edges or tent outline");}
    auto notched=c;setCampGround(notched,campGround(c,[&](int x,int y){return free(x,y)&&x!=14;},CampCell{9,13}));check(campWalkable(notched,9,13)&&!campWalkable(notched,15,13),"Player anchor selects its connected side of the lawn");
    auto hole=c;auto ring=ground;std::erase(ring,CampCell{9,11});setCampGround(hole,ring);
    for(auto e:campPerimeter(hole))check(!(e.cell.x+dx[e.direction]==9&&e.cell.y+dy[e.direction]==11),"An enclosed hole does not become an inner outline");
    CampSimulation sim;check(sim.start(c,{1,4,7,25,133,16},ground),"Six-member party starts");bool playful=false;std::set<unsigned> directions;auto initial=sim.state.party;
    for(int i=0;i<10000;++i){const auto before=sim.state;sim.update(free);check(validCamp(sim.state),"All roaming stays outside tent and inside radius");std::set<std::pair<int,int>> cells;
        for(size_t j=0;j<sim.state.party.size();++j){auto p=sim.state.party[j];check(cells.emplace(p.x,p.y).second,"Party Pokemon reserve separate positions");check(std::abs(p.x-before.party[j].x)+std::abs(p.y-before.party[j].y)<=1,"No teleporting during camp walks");playful|=p.mood!=0;directions.insert(p.facing);}}
    check(playful&&directions.size()==4&&sim.state.party[0].x!=initial[0].x,"Party wanders, turns, and plays");
    CampSimulation pet;check(pet.start(c,{1,4,7,25,133,16},ground),"Interaction camp starts");
    for(unsigned i=0;i<20;++i)pet.update(free);
    const auto selected=pet.state.party[0];const auto others=pet.state.party;
    check(campInteractionTarget(pet.state,selected.x-16,selected.y,4)==0,"X selects the camp Pokemon immediately in front");
    check(campInteractionTarget(pet.state,0,0,4)==-1,"Distant camp Pokemon cannot be petted");
    check(pet.interact(0,3,12,48)&&!pet.interact(0,3,1,48),"Start one reaction without restarting a busy Pokemon");
    for(unsigned i=1;i<48;++i){pet.update(free);check(pet.state.party[0].x==selected.x&&pet.state.party[0].y==selected.y&&pet.state.party[0].facing==3&&pet.state.party[0].emote==12,"Reacting Pokemon must hold its sub-tile position and face the trainer");}
    bool otherMoved=false;for(unsigned i=1;i<6;++i)otherMoved|=others[i].x!=pet.state.party[i].x||others[i].y!=pet.state.party[i].y;check(otherMoved,"Petting one Pokemon must not freeze the whole party");
    pet.update(free);check(!pet.state.party[0].emote&&pet.state.party[0].emoteSequence==1,"Camp emote clears after its duration");
    auto last=pet.state.party[0];bool resumed=false;for(unsigned i=0;i<200;++i){pet.update(free);const auto next=pet.state.party[0];check(std::abs(next.x-last.x)+std::abs(next.y-last.y)<=1,"Petting must not snap a paused walk on resume");resumed|=next.x!=last.x||next.y!=last.y;last=next;}check(resumed,"Pet resumes roaming");
    check(!pet.interact(6,1,1,48)&&!pet.interact(0,1,13,48),"Invalid camp reaction is rejected");
    auto invalid=c;invalid.party.resize(7);check(!validCamp(invalid),"Bound party packet");invalid=c;invalid.ground[0]|=0x8000;check(!validCamp(invalid),"Invalid area bits rejected");invalid=c;invalid.party[0].x=0;check(!validCamp(invalid),"Reject out of radius pose");
    WorldAuthority arb;auto a=report(c),b=report(tent(2));arb.report(0,a,10);arb.report(1,b,11);check(arb.snapshot().camps[0].id==1&&!arb.snapshot().camps[1].id&&arb.snapshot().campDecisions[1]==2,"One host-approved reservation wins simultaneous placement");
    b.camp=tent(3,16,10);++b.sequence;arb.report(1,b,12);check(arb.snapshot().camps[1].id==3,"Separate tents coexist");auto changed=b;changed.camp.x=17;changed.camp.party[0].x=17*16;++changed.sequence;arb.report(1,changed,13);check(arb.snapshot().camps[1].x==16,"An accepted tent cannot teleport");
    changed=b;auto reduced=campGround(changed.camp,free);reduced.pop_back();setCampGround(changed.camp,reduced);++changed.sequence;arb.report(1,changed,14);check(arb.snapshot().camps[1].ground==b.camp.ground,"Accepted walking area cannot shift as actors move");
    a.camp={};++a.sequence;arb.report(0,a,14);check(!arb.snapshot().camps[0].id,"Pack up releases reservation");arb.leave(1);check(!arb.snapshot().camps[1].id,"Disconnect clears tent and party");
    sim.state.party[0].emote=12;sim.state.party[0].emoteSequence=99;sim.state.party[0].shiny=true;sim.state.party[1].shiny=false;sim.update(free);check(sim.state.party[0].shiny&&!sim.state.party[1].shiny,"Camp motion keeps each Pokemon identity");
    sim.state.party[0].emote=12;sim.state.party[0].emoteSequence=99;auto all=report(sim.state);Session host(randomId(),"Camper"),guest(randomId(),"Visitor");host.host(0,"camp-test-key");guest.join("127.0.0.1",host.status().port,"camp-test-key");until([&]{return guest.status().connected&&host.status().peers.size()==2;});host.updateWorld(all);until([&]{return guest.status().world.camps[0].id==c.id;});check(guest.status().world.camps[0].party.size()==6&&guest.status().world.camps[0].ground==all.camp.ground,"Full camp party and walking area cross real sockets");
    Session late(randomId(),"Late visitor");late.join("127.0.0.1",host.status().port,"camp-test-key");until([&]{return late.status().world.camps[0].id==c.id;});check(late.status().world.camps[0].ground==all.camp.ground,"Late join gets identical perimeter");check(guest.status().world.camps[0].party[0].shiny&&late.status().world.camps[0].party[0].shiny&&!late.status().world.camps[0].party[1].shiny,"Mixed shiny camp party survives sockets and late join");check(guest.status().world.camps[0].party[0].emote==12&&late.status().world.camps[0].party[0].emoteSequence==99&&!guest.status().world.camps[0].party[1].emote,"Only the addressed camp member reacts across sockets and late join");
    auto guestCamp=report(tent(44,20,10));guest.updateWorld(guestCamp);until([&]{return host.status().world.camps[1].id==44&&late.status().world.camps[1].id==44;});guest.stop();until([&]{return !host.status().world.camps[1].id&&!late.status().world.camps[1].id;});all.camp={};++all.sequence;host.updateWorld(all);until([&]{return !late.status().world.camps[0].id;});late.stop();host.stop();
    std::cout<<"PASS: grass placement, party behavior, host arbitration and camp sockets\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
