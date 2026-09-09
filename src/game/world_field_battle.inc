// Direct field link battles keep each trainer's field position and private party.
struct FieldBattleRun {
    unsigned phase=0;uint64_t began=0;std::string partner;PlayerState origin,opponent;
    std::array<uint8_t,600> party{};uint8_t count=0;uint16_t previousActivity=0;bool entered=false,failed=false;
};
FieldBattleRun fieldBattle;
void restoreFieldBattle(){
    if(fieldBattle.phase<2)return;
    for(unsigned i=0;i<fieldBattle.party.size();++i)bus()->write8(0x02024284+i,fieldBattle.party[i]);bus()->write8(0x02024029,fieldBattle.count);
    bus()->write32(0x02022b4c,0);bus()->write16(0x020370c0,fieldBattle.previousActivity);
    callGuest(fieldSymbol("CloseLink"));bus()->write16(0x0202271a,0);bus()->write32(0x03003f3c,0);
    bus()->write32(0x030030f0,r32(0x03004f80));bus()->write8(0x03003529,r8(0x03003529)&~2u);
    callGuest(fieldSymbol("Overworld_ResetMapMusic"));bus()->write32(0x03005020,fieldSymbol("FieldCB_WarpExitFadeFromBlack")|1);
    callGuest(fieldSymbol("SetMainCallback2"),fieldSymbol("CB2_ReturnToField")|1);
    fieldBattle.phase=3;fieldBattle.began=uint64_t(clockMs());if(roomState.connected&&room->cableConnected())room->battleStage(3);
    std::printf("FIELD_BATTLE returning map=%u,%u tile=%d,%d failed=%u\n",fieldBattle.origin.mapGroup,fieldBattle.origin.mapNumber,fieldBattle.origin.x,fieldBattle.origin.y,fieldBattle.failed);
}
void abortFieldBattle(const std::string& reason){
    if(!fieldBattleActive)return;fieldBattle.failed=true;
    if(room&&room->cableConnected())room->disconnectCable();
    if(fieldBattle.phase==2){if(validRam(r32(0x02023ff4),4))callGuest(fieldSymbol("FreeRestoreBattleData"));restoreFieldBattle();}
    else if(fieldBattle.phase==1){callGuest(fieldSymbol("UnlockPlayerFieldControls"));callGuest(fieldSymbol("UnfreezeObjectEvents"));fieldBattle={};fieldBattleActive=false;}
    queueFieldNotice("field-battle-cancelled",reason);
}
int fieldBattleReturnHook(uint32_t,int,ArmCpuState* cpu){if(guestCall||!fieldBattleActive||fieldBattle.phase!=2)return 0;restoreFieldBattle();return guestReturn(cpu,0);}
int fieldBattleErrorHook(uint32_t,int,ArmCpuState* cpu){if(guestCall||!fieldBattleActive)return 0;abortFieldBattle("The battle connection ended. Your team is restored. Any unsettled wager will be refunded.");return guestReturn(cpu,0);}
void updateFieldBattle(){
    const online::Peer* self=nullptr;const online::Peer* other=nullptr;
    for(const auto& p:roomState.peers)if(p.slot==roomSlot)self=&p;
    if(self)for(const auto& p:roomState.peers)if(p.slot==self->partner)other=&p;
    const bool paired=self&&other&&self->activity==online::Activity::Battle;
    const auto main=bus()?r32(0x030030f4)&~1u:0;const auto time=uint64_t(clockMs());
#ifdef FR_TEST_HARNESS
    if(now%30==0){uint32_t hash=2166136261u;for(unsigned i=0;i<600;++i){hash^=r8(0x02024284+i);hash*=16777619u;}
        bool choiceReady=false;for(unsigned i=0;i<16;++i){const auto task=0x03005090+40*i;choiceReady|=r8(task+4)&&(r32(task)&~1u)==fieldSymbol("Task_MultichoiceMenu_HandleInput");}
        std::ofstream f(diagnosticPath.parent_path()/"field-battle-check.txt");f<<"frame="<<now<<" phase="<<fieldBattle.phase<<" local_stage="<<(self?unsigned(self->battleStage):0)<<" other_stage="<<(other?unsigned(other->battleStage):0)<<" entered="<<fieldBattle.entered<<" failed="<<fieldBattle.failed<<" flags="<<r32(0x02022b4c)<<" callback="<<std::hex<<main<<std::dec<<" outcome="<<unsigned(r8(0x02023e8a))<<" ui="<<unsigned(fieldDialog.prompt)<<" amount="<<fieldDialog.amount<<" memory="<<fieldDialog.memory<<" arrows="<<fieldDialog.amountArrows<<" picker="<<fieldDialog.amountWindow<<" safe="<<safeStory()<<" party_ready="<<(safeStory()?usableFieldParty():false)<<" choice_ready="<<choiceReady<<" notice="<<noticeAllocation<<" notices="<<fieldNotices.size()<<" party_hash="<<hash<<" serial_callback="<<std::hex<<r32(0x03003108)<<" serial_intr="<<r32(0x03003544)<<std::dec<<" origin="<<fieldBattle.origin.x<<','<<fieldBattle.origin.y<<'\n';}
#endif
    if(!fieldBattle.phase){
        if(!paired||!safeWallet()||noticeAllocation||fieldDialog.memory||!fieldNotices.empty()||!requestedChallenge.empty()||encounterToken||walletPendingSave)return;
        if(!usableFieldParty()){room->disconnectCable();queueFieldNotice("field-no-team","You need a Pokemon able to battle.");return;}
        fieldBattleActive=true;fieldBattle.phase=1;fieldBattle.began=time;fieldBattle.partner=other->id;fieldBattle.origin=local;fieldBattle.opponent=other->player;
        fieldBattle.previousActivity=r16(0x020370c0);fieldBattle.count=r8(0x02024029);for(unsigned i=0;i<fieldBattle.party.size();++i)fieldBattle.party[i]=r8(0x02024284+i);
        callGuest(fieldSymbol("FreezeObjectEvents"));callGuest(fieldSymbol("LockPlayerFieldControls"));room->battleStage(1);return;
    }
    if(fieldBattle.phase<3&&(!paired||other->id!=fieldBattle.partner)){abortFieldBattle("The battle was cancelled. Your team is restored. Any unsettled wager will be refunded.");return;}
    if(fieldBattle.phase==1){
        if(time>fieldBattle.began+60000){abortFieldBattle("The other trainer was not ready. The challenge was cancelled.");return;}
        if(other->battleStage<1)return;
        // Take the normal native field transition into a single link battle.
        beginBattlePresence(fieldBattle.opponent.follower,0,&fieldBattle.opponent);
        std::erase_if(localBattle.mons,[](const auto& p){return (p.position&1)!=0;});
        callGuest(fieldSymbol("SaveMapView"));callGuest(fieldSymbol("SaveQuestLogData"));
        bus()->write16(0x020370c0,1);bus()->write32(0x02022b4c,0x0a);bus()->write8(0x02023e8a,0);bus()->write16(0x020386ae,0x800);bus()->write16(0x0202271a,0x2211);bus()->write32(0x03003f3c,0);bus()->write32(0x03003f64,0);
        bus()->write32(0x030030f8,fieldSymbol("CB2_ReturnFromCableClubBattle")|1);fieldBattle.phase=2;fieldBattle.began=time;room->battleStage(2);
        // Field maps can leave RFU interrupt handlers installed. Cable Club
        // normally restores these before a wired battle; do the same here.
        callGuest(fieldSymbol("RestoreSerialTimer3IntrHandlers"));
        callGuest(fieldSymbol("SetSerialCallback"),fieldSymbol("SerialCB")|1);
        // Cable Club assigns opposite seat/battler IDs before exchanging the
        // LinkPlayer records. Field battles skip those seats, so establish the
        // same IDs here. Pair-relative order matches Session::cableControl;
        // room slots themselves can be 1/2 or 2/3, not just 0/1.
        // Without this, both IDs stay zero and native opponent-name expansion
        // searches past the active players into an unterminated empty record.
        callGuest(fieldSymbol("SetLocalLinkPlayerId"),self->slot<other->slot?0:1);
        callGuest(fieldSymbol("CreateBattleStartTask"),0,297);std::printf("FIELD_BATTLE starting at %d,%d\n",fieldBattle.origin.x,fieldBattle.origin.y);return;
    }
    if(fieldBattle.phase==2){
        fieldBattle.entered|=main==gameAddress(0x08011100);
        if(main==gameAddress(0x08011100))bus()->write32(0x0300502c,callGuest(fieldSymbol("GetMultiplayerId")));
        if(!fieldBattle.entered&&time>fieldBattle.began+90000)abortFieldBattle("The battle could not connect. Your team is restored. Any unsettled wager will be refunded.");return;
    }
    if(fieldBattle.phase==3&&local.active&&main==addresses.overworld){
        callGuest(fieldSymbol("UnlockPlayerFieldControls"));
        // Both native results must reach the existing wager ledger before the
        // cable is released. Disconnecting sooner would cancel a valid pot.
        if(paired&&other->battleStage==3&&(!roomState.wager.active()||roomState.wager.terminal())&&roomSlot<other->slot)room->disconnectCable();
        if(!paired){fieldBattle={};fieldBattleActive=false;}
    }
}

