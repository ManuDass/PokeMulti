#ifdef FR_TEST_HARNESS
extern std::string fixtureRequest;
bool testGateArrival=false;
#endif
// Included inside world.cpp's private namespace: all ROM access stays on the game thread.
online::Session* room=nullptr;
online::Status roomState;
online::WorldReport completedWorld;
std::map<unsigned,MotionTimeline> npcMotion,wildMotion;
std::map<unsigned,uint32_t> npcGeneration;
std::map<uint16_t,uint16_t> observedStory;
std::map<uint16_t,StoryValue> pendingStory;
bool storySeeded=false,storyApplied=false,wasConnected=false,rewardRequested=false;
std::string rewardNotice;
uint32_t encounterToken=0;online::EncounterKey ownedEncounter{};
uint64_t encounterBegan=0;
bool storyGateReserved();
void updateStoryGates();
void beginStoryAttempt();
void finishStoryAttempt();
void checkpointStoryBattle();
void rememberStoryActor(uint8_t actor);
bool retryNpcPinned(uint8_t actor);
uint32_t retryInteraction(unsigned selected,uint32_t position);
void updateStoryRetry();
int runStoryGate(uint32_t address,ArmCpuState* cpu);
uint32_t wildIdentity=0;
uint8_t lastWildOwner=255;
int guestReturn(ArmCpuState* cpu,uint32_t result){cpu->R[0]=result;cpu->R[15]=cpu->R[14]&~1u;if(cpu->R[14]&1)cpu->cpsr|=CPSR_T_BIT;else cpu->cpsr&=~CPSR_T_BIT;return 1;}
uint16_t storyRead(uint16_t id){const auto save=r32(0x03005008);if(!validRam(save,0x1200))return 0;return id<0x900?uint16_t((r8(save+0xee0+id/8)>>(id%8))&1):r16(save+0x1000+2*(id-0x4000));}
void storyWrite(uint16_t id,uint16_t value){const auto save=r32(0x03005008);if(!validRam(save,0x1200))return;if(id<0x900){const auto a=save+0xee0+id/8;const auto mask=1u<<(id%8);bus()->write8(a,uint8_t((r8(a)&~mask)|(value?mask:0)));}else bus()->write16(save+0x1000+2*(id-0x4000),value);}
// These two drops start hidden before their Rocket encounters. A hide flag
// alone is not evidence that the campaign has recovered the key item.
uint16_t campaignReadProof(uint16_t id){
    if(id==0x036||id==0x037)return uint16_t(callGuest(gameAddress(0x08099f40),id==0x036?356:359,1)!=0);
    return storyRead(id);
}
bool campaignRewardCollected(const RewardRule& r){return (r.claimed==0x036||r.claimed==0x037)?campaignReadProof(r.claimed)!=0:storyRead(r.claimed)!=0;}
bool roomShares(uint16_t id){return sharedStory(id,roomState.rewardPolicy);}
bool ownStarter(){return bus()&&r8(0x02024029)>0&&r8(0x02024029)<=6&&(storyRead(0x829)||(storyRead(0x828)&&storyRead(0x4055)>=4));}
bool roomPersonalObject(uint16_t id){if(ownStarter()&&(id==0x2b||id==0x2d))return false;return personalObjectFlag(id,roomState.rewardPolicy);}
uint16_t campaignValue(uint16_t id){for(const auto& v:roomState.world.story)if(v.id==id)return v.value;return 0;}
std::filesystem::path campaignNativeSave;
std::string observedCampaign;
uint32_t observedCampaignSequence=0;
bool campaignSaving=false,campaignOldManRefresh=false;
uint64_t rewardRetryAt=0;
unsigned lastWaitingRewards=0;
void queueFieldNotice(std::string key,std::string text);
void applyCampaignAccess();
void requestManualCheckpoint();
// Match FireRed's save dialog before serializing any field checkpoint.
// Otherwise a cold Continue can restore stale tiles from an earlier map.
void prepareNativeFieldSave(){
    callGuest(gameAddress(0x080590d8)); // SaveMapView
    callGuest(gameAddress(0x08112450)); // SaveQuestLogData
}
bool safeStory(){return local.active&&!replaying()&&r8(0x03000ea8)==2&&!r8(0x03000f9c)&&(r8(0x02037abf)&0x80)==0;}
const online::MapState* sharedMap(uint8_t group,uint8_t map){for(const auto& m:roomState.world.maps)if(m.group==group&&m.map==map)return &m;return nullptr;}
const online::MapState* sharedMap(){return sharedMap(local.mapGroup,local.mapNumber);}
const online::NpcState* sharedNpc(uint8_t id){if(const auto* m=sharedMap())for(const auto& n:m->npcs)if(n.localId==id)return &n;return nullptr;}
uint32_t objectTemplate(unsigned id){
    const auto events=r32(0x02036e00);if(!validRom(events,20))return 0;
    const auto count=r8(events);const auto templates=r32(events+4);if(count>64||!validRom(templates,count*24))return 0;
    for(unsigned i=0;i<count;++i)if(r8(templates+i*24)==id)return templates+i*24;return 0;
}
uint16_t objectFlag(unsigned id){const auto t=objectTemplate(id);return t?r16(t+20):0;}
bool personalService(unsigned id){const auto t=objectTemplate(id);if(!t)return false;const auto script=r32(t+16);return script==gameAddress(0x081a8cf6)||script==gameAddress(0x081a8cfc)||script==gameAddress(0x081a8d02);}
bool sharedObject(uint32_t obj){return (r32(obj)&1)&&!(r32(obj)&0x10000)&&r8(obj+8)>0&&r8(obj+8)<255&&r8(obj+5)<152&&r8(obj+9)==r8(r32(0x03005008)+5)&&r8(obj+10)==r8(r32(0x03005008)+4)&&!roomPersonalObject(objectFlag(r8(obj+8)))&&!personalService(r8(obj+8));}
void refreshStoryObjects(){
    const auto events=r32(0x02036e00);if(!validRom(events,20))return;const auto count=r8(events);const auto templates=r32(events+4);if(count>64||!validRom(templates,count*24))return;
    for(unsigned i=0;i<count;++i){const auto t=templates+i*24;const auto flag=r16(t+20);if(!flag||(!roomShares(flag)&&!(ownStarter()&&campaignValue(0x4057)>=2&&(flag==0x2d||flag==0x3a))))continue;const auto id=r8(t);bool loaded=false;
        for(unsigned j=0;j<16;++j){const auto o=0x02036e38+36*j;if((r32(o)&1)&&r8(o+8)==id){loaded=true;break;}}
        const auto* shared=sharedNpc(id);const bool acting=shared&&shared->visible&&std::any_of(roomState.world.leases.begin(),roomState.world.leases.end(),[&](const auto& l){return l.owner==shared->owner&&l.key.group==local.mapGroup&&l.key.map==local.mapNumber;});
        if(acting||retryNpcPinned(id))continue;
        if(storyRead(flag)&&loaded)callGuest(gameAddress(0x0805e4c8),id,local.mapNumber,local.mapGroup);
        else if(!storyRead(flag)&&!loaded&&std::abs(s16(t+4)-local.x)<12&&std::abs(s16(t+6)-local.y)<10)callGuest(gameAddress(0x0805e898),id,local.mapNumber,local.mapGroup);
    }
}
void synchronizeStory(){
    if(!roomState.connected||!ownStarter()||!safeStory())return;
    if(roomState.hosting&&!storySeeded){
        storySeeded=true;
        if(!roomState.world.storyReady){std::vector<StoryValue> baseline;for(auto id:storyKeys(roomState.rewardPolicy))baseline.push_back({id,campaignReadProof(id),0});room->updateStory(baseline,true);return;}
    }
    if(!roomState.world.storyReady)return;
    bool changed=false;
    if(!storyApplied){
        if(!campaignNativeSave.empty()&&std::filesystem::exists(campaignNativeSave)){
            const auto backup=std::filesystem::path(campaignNativeSave.wstring()+L".before-campaign.bak");
            if(!std::filesystem::exists(backup))std::filesystem::copy_file(campaignNativeSave,backup);
        }
        for(const auto& v:roomState.world.story){
            if(!personalStoryState(v.id)){changed|=campaignReadProof(v.id)!=v.value;if(v.id==0x4051&&storyRead(v.id)==0&&v.value>=1)campaignOldManRefresh=true;storyWrite(v.id,v.value);}
            observedStory[v.id]=campaignReadProof(v.id);
        }
        storyApplied=true;observedCampaign=roomState.campaign.id;observedCampaignSequence=roomState.campaign.sequence;
    }else{
        std::vector<StoryValue> edits;
        for(const auto& v:roomState.world.story){
            const auto live=campaignReadProof(v.id);auto pending=pendingStory.find(v.id);

            if(pending!=pendingStory.end()){
                if(pending->second.revision==v.revision)continue;
                pendingStory.erase(pending);
                if(!personalStoryState(v.id)&&live!=v.value){if(v.id==0x4051&&live==0&&v.value>=1)campaignOldManRefresh=true;storyWrite(v.id,v.value);changed=true;}
                observedStory[v.id]=campaignReadProof(v.id);continue;
            }
            if(live!=observedStory[v.id]&&live!=v.value){
                StoryValue edit{v.id,live,v.revision};
                if(validStory(edit,roomState.rewardPolicy)){edits.push_back(edit);pendingStory[v.id]=edit;observedStory[v.id]=live;continue;}
            }
            if(!personalStoryState(v.id)&&live!=v.value){if(v.id==0x4051&&live==0&&v.value>=1)campaignOldManRefresh=true;storyWrite(v.id,v.value);changed=true;}
            observedStory[v.id]=campaignReadProof(v.id);
        }
        if(!edits.empty()){room->updateStory(edits);std::printf("ROOM_STORY_COMMIT keys=%zu\n",edits.size());}
    }
    applyCampaignAccess();
    if(observedCampaign==roomState.campaign.id){
        for(const auto& c:roomState.campaign.completed)if(c.sequence>observedCampaignSequence){
            const auto* r=campaignObjective(c.objective);
            // Journal packets may precede their story packets. Wait for the
            // matching proof, then deliver eligible items before showing text.
            if(!r||campaignValue(r->proof)<r->minimum)break;
            observedCampaignSequence=c.sequence;rewardRetryAt=0;
            if(c.actor!="Earlier adventure")queueFieldNotice("campaign-"+std::to_string(c.objective),c.actor+": "+r->update);
        }
    }
    // Delivery is automatic and retried after a full pocket. The native claim
    // flag and native save keep the item and its receipt in the same transaction.
    if(pendingStory.empty()&&(rewardRequested||now>=rewardRetryAt)){
        const bool requested=rewardRequested;rewardRequested=false;rewardRetryAt=now+180;unsigned delivered=0,waiting=0;
        for(const auto& r:storyRewardRules)if(shareReward(r,roomState.rewardPolicy)&&campaignValue(r.progress)>=r.minimum&&!campaignRewardCollected(r)){
            if(callGuest(gameAddress(0x0809a084),r.item,1)){storyWrite(r.claimed,1);observedStory[r.claimed]=1;++delivered;}else ++waiting;
        }
        if(delivered){rewardNotice="Campaign rewards were added to your Bag.";queueFieldNotice("campaign-rewards",rewardNotice);}
        if(waiting&&waiting!=lastWaitingRewards)queueFieldNotice("campaign-bag","Some campaign rewards are waiting. Make room in your Bag to receive them.");
        if(requested&&!delivered&&!waiting)queueFieldNotice("campaign-rewards","Your campaign rewards are already collected.");
        lastWaitingRewards=waiting;
        if(delivered||waiting||requested)std::printf("ROOM_REWARDS delivered=%u waiting=%u\n",delivered,waiting);
    }
    if(changed){refreshStoryObjects();callGuest(gameAddress(0x08069c38));callGuest(gameAddress(0x0805a684));}
}

