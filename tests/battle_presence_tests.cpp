#include "online/session.hpp"
#include "game/motion.hpp"
#include "game/battle_staging.hpp"
#include "game/presentation.hpp"
#include <iostream>
#include <thread>
using namespace fr::game;using namespace fr::online;
void check(bool b,const char* message){if(!b)throw std::runtime_error(message);}
template<class F>void until(F fn){for(unsigned i=0;i<600;++i){if(fn())return;std::this_thread::sleep_for(std::chrono::milliseconds(10));}throw std::runtime_error("Battle display networking timed out");}
WorldReport battle(){WorldReport r;r.group=3;r.map=19;r.sequence=1;auto& b=r.battle;b.id=77;b.trainer.active=true;b.trainer.mapGroup=3;b.trainer.mapNumber=19;b.trainer.x=12;b.trainer.y=14;b.trainer.pixelX=192;b.trainer.pixelY=224;b.mons={{0,1,176,208,4,true},{1,19,208,208,3,true}};return r;}
int main(){try{
    for(unsigned revision:{0u,0x14u}){check(battleSceneTransition(0x0800fd9c+revision,revision)&&battleSceneTransition(0x080567dc+revision,revision),"Battle entry/exit must retain 3D");for(auto menu:{0x08107ee0u,0x0811eba0u,0x08137ee8u,0x0800c2d4u})check(!battleSceneTransition(menu+revision,revision),"Battle cache hid a native full-screen menu");}
    auto r=battle();check(validWorldReport(r),"Battle display survives inactive field state");auto invalid=r;invalid.battle.mons.push_back(invalid.battle.mons[0]);check(!validWorldReport(invalid),"Duplicate battle slots rejected");invalid=r;invalid.battle.mons[0].x=900;check(!validWorldReport(invalid),"Distant battle actors rejected");invalid=r;invalid.battle.mons[0].species=0;check(!validWorldReport(invalid),"Visible actor requires a species");invalid.battle.mons[0].visible=false;check(validWorldReport(invalid),"Uninitialized native foe may reserve an invisible position");
    std::set<int> heights;for(unsigned t=0;t<96;++t){const int hop=battleHop(t);check(hop<=0&&hop>=-4,"Battle hop bounds");heights.insert(hop);}check(heights.size()==5,"Battle pose must hop visibly");
    r.battle.mons[0].shiny=true;auto pose=battleMonPose(r.battle,r.battle.mons[0],1);check(pose.followerShiny,"Battle actor must carry its own shiny identity");check(pose.follower==1&&pose.followerFacing==4,"Combat pose carries actual species and facing");r.battle.mons[0].visible=false;check(!battleMonPose(r.battle,r.battle.mons[0],1).active,"Fainted Pokemon stops appearing");r.battle.mons[0].species=4;r.battle.mons[0].visible=true;check(battleMonPose(r.battle,r.battle.mons[0],1).follower==4,"Replacement becomes the battle actor");
    // Keep encounter pixels. Walk a blocking trainer/partner around each
    // other rather than replacing all positions with a preset battle layout.
    auto staged=battle().battle;staged.trainer.pixelX=208;staged.trainer.pixelY=208;staged.trainer.x=staged.trainer.y=13;
    staged.mons[0].x=192;staged.mons[0].y=208;staged.mons[1].x=208;staged.mons[1].y=208;
    const auto initial=staged;BattleStaging staging;auto lawn=[](int x,int y){return x>=10&&x<=16&&y>=10&&y<=16&&!(x==12&&y==12);};
    staging.begin(staged,lawn);check(staged.trainer.pixelX==initial.trainer.pixelX&&staged.mons[0].x==initial.mons[0].x&&staged.mons[1].x==initial.mons[1].x,"Battle entry snapped actor positions");
    bool settled=false;unsigned moving=0;for(unsigned tick=0;tick<512;++tick){const auto old=staged;settled=staging.update(staged);
        check(std::abs(staged.trainer.pixelX-old.trainer.pixelX)+std::abs(staged.trainer.pixelY-old.trainer.pixelY)<=1,"Trainer teleported during staging");
        check(std::abs(staged.mons[0].x-old.mons[0].x)+std::abs(staged.mons[0].y-old.mons[0].y)<=1,"Partner teleported during staging");
        check(staged.mons[1].x==208&&staged.mons[1].y==208,"Wild Pokemon moved from its encounter position");
        check(staged.trainer.pixelX!=staged.mons[0].x||staged.trainer.pixelY!=staged.mons[0].y,"Trainer and partner crossed through each other");
        check(lawn((staged.mons[0].x+8)/16,(staged.mons[0].y+8)/16),"Partner walked into blocked terrain");
        moving+=staged.mons[0].walking||staged.trainer.pixelX!=old.trainer.pixelX||staged.trainer.pixelY!=old.trainer.pixelY;check(validBattle(staged),"Staged battle invalid on wire");if(settled)break;
    }check(settled&&moving&&staged.settled,"Trainer/partner did not finish walking into place");
    staging.returnHome(staged);for(unsigned i=0;i<512&&!staging.update(staged);++i){}
    check(staged.trainer.pixelX==initial.trainer.pixelX&&staged.trainer.pixelY==initial.trainer.pixelY&&staged.mons[0].x==initial.mons[0].x&&staged.mons[0].y==initial.mons[0].y,"Return path did not restore original positions");
    auto already=battle().battle;already.trainer.pixelX=160;already.trainer.pixelY=208;const auto saved=already;staging.begin(already,lawn);check(staging.update(already)&&already.trainer.pixelX==saved.trainer.pixelX&&already.mons[0].x==saved.mons[0].x,"Clear encounter was needlessly rearranged");
    auto subtile=initial;subtile.trainer.pixelX=207;subtile.trainer.pixelY=160;subtile.mons[0].x=192;subtile.mons[0].y=161;subtile.mons[1].x=208;subtile.mons[1].y=160;staging.begin(subtile,[](int x,int y){return x>=10&&x<=16&&y>=7&&y<=13;});
    for(unsigned n=0;n<300;++n){const bool done=staging.update(subtile);check(subtile.trainer.pixelX!=208||subtile.trainer.pixelY!=160,"Sub-tile alignment walked through the opponent");if(done)break;}
    auto diagonal=initial;diagonal.mons[0].x=diagonal.mons[0].y=192;staging.begin(diagonal,[](int x,int y){return x>=10&&x<=16&&y>=10&&y<=16;});unsigned partnerWalk=0;bool diagonalDone=false;
    for(unsigned n=0;n<512;++n){const auto before=diagonal.mons[0];diagonalDone=staging.update(diagonal);partnerWalk+=diagonal.mons[0].walking;check(std::abs(diagonal.mons[0].x-before.x)+std::abs(diagonal.mons[0].y-before.y)<=1,"Diagonal approach snapped partner");if(diagonalDone)break;}
    check(diagonalDone&&partnerWalk>0,"Partner did not walk around the trainer for a diagonal encounter");
    auto cramped=initial;staging.begin(cramped,[](int,int){return false;});check(staging.update(cramped)&&cramped.trainer.pixelX==initial.trainer.pixelX,"Cramped encounter invented a walkable destination");
    Session host(randomId(),"Spectator"),guest(randomId(),"Battler");host.host(0,"battle-view-test");guest.join("127.0.0.1",host.status().port,"battle-view-test");until([&]{return guest.status().connected&&host.status().peers.size()==2;});guest.updateWorld(r);until([&]{return host.status().world.battles[1].id==77;});check(!host.status().peers[1].player.active&&host.status().world.battles[1].trainer.active,"Inactive field avatar retains visible battle trainer");
    Session late(randomId(),"Late spectator");late.join("127.0.0.1",host.status().port,"battle-view-test");until([&]{return late.status().world.battles[1].id==77;});
    r.battle.settled=false;r.battle.mons[0].shiny=true;r.battle.mons[0].walking=true;r.battle.mons[0].frame=2;++r.sequence;guest.updateWorld(r);until([&]{return late.status().world.battles[1].mons[0].walking;});check(late.status().world.battles[1].mons[0].shiny&&host.status().world.battles[1].mons[0].shiny,"Shiny battle actor reaches host and spectator");check(late.status().world.battles[1].mons[0].frame==2&&!late.status().world.battles[1].settled,"Walking phase/frame lost on wire");
    r.battle.mons[0].visible=false;++r.sequence;guest.updateWorld(r);until([&]{return !host.status().world.battles[1].mons[0].visible;});r.battle.mons[0].species=7;r.battle.mons[0].shiny=false;r.battle.mons[0].visible=true;++r.sequence;guest.updateWorld(r);until([&]{return late.status().world.battles[1].mons[0].species==7&&late.status().world.battles[1].mons[0].visible&&!late.status().world.battles[1].mons[0].shiny;});
    r.battle={};r.field=true;++r.sequence;guest.updateWorld(r);until([&]{return !host.status().world.battles[1].id&&!late.status().world.battles[1].id;});r=battle();r.sequence=10;guest.updateWorld(r);until([&]{return host.status().world.battles[1].id==77;});guest.stop();until([&]{return !host.status().world.battles[1].id&&!late.status().world.battles[1].id;});late.stop();host.stop();
    WorldAuthority world;WorldReport wild;wild.field=true;wild.group=3;wild.map=19;wild.wild={{42,19,5,3,12,14,192,224,3,2}};world.report(0,wild,10);world.report(0,wild,20);check(world.snapshot().maps[0].wild[0].facing==3&&world.snapshot().maps[0].wild[0].frame==2,"Wild animation and orientation survive authority");wild.wild[0].frame=4;check(!validWorldReport(wild),"Wild sheet frame bounded");
    std::cout<<"Battle presence: native-semantic poses, switches, fainting, lifecycle and sockets passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
