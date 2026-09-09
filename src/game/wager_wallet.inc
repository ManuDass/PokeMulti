// All native wallet reads/writes and saves happen on the game thread.
struct WalletJournal {std::string id,host;uint32_t stake=0,phase=0;};
WalletJournal walletJournal;
bool walletLoaded=false,walletBattleSeen=false,walletKnown=false;
uint32_t walletPendingSave=0,shownBalance=0;
uint64_t walletSent=0;
std::string walletMessage,walletResultNotified;
constexpr uint32_t WalletReceipt=0x37574d50;
std::filesystem::path walletFile(){return diagnosticPath.parent_path()/"wager-wallet.cfg";}
void storeWallet(){std::ostringstream out;out<<"PMW1 "<<walletJournal.id<<' '<<walletJournal.host<<' '<<walletJournal.stake<<' '<<walletJournal.phase<<'\n';fr::replaceText(walletFile(),out.str());}
uint64_t walletHash(const std::string& id){return std::stoull(id.substr(0,16),nullptr,16);}
uint32_t walletReceiptPhase(const std::string& id){
    const auto save=r32(0x03005008);const auto hash=walletHash(id);
    return r32(save+0x3d24)==WalletReceipt&&r32(save+0x3d28)==uint32_t(hash)&&r32(save+0x3d2c)==uint32_t(hash>>32)?r32(save+0x3d30):0;
}
bool safeWallet(){return local.active&&bus()&&!r32(0x03003f64)&&validRam(r32(0x03005008),0x3d68)&&validRam(r32(0x0300500c),0xf24)&&r8(0x02024029)>0&&r8(0x02024029)<=6&&!replaying()&&(r32(0x030030f4)&~1u)==addresses.overworld&&r8(0x03000ea8)==2&&!r8(0x03000f9c)&&!(r8(0x02037abf)&0x80);}
void walletSave(uint32_t balance,uint32_t phase){
    const auto save=r32(0x03005008);const auto hash=walletHash(walletJournal.id);
    callGuest(gameAddress(0x0809fd70),save+0x290,balance);
    // A transaction receipt in FireRed's unused 16-byte save field makes a
    // native save + sidecar recovery idempotent across a crash between writes.
    bus()->write32(save+0x3d24,WalletReceipt);bus()->write32(save+0x3d28,uint32_t(hash));bus()->write32(save+0x3d2c,uint32_t(hash>>32));bus()->write32(save+0x3d30,phase);
    prepareNativeFieldSave();
    if(callGuest(gameAddress(0x080da364),0,0,0,0,0,20000000)!=1)throw std::runtime_error("Native wager save failed; the deposit was not acknowledged");
    walletPendingSave=phase;walletMessage="Saving wager money...";
}
void synchronizeWallet(){
    if(!bus()||!room)return;
    if(!walletLoaded){walletLoaded=true;std::ifstream file(walletFile());std::string magic;
        if(file>>magic){file>>walletJournal.id>>walletJournal.host>>walletJournal.stake>>walletJournal.phase;
            if(!file||magic!="PMW1"||!online::validIdentity(walletJournal.id,"Wager")||!online::validIdentity(walletJournal.host,"Host")||!walletJournal.stake||walletJournal.stake>online::WagerLimit||walletJournal.phase>2)throw std::runtime_error("Invalid saved wager receipt");}
    }
    const bool safe=safeWallet();
    // Display reads are independent of the stricter safe-to-save wager gate.
    // Once loaded, money remains live during shopping, dialogue and battle.
    if((local.active||walletKnown)&&validRam(r32(0x03005008),0x294)&&validRam(r32(0x0300500c),0xf24)){
        const auto balance=r32(r32(0x03005008)+0x290)^r32(r32(0x0300500c)+0xf20);
        if(balance<=online::MoneyLimit){shownBalance=balance;walletKnown=true;}
    }
#ifdef FR_TEST_HARNESS
    if(now%60==0){std::ofstream f(diagnosticPath.parent_path()/"wallet-check.txt");const auto& current=roomState.wager;
        f<<"frame="<<now<<" safe="<<safe<<" balance="<<shownBalance<<" held="<<(walletJournal.phase==1?walletJournal.stake:0)<<" phase="<<unsigned(current.phase)<<" receipt="<<walletJournal.phase<<" saving="<<walletPendingSave<<" dirty="<<bus()->save().dirty()<<" party="<<unsigned(r8(0x02024029))<<" id="<<current.id<<" policy="<<unsigned(roomState.rewardPolicy)<<'\n';
        f<<walletMessage<<'\n';}
#endif
    if(shownBalance>online::MoneyLimit)return;
    if(walletPendingSave){
        if(!bus()->save().dirty()){walletJournal.phase=walletPendingSave;walletPendingSave=0;storeWallet();walletSent=0;walletMessage=walletJournal.phase==1?"Deposit saved. Getting ready to battle.":"Wager money saved.";}
        else return;
    }
    if(safe&&!walletJournal.id.empty()&&walletJournal.phase<2){
        const auto receipt=walletReceiptPhase(walletJournal.id);
        if(receipt>walletJournal.phase&&receipt<=2){if(bus()->save().dirty()){walletPendingSave=receipt;return;}walletJournal.phase=receipt;storeWallet();walletSent=0;}
    }
    const auto& w=roomState.wager;
    if(roomState.connected&&now%30==0)room->updateWallet(shownBalance,(safe||fieldMoneyOfferReady())&&(walletJournal.id.empty()||walletJournal.phase==2)&&!w.active());
    if(!roomState.connected||!w.active()){
        if(!walletJournal.id.empty()&&walletJournal.phase<2)walletMessage="Rejoin the wager host to finish or refund your deposit.";
        return;
    }
    const int side=w.side(room->id());if(side<0)return;
    if(walletJournal.id!=w.id){
        if(!walletJournal.id.empty()&&walletJournal.phase<2){walletMessage="Finish your previous wager with its host first.";return;}
        if(!safe)return;
        walletJournal={w.id,w.host,w.stake,0};walletBattleSeen=false;walletSent=0;storeWallet();
    }
    if(walletJournal.host!=w.host||walletJournal.stake!=w.stake)throw std::runtime_error("Wager terms changed after agreement");
    const auto tick=uint64_t(clockMs());
    if(walletJournal.phase==2){
        // Phase 2 is reached only after the native payout/refund save has
        // flushed (or its persisted receipt has been recovered). Queue the
        // result before acknowledging it: the host may then clear this wager.
        if(w.terminal()&&walletResultNotified!=w.id){
            walletResultNotified=w.id;walletMessage=wagerResultText(w,side);
            queueFieldNotice("wager-result-"+w.id,walletMessage);
        }
        if(tick>walletSent+500){room->wagerEvent(w.id,4);walletSent=tick;}return;
    }
    if(w.phase==online::WagerPhase::Reserving){
        if(walletJournal.phase==1){if(tick>walletSent+500){room->wagerEvent(w.id,1,1);walletSent=tick;}return;}
        if(!safe)return;
        if(shownBalance<w.stake||shownBalance+w.stake>online::MoneyLimit){walletMessage="Wager cancelled: insufficient money or wallet capacity.";if(tick>walletSent+500){room->wagerEvent(w.id,1,0);walletSent=tick;}return;}
        walletSave(shownBalance-w.stake,1);return;
    }
    if(w.terminal()){
        if(walletJournal.phase==0){
            if(w.reserved[side]){walletMessage="The saved deposit receipt could not be verified.";return;}
            walletJournal.phase=2;storeWallet();walletSent=0;return;
        }
        if(!w.reserved[side]){if(tick>walletSent+500){room->wagerEvent(w.id,1,1);walletSent=tick;}return;}
        if(!safe){walletMessage="Return to the field to save your wager payout or refund.";return;}
        const auto credit=w.credit(side);
        if(credit>online::MoneyLimit-shownBalance){walletMessage="Wager payout waiting: spend money to make room in your wallet.";return;}
        walletSave(shownBalance+credit,2);return;
    }
    // Only the original linked battle can settle a wager. NPC/wild battles,
    // stale outcomes and screenshots cannot report a result.
    const auto main=r32(0x030030f4)&~1u;const auto outcome=r8(0x02023e8a);
    if((r32(0x02022b4c)&2)&&main==gameAddress(0x08011100)&&!outcome){
        walletBattleSeen=true;if(!w.began[side]&&tick>walletSent+500){room->wagerEvent(w.id,2);walletSent=tick;}
    }
    if(walletBattleSeen&&w.began[side]&&!w.outcome[side]&&outcome&&tick>walletSent+500){
        const auto result=outcome&0x7f;room->wagerEvent(w.id,3,uint8_t(result==1?1:result==2?2:3));walletSent=tick;
    }
}
