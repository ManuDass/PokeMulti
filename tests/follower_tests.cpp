#include "game/follower.hpp"
#include "game/follower_effect.hpp"
#include "game/follower_art.hpp"
#include "game/motion.hpp"
#include <iostream>
#include <stdexcept>
using namespace fr::game;
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){try{
    FollowerPath path;PlayerState p;p.active=true;p.facing=4;p.pixelX=32;p.pixelY=64;
    path.seed(p,16,64);path.update(p,true);check(p.followerVisible&&p.followerX==16&&p.followerY==64,"Follower must appear beside the stationary trainer");
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
    path.seed(p,284,64);path.update(p,true);auto inactive=p;inactive.active=false;path.update(inactive,true);path.translate(-256,16);p.pixelX-=256;p.pixelY+=16;path.update(p,true);check(p.followerVisible&&p.followerX==28&&p.followerY==80,"Connected map transition lost follower history");
    FollowerVisual visual;p.follower=1;p.followerToken=11;visual.update(p,true,0);check(!visual.sprite(0),"Send-out starts with supplied ball");visual.update(p,true,30);check(visual.sprite(30),"Send-out reveals follower");visual.update(p,false,31);check(visual.phase==FollowerVisual::Leaving&&visual.sprite(31),"Recall must animate old sprite");visual.update(p,false,61);check(visual.phase==FollowerVisual::Hidden,"Recall must finish");visual.update(p,true,62);visual.update(p,true,92);p.followerToken=12;visual.update(p,true,93);check(visual.phase==FollowerVisual::Leaving,"Same-species party switch must recall old individual");visual.update(p,true,123);check(visual.phase==FollowerVisual::Arriving&&visual.pose.followerToken==12,"Party switch must send new individual");
    check(followerBallPattern(false,4)==1&&followerBallPattern(true,1)==5&&followerEmotePattern(1,4)==8&&followerEmotePattern(2,24)==-1,"Original supplied animation sequence");
    for(unsigned frame=1;frame<10;++frame){
        const int send=followerBallPattern(false,frame),recall=followerBallPattern(true,frame);
        check(send>=0&&send<=3,"Send-out must use only the four populated top-row cells");
        check(recall>=5&&recall<=7,"Return must use only the three populated bottom-row cells");
    }
    check(followerBallPattern(false,10)==-1&&followerBallPattern(true,10)==-1,"Finished effects must leave no animation cell");
    check(followerRow(1)==0&&followerRow(2)==3&&followerRow(3)==1&&followerRow(4)==2,"Sheet direction rows are wrong");
    MotionTimeline timeline;PlayerState a;a.active=true;a.identity=1;a.followerVisible=true;a.followerX=30;a.followerY=50;a.followerFacing=4;a.sequence=1;a.sampleTime=100;
    auto b=a;b.sequence=2;b.sampleTime=120;b.followerY=48;b.followerFacing=2;timeline.push(a,1000);timeline.push(b,1020);
    check(timeline.sample(1090).followerY==49&&timeline.sample(1090).followerFacing==2,"Network interpolation walked backward through turn");
    // Owner-supplied source art is optional for portable tests. Verify lossless
    // native-pixel recovery against every source texel whenever it is present.
    const auto file=fr::localAsset("Following Pokemon EX/Graphics/Characters/Followers/BULBASAUR.png");
    if(!file.empty()){const auto original=fr::readImage(file);const auto sheet=decodeFollowerSheet(original);check(sheet.cellW==32&&sheet.cellH==32,"Follower sheet native dimensions");
        for(int y=0;y<original.height;++y)for(int x=0;x<original.width;++x)check(original.pixels[y*original.width+x]==sheet.image.pixels[(y/2)*sheet.image.width+x/2],"Changed an original follower texel");}
    const auto normalFile=fr::localAsset("LocalAssets/Followers/PONYTA.png"),shinyFile=fr::localAsset("LocalAssets/Followers shiny/PONYTA.png");
    check(!normalFile.empty()&&!shinyFile.empty(),"Both original sprite variants must be packaged");
    const auto normal=decodeFollowerSheet(fr::readImage(normalFile)),shiny=decodeFollowerSheet(fr::readImage(shinyFile));
    check(normal.cellW==shiny.cellW&&normal.cellH==shiny.cellH&&normal.feet==shiny.feet,"Shiny variant shifts the animated sprite anchor");
    check(normal.image.pixels!=shiny.image.pixels,"Shiny palette is missing");
    const auto originalShiny=fr::readImage(shinyFile);const int factor=originalShiny.width/shiny.image.width;
    for(int y=0;y<originalShiny.height;++y)for(int x=0;x<originalShiny.width;++x)check(originalShiny.pixels[y*originalShiny.width+x]==shiny.image.pixels[(y/factor)*shiny.image.width+x/factor],"Changed supplied shiny sprite pixels");
    std::cout<<"Follower directions, corners, reversals, idle, warps, network playback and supplied sheet pixels passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
