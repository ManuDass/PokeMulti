#include "online/connection.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
using namespace fr::online;
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
template<class F>void until(F fn){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);while(!fn()){if(std::chrono::steady_clock::now()>end)throw std::runtime_error("Lookup state timed out");std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
int main(int argc,char** argv){try{
 if(argc==2&&std::string(argv[1])=="--live"){check(validConnectionIPv4(discoverPublicIPv4(),true),"Live public address is invalid");std::cout<<"PASS: live HTTPS lookup returned a public IPv4 (address omitted)\n";return 0;}
 for(const auto* ip:{"1.1.1.1","8.8.8.8","192.0.0.9","192.0.0.10"})check(validConnectionIPv4(ip,true),"Public address rejected");
 for(const auto* ip:{"10.0.0.1","172.16.0.1","172.31.255.255","192.168.1.4","127.0.0.1","169.254.1.2","100.64.0.1","100.127.255.254","192.0.0.8","192.0.2.1","198.18.0.1","198.19.255.254","198.51.100.1","203.0.113.1"})check(validConnectionIPv4(ip)&&!validConnectionIPv4(ip,true),"Non-public address became Internet invite");
 for(const auto* ip:{"","0.0.0.0","255.255.255.255","224.0.0.1","256.1.1.1","1.2.3","1.2.3.4.5","01.2.3.4","1.2.3.-1","1.2.3.4\n","<html>","::1","1.1.1.1, 8.8.8.8"})check(!validConnectionIPv4(ip),"Malformed/non-unicast address accepted");
 for(const auto* ip:{"1.1.1.1","127.0.0.1","192.168.1.2"}){
  ConnectionInvite original{ip," Room: Key with spaces ",65535};auto text=connectionInvite(original);auto parsed=parseConnectionInvite(text+"\r\n");
  check(parsed&&parsed->address==original.address&&parsed->key==original.key&&parsed->port==original.port,"Invite round trip changed address, key or port");
 }
 check(connectionInvite({"192.168.1.2","test-room-key",38475},true).empty(),"Internet copy fell back to LAN");
 check(!connectionInvite({"8.8.8.8","test-room-key",45678},true).empty(),"Custom public port rejected");
 for(const auto* text:{"1.1.1.1:0  Key: password","1.1.1.1:65536  Key: password","1.1.1.1:-1  Key: password","1.1.1.1:123junk  Key: password","1.1.1.1:123  Key: short","1.1.1.1:123  Key: password\nnew-line","not an invitation"})check(!parseConnectionInvite(text),"Malformed invitation accepted");
 PublicAddressLookup immediate([]{return std::string("1.1.1.1");});immediate.tick(false,0);check(immediate.state()==PublicAddressLookup::State::Idle,"Joiner started lookup");
 immediate.tick(true,38475);until([&]{immediate.tick(true,38475);return immediate.state()==PublicAddressLookup::State::Ready;});check(immediate.address()=="1.1.1.1","Public lookup lost result");
 immediate.tick(false,0);check(immediate.state()==PublicAddressLookup::State::Idle&&immediate.address().empty(),"Leaving retained public invite");
 std::atomic_int attempts=0;PublicAddressLookup retry([&]{if(++attempts==1)throw std::runtime_error("offline");return std::string("8.8.8.8");});
 retry.tick(true,38475);until([&]{retry.tick(true,38475);return retry.state()==PublicAddressLookup::State::Unavailable;});check(retry.address().empty(),"Failed lookup exposed an address");
 retry.refresh();until([&]{retry.tick(true,38475);return retry.state()==PublicAddressLookup::State::Ready;});check(retry.address()=="8.8.8.8","Refresh did not recover");
 for(const auto* bad:{"192.168.1.1","100.64.1.1","<html>error</html>"}){PublicAddressLookup rejected([bad]{return std::string(bad);});rejected.tick(true,1);until([&]{rejected.tick(true,1);return rejected.state()==PublicAddressLookup::State::Unavailable;});check(rejected.address().empty(),"Provider response advertised private/invalid address");}
 auto gate=std::make_shared<std::promise<std::string>>();auto blocked=gate->get_future().share();std::atomic_int calls=0;
 PublicAddressLookup changed([&]{if(++calls==1)return blocked.get();return std::string("8.8.8.8");});changed.tick(true,1000);until([&]{return calls==1;});changed.tick(false,0);changed.tick(true,2000);gate->set_value("1.1.1.1");
 until([&]{changed.tick(true,2000);return changed.state()==PublicAddressLookup::State::Ready;});check(calls==2&&changed.address()=="8.8.8.8","Previous room result replaced new room lookup");
 std::cout<<"PASS: public/private IPv4, invite parsing, async lookup failure/retry and stale-room isolation\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}return 0;}
