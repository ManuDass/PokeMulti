// Follower placement, presentation and interaction. Never alter supplied PNGs.
PlayerState lastFollowerField;
std::array<FollowerVisual,MaxRoomPlayers> followerVisuals;
std::array<Image,4> followerEffects;
std::vector<std::array<int,4>> followerEmoteBounds;
uint64_t petUntil=0;uint16_t petSequence=0;uint8_t petKind=0;
bool followerTile(int x,int y){
    const auto layout=r32(0x02036dfc);if(!validRom(layout,24))return false;
    const auto width=r32(layout),height=r32(layout+4);
    if(!width||!height||width>512||height>512||x< -7||y< -7||x>=int(width)+7||y>=int(height)+7||size_t(width+15)*(height+14)*2>0x5000)return false;
    const auto v=r16(0x02031dfc+2*((y+7)*(width+15)+x+7));
    if((v&0xc00)||((v>>12)&&local.elevation&&(v>>12)!=local.elevation))return false;
    const unsigned id=v&1023;const auto ts=r32(layout+(id<640?16:20));if(!validRom(ts,24))return false;
    const auto a=r32(ts+20)+4*(id<640?id:id-640);if(!validRom(a,4))return false;const unsigned behavior=r32(a)&511;
    if((behavior>=0x10&&behavior<=0x1b)||(behavior>=0x40&&behavior<=0x47)||worldOccupied(x,y))return false;
    for(unsigned i=0;i<16;++i){const auto o=0x02036e38+36*i;if((r32(o)&1)&&!(r32(o)&0x2000)&&s16(o+16)-7==x&&s16(o+18)-7==y)return false;}
    return true;
}
bool translateFollower(const PlayerState& a,const PlayerState& b){
    if(!a.active||!b.active||a.elevation!=b.elevation)return false;
    const auto ah=releaseHeader(a.mapGroup,a.mapNumber),bh=releaseHeader(b.mapGroup,b.mapNumber);if(!ah||!bh)return false;
    const auto al=r32(ah),bl=r32(bh),links=r32(ah+12);
    if(!validRom(al,24)||!validRom(bl,24)||!validRom(links,8)||r32(links)>8)return false;
    const auto rows=r32(links+4);if(!validRom(rows,r32(links)*12))return false;
    for(unsigned i=0;i<r32(links);++i){const auto row=rows+12*i;if(r8(row+8)!=b.mapGroup||r8(row+9)!=b.mapNumber)continue;
        const auto d=r32(row);const int offset=int32_t(r32(row+4));int dx=0,dy=0;
        if(d==1){dx=-offset;dy=-int(r32(al+4));}else if(d==2){dx=-offset;dy=int(r32(bl+4));}
        else if(d==3){dx=int(r32(bl));dy=-offset;}else if(d==4){dx=-int(r32(al));dy=-offset;}else continue;
        dx*=16;dy*=16;
        if(std::abs(a.pixelX+dx-b.pixelX)+std::abs(a.pixelY+dy-b.pixelY)>32)return false;
        followerPath.translate(dx,dy);return true;
    }return false;
}
void updateFollower(){
    if(!local.active)return;
    // Native ledges temporarily switch elevation mid-jump. That is continuous
    // motion, not a warp; reseeding here teleports the follower sideways.
    if(lastFollowerField.active&&!sameMap(lastFollowerField,local)){
        if(!translateFollower(lastFollowerField,local))followerPath.clear();
    }else if(lastFollowerField.active&&std::abs(local.pixelX-lastFollowerField.pixelX)+std::abs(local.pixelY-lastFollowerField.pixelY)>32)followerPath.clear();
    const bool allowed=followOn&&local.follower&&ownStarter()&&!(avatarFlags&0x1e)&&!localCamp.state.id;
    if(allowed&&followerPath.empty()){
        constexpr int dx[]{0,0,0,-1,1},dy[]{0,1,-1,0,0};constexpr unsigned opposite[]{0,2,1,4,3};
        const unsigned behind=opposite[std::clamp(unsigned(local.facing),1u,4u)];
        for(unsigned direction:{behind,3u,4u,1u,2u}){const int x=local.x+dx[direction],y=local.y+dy[direction];if(followerTile(x,y)){followerPath.seed(local,x*16,y*16);break;}}
    }
    // An unseeded path still accumulates safe movement if all neighbors block.
    followerPath.update(local,allowed);
    local.followerEmoteSequence=petSequence;local.followerEmote=allowed&&now<petUntil?petKind:0;
    if(local.followerEmote){local.followerFacing=movementFacing(local.pixelX-local.followerX,local.pixelY-local.followerY,local.followerFacing);local.followerFrame=0;}
    lastFollowerField=local;
}
uint32_t interactWithFollower(){
    if(fieldUiBusy()||noticeAllocation||!safeStory()||now<petUntil||campPending)return 0;
    constexpr int dx[]{0,0,0,-1,1},dy[]{0,1,-1,0,0};const unsigned face=std::clamp(unsigned(local.facing),1u,4u);
    const int targetX=local.pixelX+dx[face]*16,targetY=local.pixelY+dy[face]*16;
    // Native NPCs on the addressed tile retain their original interaction.
    for(unsigned i=0;i<16;++i){const auto o=0x02036e38+36*i;if((r32(o)&1)&&!(r32(o)&0x2000)&&s16(o+16)-7==(targetX+8)/16&&s16(o+18)-7==(targetY+8)/16)return 0;}
    const bool atCamp=localCamp.state.id!=0;
    const int member=atCamp?campInteractionTarget(localCamp.state,local.pixelX,local.pixelY,face):-1;
    if(atCamp){if(member<0)return 0;}
    else if(!followOn||!local.follower||!local.followerVisible||std::abs(local.followerX-targetX)>4||std::abs(local.followerY-targetY)>4)return 0;
    const unsigned kind=petSequence%FollowerReactionCount+1;
    constexpr const char* text[]{"Your Pokemon looks happy to be with you!","Your Pokemon enjoys a gentle pat!","Your Pokemon hums happily!","Your Pokemon tilts its head curiously!","Your Pokemon perks up at your voice!","Your Pokemon seems to be wondering something!","Your Pokemon gives you a cheerful smile!","Your Pokemon bounces happily!","Your Pokemon stamps its feet!","Your Pokemon watches quietly.","Your Pokemon lets out a little sigh.","Your Pokemon opens its mouth playfully!"};
    const auto script=makeFieldNoticeScript(text[kind-1]);if(!script)return 0;
    ++petSequence;petKind=atCamp?0:uint8_t(kind);petUntil=now+3*followerEmoteFrames(kind);
    const auto species=atCamp?localCamp.state.party[member].species:local.follower;
    if(atCamp){const auto& p=localCamp.state.party[member];localCamp.interact(unsigned(member),movementFacing(local.pixelX-p.x,local.pixelY-p.y,p.facing),uint8_t(kind),3*followerEmoteFrames(kind));}
    callGuest(gameAddress(0x08071df0),species,0);return script;
}
void drawFollowerEffect(unsigned sheet,int pattern,int x,int y){
    if(pattern<0||sheet>=followerEffects.size())return;
    auto& art=followerEffects[sheet];if(art.empty()){
        const auto file=localAsset(std::string("LocalAssets/Follower effects/")+std::array<const char*,4>{"Follower_ComeInOut.png","Follower_Emote2.png","Follower_Emote1.png","Follower_Emote3.png"}[sheet]);
        if(file.empty())return;art=readImage(file);
    }
    constexpr int cell=192;const int columns=art.width/cell;if(!columns||art.height<cell||pattern>=columns*(art.height/cell))return;
    constexpr int scale=2; // All supplied effects display at half size, nearest neighbor; PNGs stay intact.
    for(int yy=0;yy<cell;yy+=scale)for(int xx=0;xx<cell;xx+=scale){const auto rgba=art.pixels[((pattern/columns)*cell+yy)*art.width+(pattern%columns)*cell+xx];if(!(rgba>>24))continue;
        const auto color=uint16_t(((rgba&255)>>3)|((((rgba>>8)&255)>>3)<<5)|((((rgba>>16)&255)>>3)<<10));hostPixel(x+(xx-cell/2)/scale,y+(yy-cell/2)/scale,color,rgba);
    }
}
void drawPokemonReaction(const PlayerState& pose,unsigned kind,unsigned f,int x,int y){
    int top=y;
        if(const auto* sheet=followerSheet(pose.follower,pose.followerShiny)){
            const unsigned row=followerRow(pose.followerFacing),frame=pose.followerFrame%4;int first=sheet->cellH;
            for(int yy=0;yy<sheet->cellH&&first==sheet->cellH;++yy)for(int xx=0;xx<sheet->cellW;++xx)if(sheet->image.pixels[(row*sheet->cellH+yy)*sheet->image.width+frame*sheet->cellW+xx]>>24){first=yy;break;}
            top=y+15-sheet->feet[row*4+frame]+first;
        }
        if(f<followerEmoteFrames(kind))followerEmoteBounds.push_back({x-8,top-25,24,25});
        drawFollowerEffect(followerEmoteSheet(kind),followerEmotePattern(kind,f),x+3,top-3+(f<3?(-9+int(f)*3)/2:0));
}
void drawCampReaction(const CampMon& mon,const PlayerState& pose,unsigned slot,unsigned member,uint32_t camp,int x,int y){
    if(!mon.emote||slot>=MaxRoomPlayers||member>=6)return;
    struct Stamp {uint32_t camp=0;uint16_t sequence=0;uint64_t began=0;};
    static std::array<std::array<Stamp,6>,MaxRoomPlayers> stamps;
    auto& stamp=stamps[slot][member];if(stamp.camp!=camp||stamp.sequence!=mon.emoteSequence)stamp={camp,mon.emoteSequence,now};
    const unsigned frame=unsigned((now-stamp.began)/3);drawPokemonReaction(pose,mon.emote,frame,x,y);
#ifdef FR_TEST_HARNESS
    static std::ofstream trace(diagnosticPath.parent_path()/"camp-emotes.csv");trace<<now<<','<<slot<<','<<member<<','<<unsigned(mon.emote)<<','<<mon.emoteSequence<<','<<frame<<','<<followerEmotePattern(mon.emote,frame)<<'\n';if(now%15==0)trace.flush();
#endif
}
void drawCompanion(uint8_t* rgb,unsigned w,unsigned h,const PlayerState& p,unsigned slot,int dx,int dy){
    if(slot>=MaxRoomPlayers)return;auto& visual=followerVisuals[slot];
    if(visual.pose.identity!=p.identity)visual={};
    visual.update(p,followOn&&p.follower&&p.followerVisible&&!campingSlot(slot),now);
    if(visual.phase==FollowerVisual::Hidden||!sameScene(local,visual.pose))return;
    const auto& pose=visual.pose;const int x=pose.followerX+dx+8,groundY=pose.followerY+dy,y=groundY+pose.followerOffsetY;
    drawingKey=spriteKey(pose.elevation,groundY+16,16+slot);
    // Effect pixels win the same object's overlap; native UI still wins depth.
    if(visual.phase==FollowerVisual::Arriving||visual.phase==FollowerVisual::Leaving)drawFollowerEffect(0,followerBallPattern(visual.phase==FollowerVisual::Leaving,visual.frame(now)),x,y+4);
    struct Emote {uint16_t sequence=0;uint64_t began=0,identity=0;};static std::array<Emote,MaxRoomPlayers> emotes;
    auto& emote=emotes[slot];if(p.followerEmote&&(emote.sequence!=p.followerEmoteSequence||emote.identity!=p.identity))emote={p.followerEmoteSequence,now,p.identity};
    if(p.followerEmote&&visual.phase==FollowerVisual::Present)drawPokemonReaction(pose,p.followerEmote,unsigned((now-emote.began)/3),x,y);
#ifdef FR_TEST_HARNESS
    if(slot==roomSlot){static std::ofstream trace(diagnosticPath.parent_path()/"follower-effects.csv");trace<<now<<','<<unsigned(p.mapGroup)<<','<<unsigned(p.mapNumber)<<','<<visual.phase<<','<<visual.frame(now)<<','<<p.followerVisible<<','<<p.follower<<','<<p.followerShiny<<','<<p.followerToken<<','<<unsigned(p.followerEmote)<<','<<p.followerEmoteSequence<<','<<p.followerX<<','<<p.followerY<<'\n';if(now%15==0)trace.flush();}
#endif
    if(visual.sprite(now))follower(rgb,w,h,pose,x,y);
}
