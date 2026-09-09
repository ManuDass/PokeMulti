// Conditional coordinate events are part of collision while another trainer
// owns the scene. Reserve before walking, not after arriving on its trigger.
// Weather and unconditional immediate scripts retain their native behavior.
struct StoryGateStep {uint32_t event=0;uint8_t group=0,map=0;uint64_t began=0;};
StoryGateStep gateStep;
uint32_t gateWaitAllocation=0,gateWaitEvent=0;
uint8_t gateWaitGroup=0,gateWaitMap=0;
uint64_t gateWaitStarted=0,gateRetryAt=0,gateNoticeAt=0;
uint64_t gateBlockedSteps=0,gateReservations=0,gateDeferred=0;
bool storyGateReserved(){return gateStep.event!=0;}
bool conditionalStoryGate(uint32_t event){
    return validRom(event,16)&&r16(event+6)>=0x4000&&r16(event+6)<0x4100&&validRom(r32(event+12),1);
}
bool activeStoryGate(uint32_t event,bool canonical){
    if(!conditionalStoryGate(event))return false;
    const auto trigger=r16(event+6);
    const auto value=canonical&&roomShares(trigger)?campaignValue(trigger):storyRead(trigger);
    // The original engine casts the coordinate event's index to u8.
    return value==uint8_t(r16(event+8));
}
bool claimStoryGate(uint32_t event){
    const auto trigger=r16(event+6);
    if(roomShares(trigger)&&storyRead(trigger)!=campaignValue(trigger))return false;
    return claim(online::EncounterKind::Story,0,roomShares(trigger)?trigger:0);
}
bool blockStoryGate(int x,int y){
#ifdef FR_TEST_HARNESS
    if(testGateArrival)return false; // exercise arrival without a collision query
#endif
    const auto events=r32(0x02036e00);if(!validRom(events,20))return false;
    const auto count=r8(events+2);const auto list=r32(events+12);
    if(!count||!validRom(list,uint32_t(count)*16))return false;
    for(unsigned i=0;i<count;++i){const auto event=list+16*i;
        if(int16_t(r16(event))!=x||int16_t(r16(event+2))!=y||(r8(event+4)&&r8(event+4)!=local.elevation)||!conditionalStoryGate(event))continue;
        // Hold both sides of an unapplied room update; a stale local save must
        // neither skip a closed barrier nor start an already completed scene.
        if(storyApplied&&!activeStoryGate(event,false)&&!activeStoryGate(event,true))continue;
        if(storyApplied&&claimStoryGate(event)){
            gateStep={event,local.mapGroup,local.mapNumber,now};++gateReservations;return false;
        }
        ++gateBlockedSteps;
        if(now>=gateNoticeAt){queueFieldNotice("story-path-busy","This path is still blocked. Please wait for the current event to finish.");gateNoticeAt=now+240;}
        return true;
    }
    return false;
}
int runStoryGate(uint32_t address,ArmCpuState* cpu){
    const auto event=cpu->R[0];if(!conditionalStoryGate(event))return 0;
#ifdef FR_TEST_HARNESS
    testGateArrival=false;
#endif
    const auto script=callGuest(address,event);
    if(!script){if(gateStep.event==event)gateStep={};return guestReturn(cpu,0);}
    if(claimStoryGate(event)){gateStep={};encounterBegan=now;return guestReturn(cpu,script);}
    // Defensive path for entry without a voluntary collision query (warps,
    // scripted arrival). A native delay/goto holds this trainer until the host
    // grants the scene or its story condition clears. Never discard the event.
    gateWaitEvent=event;gateWaitGroup=local.mapGroup;gateWaitMap=local.mapNumber;
    gateWaitStarted=now;gateRetryAt=now+20;++gateDeferred;
    if(!gateWaitAllocation){
        gateWaitAllocation=callGuest(0x08002bb0+codeOffset(),9);
        if(!validRam(gateWaitAllocation,9)){gateWaitAllocation=0;gateStep={};return guestReturn(cpu,0);}
        bus()->write8(gateWaitAllocation,0x28);bus()->write16(gateWaitAllocation+1,1);
        bus()->write8(gateWaitAllocation+3,0x05);bus()->write32(gateWaitAllocation+4,gateWaitAllocation);
        bus()->write8(gateWaitAllocation+8,0x02);
    }
    gateStep={};return guestReturn(cpu,gateWaitAllocation);
}
void updateStoryGates(){
    if(gateStep.event&&(!roomState.connected||local.mapGroup!=gateStep.group||local.mapNumber!=gateStep.map||now>gateStep.began+120)){
        // A collision preview or canceled movement must not strand ownership.
        if(encounterToken&&room){room->releaseEncounter(encounterToken);encounterToken=0;}
        gateStep={};
    }
    // If the tiny script allocation fails, keep input closed and retry the
    // original event directly once ownership is available; never fail open.
    if(gateWaitEvent&&!gateWaitAllocation&&now>=gateRetryAt){
        gateRetryAt=now+20;
        if(local.mapGroup!=gateWaitGroup||local.mapNumber!=gateWaitMap||!(roomState.connected?activeStoryGate(gateWaitEvent,true):activeStoryGate(gateWaitEvent,false)))gateWaitEvent=0;
        else if(!roomState.connected||claimStoryGate(gateWaitEvent)){
            const auto script=r32(gateWaitEvent+12);gateWaitEvent=0;encounterBegan=now;callGuest(0x08069ae4+codeOffset(),script);
        }
    }
    if(gateWaitAllocation){
        if(gateWaitEvent&&now>=gateRetryAt){
            gateRetryAt=now+20;uint32_t next=0;
            const bool same=local.mapGroup==gateWaitGroup&&local.mapNumber==gateWaitMap;
            if(!same||!(roomState.connected?activeStoryGate(gateWaitEvent,true):activeStoryGate(gateWaitEvent,false)))next=gateWaitAllocation+8;
            else if(!roomState.connected||claimStoryGate(gateWaitEvent))next=r32(gateWaitEvent+12);
            if(next){bus()->write32(gateWaitAllocation+4,next);gateWaitEvent=0;encounterBegan=now;}
        }
        if(!gateWaitEvent&&now>gateWaitStarted+2&&safeStory()){
            callGuest(0x08002bc4+codeOffset(),gateWaitAllocation);gateWaitAllocation=0;
        }
    }
#ifdef FR_TEST_HARNESS
    if(now%15==0&&!diagnosticPath.empty()){
        std::ofstream f(diagnosticPath.parent_path()/"story-gate-check.txt");
        f<<"frame="<<now<<" map="<<unsigned(local.mapGroup)<<','<<unsigned(local.mapNumber)<<" tile="<<local.x<<','<<local.y
         <<" elevation="<<unsigned(local.elevation)<<" active="<<local.active<<" safe="<<safeStory()<<" lease="<<encounterToken
         <<" reserved="<<gateStep.event<<" deferred="<<gateWaitEvent<<" blocked="<<gateBlockedSteps<<" reservations="<<gateReservations<<" deferrals="<<gateDeferred<<'\n';
        f<<"oldman="<<storyRead(0x4051)<<" pewter="<<storyRead(0x406c)<<'\n';
    }
#endif
}
