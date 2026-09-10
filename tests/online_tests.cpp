#include "online/session.hpp"
#include <chrono>
#include <algorithm>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace fr::online;
std::vector<ChatMessage> chat(const Session& s){auto rows=s.status().chat;std::erase_if(rows,[](const auto& m){return m.kind!=0;});return rows;}
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
template<class F>void until(F fn){for(int i=0;i<500;++i){if(fn())return;std::this_thread::sleep_for(std::chrono::milliseconds(10));}throw std::runtime_error("Room operation timed out");}
int main(){
    try{
        check(validIdentity(randomId(),"Trainer"),"Generated ID");
        check(!validIdentity("bad","Trainer"),"Invalid ID");
        Session host(randomId(),"Host"),one(randomId(),"One"),two(randomId(),"Two"),three(randomId(),"Three");
        host.host(0,"test-room-123");
        const auto port=host.status().port;check(port!=0,"Ephemeral listen port");
        one.join("127.0.0.1",port,"test-room-123");two.join("127.0.0.1",port,"test-room-123");three.join("127.0.0.1",port,"test-room-123");
        until([&]{return three.status().peers.size()==4&&host.status().peers.size()==4;});
        for(const auto& invalid:std::vector<std::string>{"", "   ", "line\nbreak", std::string(129,'a'), std::string("\xc0\xaf",2)})check(!validChat(invalid),"Invalid chat accepted");
        const std::string greeting="Hello, Pok\xc3\xa9mon!";
        one.sendChat(greeting);two.sendChat(std::string(128,'w'));host.sendChat("Welcome!");
        until([&]{return chat(host).size()==3&&chat(one).size()==3&&chat(two).size()==3&&chat(three).size()==3;});
        const auto messages=chat(host);
        for(const auto* client:{&one,&two,&three}){
            const auto received=chat(*client);
            for(size_t i=0;i<messages.size();++i)check(received[i].sequence==messages[i].sequence&&received[i].id==messages[i].id&&received[i].text==messages[i].text,"Chat order and echo must match every client");
        }
        const auto hello=std::find_if(messages.begin(),messages.end(),[&](const ChatMessage& m){return m.text==greeting;});
        check(hello!=messages.end()&&hello->id==one.id()&&hello->name=="One"&&hello->slot==one.status().slot&&hello->receivedAt>0,"Chat attribution must use authenticated identity");
        bool limited=false;try{one.sendChat("too fast");}catch(const std::exception&){limited=true;}check(limited,"Chat rate limit");
        fr::game::PlayerState p;p.active=true;p.x=17;p.y=12;p.mapGroup=3;p.mapNumber=1;p.elevation=3;p.follower=25;p.pixelX=273;p.pixelY=193;p.spriteFrame=7;p.flip=1;p.sequence=42;p.sampleTime=234567;p.followerFacing=3;p.followerFrame=2;p.partyCount=6;p.partyEggs=0x22;p.followerShiny=true;p.followerToken=123456;p.followerEmote=2;p.followerEmoteSequence=5;
        two.update(p);const int twoSlot=two.status().slot;
        until([&]{for(const auto& peer:one.status().peers)if(peer.slot==twoSlot&&peer.player.x==17&&peer.player.follower==25&&peer.player.pixelX==273&&peer.player.spriteFrame==7&&peer.player.flip==1&&peer.player.sequence==42&&peer.player.sampleTime==234567&&peer.player.followerFacing==3&&peer.player.followerFrame==2&&peer.player.partyCount==6&&peer.player.partyEggs==0x22&&peer.player.followerShiny&&peer.player.followerToken==123456&&peer.player.followerEmote==2&&peer.player.followerEmoteSequence==5)return true;return false;});
        for(unsigned count:{1u,0u,3u,6u}){
            p.followerShiny=(count%2)==0;p.partyCount=uint8_t(count);p.partyEggs=count?uint8_t(1u<<(count-1)):0;++p.sequence;two.update(p);
            until([&]{for(auto* client:{&host,&one,&three}){
                const auto state=client->status();const auto found=std::find_if(state.peers.begin(),state.peers.end(),[&](const auto& peer){return peer.slot==twoSlot;});
                if(found==state.peers.end()||found->player.sequence!=p.sequence||found->player.partyCount!=count||found->player.partyEggs!=p.partyEggs||found->player.followerShiny!=p.followerShiny)return false;
            }return true;});
        }
        host.invite(uint8_t(one.status().slot),Activity::Battle);
        until([&]{return one.status().invitation.from==0;});
        one.reply(false);
        until([&]{return host.status().message.find("declined")!=std::string::npos;});
        host.invite(uint8_t(one.status().slot),Activity::Trade);
        until([&]{return one.status().invitation.from==0;});one.reply(true);
        until([&]{for(const auto& peer:one.status().peers)if(peer.slot==one.status().slot&&peer.partner==0)return true;return false;});
        host.cableControl(0x6003);one.cableControl(0x6003);
        until([&]{return (host.cableControl(0x6003)&8)!=0&&(one.cableControl(0x6003)&8)!=0;});
        check((host.cableControl(0x6003)&0x34)==0,"Master cable role");
        check((one.cableControl(0x6003)&0x34)==0x14,"Slave cable role");
        {
            host.cableControl(0x2003);one.cableControl(0x2003);
            // Wait for both asynchronous port-mode snapshots before starting
            // the polled transfer; a still-in-flight reset legitimately cancels it.
            until([&]{for(auto* s:{&host,&one})for(const auto& p:s->status().peers)if(p.partner>=0&&p.cableClock)return false;return true;});
            std::array<uint16_t,4> a{},b{};uint32_t interval=0;
            auto master=std::async(std::launch::async,[&]{return host.cableStartTimed(0x7654,0,a);});
            until([&]{return one.cableTryRequest(interval);});
            check(one.cableRespond(0x3210,b)&&master.get()&&a==b,"polled serial exchange does not require IRQ enable");
            host.cableControl(0x6003);one.cableControl(0x6003);
        }
        for(unsigned n=0;n<32;++n){
            std::array<uint16_t,4> a{},b{};
            auto transfer=std::async(std::launch::async,[&]{return host.cableStart(uint16_t(0x1200+n),a);});
            until([&]{return one.cablePoll(uint16_t(0x3400+n),b);});
            check(transfer.get(),"Serial exchange");
            check(a==b&&a[0]==0x1200+n&&a[1]==0x3400+n&&a[2]==0xffff,"Serial words and disconnected slots");
        }
        {
            std::array<uint16_t,4> a{},b{};
            auto waiting=std::async(std::launch::async,[&]{return one.cableAwait(0x9876,b);});
            check(waiting.wait_for(std::chrono::milliseconds(40))==std::future_status::timeout,"Slave must wait at the send-word boundary");
            three.stop();check(three.status().peers.empty()&&!three.status().hosting&&three.status().invitation.from<0,"Leaving must clear friend presence and invitations");until([&]{return host.status().peers.size()==3;});
            check(waiting.wait_for(std::chrono::milliseconds(40))==std::future_status::timeout,"Unrelated disconnect must preserve the cable");
            check(host.cableStart(0x5432,a)&&waiting.get()&&a==b&&a[1]==0x9876,"Synchronized exchange preserves the fresh slave word");
        }
        {
            std::array<uint16_t,4> a{},b{};uint32_t interval=0;
            auto master=std::async(std::launch::async,[&]{return host.cableStartTimed(0x1357,19000,a);});
            check(one.cableRequest(interval)&&interval==19000,"wire preserves master transfer interval");
            check(master.wait_for(std::chrono::milliseconds(30))==std::future_status::timeout,"master must wait until slave reaches the scheduled transfer");
            check(one.cableRespond(0x2468,b)&&master.get()&&a==b&&b[1]==0x2468,"scheduled exchange captures the slave's current word");
        }
        {
            constexpr unsigned count=1024;
            const auto begin=std::chrono::steady_clock::now();
            auto slave=std::async(std::launch::async,[&]{
                for(unsigned n=0;n<count;++n){uint32_t interval=0;std::array<uint16_t,4> words{};
                    check(one.cableRequest(interval)&&interval==19000,"continuous serial clock");
                    check(one.cableRespond(uint16_t(n^0xaaaa),words)&&words[0]==n,"continuous fresh serial words");
                }
            });
            for(unsigned n=0;n<count;++n){std::array<uint16_t,4> words{};
                check(host.cableStartTimed(uint16_t(n),19000,words)&&words[1]==uint16_t(n^0xaaaa),"continuous transfer reply order");
            }
            slave.get();
            std::cout<<"1024 clocked loopback transfers: "<<std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count()<<" ms\n";
        }
        {
            host.cableControl(0x6003);one.cableControl(0x6003);
            until([&]{return host.cableClockReady();});
            std::array<uint16_t,4> words{};uint32_t interval=0;
            auto pending=std::async(std::launch::async,[&]{return host.cableStartTimed(0x55aa,19000,words);});
            check(one.cableRequest(interval),"reset test has a pending transfer");
            one.cableControl(0x2003);
            check(pending.wait_for(std::chrono::milliseconds(500))==std::future_status::ready&&!pending.get(),"port reset cancels an in-flight transfer without stalling the guest for 15 seconds");
            one.cableControl(0x6003);
        }
        auto canceled=std::async(std::launch::async,[&]{std::array<uint16_t,4> words{};return one.cableAwait(0,words);});
        one.disconnectCable();
        until([&]{for(const auto& peer:host.status().peers)if(peer.slot==0&&peer.partner<0)return true;return false;});
        three.stop();check(three.status().peers.empty()&&!three.status().hosting&&three.status().invitation.from<0,"Leaving must clear friend presence and invitations");until([&]{return host.status().peers.size()==3;});
        check(canceled.wait_for(std::chrono::seconds(1))==std::future_status::ready&&!canceled.get(),"Disconnect must release a waiting slave promptly");
        Session bad(randomId(),"Wrong key");bad.join("127.0.0.1",port,"wrong-key-123");
        until([&]{return !bad.status().running;});check(!bad.status().connected,"Wrong key accepted");
        host.stop();check(host.status().peers.empty()&&chat(host).empty()&&!host.status().hosting,"Host stop must clear room presence");until([&]{return !one.status().connected&&!two.status().connected;});
        std::cout<<"Four-player room, identity, movement, consent, 32 serial exchanges, disconnect and wrong-key checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
