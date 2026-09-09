#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace fr::online {
constexpr uint32_t MoneyLimit=999999,WagerLimit=499999;
enum class WagerPhase:uint8_t {None=0,Reserving=1,Ready=2,Battling=3,Refund=4,Settled=5};
struct Wager {
    std::string id,host;
    std::array<std::string,2> players{};
    uint32_t stake=0;
    WagerPhase phase=WagerPhase::None;
    std::array<bool,2> reserved{},began{},paid{};
    std::array<uint8_t,2> outcome{};
    int winner=-1;
    int side(const std::string& player)const{return players[0]==player?0:players[1]==player?1:-1;}
    bool terminal()const{return phase==WagerPhase::Refund||phase==WagerPhase::Settled;}
    bool active()const{return !id.empty();}
    uint32_t credit(int side)const{return side<0||side>1||!reserved[side]?0:phase==WagerPhase::Refund?stake:phase==WagerPhase::Settled&&winner==side?stake*2:0;}
};
class WagerBook {
    std::filesystem::path path;
    std::vector<Wager> records;
    void persist()const;
public:
    void open(const std::filesystem::path& file);
    Wager forPlayer(const std::string& player)const;
    const Wager* find(const std::string& id)const;
    bool busy(const std::string& player)const;
    void create(Wager wager);
    // Events: 1 deposit saved, 2 native battle started, 3 native outcome,
    // 4 payout/refund saved, 5 cancellation. Sender is authenticated by Session.
    bool event(const std::string& player,const std::string& id,uint8_t event,uint8_t value);
    void cancel(const std::string& player);
};
}
