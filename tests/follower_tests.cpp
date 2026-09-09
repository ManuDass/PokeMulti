#include "game/follower.hpp"
#include "game/follower_art.hpp"
#include "game/motion.hpp"
#include <iostream>
#include <stdexcept>
using namespace fr::game;
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){try{
    FollowerPath path;PlayerState p;p.active=true;p.facing=4;p.pixelX=32;p.pixelY=64;
    path.update(p,true);check(!p.followerVisible,"Follower spawned on trainer");
    for(int x=33;x<=80;++x){p.pixelX=int16_t(x);path.update(p,true);if(x>48)check(p.followerFacing==4&&p.followerX==x-16,"Follower walked backwards going right");}
    int ox=p.followerX,oy=p.followerY;bool oldDirection=false,newDirection=false;
    p.facing=2;
    for(int y=63;y>=32;--y){p.pixelY=int16_t(y);path.update(p,true);const int dx=p.followerX-ox,dy=p.followerY-oy;
        if(dx){check(p.followerFacing==4,"Trainer's early turn rotated a follower still walking right");oldDirection=true;}
        if(dy){check(p.followerFacing==2,"Follower failed to face up after reaching corner");newDirection=true;}ox=p.followerX;oy=p.followerY;}
    check(oldDirection&&newDirection,"Did not exercise corner lag");
    p.facing=1;for(int y=33;y<=64;++y){p.pixelY=int16_t(y);path.update(p,true);const int dy=p.followerY-oy;if(dy)check(p.followerFacing==(dy>0?1:2),"Follower moonwalked during reversal");oy=p.followerY;}
    for(int i=0;i<3;++i)path.update(p,true);check(p.followerFrame==0,"Idle follower keeps walking");p.facing=3;path.update(p,true);check(p.followerFacing==3,"Idle follower did not reorient");
    p.pixelX=300;path.update(p,true);check(!p.followerVisible,"Follower interpolated across warp");
    check(followerRow(1)==0&&followerRow(2)==3&&followerRow(3)==1&&followerRow(4)==2,"Sheet direction rows are wrong");
    MotionTimeline timeline;PlayerState a;a.active=true;a.identity=1;a.followerVisible=true;a.followerX=30;a.followerY=50;a.followerFacing=4;a.sequence=1;a.sampleTime=100;
    auto b=a;b.sequence=2;b.sampleTime=120;b.followerY=48;b.followerFacing=2;timeline.push(a,1000);timeline.push(b,1020);
    check(timeline.sample(1090).followerY==49&&timeline.sample(1090).followerFacing==2,"Network interpolation walked backward through turn");
    // Owner-supplied source art is optional for portable tests. Verify lossless
    // native-pixel recovery against every source texel whenever it is present.
    const auto file=fr::localAsset("Following Pokemon EX/Graphics/Characters/Followers/BULBASAUR.png");
    if(!file.empty()){const auto original=fr::readImage(file);const auto sheet=decodeFollowerSheet(original);check(sheet.cellW==32&&sheet.cellH==32,"Follower sheet native dimensions");
        for(int y=0;y<original.height;++y)for(int x=0;x<original.width;++x)check(original.pixels[y*original.width+x]==sheet.image.pixels[(y/2)*sheet.image.width+x/2],"Changed an original follower texel");}
    std::cout<<"Follower directions, corners, reversals, idle, warps, network playback and supplied sheet pixels passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
