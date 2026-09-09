// Native adapters for shared world permissions and personal trainer careers.
void applyCampaignAccess(){
    bool changed=false;
    if(campaignOldManRefresh){
        const bool here=local.mapGroup==3&&local.mapNumber==1;
        const bool acting=std::any_of(roomState.world.leases.begin(),roomState.world.leases.end(),[](const auto& l){return l.key.group==3&&l.key.map==1;});
        if(!here)campaignOldManRefresh=false;
        else if(!acting){
            // The original transition subscript changes the saved object
            // template and graphics. Recreate the live actor at that position.
            callGuest(0x08069b48+codeOffset(),0x0816590f+(codeOffset()?0x78:0));
            const auto events=r32(0x02036e00);
            if(validRom(events,20))for(unsigned id=1;id<64;++id){const auto t=objectTemplate(id);
                if(t&&r32(t+16)==0x081659f6u+(codeOffset()?0x78u:0u)){
                    callGuest(0x0805e4c8+codeOffset(),id,1,3);
                    if(std::abs(local.x-21)<12&&std::abs(local.y-8)<10)callGuest(0x0805e898+codeOffset(),id,1,3);
                    break;
                }
            }
            campaignOldManRefresh=false;changed=true;
        }
    }
    const auto minimum=[&](uint16_t id,uint16_t value){if(storyRead(id)<value){if(id==0x4051&&storyRead(id)==0)campaignOldManRefresh=true;storyWrite(id,value);if(!roomShares(id))observedStory[id]=value;changed=true;}};
    const auto parcel=campaignValue(0x4057);
    if(parcel>=1&&storyRead(0x4055)>=4&&storyRead(0x4055)<6)minimum(0x4055,5);
    if(parcel>=2&&storyRead(0x4055)>=4){
        const bool newDex=!storyRead(0x829);
        minimum(0x829,1);minimum(0x4055,6);minimum(0x407c,1);minimum(0x4058,1);
        minimum(0x4051,1);minimum(0x4054,1);minimum(0x3a,1);minimum(0x2d,1);
        if(newDex){callGuest(0x0810b810+codeOffset());queueFieldNotice("campaign-dex","Oak's parcel was delivered. Your Pokedex is ready!");}
        // A parcel carried over from a prior individual save is no longer a
        // second delivery opportunity after this campaign has completed it.
        if(callGuest(0x08099f40+codeOffset(),349,1))callGuest(0x0809a1d8+codeOffset(),349,1);
    }
    if(campaignValue(0x820)){minimum(0x2e,1);minimum(0x406c,1);}
    bool viridian=true;for(uint16_t id=0x821;id<=0x826;++id)viridian&=campaignValue(id)!=0;
    if(viridian)minimum(0x405a,1);
    if(changed){campaignSaveRequested=true;refreshStoryObjects();callGuest(0x08069c38+codeOffset());callGuest(0x0805a684+codeOffset());}
}
void saveCampaignProgress(){
    if(campaignSaving){if(!bus()->save().dirty())campaignSaving=false;else return;}
    if(!campaignSaveRequested||!storyApplied||!safeWallet()||encounterToken||!pendingStory.empty()||roomState.wager.active()||walletPendingSave)return;
    prepareNativeFieldSave();
    if(callGuest(0x080da364+codeOffset(),0,0,0,0,0,20000000)!=1)throw std::runtime_error("The native campaign save failed. Progress remains in memory.");
    campaignSaving=true;campaignSaveRequested=false;
    std::printf("CAMPAIGN_NATIVE_SAVE\n");
}
// FlagGet is overridden only for world travel checks. The trainer card,
// obedience calculations, Gym scripts, payouts and League remain personal.
int campaignFlagHook(uint32_t,int,ArmCpuState* cpu){
    if(guestCall||!roomState.connected||!storyApplied||replaying()||nativeLinkScene())return 0;
    const auto id=uint16_t(cpu->R[0]);const auto caller=(cpu->R[14]&~1u)-codeOffset();
    const bool script=caller>=0x0806a854&&caller<0x0806a86e;
    // Keep receipts personal in the save, but a claimant-only NPC must see a
    // completed campaign claim. Re-enabling sharing can still deliver a copy.
    if(script)for(const auto& r:storyRewardRules)if(id==r.claimed&&!shareReward(r,roomState.rewardPolicy)&&campaignValue(id))return guestReturn(cpu,1);
    if(id<0x820||id>0x827)return 0;
    const bool fieldMove=(caller>=0x081245a4&&caller<0x081247c4)||(caller>=0x0806d548&&caller<0x0806d5e8);
    // Checkflag executes in map scripts. Only the outdoor travel maps and
    // the badge-check entrance use campaign badges, never personal NPC praise.
    const bool travelMap=(local.mapGroup==3&&(local.mapNumber==1||local.mapNumber==2||local.mapNumber==41||local.mapNumber==42))
        ||(local.mapGroup==28&&local.mapNumber==0);
    if((fieldMove||(travelMap&&script))&&campaignValue(id))return guestReturn(cpu,1);
    return 0;
}

// Gym victory scripts jump unconditionally into their gift section. A trainer
// who already received that TM through the campaign must not receive it again.
int campaignGymGiftHook(uint32_t,int,ArmCpuState* cpu){
    if(guestCall||!roomState.connected||!storyApplied||replaying()||!personalChallengeMap(local.mapGroup,local.mapNumber))return 0;
    const auto ctx=cpu->R[0];if(!validRam(ctx,12))return 0;
    const auto pc=r32(ctx+8);if(!validRom(pc,4))return 0;
    const auto destination=r32(pc)-(codeOffset()?0x78:0);
    constexpr uint32_t gifts[]{0x0816a5f3,0x0816aaf9,0x0816b9af,0x0816d0c6,0x0816d5a6,0x0816ee3d,0x0816da34,0x08169f2f};
    for(unsigned i=0;i<8;++i)if(destination==gifts[i]){
        const auto& rule=storyRewardRules[i];
        if(!storyRead(rule.claimed)&&!(!shareReward(rule,roomState.rewardPolicy)&&campaignValue(rule.claimed)))return 0;
        const auto script=makeFieldNoticeScript(storyRead(rule.claimed)?"Congratulations! You already received this Gym's TM.":"Congratulations! This campaign's TM was already claimed by another trainer.",true);
        if(script){bus()->write32(ctx+8,script);return guestReturn(cpu,0);}return 0;
    }
    return 0;
}
