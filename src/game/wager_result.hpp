#pragma once
#include "online/wager.hpp"
#include <string>
namespace fr::game {
// This describes a committed receipt, never a speculative battle outcome.
// The pot contains the winner's own deposit as well as their net winnings.
inline std::string wagerResultText(const online::Wager& wager,int side){
    if(side<0||side>1||!wager.terminal())return {};
    if(!wager.reserved[side])return "The wager was cancelled. No money was taken.";
    const auto amount="P"+std::to_string(wager.stake);
    if(wager.phase==online::WagerPhase::Refund)return "Your "+amount+" wager was refunded.";
    if(wager.winner==side)return "You won "+amount+"!\nYour "+amount+" stake was returned.";
    return "You lost your "+amount+" wager.";
}
}
