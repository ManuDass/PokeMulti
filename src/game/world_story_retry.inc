// A failed native trainer battle abandons an attempt, not the room objective.
// Only semantic NPC poses/IDs cross the room; native scripts stay in each ROM.
struct StoryAttempt {
    uint32_t token=0;bool battle=false;
    std::vector<uint8_t> actors,reuse;
    std::vector<StoryValue> before;
};
StoryAttempt storyAttempt;
uint64_t retryMovements=0,retryBlackouts=0;
void finishStoryAttempt(){storyAttempt={};}
void rememberStoryActor(uint8_t actor){
    if(!storyAttempt.token||!actor||actor==255||storyAttempt.battle)return;
    if(ownedEncounter.kind==online::EncounterKind::Npc&&actor!=ownedEncounter.id)return;
    if(storyAttempt.actors.size()<15&&std::find(storyAttempt.actors.begin(),storyAttempt.actors.end(),actor)==storyAttempt.actors.end())storyAttempt.actors.push_back(actor);
}
void beginStoryAttempt(){
    finishStoryAttempt();
    if(ownedEncounter.kind!=online::EncounterKind::Story&&ownedEncounter.kind!=online::EncounterKind::Npc)return;
    storyAttempt.token=encounterToken;
    for(const auto& retry:roomState.world.retries)if(retry.key==ownedEncounter){storyAttempt.reuse=retry.actors;break;}
    storyAttempt.actors=storyAttempt.reuse;
    if(ownedEncounter.kind==online::EncounterKind::Npc)rememberStoryActor(uint8_t(ownedEncounter.id));
    for(const auto& value:roomState.world.story)if(!personalStoryState(value.id))storyAttempt.before.push_back({value.id,storyRead(value.id),value.revision});
}
bool retryNpcPinned(uint8_t actor){
    for(const auto& retry:roomState.world.retries)if(retry.key.group==local.mapGroup&&retry.key.map==local.mapNumber&&std::find(retry.actors.begin(),retry.actors.end(),actor)!=retry.actors.end()){
        if(encounterToken&&ownedEncounter==retry.key)return false;
        return true;
    }
    return false;
}
uint32_t storyActorObject(uint8_t actor){
    for(unsigned i=0;i<16;++i){const auto obj=0x02036e38+36*i;if((r32(obj)&1)&&r8(obj+8)==actor&&r8(obj+9)==local.mapNumber&&r8(obj+10)==local.mapGroup)return obj;}
    return 0;
}
uint8_t facingTile(int x,int y,int targetX,int targetY,uint8_t fallback){
    const int dx=targetX-x,dy=targetY-y;if(!dx&&!dy)return fallback;
    return std::abs(dx)>=std::abs(dy)?uint8_t(dx<0?3:4):uint8_t(dy<0?2:1);
}
uint32_t faceMovement(uint8_t direction){
    constexpr const char* names[]{"Common_Movement_WalkInPlaceFasterDown","Common_Movement_WalkInPlaceFasterDown","Common_Movement_WalkInPlaceFasterUp","Common_Movement_WalkInPlaceFasterLeft","Common_Movement_WalkInPlaceFasterRight"};
    return fieldSymbol(names[direction<=4?direction:1]);
}
int storyRetryMovementHook(uint32_t address,int,ArmCpuState* cpu){
    if(guestCall||!roomState.connected||!encounterToken||storyAttempt.token!=encounterToken||storyAttempt.battle||!local.active||replaying())return 0;
    if(cpu->R[1]!=local.mapNumber||cpu->R[2]!=local.mapGroup||cpu->R[0]>255)return 0;
    const auto actor=uint8_t(cpu->R[0]);rememberStoryActor(actor);
    if(storyAttempt.reuse.empty())return 0;
    const bool player=actor==255;
    if(!player&&std::find(storyAttempt.reuse.begin(),storyAttempt.reuse.end(),actor)==storyAttempt.reuse.end())return 0;
    const auto obj=player?0x02036e38+36*r8(0x0203707d):storyActorObject(actor);if(!obj)return 0;
    const int x=s16(obj+16)-7,y=s16(obj+18)-7;
    int targetX=local.x,targetY=local.y;
    if(player){int distance=100000;for(auto id:storyAttempt.reuse)if(const auto other=storyActorObject(id)){
        const int ox=s16(other+16)-7,oy=s16(other+18)-7,d=std::abs(ox-x)+std::abs(oy-y);if(d<distance){distance=d;targetX=ox;targetY=oy;}}
    }else{
        const auto avatar=0x02036e38+36*r8(0x0203707d);
        callGuest(fieldSymbol("ObjectEventTurn"),avatar,facingTile(local.x,local.y,x,y,local.facing));
    }
    ++retryMovements;
    // A real native facing movement still completes waitmovement normally.
    // It does not replay the relative walk from a now-staged NPC position.
    return guestReturn(cpu,callGuest(address,actor,local.mapNumber,local.mapGroup,faceMovement(facingTile(x,y,targetX,targetY,r8(obj+24)&15))));
}
void checkpointStoryBattle(){
    if(!roomState.connected||!encounterToken||storyAttempt.token!=encounterToken||storyAttempt.battle||!(r32(0x02022b4c)&8)||(r32(0x02022b4c)&2))return;
    captureNpcs();std::vector<online::NpcState> actors;
    for(const auto& actor:completedWorld.npcs)if(actor.visible&&std::find(storyAttempt.actors.begin(),storyAttempt.actors.end(),actor.localId)!=storyAttempt.actors.end())actors.push_back(actor);
    room->checkpointEncounter(encounterToken,actors);storyAttempt.battle=true;
}
int storyBlackoutHook(uint32_t,int,ArmCpuState*){
    publishDeparture();
    if(guestCall||!roomState.connected||!encounterToken||storyAttempt.token!=encounterToken)return 0;
    // Native scripts can set scene variables before battle. A failed attempt
    // must not later publish those unfinished changes as a room completion.
    for(const auto& before:storyAttempt.before)if(storyRead(before.id)!=before.value){
        auto restore=before.value;for(const auto& current:roomState.world.story)if(current.id==before.id&&current.revision!=before.revision){restore=current.value;break;}
        storyWrite(before.id,restore);if(!pendingStory.contains(before.id))observedStory[before.id]=restore;
    }
    room->releaseEncounter(encounterToken,true);encounterToken=0;finishStoryAttempt();++retryBlackouts;
    return 0; // original blackout, money loss, healing and warp still run
}
uint32_t retryInteraction(unsigned selected,uint32_t position){
    if(selected>=16||!storyApplied||!validRam(position,6))return 0;
    const auto obj=0x02036e38+36*selected;if(!sharedObject(obj)||s16(obj+16)!=s16(position)||s16(obj+18)!=s16(position+2))return 0;const auto actor=r8(obj+8);
    const auto events=r32(0x02036e00);if(!validRom(events,20))return 0;const auto count=r8(events+2);const auto list=r32(events+12);if(!validRom(list,uint32_t(count)*16))return 0;
    for(const auto& retry:roomState.world.retries)if(retry.key.group==local.mapGroup&&retry.key.map==local.mapNumber&&retry.key.kind==online::EncounterKind::Story&&retry.key.condition&&std::find(retry.actors.begin(),retry.actors.end(),actor)!=retry.actors.end()){
        uint32_t chosen=0;int distance=100000;
        for(unsigned i=0;i<count;++i){const auto event=list+16*i;if(!conditionalStoryGate(event)||r16(event+6)!=retry.key.condition||uint8_t(r16(event+8))!=retry.key.expected||!activeStoryGate(event,true))continue;
            const int d=std::abs(s16(event)-local.x)+std::abs(s16(event+2)-local.y);if(d<distance){distance=d;chosen=event;}}
        if(chosen&&claimStoryGate(chosen))return r32(chosen+12);
        return 0;
    }
    return 0;
}
void updateStoryRetry(){
    if(!roomState.connected&&storyAttempt.token)finishStoryAttempt();
#ifdef FR_TEST_HARNESS
    if(now%15==0&&!diagnosticPath.empty()){
        std::ofstream f(diagnosticPath.parent_path()/"story-retry-check.txt");
        f<<"frame="<<now<<" lease="<<encounterToken<<" battle="<<storyAttempt.battle<<" skipped="<<retryMovements<<" blackouts="<<retryBlackouts<<" retries="<<roomState.world.retries.size()<<" route22="<<storyRead(0x4054)<<" canonical="<<campaignValue(0x4054)<<" outcome="<<unsigned(r8(0x02023e8a))<<" callback="<<std::hex<<(r32(0x030030f4)&~1u)<<std::dec<<'\n';
        f<<"hp="<<r16(0x02024284+86)<<" maxhp="<<r16(0x02024284+88)<<'\n';
        if(const auto* map=sharedMap(3,41))for(const auto& n:map->npcs)if(n.localId==1)f<<"gary="<<n.pose.x<<','<<n.pose.y<<" face="<<unsigned(n.pose.facing)<<" visible="<<n.visible<<" owner="<<unsigned(n.owner)<<'\n';
        for(const auto& retry:roomState.world.retries){f<<"retry="<<unsigned(retry.key.group)<<','<<unsigned(retry.key.map)<<" condition="<<retry.key.condition<<" expected="<<retry.key.expected;for(auto actor:retry.actors)f<<" actor="<<unsigned(actor);f<<'\n';}
    }
#endif
}
void registerStoryRetryHooks(){
    for(const auto& hook:std::vector<std::pair<const char*,decltype(&storyBlackoutHook)>>{{"CB2_WhiteOut",storyBlackoutHook},{"ScriptMovement_StartObjectMovementScript",storyRetryMovementHook}}){
        const std::string name=std::string("pokemulti.story-")+hook.first;gba_mod_register_function_entry_plugin(name.c_str(),fieldSymbol(hook.first),1,hook.second);gba_mod_set_function_hook_enabled(name.c_str(),1);
    }
}