void captureNpcs(){
    completedWorld={};completedWorld.group=completedPlayer.mapGroup;completedWorld.map=completedPlayer.mapNumber;
    completedWorld.field=completedPlayer.active;completedWorld.started=ownStarter();completedWorld.wildEnabled=completedPlayer.follower!=0;completedWorld.sequence=completedPlayer.sequence;
    if(!completedWorld.field||!completedWorld.started||!roomState.connected)return;
    for(unsigned i=0;i<16;++i){const auto obj=0x02036e38+i*36;if(!sharedObject(obj))continue;const auto sprite=r8(obj+4);if(sprite>=64)continue;const auto sp=0x0202063c+68*sprite;
        online::NpcState n;n.localId=r8(obj+8);n.oldX=int16_t(s16(obj+20)-7);n.oldY=int16_t(s16(obj+22)-7);n.visible=(r32(obj)&0x2000)==0;
        auto& p=n.pose;p.active=true;p.mapGroup=completedPlayer.mapGroup;p.mapNumber=completedPlayer.mapNumber;p.x=int16_t(s16(obj+16)-7);p.y=int16_t(s16(obj+18)-7);p.facing=r8(obj+24)&15;p.graphics=r8(obj+5);p.elevation=r8(obj+11)&15;
        p.pixelX=int16_t(s16(sp+32)-completedOriginX-8);p.pixelY=int16_t(s16(sp+34)-completedOriginY-16-int8_t(r8(sp+41)));p.offsetX=int8_t(std::clamp<int>(s16(sp+36),-64,64));p.offsetY=int8_t(std::clamp<int>(s16(sp+38),-64,64));p.flip=uint8_t((r16(sp+2)>>12)&3);
        n.anim=r8(sp+42);n.command=r8(sp+43);p.sequence=completedPlayer.sequence;p.sampleTime=completedPlayer.sampleTime;
        if(p.facing<1||p.facing>4||p.x<0||p.y<0||p.x>511||p.y>511||n.anim>=32||n.command>=64||p.pixelX< -32||p.pixelY< -32||p.pixelX>8224||p.pixelY>8224)continue;
        completedWorld.npcs.push_back(n);
    }
    // Capture runs before frame() advances local to completedPlayer. At a
    // connected-map boundary local still names the map we just left. Hidden
    // NPC records must come from the same map as this completed snapshot.
    if(const auto* m=sharedMap(completedWorld.group,completedWorld.map))for(const auto& n:m->npcs)if(n.owner==roomSlot&&completedWorld.npcs.size()<15&&std::none_of(completedWorld.npcs.begin(),completedWorld.npcs.end(),[&](const auto& a){return a.localId==n.localId;})){
        const auto flag=objectFlag(n.localId);if(flag&&(roomShares(flag)||flag<0x20)&&storyRead(flag)){auto removed=n;removed.visible=false;completedWorld.npcs.push_back(removed);}
    }
}
#include "wager_wallet.inc"
void syncWorldFrame(){
    if(!room)return;roomState=room->status();
    synchronizeWallet();
    if(!roomState.connected){if(wasConnected){npcMotion.clear();wildMotion.clear();npcGeneration.clear();observedStory.clear();pendingStory.clear();storySeeded=false;storyApplied=false;encounterToken=0;wild.clear();lastWildOwner=255;observedCampaign.clear();observedCampaignSequence=0;rewardRetryAt=0;}wasConnected=false;return;}
    wasConnected=true;roomSlot=uint8_t(roomState.slot);
#ifdef FR_TEST_HARNESS
    if(safeStory()){
        static uint64_t testCommand=0;std::ifstream input(diagnosticPath.parent_path()/"test-world-command.txt");uint64_t seq=0;std::string action;unsigned id=0,value=0;
        if(input>>seq>>action>>std::hex>>id>>value&&seq!=testCommand){testCommand=seq;
            if(action=="camp")toggleCamp();
            if(action=="face-follower")callGuest(fieldSymbol("ObjectEventTurn"),0x02036e38+36*r8(0x0203707d),movementFacing(local.followerX-local.pixelX,local.followerY-local.pixelY,local.facing));
            if(action=="face-camp"&&id<localCamp.state.party.size()){const auto& p=localCamp.state.party[id];callGuest(fieldSymbol("ObjectEventTurn"),0x02036e38+36*r8(0x0203707d),movementFacing(p.x-local.pixelX,p.y-local.pixelY,local.facing));}
            if(action=="party-faint"&&id<6)bus()->write16(0x02024284+100*id+86,0);
            if(action=="gate-arrival")testGateArrival=true;
            if(action=="party-shiny"&&id>0&&id<=411&&value<=1){
                createVisibleWild(uint16_t(id),10,value!=0);for(unsigned i=0;i<100;++i)bus()->write8(0x02024284+i,r8(0x0202402c+i));
            }
            if(action=="party-size"&&value<=6){
                std::array<uint8_t,100> mon{};for(unsigned i=0;i<100;++i)mon[i]=r8(0x02024284+i);
                for(unsigned slot=0;slot<6;++slot)for(unsigned i=0;i<100;++i)bus()->write8(0x02024284+slot*100+i,slot<value?mon[i]:0);
                bus()->write8(0x02024029,uint8_t(value));
            }
            if(action=="party-eggs"&&value<64){
                const auto scratch=callGuest(gameAddress(0x08002bb0),4);
                if(validRam(scratch,4)){for(unsigned i=0;i<std::min<unsigned>(r8(0x02024029),6);++i){bus()->write32(scratch,(value>>i)&1);callGuest(gameAddress(0x0804037c),0x02024284+i*100,45,scratch);}callGuest(gameAddress(0x08002bc4),scratch);}
            }
            if(action=="chat")room->sendChat("Ready for an adventure!");
            if(action=="journal")showCampaignJournal();
            if(action=="campaign-fixture"){constexpr const char* choices[]{"campaign-start","campaign-mart","campaign-oak","campaign-brock","campaign-warden","campaign-trainer","campaign-fly","campaign-president","campaign-oldman","campaign-gate-oldman-a","campaign-gate-oldman-b","campaign-gate-pewter-a","campaign-gate-pewter-b"};if(id<std::size(choices))fixtureRequest=choices[id];}
            if(action=="story"&&(id<0x900||(id>=0x4000&&id<0x4100)))storyWrite(uint16_t(id),uint16_t(value));
            if(action=="wager-cancel"&&roomState.wager.active())room->wagerEvent(roomState.wager.id,5);
            if(action=="invite")room->invite(uint8_t(id),online::Activity::Battle,value);
            if(action=="fixture")fixtureRequest=value==11?"field-center-b":value==10?"follower-edge":value==9?"released-b":value==8?"released-a":value==7?"camp-b":value==6?"camp-a":value==5?"wager-b":value==4?"wager-a":value==3?"unique-eevee":value==2?"story-rival":value?"wild":"cable-a";
            if(action=="walk")walkTo(int(id),int(value));
            if(action=="chase")chaseWild();
            if(action=="rewards")rewardRequested=true;
            if(action=="campaign-save"||action=="world-save")requestManualCheckpoint();
        }
    }
#endif
    synchronizeStory();
    if(encounterToken&&local.active&&(local.mapGroup!=ownedEncounter.group||local.mapNumber!=ownedEncounter.map)){
        room->releaseEncounter(encounterToken,true);encounterToken=0;finishStoryAttempt();
    }
    if(encounterToken&&now>encounterBegan+8&&!storyGateReserved()&&safeStory()&&pendingStory.empty()){
        room->releaseEncounter(encounterToken);encounterToken=0;finishStoryAttempt();
    }
    if(const auto* m=sharedMap()){
        if(local.active&&!replaying()&&ownStarter())for(const auto& n:m->npcs){
            if(((n.owner==roomSlot||n.owner==255)&&!retryNpcPinned(n.localId))||roomPersonalObject(objectFlag(n.localId)))continue;
            bool loaded=false;for(unsigned j=0;j<16;++j){const auto o=0x02036e38+36*j;if((r32(o)&1)&&r8(o+8)==n.localId){loaded=true;break;}}
            const auto flag=objectFlag(n.localId);const auto saved=flag?storyRead(flag):uint16_t(0);
            if(!n.visible&&loaded)callGuest(gameAddress(0x0805e4c8),n.localId,local.mapNumber,local.mapGroup);
            else if(n.visible&&!loaded&&std::abs(n.pose.x-local.x)<12&&std::abs(n.pose.y-local.y)<10)callGuest(gameAddress(0x0805e898),n.localId,local.mapNumber,local.mapGroup);
            if(flag)storyWrite(flag,saved); // presentation does not claim or rewrite story state
        }
        for(const auto& n:m->npcs){auto pose=n.pose;pose.spriteFrame=n.anim;pose.follower=n.command;pose.followerX=n.oldX;pose.followerY=n.oldY;pose.identity=(uint64_t(n.generation)<<8)|n.localId;const unsigned key=(unsigned(m->group)<<16)|(unsigned(m->map)<<8)|n.localId;npcMotion[key].push(pose,clockMs());}
        for(const auto& w:m->wild){PlayerState pose{};pose.active=true;pose.mapGroup=m->group;pose.mapNumber=m->map;pose.elevation=w.elevation;pose.x=w.x;pose.y=w.y;pose.pixelX=w.pixelX;pose.pixelY=w.pixelY;pose.sequence=m->revision;pose.sampleTime=m->sampleTime;pose.identity=w.id;pose.follower=w.species;pose.followerShiny=w.shiny;pose.followerFacing=w.facing;pose.followerFrame=w.frame;wildMotion[w.id].push(pose,clockMs());}
        std::erase_if(wildMotion,[&](const auto& v){return std::none_of(m->wild.begin(),m->wild.end(),[&](const auto& w){return w.id==v.first;});});
        std::erase_if(wild,[&](const Wild& w){return std::find(m->consumed.begin(),m->consumed.end(),w.id)!=m->consumed.end();});
        if(m->wildOwner!=roomSlot||lastWildOwner!=m->wildOwner){
            wild.clear();for(const auto& w:m->wild)wild.push_back(adoptWild(w));
        }
        lastWildOwner=m->wildOwner;
    }
}
std::vector<PlayerState> worldViewers(){std::vector<PlayerState> viewers{local};if(roomState.connected)for(const auto& peer:roomState.peers)if(peer.slot!=roomSlot&&sameMap(local,peer.player))viewers.push_back(peer.player);return viewers;}
bool worldInView(int x,int y){if(!roomState.connected)return inView(x,y);for(const auto& p:worldViewers())if(std::abs(x-p.x)<=7&&std::abs(y-p.y)<=5)return true;return false;}
bool worldOccupied(int x,int y){if(releasedOccupied(x,y)||battleOccupied(x,y)||campBlocked(x,y)||occupied(x,y))return true;for(const auto& p:worldViewers())if(p.x==x&&p.y==y)return true;if(const auto* m=sharedMap())for(const auto& n:m->npcs)if(n.visible&&n.pose.x==x&&n.pose.y==y)return true;return false;}
bool controlsWild(){if(!roomState.connected)return true;const auto* m=sharedMap();return local.follower&&m&&m->wildOwner==roomSlot;}
void publishWorld(){
    if(!room||!roomState.connected)return;auto report=completedWorld;report.field=local.active;report.started=ownStarter();report.wildEnabled=local.follower!=0;report.sequence=local.sequence;report.camp=localCamp.state;report.battle=localBattle;
    for(const auto& w:wild){const auto p=wildPose(w);report.wild.push_back({w.id,w.species,w.level,local.elevation,int16_t(w.x),int16_t(w.y),p.pixelX,p.pixelY,p.followerFacing,p.followerFrame,w.shiny});}
    if(!report.field){report.camp={};report.npcs.clear();report.wild.clear();}room->updateWorld(std::move(report));
#ifdef FR_TEST_HARNESS
    if(now%60==0&&!diagnosticPath.empty()){std::ofstream f(diagnosticPath.parent_path()/"shared-world.txt");f<<"frame="<<now<<" connected="<<roomState.connected<<" ready="<<roomState.world.storyReady<<" applied="<<storyApplied<<" starter="<<ownStarter()<<" status="<<unsigned(r8(0x03000ea8))<<" locked="<<unsigned(r8(0x03000f9c))<<" replay="<<replaying()<<" lease="<<encounterToken<<" pending="<<pendingStory.size()<<" rewards="<<unclaimedStoryRewards().size()<<'\n';
        for(const auto& l:roomState.world.leases)f<<"lease_owner="<<unsigned(l.owner)<<" kind="<<unsigned(l.key.kind)<<" id="<<l.key.id<<'\n';
        if(const auto* m=sharedMap()){f<<"wild_owner="<<unsigned(m->wildOwner)<<" revision="<<m->revision<<'\n';for(const auto& n:m->npcs)f<<"npc="<<unsigned(n.localId)<<" owner="<<unsigned(n.owner)<<" tile="<<n.pose.x<<','<<n.pose.y<<" pixel="<<n.pose.pixelX<<','<<n.pose.pixelY<<" face="<<unsigned(n.pose.facing)<<" anim="<<unsigned(n.anim)<<" command="<<unsigned(n.command)<<'\n';for(const auto& w:m->wild)f<<"wild="<<w.id<<" species="<<w.species<<" tile="<<w.x<<','<<w.y<<" face="<<unsigned(w.facing)<<" anim="<<unsigned(w.frame)<<'\n';}
        if(safeStory())f<<"tm39_count_at_least_one="<<callGuest(gameAddress(0x08099f40),327,1)<<" tm39_count_at_least_two="<<callGuest(gameAddress(0x08099f40),327,2)<<'\n';
        if(safeStory()){
            const auto layout=r32(0x02036dfc);uint32_t hash=2166136261u;
            if(validRom(layout,24)&&r32(layout)<=512&&(r32(layout)+15)*(r32(layout+4)+14)*2<=0x5000){
                const int width=int(r32(layout))+15,height=int(r32(layout+4))+14;
                for(int y=local.y+3;y<=local.y+11;++y)for(int x=local.x+2;x<=local.x+12;++x)if(x>=0&&x<width&&y>=0&&y<height)hash=(hash^r16(0x02031dfc+2*(y*width+x)))*16777619u;
                f<<"field_tiles="<<hash<<'\n';
            }
        }
        if(safeStory())f<<"masterball_count_at_least_one="<<callGuest(gameAddress(0x08099f40),1,1)<<" masterball_count_at_least_two="<<callGuest(gameAddress(0x08099f40),1,2)<<'\n';
        f<<"campaign="<<roomState.campaign.id<<" objectives="<<roomState.campaign.completed.size()<<" saved="<<!campaignSaving<<'\n';
        for(auto id:{uint16_t(0x4057),uint16_t(0x4055),uint16_t(0x829),uint16_t(0x237),uint16_t(0x238),uint16_t(0x23a),uint16_t(0x53a),uint16_t(0x555),uint16_t(0x574),uint16_t(0x820),uint16_t(0x254),uint16_t(0x4051),uint16_t(0x250),uint16_t(0x4010)})f<<"progress="<<id<<" local="<<storyRead(id)<<" world="<<campaignValue(id)<<'\n';
        for(auto id:{uint16_t(0x820),uint16_t(0x254),uint16_t(0x4052),uint16_t(0x52a),uint16_t(0x263),uint16_t(0x57)})f<<"story="<<id<<" value="<<storyRead(id)<<'\n';}
#endif
}
bool claim(online::EncounterKind kind,uint32_t id,uint16_t condition=0){
    const bool wildEncounter=kind==online::EncounterKind::Wild;
    if(!roomState.connected||(!wildEncounter&&!ownStarter()))return true;
    if(wildEncounter?!local.follower:!storyApplied)return false;
    const online::EncounterKey key{local.mapGroup,local.mapNumber,kind,id,condition,condition?storyRead(condition):uint16_t(0)};
    for(const auto& lease:roomState.world.leases)if(lease.owner!=roomSlot&&lease.key.group==key.group&&lease.key.map==key.map){
        if((lease.key.kind==kind&&lease.key.id==id)||(kind!=online::EncounterKind::Wild&&lease.key.kind!=online::EncounterKind::Wild&&(kind==online::EncounterKind::Story||kind==online::EncounterKind::Unique||lease.key.kind==online::EncounterKind::Story||lease.key.kind==online::EncounterKind::Unique)))return false;
    }
    if(encounterToken)return ownedEncounter==key||(ownedEncounter.kind==online::EncounterKind::Story&&kind==online::EncounterKind::Npc);
    encounterToken=room->claimEncounter(key);if(!encounterToken)return false;ownedEncounter=key;encounterBegan=now;roomState=room->status();
    beginStoryAttempt();
    // An interpolated replica may still be behind the canonical NPC position.
    // Adopt coordinates as well as movement state before installing a script.
    if(const auto* m=sharedMap())for(const auto& n:m->npcs)if(n.owner==roomSlot){
        const unsigned actorKey=(unsigned(m->group)<<16)|(unsigned(m->map)<<8)|n.localId;
        if(!npcGeneration.contains(actorKey)||npcGeneration[actorKey]!=n.generation)for(unsigned i=0;i<16;++i){
            const auto obj=0x02036e38+36*i;if(!(r32(obj)&1)||r8(obj+8)!=n.localId||r8(obj+9)!=m->map||r8(obj+10)!=m->group)continue;
            const auto sprite=r8(obj+4);if(sprite>=64)continue;const auto sp=0x0202063c+68*sprite;const auto& pose=n.pose;
            bus()->write32(obj,r32(obj)&~0x42u);bus()->write8(obj+28,255);for(unsigned offset=48;offset<62;offset+=2)bus()->write16(sp+offset,0);
            bus()->write16(obj+16,uint16_t(pose.x+7));bus()->write16(obj+18,uint16_t(pose.y+7));bus()->write16(obj+20,uint16_t(n.oldX+7));bus()->write16(obj+22,uint16_t(n.oldY+7));bus()->write8(obj+11,uint8_t(pose.elevation|(pose.elevation<<4)));
            bus()->write16(sp+32,uint16_t(pose.pixelX+originX+8));bus()->write16(sp+34,uint16_t(pose.pixelY+originY+16+int8_t(r8(sp+41))));bus()->write16(sp+36,uint16_t(int16_t(pose.offsetX)));bus()->write16(sp+38,uint16_t(int16_t(pose.offsetY)));
            callGuest(fieldSymbol("ObjectEventTurn"),obj,pose.facing);
        }
        npcGeneration[actorKey]=n.generation;
    }
    return true;
}
int interactedHook(uint32_t address,int,ArmCpuState* cpu){
    if(guestCall||!local.active||replaying()||!ownStarter())return 0;
    if(roomState.connected)if(const auto challenge=interactWithTrainer())return guestReturn(cpu,challenge);
    if(const auto reaction=interactWithFollower())return guestReturn(cpu,reaction);
    if(!roomState.connected)return 0;
    const auto script=callGuest(address,cpu->R[0],cpu->R[1],cpu->R[2]);const auto selected=r8(0x03005074);
    if(!script)if(const auto retry=retryInteraction(selected,cpu->R[0]))return guestReturn(cpu,retry);
    if(const auto* unique=uniqueScript(script,gameRevision);unique&&!(roomState.rewardPolicy&ShareSpecialPokemon)){
        if(storyRead(unique->claimed)||!claim(online::EncounterKind::Unique,unique->claimed,unique->claimed)){
            rewardNotice=std::string(unique->name)+" is already claimed or being encountered by another trainer.";
            queueFieldNotice("campaign-claimed",rewardNotice);return guestReturn(cpu,0);
        }
        return guestReturn(cpu,script);
    }
    if(script&&local.mapGroup==4&&local.mapNumber==3&&selected<16&&objectFlag(r8(0x02036e38+36*selected+8))==0x2b&&campaignValue(0x4057)<2){
        if(!claim(online::EncounterKind::Story,0,0x4057))return guestReturn(cpu,0);
    }
    if(script&&selected<16&&sharedObject(0x02036e38+36*selected)){
        const auto actor=r8(0x02036e38+36*selected+8);const auto flag=objectFlag(actor);const bool story=flag&&roomShares(flag);
        if(!claim(story?online::EncounterKind::Story:online::EncounterKind::Npc,story?0:actor,story?flag:0))return guestReturn(cpu,0);
        rememberStoryActor(actor);
    }
    return guestReturn(cpu,script);
}
int coordHook(uint32_t address,int,ArmCpuState* cpu){
    if(guestCall||!roomState.connected||!local.active||replaying()||!ownStarter())return 0;
    return runStoryGate(address,cpu);
}
int onFrameHook(uint32_t,int,ArmCpuState* cpu){
    if(guestCall||!roomState.connected||replaying()||!ownStarter())return 0;
    if(!local.active||!storyApplied||local.mapGroup!=r8(r32(0x03005008)+4)||local.mapNumber!=r8(r32(0x03005008)+5))return guestReturn(cpu,0);
    const auto script=callGuest(gameAddress(0x08069bd8),2);if(!script)return guestReturn(cpu,0);
    const auto table=callGuest(gameAddress(0x08069b80),2);if(!validRom(table,2))return 0;
    for(unsigned i=0;i<64;++i){const auto row=table+i*8;if(!validRom(row,8)||!r16(row))break;if(r32(row+4)==script){const auto trigger=r16(row);if(roomShares(trigger)&&!claim(online::EncounterKind::Story,0,trigger))return guestReturn(cpu,0);break;}}
    return 0;
}
int trainerHook(uint32_t,int,ArmCpuState* cpu){
    if(guestCall||!roomState.connected||!local.active||replaying()||!ownStarter())return 0;
    const unsigned index=cpu->R[0]&255;if(index>=16)return 0;const auto obj=0x02036e38+36*index;
    if(!sharedObject(obj))return 0;const auto script=callGuest(gameAddress(0x0805fc38),index);if(!validRom(script,8)||callGuest(gameAddress(0x08080378),script))return 0;
    if(!personalChallengeMap(local.mapGroup,local.mapNumber)&&campaignValue(uint16_t(0x500+r16(script+2))))return guestReturn(cpu,0);
    if(callGuest(gameAddress(0x08081bec),obj)&&!claim(online::EncounterKind::Npc,r8(obj+8),uint16_t(0x500+r16(script+2))))return guestReturn(cpu,0);return 0;
}
int npcMovementHook(uint32_t address,int,ArmCpuState* cpu){
    if(guestCall||!roomState.connected||!local.active||replaying()||!ownStarter())return 0;
    const auto obj=cpu->R[0],sp=cpu->R[1];if(obj<0x02036e38||obj>=0x02036e38+36*16||(obj-0x02036e38)%36||!sharedObject(obj)||sp<0x0202063c||sp>=0x0202063c+68*64)return 0;
    const auto* n=sharedNpc(r8(obj+8));if(!n||n->owner==255)return 0;
    const unsigned key=(unsigned(local.mapGroup)<<16)|(unsigned(local.mapNumber)<<8)|n->localId;
    const bool takeover=npcGeneration.contains(key)&&npcGeneration[key]!=n->generation;npcGeneration[key]=n->generation;
    if(n->owner==roomSlot&&!takeover&&!retryNpcPinned(n->localId))return 0;
    auto pose=n->pose;pose.spriteFrame=n->anim;pose.follower=n->command;pose.followerX=n->oldX;pose.followerY=n->oldY;if(n->owner!=roomSlot){const auto i=npcMotion.find(key);if(i!=npcMotion.end()){auto sampled=i->second.sample(clockMs());if(sampled.active)pose=sampled;}}
    PlayerState live{};if(!readPlayer(live)||!sameMap(live,local))return 0;
    bus()->write16(obj+16,uint16_t(pose.x+7));bus()->write16(obj+18,uint16_t(pose.y+7));bus()->write16(obj+20,uint16_t(pose.followerX+7));bus()->write16(obj+22,uint16_t(pose.followerY+7));bus()->write8(obj+24,uint8_t(pose.facing|(pose.facing<<4)));bus()->write8(obj+11,uint8_t(pose.elevation|(pose.elevation<<4)));
#ifdef FR_TEST_HARNESS
    if(!diagnosticPath.empty()){static std::ofstream trace(diagnosticPath.parent_path()/"npc-applied.csv");trace<<now<<','<<unsigned(n->localId)<<','<<unsigned(n->owner)<<','<<pose.pixelX<<','<<pose.pixelY<<','<<unsigned(pose.facing)<<','<<unsigned(n->anim)<<','<<unsigned(n->command)<<'\n';if(now%60==0)trace.flush();}
#endif
    const auto original=r32(obj);bus()->write32(obj,(original&~(0x2042u))|0x100|(n->visible?0:0x2000));
    bus()->write16(sp+32,uint16_t(pose.pixelX+originX+8));bus()->write16(sp+34,uint16_t(pose.pixelY+originY+16+int8_t(r8(sp+41))));bus()->write16(sp+36,uint16_t(int16_t(pose.offsetX)));bus()->write16(sp+38,uint16_t(int16_t(pose.offsetY)));
    if(takeover){for(unsigned a=48;a<62;a+=2)bus()->write16(sp+a,0);bus()->write8(obj+28,255);}
    callGuest(address,obj,sp,cpu->R[2]);
    const auto anims=r32(sp+8);if(validRom(anims,(pose.spriteFrame+1)*4)){const auto commands=r32(anims+pose.spriteFrame*4);if(validRom(commands,(pose.follower+1)*4)){bus()->write8(sp+42,pose.spriteFrame);callGuest(gameAddress(0x080083c0),sp,pose.follower);}}
    bus()->write16(sp+2,uint16_t((r16(sp+2)&~0x3000u)|(pose.flip<<12)));
    bus()->write32(obj,(r32(obj)&~0x100u)|(original&0x100));
    return guestReturn(cpu,0);
}

void commitUniquePokemon(){
    if(!room||!encounterToken||ownedEncounter.kind!=online::EncounterKind::Unique)return;
    const auto* rule=uniqueClaim(uint16_t(ownedEncounter.id));if(!rule)return;
    for(const auto& v:roomState.world.story)if(v.id==rule->claimed&&!v.value){
        storyWrite(v.id,1);StoryValue update{v.id,1,v.revision};observedStory[v.id]=1;pendingStory[v.id]=update;room->updateStory({update});
        std::printf("UNIQUE_POKEMON_RECEIVED name=%s claim=%u\n",rule->name,v.id);break;
    }
}
int uniqueGiftHook(uint32_t address,int,ArmCpuState* cpu){
    if(guestCall||!roomState.connected||!encounterToken||ownedEncounter.kind!=online::EncounterKind::Unique)return 0;
    const auto result=callGuest(address,cpu->R[0],cpu->R[1],cpu->R[2],cpu->R[3],r32(cpu->R[13]));
    if(result<=1)commitUniquePokemon();
    return guestReturn(cpu,result);
}
