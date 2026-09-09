// A room-side scene while the owning player runs the original battle UI.
BattlePresence localBattle;
std::array<std::array<MotionTimeline,5>,MaxRoomPlayers> battleMotion;
uint32_t battleIdentity=0;uint64_t battleBegan=0;
bool battleEntered=false,battleMonDataReady=false;
bool battlingSlot(unsigned slot){return roomState.connected&&slot<MaxRoomPlayers&&roomState.world.battles[slot].id;}
bool battleOccupied(int x,int y){
    if(!roomState.connected)return false;
    for(const auto& b:roomState.world.battles){if(!b.id||b.trainer.mapGroup!=local.mapGroup||b.trainer.mapNumber!=local.mapNumber)continue;
        if(b.trainer.x==x&&b.trainer.y==y)return true;for(const auto& p:b.mons)if(p.visible&&p.x/16==x&&p.y/16==y)return true;
    }return false;
}
BattleStaging battleStaging;
std::array<uint32_t,MaxRoomPlayers> battleVisualId{};
bool battleTile(int x,int y){
    const auto layout=r32(0x02036dfc);if(!validRom(layout,24))return false;const auto width=r32(layout),height=r32(layout+4);
    if(!width||!height||width>512||height>512||x<0||y<0||unsigned(x)>=width||unsigned(y)>=height||(width+15)*(height+14)*2>0x5000)return false;
    const auto tile=r16(0x02031dfc+2*((y+7)*(width+15)+x+7));
    if((tile&0xc00)||(tile>>12)!=localBattle.trainer.elevation||campBlocked(x,y))return false;
    for(unsigned i=0;i<16;++i){const auto o=0x02036e38+36*i;if(i!=r8(0x0203707d)&&(r32(o)&1)&&!(r32(o)&0x2000)&&s16(o+16)-7==x&&s16(o+18)-7==y)return false;}
    for(const auto& peer:roomState.peers)if(peer.slot!=roomSlot&&sameScene(localBattle.trainer,peer.player)&&peer.player.x==x&&peer.player.y==y)return false;
    for(const auto& m:wild)if(m.x==x&&m.y==y)return false;
    return true;
}
void beginBattlePresence(uint16_t enemy,uint32_t wildId,const PlayerState* encounterPose){
    if(localBattle.id||!bus()||replaying()||nativeLinkScene())return;
    const auto flags=enemy?0u:r32(0x02022b4c);if(flags&(2u|0x200u|0x10000u))return;
    PlayerState pose=local;if(!pose.active&&!readPlayer(pose))return;
    localBattle={};localBattle.id=++battleIdentity;if(!localBattle.id)localBattle.id=++battleIdentity;localBattle.trainer=pose;localBattle.trainer.pixelX+=pose.offsetX;localBattle.trainer.pixelY+=pose.offsetY;standFacing(localBattle.trainer,pose.facing);
    const bool safari=(flags&0x80)!=0;const unsigned count=(flags&1)?4:2;
    unsigned ours=0,theirs=0;
    for(unsigned position=0;position<count;++position){const bool foe=position&1;if(!foe&&safari)continue;uint16_t species=foe?enemy:0;
        auto& slot=foe?theirs:ours;const unsigned partyCount=foe?6:std::min(6u,unsigned(r8(0x02024029)));const auto base=foe?0x0202402cu:0x02024284u;
        while(!species&&slot<partyCount&&!(foe&&(flags&8))){const auto mon=base+100*slot++;if(!r16(mon+86))continue;const auto candidate=callGuest(0x0803fbe8+codeOffset(),mon,11);if(candidate&&candidate<=411&&!callGuest(0x0803fbe8+codeOffset(),mon,45))species=uint16_t(candidate);}
        BattleMon mon{uint8_t(position),species,localBattle.trainer.pixelX,localBattle.trainer.pixelY,pose.facing,species!=0};
        if(!foe&&position==0&&pose.followerVisible&&pose.follower==species){mon.x=pose.followerX;mon.y=pose.followerY;mon.facing=pose.followerFacing;mon.frame=pose.followerFrame;}
        else if(foe&&position==1&&encounterPose){mon.x=encounterPose->pixelX;mon.y=encounterPose->pixelY;mon.facing=encounterPose->followerFacing;mon.frame=encounterPose->followerFrame;}
        else if(foe){constexpr int dx[]{0,0,0,-1,1},dy[]{0,1,-1,0,0};mon.x=int16_t(std::clamp(int(mon.x)+dx[pose.facing]*32,0,8176));mon.y=int16_t(std::clamp(int(mon.y)+dy[pose.facing]*32,0,8176));}
        localBattle.mons.push_back(mon);
    }
    battleStaging.begin(localBattle,battleTile);battleBegan=now;battleEntered=battleMonDataReady=false;
#ifdef FR_TEST_HARNESS
    if(!diagnosticPath.empty()){std::ofstream f(diagnosticPath.parent_path()/"battle-entry.txt");f<<"frame="<<now<<" trainer="<<localBattle.trainer.pixelX<<','<<localBattle.trainer.pixelY<<" follower="<<pose.followerX<<','<<pose.followerY<<" visible="<<pose.followerVisible<<'\n';for(auto m:localBattle.mons)f<<"mon="<<unsigned(m.position)<<" pixel="<<m.x<<','<<m.y<<" face="<<unsigned(m.facing)<<'\n';}
#endif
    if(wildId)std::erase_if(wild,[&](const auto& w){return w.id==wildId;});
}
int battleStartHook(uint32_t,int,ArmCpuState*){if(!guestCall){checkpointStoryBattle();beginBattlePresence();}return 0;}
void updateBattlePresence(){
#ifdef FR_TEST_HARNESS
    if(localBattle.id&&battleEntered){static uint32_t testSequence=0;std::ifstream input(diagnosticPath.parent_path()/"test-battle-command.txt");uint32_t seq=0,value=0;std::string action;
        if(input>>seq>>action>>value&&seq!=testSequence){testSequence=seq;if(action=="money"&&value<=online::MoneyLimit)callGuest(0x0809fd70+codeOffset(),r32(0x03005008)+0x290,value);}
    }
#endif
    if(localBattle.id){
        const auto main=bus()?r32(0x030030f4)&~1u:0;const bool battle=main==0x08011100+codeOffset();
        if(replaying()||(nativeLinkScene()&&!fieldBattleActive)||(battleEntered&&local.active&&(local.mapGroup!=localBattle.trainer.mapGroup||local.mapNumber!=localBattle.trainer.mapNumber||local.elevation!=localBattle.trainer.elevation))||(!battleEntered&&now>battleBegan+1800)){localBattle={};battleEntered=battleMonDataReady=false;}
        else {
            ++localBattle.tick;
            localBattle.trainer.sequence=localBattle.tick;localBattle.trainer.sampleTime=uint32_t(clockMs());
            if(battleEntered&&local.active&&!localBattle.returning)battleStaging.returnHome(localBattle);
            const bool staged=battleStaging.update(localBattle);
            if(localBattle.returning&&staged){
                auto& p=localBattle.trainer;const int tx=local.pixelX+local.offsetX,ty=local.pixelY+local.offsetY;
                if(p.pixelX!=tx)p.pixelX+=p.pixelX<tx?1:-1;else if(p.pixelY!=ty)p.pixelY+=p.pixelY<ty?1:-1;
                else {localBattle={};battleEntered=battleMonDataReady=false;}
            }
            if(battle){battleEntered=true;const auto count=r8(0x02023bcc);
                if(count==2||count==4){bool ready=true;for(unsigned i=0;i<count;++i){const auto mon=0x02023be4+88*i;const unsigned pos=r8(0x02023bd6+i);if(pos>3){ready=false;break;}if(!(r32(0x02022b4c)&0x80)||pos&1)ready&=r16(mon)>0&&r16(mon)<=411&&r16(mon+44)>0;}
                    battleMonDataReady|=ready;
                    if(battleMonDataReady)for(unsigned i=0;i<count;++i){const auto mon=0x02023be4+88*i;const unsigned pos=r8(0x02023bd6+i),species=r16(mon);if(pos>3||!species||species>411)continue;
                        for(auto& m:localBattle.mons)if(m.position==pos){m.species=uint16_t(species);m.visible=r16(mon+40)>0;}
                    }
                }
            }
        }
    }
    for(unsigned slot=0;slot<MaxRoomPlayers;++slot){const auto& b=roomState.world.battles[slot];const bool entering=b.id&&battleVisualId[slot]!=b.id;battleVisualId[slot]=b.id;for(unsigned i=0;i<5;++i){auto& timeline=battleMotion[slot][i];if(!roomState.connected||!b.id||slot==roomSlot){timeline.clear();continue;}
            PlayerState pose;
            if(i==4){pose=b.trainer;pose.offsetY=0;pose.identity=(uint64_t(b.id)<<8)|slot;}
            else{const auto m=std::find_if(b.mons.begin(),b.mons.end(),[&](const auto& mon){return mon.position==i;});if(m==b.mons.end()){timeline.clear();continue;}pose=battleMonPose(b,*m,slot);}
            if(entering&&(i==4||i==0)&&sameScene(b.trainer,lastRemoteField[slot])){
                auto prior=lastRemoteField[slot];if(i==0){prior.active=prior.followerVisible;prior.pixelX=prior.followerX;prior.pixelY=prior.followerY;}
                prior.identity=pose.identity;prior.sequence=pose.sequence-1;prior.sampleTime=pose.sampleTime-16;
                timeline.clear();timeline.push(prior,clockMs()-16);
            }
            timeline.push(pose,clockMs());
        }}
#ifdef FR_TEST_HARNESS
    if(localBattle.id&&!diagnosticPath.empty()){static std::ofstream trace(diagnosticPath.parent_path()/"battle-motion.csv");trace<<now<<','<<localBattle.id<<','<<localBattle.trainer.pixelX<<','<<localBattle.trainer.pixelY<<','<<localBattle.settled<<','<<localBattle.returning;for(const auto& m:localBattle.mons)trace<<','<<unsigned(m.position)<<','<<m.x<<','<<m.y<<','<<unsigned(m.facing)<<','<<m.walking;trace<<'\n';if(now%30==0)trace.flush();}
    if(now%30==0&&!diagnosticPath.empty()){
        std::ofstream f(diagnosticPath.parent_path()/"battle-presence.txt");f<<"frame="<<now<<" field="<<local.active<<" entered="<<battleEntered<<" ready="<<battleMonDataReady<<" local="<<localBattle.id<<" main="<<std::hex<<(r32(0x030030f4)&~1u)<<std::dec<<" outcome="<<unsigned(r8(0x02023e8a))<<'\n';
        for(unsigned slot=0;slot<MaxRoomPlayers;++slot){const auto& b=roomState.world.battles[slot];if(!b.id)continue;f<<"battle="<<slot<<" id="<<b.id<<" map="<<unsigned(b.trainer.mapGroup)<<','<<unsigned(b.trainer.mapNumber)<<" trainer="<<b.trainer.x<<','<<b.trainer.y<<" pixel="<<b.trainer.pixelX<<','<<b.trainer.pixelY<<" settled="<<b.settled<<" returning="<<b.returning<<" tick="<<b.tick<<'\n';for(const auto& m:b.mons)f<<"mon="<<slot<<" position="<<unsigned(m.position)<<" species="<<m.species<<" visible="<<m.visible<<" pixel="<<m.x<<','<<m.y<<" face="<<unsigned(m.facing)<<'\n';}
    }
#endif
}
void drawBattles(uint8_t* rgb,unsigned w,unsigned h,int dx,int dy){
    for(unsigned slot=0;slot<MaxRoomPlayers;++slot){if(slot==roomSlot||!battlingSlot(slot)||departingSlot(slot))continue;const auto& b=roomState.world.battles[slot];if(!sameScene(local,b.trainer))continue;
        auto p=battleMotion[slot][4].sample(clockMs());if(!p.active)continue;p.netSlot=uint8_t(slot);
        drawingKey=spriteKey(p.elevation,p.pixelY+dy+16,slot);const int top=trainer(rgb,w,h,p,p.pixelX+dx+8,p.pixelY+dy);
        if(!captions[slot].info.name.empty())captionAnchors.push_back({slot,p.pixelX+dx+8,top,p.pixelY+dy+16+p.offsetY,p.partyCount,p.partyEggs});
        for(const auto& mon:b.mons){const auto pose=battleMotion[slot][mon.position].sample(clockMs());if(!pose.active)continue;drawingKey=spriteKey(p.elevation,pose.pixelY+dy+16,120+slot*4+mon.position);follower(rgb,w,h,pose,pose.pixelX+dx+8,pose.pixelY+dy+pose.offsetY);}
    }
}
