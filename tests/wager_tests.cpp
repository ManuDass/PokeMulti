#include "online/session.hpp"
#include "game/wager_result.hpp"
#include "platform/atomic_file.hpp"
#include <fstream>
#include <future>
#include <iostream>
#include <thread>
using namespace fr::online;
void check(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}
template<class F>void until(F f){for(int i=0;i<500;++i){if(f())return;std::this_thread::sleep_for(std::chrono::milliseconds(10));}throw std::runtime_error("Wager socket timeout");}
Wager sample(){Wager w;w.id=randomId();w.host=randomId();w.players={randomId(),randomId()};w.stake=100;return w;}
void deposit(WagerBook& b,const Wager& w){check(b.event(w.players[0],w.id,1,1)&&b.event(w.players[1],w.id,1,1),"Both deposits saved");}
int main(){try{
 {
  auto result=sample();
  check(fr::game::wagerResultText(result,0).empty(),"No prize announcement before settlement");
  result.phase=WagerPhase::Settled;result.reserved={true,true};result.winner=0;
  check(fr::game::wagerResultText(result,0)=="You won P100!\nYour P100 stake was returned.","Report net winnings separately from the returned stake");
  check(fr::game::wagerResultText(result,1)=="You lost your P100 wager.","Only the losing trainer reports losing their stake");
  result.stake=WagerLimit;result.winner=1;
  check(fr::game::wagerResultText(result,1).find("P499999")!=std::string::npos,"The full agreed amount is preserved");
  check(fr::game::wagerResultText(result,-1).empty(),"Spectators do not receive a personal result");
  result.phase=WagerPhase::Refund;
  check(fr::game::wagerResultText(result,0)=="Your P499999 wager was refunded.","A cancelled or drawn wager is a refund, not winnings");
  result.reserved[1]=false;
  check(fr::game::wagerResultText(result,1)=="The wager was cancelled. No money was taken.","Never claim to refund an unreserved stake");
 }
 const auto folder=std::filesystem::temp_directory_path()/("pokemulti-wager-"+randomId());std::filesystem::create_directories(folder);
 const auto heldFile=folder/"held.cfg";fr::replaceText(heldFile,"old");const auto handle=CreateFileW(heldFile.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);check(handle!=INVALID_HANDLE_VALUE,"Open temporary test reader");
 auto release=std::async(std::launch::async,[&]{std::this_thread::sleep_for(std::chrono::milliseconds(60));CloseHandle(handle);});fr::replaceText(heldFile,"new");release.get();{std::ifstream in(heldFile);std::string text;in>>text;check(text=="new","Account commit tolerates a transient Windows read lock");}
 WagerBook b;b.open(folder/"ledger.cfg");auto w=sample();b.create(w);
 check(!b.event(randomId(),w.id,1,1),"Other trainers cannot authorize a deposit");check(!b.event(w.players[0],w.id,3,1),"No result before native battle");
 check(b.event(w.players[0],w.id,1,1)&&b.event(w.players[0],w.id,1,1),"Duplicate deposit acknowledgement is idempotent");check(b.find(w.id)->phase==WagerPhase::Reserving,"One deposit does not open cable");b.event(w.players[1],w.id,1,1);check(b.find(w.id)->phase==WagerPhase::Ready,"Both deposits enable cable");
 b.event(w.players[0],w.id,2,0);b.event(w.players[1],w.id,2,0);b.event(w.players[0],w.id,3,1);check(!b.find(w.id)->terminal(),"One outcome cannot pay pot");b.event(w.players[1],w.id,3,2);check(b.find(w.id)->credit(0)==200&&b.find(w.id)->credit(1)==0,"Winner receives exactly both deposits");check(!b.event(w.players[1],w.id,3,1),"Result cannot be changed");
 WagerBook recovered;recovered.open(folder/"ledger.cfg");check(recovered.find(w.id)->phase==WagerPhase::Settled,"Restart preserves a committed winner");recovered.cancel(w.players[0]);check(recovered.find(w.id)->phase==WagerPhase::Settled,"Disconnect cannot refund committed winnings");recovered.event(w.players[0],w.id,4,0);recovered.event(w.players[0],w.id,4,0);check(!recovered.forPlayer(w.players[0]).active(),"Paid trainer cannot receive another payout");
 for(const auto results:std::array<std::array<uint8_t,2>,3>{{{3,3},{1,1},{2,2}}}){auto draw=sample();b.create(draw);deposit(b,draw);b.event(draw.players[0],draw.id,2,0);b.event(draw.players[1],draw.id,2,0);b.event(draw.players[0],draw.id,3,results[0]);b.event(draw.players[1],draw.id,3,results[1]);check(b.find(draw.id)->phase==WagerPhase::Refund&&b.find(draw.id)->credit(0)==100&&b.find(draw.id)->credit(1)==100,"Draw/mismatched results refund deposits");}
 auto partial=sample();b.create(partial);b.event(partial.players[0],partial.id,1,1);b.cancel(partial.players[1]);check(b.find(partial.id)->credit(0)==100&&b.find(partial.id)->credit(1)==0,"Disconnect returns only actually saved deposits");b.event(partial.players[1],partial.id,1,1);check(b.find(partial.id)->credit(1)==100,"Late saved deposit receives refund");
 auto restart=sample();b.create(restart);deposit(b,restart);recovered.open(folder/"ledger.cfg");check(recovered.find(restart.id)->phase==WagerPhase::Refund,"Host restart refunds unfinished battle");
 auto excessive=sample();excessive.stake=WagerLimit+1;bool rejected=false;try{b.create(excessive);}catch(...){rejected=true;}check(rejected,"Pot must fit native money limit");
 // Real session transport: terms and consent, deposits, and authenticated outcomes.
 Session host(randomId(),"Host",folder/"host"),guest(randomId(),"Guest",folder/"guest"),observer(randomId(),"Observer");host.host(0,"wager-test-room");guest.join("127.0.0.1",host.status().port,"wager-test-room");observer.join("127.0.0.1",host.status().port,"wager-test-room");until([&]{return host.status().peers.size()==3&&guest.status().connected;});
 host.invite(uint8_t(guest.status().slot),Activity::Battle);until([&]{return guest.status().invitation.from==0;});
 observer.cancelInvitation();std::this_thread::sleep_for(std::chrono::milliseconds(40));check(guest.status().invitation.from==0,"Unrelated trainer cannot cancel another pair's invitation");
 host.cancelInvitation();until([&]{return guest.status().invitation.from<0;});check(!host.cableConnected()&&!guest.status().wager.active(),"Cancelled invitation never starts or charges a battle");
 host.updateWallet(3000,true);guest.updateWallet(1000,true);std::this_thread::sleep_for(std::chrono::milliseconds(100));host.invite(uint8_t(guest.status().slot),Activity::Battle,100);until([&]{return guest.status().invitation.from==0;});check(guest.status().invitation.stake==100&&guest.status().invitation.nonce!=0,"Recipient sees exact amount and invitation identity");check(!host.status().wager.active()&&!host.cableConnected(),"Invitation alone does not charge or connect");guest.reply(true);until([&]{return host.status().wager.active()&&guest.status().wager.active();});
 const auto id=host.status().wager.id;check(!host.cableConnected(),"Consent still waits for saved deposits");observer.wagerEvent(id,1,1);host.wagerEvent(id,1,1);until([&]{return guest.status().wager.reserved[0];});check(!host.cableConnected()&&!host.status().wager.reserved[1],"Spectator cannot supply other player's deposit");guest.wagerEvent(id,1,1);until([&]{return host.cableConnected()&&guest.cableConnected();});
 auto stage=[&](Session& session,int slot){for(const auto& p:session.status().peers)if(p.slot==slot)return int(p.battleStage);return -1;};
 for(const auto& p:host.status().peers)if(p.partner>=0)check(p.activity==Activity::Battle&&p.battleStage==0,"Wager pairs start as unprepared field battles");
 host.battleStage(2);observer.battleStage(1);std::this_thread::sleep_for(std::chrono::milliseconds(60));check(stage(host,0)==0&&stage(host,observer.status().slot)==0,"Cannot skip readiness or ready an unrelated player");
 host.battleStage(1);guest.battleStage(1);until([&]{return stage(host,guest.status().slot)==1&&stage(guest,0)==1;});
 host.battleStage(2);guest.battleStage(2);until([&]{return stage(host,guest.status().slot)==2&&stage(guest,0)==2;});
 host.battleStage(1);std::this_thread::sleep_for(std::chrono::milliseconds(40));check(stage(host,0)==2,"Readiness cannot go backwards during a battle");
 host.wagerEvent(id,2);guest.wagerEvent(id,2);until([&]{return host.status().wager.phase==WagerPhase::Battling;});host.wagerEvent(id,3,1);guest.wagerEvent(id,3,2);until([&]{return guest.status().wager.phase==WagerPhase::Settled;});check(guest.status().wager.credit(0)==200,"Matching native results produce conserved payout");host.wagerEvent(id,4);guest.wagerEvent(id,4);until([&]{return !host.status().wager.active()&&!guest.status().wager.active();});observer.stop();guest.stop();host.stop();
 std::filesystem::remove_all(folder);std::cout<<"Agreed terms, private wallets, saved deposits, outcomes, refunds, restart recovery and no duplicate payouts passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
