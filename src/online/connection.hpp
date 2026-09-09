#pragma once
#include <cstdint>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <string_view>
namespace fr::online {
bool validConnectionIPv4(std::string_view address,bool internet=false);
struct ConnectionInvite {std::string address,key;uint16_t port=0;};
std::optional<ConnectionInvite> parseConnectionInvite(std::string_view text);
std::string connectionInvite(const ConnectionInvite& invite,bool internet=false);
std::string discoverPublicIPv4();
// UI-owned lookup. A stale room/refresh result cannot replace current state.
class PublicAddressLookup {
public:
 enum class State {Idle,Looking,Ready,Unavailable};
 explicit PublicAddressLookup(std::function<std::string()> fetch=discoverPublicIPv4):fetch_(std::move(fetch)){}
 void tick(bool hosting,uint16_t port);
 void refresh();
 State state()const{return state_;}
 const std::string& address()const{return address_;}
private:
 struct Result {uint64_t generation;std::string address;};
 std::function<std::string()> fetch_;
 std::future<Result> pending_;
 uint64_t generation_=0;
 uint16_t port_=0;
 bool active_=false,wanted_=false;
 State state_=State::Idle;
 std::string address_;
};
}