#ifdef FR_TEST_HARNESS
int fieldBattleTextTrace(uint32_t,int,ArmCpuState* cpu){
    if(guestCall||!fieldBattleActive)return 0;
    std::ofstream f(diagnosticPath.parent_path()/"battle-text.txt",std::ios::app);
    f<<"frame="<<now<<" local="<<callGuest(fieldSymbol("GetMultiplayerId"))<<" text="<<std::hex;
    // This function also prints the VS names and menu labels directly from
    // other buffers. Trace its actual text argument, not the unused dialogue
    // buffer (which is still zero-filled during a fresh game's VS intro).
    for(unsigned j=0;j<300;++j){const auto c=r8(cpu->R[0]+j);f<<std::setw(2)<<std::setfill('0')<<unsigned(c);if(c==0xff)break;}
    for(unsigned i=0;i<5;++i){const auto p=0x0202273c+i*28;f<<" player"<<i<<"="<<r16(p+24)<<":";for(unsigned j=0;j<8;++j)f<<std::setw(2)<<std::setfill('0')<<unsigned(r8(p+8+j));}
    f<<'\n';return 0;
}
#endif

void registerFieldHooks(){
#ifdef FR_TEST_HARNESS
    gba_mod_register_function_entry_plugin("pokemulti.test-battle-text",fieldSymbol("BattlePutTextOnWindow"),1,fieldBattleTextTrace);
    gba_mod_set_function_hook_enabled("pokemulti.test-battle-text",1);
#endif
    for(const auto& hook:std::vector<std::pair<const char*,decltype(&fieldBattleReturnHook)>>{
        {"SetUpStartMenu",campMenuSetupHook},{"CreateStartMenuWindow",campMenuWindowHook},{"PrintStartMenuItems",campMenuRowsHook},{"Menu_InitCursor",campMenuCursorHook},{"StringExpandPlaceholders",campMenuTextHook},{"DrawHelpMessageWindowWithText",campMenuHelpHook},{"PrintTextOnHelpMessageWindow",campMenuHelpHook},{"StartMenuSafariZoneRetireCallback",campMenuActionHook},{"DrawVerticalMultichoiceMenu",fieldMultichoiceHook},{"CB2_ReturnFromCableClubBattle",fieldBattleReturnHook},{"CB2_LinkError",fieldBattleErrorHook}}){
        const std::string name=std::string("pokemulti.field-")+hook.first;gba_mod_register_function_entry_plugin(name.c_str(),fieldSymbol(hook.first),1,hook.second);gba_mod_set_function_hook_enabled(name.c_str(),1);
    }
}
