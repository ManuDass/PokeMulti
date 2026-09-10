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
    FollowerPath ledge;PlayerState jumping;jumping.active=true;jumping.pixelX=128;jumping.pixelY=208;jumping.facing=1;jumping.elevation=3;
    ledge.seed(jumping,128,192);ledge.update(jumping,true);int lastY=jumping.followerY;bool airborne=false;
    for(int y=209;y<=256;++y){jumping.pixelY=int16_t(y);jumping.elevation=y>=225&&y<240?0:3;jumping.offsetY=y>224&&y<256?int8_t(-std::min(y-224,256-y)):0;
        ledge.update(jumping,true);check(jumping.followerVisible&&jumping.followerX==128&&jumping.followerY>=lastY&&jumping.followerY-lastY<=1,"Follower must cross a ledge on one continuous cardinal path");
        check(jumping.followerFacing==1,"Ledge must not rotate follower backward");airborne|=jumping.followerOffsetY<0;lastY=jumping.followerY;
    }
    for(int tick=0;tick<32;++tick){ledge.update(jumping,true);check(jumping.followerY>=lastY&&jumping.followerY-lastY<=1,"Stopped trainer must not snap the follower landing");lastY=jumping.followerY;}
    check(airborne&&jumping.followerOffsetY==0&&jumping.followerY==256,"Follower must finish its jump if the trainer stops");
    for(int y=257;y<=288;++y){jumping.pixelY=int16_t(y);ledge.update(jumping,true);check(jumping.followerY>=lastY,"Restoring follower spacing must not rewind");lastY=jumping.followerY;}
    check(jumping.followerY==272,"Walking must restore one-tile follower spacing");
    FollowerVisual visual;p.follower=1;p.followerToken=11;visual.update(p,true,0);check(!visual.sprite(0),"Send-out starts with supplied ball");visual.update(p,true,30);check(visual.sprite(30),"Send-out reveals follower");visual.update(p,false,31);check(visual.phase==FollowerVisual::Leaving&&visual.sprite(31),"Recall must animate old sprite");visual.update(p,false,61);check(visual.phase==FollowerVisual::Hidden,"Recall must finish");visual.update(p,true,62);visual.update(p,true,92);p.followerToken=12;visual.update(p,true,93);check(visual.phase==FollowerVisual::Leaving,"Same-species party switch must recall old individual");visual.update(p,true,123);check(visual.phase==FollowerVisual::Arriving&&visual.pose.followerToken==12,"Party switch must send new individual");
    check(followerBallPattern(false,4)==1&&followerBallPattern(true,1)==5&&followerEmotePattern(1,4)==9&&followerEmotePattern(2,24)==-1,"Original supplied animation sequence");
    for(unsigned frame=1;frame<10;++frame){
        const int send=followerBallPattern(false,frame),recall=followerBallPattern(true,frame);
        check(send>=0&&send<=3,"Send-out must use only the four populated top-row cells");
        check(recall>=5&&recall<=7,"Return must use only the three populated bottom-row cells");
    }
    check(followerBallPattern(false,10)==-1&&followerBallPattern(true,10)==-1,"Finished effects must leave no animation cell");
    check(followerEmoteSheet(4)==2&&followerEmoteSheet(5)==2,"Question and exclamation must use Emote1");
    for(unsigned frame=0;frame<24;++frame){
        const auto question=followerEmotePattern(4,frame),surprise=followerEmotePattern(5,frame);
        check(question>=0&&question<=3,"Question must stay in top cells 1 through 4");
        check(surprise>=4&&surprise<=9,"Exclamation must start at top cell 5 and stay in its own sequence");
        if(frame<8)check(question==int(frame/2),"Question frames play in order");
        if(frame<12)check(surprise==4+int(frame/2),"Exclamation must continue across the row boundary");
    }
    check(followerEmotePattern(4,24)==-1&&followerEmotePattern(5,24)==-1,"Emote1 must clear when finished");
    for(unsigned kind:{1u,2u,3u,6u,7u}){
        check(followerEmoteSheet(kind)==1&&followerEmoteFrames(kind)==16,"Emote2 must use its own sheet and exactly two loops");
        const auto a=followerEmotePattern(kind,0),b=followerEmotePattern(kind,4);
        check(a!=b,"Two-frame emotes must animate distinct cells");
        for(unsigned frame=0;frame<16;++frame)check(followerEmotePattern(kind,frame)==((frame/4)%2?b:a),"Emote2 must play A B A B");
        check(followerEmotePattern(kind,16)==-1,"Emote2 must not start a third loop");
    }
    check(followerEmotePattern(6,0)==6&&followerEmotePattern(6,4)==7,"Emote2 question uses bottom 2 and 3");
    check(followerEmotePattern(7,0)==3&&followerEmotePattern(7,4)==4,"Emote2 cheerful face uses top 4 and 5");
    check(followerEmotePattern(3,0)==0&&followerEmotePattern(3,4)==5,"Music uses top 1 and bottom 1");
    constexpr int emote3Pairs[5][2]{{0,1},{2,3},{4,9},{5,6},{7,8}};
    for(unsigned kind=8;kind<=FollowerReactionCount;++kind){
        check(followerEmoteSheet(kind)==3&&followerEmoteFrames(kind)==16,"Emote3 must play exactly two loops on its own sheet");
        for(unsigned frame=0;frame<16;++frame)check(followerEmotePattern(kind,frame)==emote3Pairs[kind-8][(frame/4)%2],"Emote3 pair or two-loop ordering is wrong");
        check(followerEmotePattern(kind,16)==-1,"Emote3 must clear after its second loop");
    }
    check(followerEmotePattern(0,0)==-1&&followerEmotePattern(FollowerReactionCount+1,0)==-1,"Unknown emotes must not draw a cell");
    check(followerRow(1)==0&&followerRow(2)==3&&followerRow(3)==1&&followerRow(4)==2,"Sheet direction rows are wrong");
    MotionTimeline timeline;PlayerState a;a.active=true;a.identity=1;a.followerVisible=true;a.followerX=30;a.followerY=50;a.followerFacing=4;a.sequence=1;a.sampleTime=100;
    auto b=a;b.sequence=2;b.sampleTime=120;b.followerY=48;b.followerFacing=2;b.followerOffsetY=-12;timeline.push(a,1000);timeline.push(b,1020);
    check(timeline.sample(1090).followerOffsetY==-6&&timeline.sample(1090).followerY==49&&timeline.sample(1090).followerFacing==2,"Network interpolation walked backward through turn");
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
