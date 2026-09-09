#include <winsock2.h>
#include <ws2tcpip.h>
#include "online/session.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace fr::online;
std::vector<ChatMessage> chat(const Session& s){auto rows=s.status().chat;std::erase_if(rows,[](const auto& m){return m.kind!=0;});return rows;}
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
template<class F>void until(F fn){for(int i=0;i<400;++i){if(fn())return;std::this_thread::sleep_for(std::chrono::milliseconds(10));}throw std::runtime_error("Chat operation timed out");}
struct Wire {
 SOCKET socket=INVALID_SOCKET;
 Wire(uint16_t port){socket=::socket(AF_INET,SOCK_STREAM,0);sockaddr_in a{};a.sin_family=AF_INET;a.sin_port=htons(port);inet_pton(AF_INET,"127.0.0.1",&a.sin_addr);check(connect(socket,reinterpret_cast<sockaddr*>(&a),sizeof(a))==0,"Connect raw client");}
 ~Wire(){if(socket!=INVALID_SOCKET)closesocket(socket);}
 static std::string text(const std::string& s){return std::string(1,char(s.size()))+s;}
 void packet(unsigned type,std::string body){std::string b="FRMP";b+=char(RoomProtocolVersion);b+=char(type);b+=char(body.size()&255);b+=char(body.size()>>8);b+=body;check(send(socket,b.data(),int(b.size()),0)==int(b.size()),"Send raw packet");}
 void hello(){packet(1,text("chat-test-123")+text(randomId())+text("Wire")+text("")+text(""));}
};
int main(){try{
 Session host(randomId(),"Host"),one(randomId(),"One"),two(randomId(),"Two"),three(randomId(),"Three");
 host.host(0,"chat-test-123");const auto port=host.status().port;
 {
  Wire wire(port);wire.hello();until([&]{return host.status().peers.size()==2;});
  for(int i=0;i<10;++i)wire.packet(13,Wire::text("burst"));
  until([&]{return chat(host).size()==1;});std::this_thread::sleep_for(std::chrono::milliseconds(150));
  check(chat(host).size()==1,"Host must enforce rate limit for untrusted clients");
  check(chat(host)[0].name=="Wire"&&chat(host)[0].slot==1,"Host must supply chat author");
  wire.packet(14,{});until([&]{return host.status().peers.size()==1;});
  check(chat(host).size()==1,"Client must not inject a server broadcast");
 }
 {
  Wire wire(port);wire.hello();until([&]{return host.status().peers.size()==2;});
  std::string pose(37,0);pose[4]=1;pose[25]=1;pose[35]=7;
  wire.packet(3,pose);until([&]{return host.status().peers.size()==1;});
  check(host.status().running,"Invalid party counts must drop the sender, not the room");
 }
 one.join("127.0.0.1",port,"chat-test-123");two.join("127.0.0.1",port,"chat-test-123");three.join("127.0.0.1",port,"chat-test-123");
 until([&]{return three.status().peers.size()==4;});check(chat(one).empty(),"New arrivals must not replay expired speech");
 for(unsigned batch=0;batch<26;++batch){
  for(auto* s:{&host,&one,&two,&three})s->sendChat("Batch "+std::to_string(batch));
  std::this_thread::sleep_for(std::chrono::milliseconds(530));
 }
 until([&]{return chat(three).size()==ChatHistoryLimit&&chat(one).back().sequence==chat(host).back().sequence;});
 const auto expected=chat(host);
 check(expected.size()==ChatHistoryLimit&&expected.front().sequence+99==expected.back().sequence,"Room history must evict oldest entries");
 for(auto* s:{&one,&two,&three}){const auto actual=chat(*s);
  check(actual.size()==expected.size(),"History bounded on each receiver");
  for(size_t i=0;i<actual.size();++i)check(actual[i].sequence==expected[i].sequence&&actual[i].id==expected[i].id&&actual[i].text==expected[i].text,"Shared ordered chat history diverged");
 }
 one.stop();check(chat(one).empty(),"Leaving must clear room history");
 until([&]{return host.status().peers.size()==3;});one.join("127.0.0.1",port,"chat-test-123");until([&]{return one.status().peers.size()==4;});
 check(chat(one).empty(),"Rejoin must not resurrect speech");
 for(const auto& p:host.status().peers)if(p.id==one.id())check(p.chatAfter>=expected.back().sequence,"Rejoining presence must not inherit its previous overhead message");
 one.sendChat("Back again");until([&]{return chat(one).size()==1&&chat(host).back().text=="Back again";});
 host.stop();until([&]{return !one.status().running;});check(chat(one).empty(),"Host loss must clear speech");
 std::cout<<"Chat author validation, forged packet rejection, host rate limit, 100-message history, join/leave and reconnect passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
