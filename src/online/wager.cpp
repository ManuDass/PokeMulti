#include "online/wager.hpp"
#include "platform/atomic_file.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
namespace fr::online {
void WagerBook::persist(bool force)const{
    if(path.empty()||(manual&&!force))return;std::ostringstream out;out<<"PMW1\n";
    for(const auto& w:records){out<<w.id<<' '<<w.host<<' '<<w.players[0]<<' '<<w.players[1]<<' '<<w.stake<<' '<<unsigned(w.phase)<<' '<<w.winner;
        for(int i=0;i<2;++i)out<<' '<<w.reserved[i]<<' '<<w.began[i]<<' '<<w.paid[i]<<' '<<unsigned(w.outcome[i]);out<<'\n';}
    fr::replaceText(path,out.str());
}
void WagerBook::open(const std::filesystem::path& file){
    path=file;records.clear();if(path.empty()||!std::filesystem::exists(path))return;
    std::ifstream in(path);std::string magic;in>>magic;if(magic!="PMW1")throw std::runtime_error("Invalid wager history");
    Wager w;
    while(in>>w.id){unsigned phase=0;in>>w.host>>w.players[0]>>w.players[1]>>w.stake>>phase>>w.winner;
        for(int i=0;i<2;++i){unsigned result=0;in>>w.reserved[i]>>w.began[i]>>w.paid[i]>>result;if(result>3)throw std::runtime_error("Invalid saved battle outcome");w.outcome[i]=uint8_t(result);}
        if(!in||w.id.size()!=32||w.host.size()!=32||w.players[0].size()!=32||w.players[1].size()!=32||w.players[0]==w.players[1]||!w.stake||w.stake>WagerLimit||phase<1||phase>5||w.winner< -1||w.winner>1||records.size()>=1024)throw std::runtime_error("Invalid wager history entry");
        w.phase=WagerPhase(phase);if(!w.terminal())w.phase=WagerPhase::Refund;
        records.push_back(w);
    }
    in.close();
    persist(); // Restart cancels incomplete battles, retaining committed payouts.
}
Wager WagerBook::forPlayer(const std::string& player)const{for(const auto& w:records){const int side=w.side(player);if(side>=0&&!w.paid[side])return w;}return {};}
const Wager* WagerBook::find(const std::string& id)const{for(const auto& w:records)if(w.id==id)return &w;return nullptr;}
bool WagerBook::busy(const std::string& player)const{return forPlayer(player).active();}
void WagerBook::create(Wager w){
    if(!w.stake||w.stake>WagerLimit||w.id.size()!=32||w.host.size()!=32||w.players[0]==w.players[1]||busy(w.players[0])||busy(w.players[1])||find(w.id))throw std::runtime_error("A trainer has an unfinished wager");
    std::erase_if(records,[](const auto& old){return old.paid[0]&&old.paid[1];});
    if(records.size()>=1024)throw std::runtime_error("Settle existing wagers before creating another");
    w.phase=WagerPhase::Reserving;records.push_back(std::move(w));persist();
}
bool WagerBook::event(const std::string& player,const std::string& id,uint8_t event,uint8_t value){
    auto it=std::find_if(records.begin(),records.end(),[&](const auto& w){return w.id==id;});if(it==records.end())return false;
    auto& w=*it;const int side=w.side(player);if(side<0)return false;
    if(event==1){
        if(value>1||w.paid[side]||(w.phase!=WagerPhase::Reserving&&w.phase!=WagerPhase::Refund))return false;
        if(value)w.reserved[side]=true;else if(!w.reserved[side])w.phase=WagerPhase::Refund;
        if(w.phase==WagerPhase::Reserving&&w.reserved[0]&&w.reserved[1])w.phase=WagerPhase::Ready;
    }else if(event==2){
        if(value||w.terminal()||!w.reserved[0]||!w.reserved[1])return false;
        w.began[side]=true;if(w.began[0]&&w.began[1])w.phase=WagerPhase::Battling;
    }else if(event==3){
        if(value<1||value>3||w.terminal()||!w.began[side]||w.outcome[side])return false;
        w.outcome[side]=value;
        if(w.outcome[0]&&w.outcome[1]){
            if(w.began[0]&&w.began[1]&&((w.outcome[0]==1&&w.outcome[1]==2)||(w.outcome[0]==2&&w.outcome[1]==1))){w.phase=WagerPhase::Settled;w.winner=w.outcome[0]==1?0:1;}
            else w.phase=WagerPhase::Refund;
        }
    }else if(event==4){if(value||!w.terminal())return false;w.paid[side]=true;
    }else if(event==5){if(value||w.terminal())return false;w.phase=WagerPhase::Refund;
    }else return false;
    persist();return true;
}
void WagerBook::cancel(const std::string& player){bool changed=false;for(auto& w:records)if(w.side(player)>=0&&!w.terminal()){w.phase=WagerPhase::Refund;changed=true;}if(changed)persist();}
}
