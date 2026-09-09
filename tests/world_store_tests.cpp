#include "frontend/world_store.hpp"
#include "online/session.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>
using namespace fr;
using namespace fr::online;
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class F>void until(F fn){for(unsigned i=0;i<1000;++i){if(fn())return;std::this_thread::sleep_for(std::chrono::milliseconds(10));}throw std::runtime_error("World session timed out.");}
int main(){try{
    const auto root=std::filesystem::temp_directory_path()/("pokemulti-world-test-"+worldRandomId());
    const auto hash=std::string(64,'a');auto a=createWorld(root,"First",hash),b=createWorld(root,"Second",hash);
    check(listWorlds(root,hash).size()==2&&a.id!=b.id,"Independent world selection");
    const auto id=worldRandomId(),secret=worldRandomId();WorldPlayers players(a);
    check(players.authenticate(id,secret)&&players.authenticate(id,secret)&&!players.authenticate(id,worldRandomId()),"Private trainer credential");
    std::vector<uint8_t> flash(131072,255);flash[0]=42;auto first=captureCheckpoint(root/"runtime",flash);players.commit(id,first);
    flash[0]=43;auto second=captureCheckpoint(root/"runtime",flash);players.commit(id,second);check(players.load(id)==second,"Latest committed checkpoint");
    check(WorldPlayers(b).load(id).empty(),"Other worlds do not inherit party/money");
    auto broken=second;broken[50]^=1;check(!validCheckpoint(broken),"Corruption rejected");
    bool rejected=false;try{players.commit(id,broken);}catch(...){rejected=true;}check(rejected&&players.load(id)==second,"Bad upload cannot replace checkpoint");
    atomicWorldFile(a.folder/"players"/id/"checkpoint.pmsv",broken);check(players.load(id)==first,"Crash/corruption recovery uses previous checkpoint");
    restoreCheckpoint(root/"restored",first);check(readWorldFile(root/"restored"/"trainer.sav")[0]==42,"Flash restores exactly");
    rejected=false;try{players.load("../escape");}catch(...){rejected=true;}check(rejected,"Player path confined to world");
    Session host(worldRandomId(),"Host",a.folder),guest(id,"Guest");
    host.configureWorld(a.folder,hash,worldRandomId());guest.configureWorld({},hash,secret);host.host(0,"world-test-key",3,false,8);auto port=host.status().port;
    guest.join("127.0.0.1",port,"world-test-key");until([&]{return guest.status().checkpointReady;});check(guest.downloadedSave()==first&&guest.status().worldId==a.id,"Host provides world-specific trainer before boot");
    const auto serial=guest.storeCheckpoint(second);until([&]{return guest.status().checkpointAck==serial;});check(players.load(id)==second,"Receipt follows durable host commit");
    host.requestWorldSave();until([&]{return guest.status().checkpointRequest==1;});
    guest.stop();until([&]{return host.status().peers.size()==1;});
    Session imposter(id,"Guest");imposter.configureWorld({},hash,worldRandomId());imposter.join("127.0.0.1",port,"world-test-key");until([&]{return !imposter.status().running;});check(host.status().running,"Wrong credential refused without stopping host");
    guest.join("127.0.0.1",port,"world-test-key");until([&]{return guest.status().checkpointReady;});check(guest.downloadedSave()==second,"Reconnect downloads host checkpoint, ignoring local continuation");
    host.stop();until([&]{return !guest.status().running;});
    Session many(worldRandomId(),"Many");many.host(0,"capacity-test-key",3,false,32);port=many.status().port;
    std::vector<std::unique_ptr<Session>> clients;
    for(unsigned i=1;i<32;++i){auto c=std::make_unique<Session>(worldRandomId(),"Trainer "+std::to_string(i));c->join("127.0.0.1",port,"capacity-test-key");clients.push_back(std::move(c));}
    until([&]{return many.status().peers.size()==32&&clients.back()->status().peers.size()==32;});check(clients.back()->status().capacity==32,"32 negotiated room slots");
    Session full(worldRandomId(),"Overflow");full.join("127.0.0.1",port,"capacity-test-key");until([&]{return !full.status().running;});check(many.status().peers.size()==32,"Selected capacity enforced");
    const auto last=clients.back()->id();clients.back()->stop();until([&]{return many.status().peers.size()==31;});
    auto state=many.status();check(std::any_of(state.chat.begin(),state.chat.end(),[&](const auto& m){return m.kind==2&&m.id==last&&m.text=="Trainer 31 left server";}),"Host-authored leave notice");
    many.stop();for(auto& c:clients)c->stop();
    std::cout<<"World isolation, credentials, atomic recovery, host checkpoint transfers, reconnect and 32-client capacity passed. Evidence: "<<root<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
