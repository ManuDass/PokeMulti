// Native script/text/menu presentation for field challenges and camping.
enum class FieldPrompt {None,Challenge,Amount,Confirm,Invitation,CancelPending,Expired};
struct FieldDialog {
    FieldPrompt prompt=FieldPrompt::None;uint32_t memory=0;uint64_t began=0;
    std::vector<std::string> choices;std::string target,name;uint32_t nonce=0,amount=0;
    int amountWindow=-1,amountArrows=-1,choiceWindow=-1,digit=3;uint64_t inputFrame=0;
};
FieldDialog fieldDialog;
std::string requestedChallenge;
uint32_t seenFieldInvitation=0,fieldNoticeSequence=0;
std::string outgoingChallenge;
bool campMenuActive=false;
bool fieldMoneyOfferReady(){return local.active&&(fieldDialog.memory||noticeAllocation)&&!fieldBattleActive;}
bool fieldUiBusy(){return fieldDialog.memory||fieldBattleActive||!requestedChallenge.empty();}
bool nearbyTrainer(const PlayerState& a,const PlayerState& b){return sameScene(a,b)&&std::abs(int(a.x)-b.x)+std::abs(int(a.y)-b.y)<=1;}
const online::Peer* fieldPeer(const std::string& id){for(const auto& p:roomState.peers)if(p.id==id&&p.slot!=roomSlot)return &p;return nullptr;}
bool usableFieldParty(){
    const auto count=r8(0x02024029);if(!count||count>6)return false;
    for(unsigned i=0;i<count;++i){const auto p=0x02024284+100*i;if(callGuest(gameAddress(0x0803fbe8),p,11,0)&&!callGuest(gameAddress(0x0803fbe8),p,45,0)&&r16(p+86))return true;}
    return false;
}
uint32_t writeFieldText(uint32_t at,const std::string& text){
    for(unsigned char c:text)bus()->write8(at++,dialogueGlyph(c));bus()->write8(at++,0xff);return at;
}
uint32_t openFieldPrompt(FieldPrompt prompt,const std::string& message,std::vector<std::string> choices,bool start=true){
    if(fieldDialog.memory||noticeAllocation)return 0;
    const auto memory=callGuest(fieldSymbol("AllocZeroed"),4096);if(!validRam(memory,4096))return 0;
    fieldDialog.memory=memory;fieldDialog.prompt=prompt;fieldDialog.choices=std::move(choices);fieldDialog.began=now;fieldDialog.amountWindow=fieldDialog.choiceWindow=-1;
    const auto text=dialogueText(message,[](uint8_t glyph){return unsigned(std::clamp<int>(r8(gameAddress(0x08207300)+glyph),1,16));});
    for(unsigned i=0;i<text.size();++i)bus()->write8(memory+32+i,text[i]);
    const uint32_t ptr=memory+32;const uint8_t script[]{0x69,0xc7,2,0x67,uint8_t(ptr),uint8_t(ptr>>8),uint8_t(ptr>>16),uint8_t(ptr>>24),0x66,0x6f,17,1,uint8_t(prompt==FieldPrompt::Amount?254:255),0,0x6b,0x02};
    for(unsigned i=0;i<sizeof(script);++i)bus()->write8(memory+i,script[i]);
    std::printf("FIELD_PROMPT %u %s\n",unsigned(prompt),message.c_str());
    if(start)callGuest(fieldSymbol("ScriptContext_SetupScript"),memory);return memory;
}
// Native Bag sell-quantity layout: label at left, amount at right, and the
// ROM's original animated red arrow pair above/below the amount.
constexpr unsigned wagerWindowLeft=14,wagerWindowTop=7,wagerWindowWidth=14;
void drawWagerAmount(){
    if(fieldDialog.amountWindow<0)return;const auto window=uint32_t(fieldDialog.amountWindow),text=fieldDialog.memory+2400;
    char digits[7];std::snprintf(digits,sizeof(digits),"%06u",fieldDialog.amount);
    // Keep the selected place visible when editing beyond the first digit,
    // without padding an ordinary P100 wager with three unnecessary zeroes.
    int first=0;while(first<5&&first<fieldDialog.digit&&digits[first]=='0')++first;
    uint32_t at=text;bus()->write8(at++,0xfc);bus()->write8(at++,1);bus()->write8(at++,2);bus()->write8(at++,0xb7); // native Pokédollar glyph
    for(int i=first;i<6;++i){bus()->write8(at++,0xfc);bus()->write8(at++,1);bus()->write8(at++,i==fieldDialog.digit?4:2);bus()->write8(at++,dialogueGlyph(digits[i]));}
    bus()->write8(at++,0xff);callGuest(fieldSymbol("FillWindowPixelBuffer"),window,0x11);
    const auto label=fieldDialog.memory+2528;writeFieldText(label,"Wager");
    callGuestWords(fieldSymbol("AddTextPrinterParameterized"),{window,0,label,8,10,0xff,0});
    const auto width=callGuest(fieldSymbol("GetStringWidth"),0,text,0);
    const auto x=wagerWindowWidth*8-8-std::min(width,56u);
    callGuestWords(fieldSymbol("AddTextPrinterParameterized"),{window,0,text,x,10,0xff,0});
    callGuest(fieldSymbol("CopyWindowToVram"),window,3);
    // CreateWindowFromRect insets its content origin by one tile.
    const unsigned arrowX=(wagerWindowLeft+1)*8+x+width/2;
    if(fieldDialog.amountArrows<0){
        // As in the Bag quantity picker, offset 1 between thresholds 0/2
        // keeps both arrows visible. Its storage outlives the native task.
        bus()->write16(fieldDialog.memory+2560,1);
        const auto task=callGuestWords(fieldSymbol("AddScrollIndicatorArrowPairParameterized"),{2,arrowX,(wagerWindowTop+1)*8+8,(wagerWindowTop+1)*8+24,2,0x6f70,0x6f70,fieldDialog.memory+2560});
        if(task<16)fieldDialog.amountArrows=int(task);
    }else{
        // Preserve the native bounce phase while the number width changes.
        const auto data=0x03005090+40*unsigned(fieldDialog.amountArrows)+8;
        for(unsigned offset:{12u,13u}){const auto sprite=r8(data+offset);if(sprite<64)bus()->write16(0x0202063c+68*sprite+32,uint16_t(arrowX));}
    }
}
void closeWagerAmount(){
    if(fieldDialog.amountArrows>=0){callGuest(fieldSymbol("RemoveScrollIndicatorArrowPair"),uint32_t(fieldDialog.amountArrows));fieldDialog.amountArrows=-1;}
    if(fieldDialog.amountWindow>=0){callGuest(fieldSymbol("DestroyScriptMenuWindow"),uint32_t(fieldDialog.amountWindow));fieldDialog.amountWindow=-1;}
}
int fieldMultichoiceHook(uint32_t,int,ArmCpuState* cpu){
    if(guestCall||!fieldDialog.memory||(cpu->R[2]!=254&&cpu->R[2]!=255))return 0;
    if(fieldDialog.prompt==FieldPrompt::Amount){
        const auto window=callGuest(fieldSymbol("CreateWindowFromRect"),wagerWindowLeft,wagerWindowTop,wagerWindowWidth,4);
        if(window>=32){bus()->write16(0x020370d0,127);callGuest(fieldSymbol("ScriptContext_Enable"));return guestReturn(cpu,0);}
        fieldDialog.amountWindow=int(window);fieldDialog.amountArrows=-1;
        callGuest(fieldSymbol("SetStdWindowBorderStyle"),uint32_t(fieldDialog.amountWindow),0);drawWagerAmount();fieldDialog.inputFrame=now+2;
    }else{
        const auto count=unsigned(fieldDialog.choices.size());uint32_t cursor=fieldDialog.memory+2048;const auto list=fieldDialog.memory+1800;
        for(unsigned i=0;i<count;++i){bus()->write32(list+8*i,cursor);cursor=writeFieldText(cursor,fieldDialog.choices[i]);}
        const auto window=callGuest(fieldSymbol("CreateWindowFromRect"),14,1,14,count==3?6:4);fieldDialog.choiceWindow=int(window);
        callGuest(fieldSymbol("SetStdWindowBorderStyle"),window,0);
        callGuestWords(fieldSymbol("MultichoiceList_PrintItems"),{window,2,8,2,14,count,list,0,2});
        callGuestWords(fieldSymbol("Menu_InitCursor"),{window,2,0,2,14,count,0});
        callGuest(fieldSymbol("CreateMCMenuInputHandlerTask"),0,count,window,255);
    }
    callGuest(fieldSymbol("ScheduleBgCopyTilemapToVram"),0);return guestReturn(cpu,0);
}
void pollWagerAmount(){
    if(fieldDialog.amountWindow<0||now<=fieldDialog.inputFrame)return;
    const auto keys=r16(0x0300311e),repeat=r16(0x03003120);bool changed=false;
    if(keys&0x20){fieldDialog.digit=(fieldDialog.digit+5)%6;changed=true;}
    if(keys&0x10){fieldDialog.digit=(fieldDialog.digit+1)%6;changed=true;}
    const uint32_t places[]{100000,10000,1000,100,10,1},step=places[fieldDialog.digit],limit=std::min({shownBalance,online::WagerLimit,online::MoneyLimit-shownBalance});
    if(repeat&0x40){fieldDialog.amount=uint32_t(std::min<uint64_t>(uint64_t(fieldDialog.amount)+step,limit));changed=true;}
    if(repeat&0x80){fieldDialog.amount=fieldDialog.amount>step?fieldDialog.amount-step:0;changed=true;}
    if(changed)drawWagerAmount();
    if(keys&3){bus()->write16(0x020370d0,(keys&2)?127:0);closeWagerAmount();callGuest(fieldSymbol("ScriptContext_Enable"));}
}
void requestFieldChallenge(uint8_t slot){
    if(fieldDialog.memory||fieldBattleActive)return;
    for(const auto& p:roomState.peers)if(p.slot==slot&&slot!=roomSlot){requestedChallenge=p.id;return;}
}
uint32_t challengePrompt(const online::Peer& p,bool start){
    if(p.partner>=0||room->cableConnected()||roomState.wager.active()||!usableFieldParty())return 0;
    fieldDialog.target=p.id;fieldDialog.name=p.name;fieldDialog.amount=0;
    if(outgoingChallenge==p.id)return openFieldPrompt(FieldPrompt::CancelPending,"Waiting for "+p.name+". Cancel your challenge?",{"Cancel challenge","Keep waiting"},start);
    return openFieldPrompt(FieldPrompt::Challenge,"Challenge "+p.name+" to a Pokemon battle?",{"Friendly battle","Set a wager","Cancel"},start);
}
uint32_t interactWithTrainer(){
    if(fieldUiBusy()||noticeAllocation||!safeStory()||!roomState.connected)return 0;
    constexpr int dx[]{0,0,0,-1,1},dy[]{0,1,-1,0,0};
    for(const auto& p:roomState.peers)if(p.slot!=roomSlot&&sameScene(local,p.player)&&p.player.x==local.x+dx[local.facing]&&p.player.y==local.y+dy[local.facing])return challengePrompt(p,false);
    return 0;
}
void updateFieldUi(){
    if(!outgoingChallenge.empty()){
        if(!roomState.connected){outgoingChallenge.clear();queueFieldNotice("field-challenge-result","You left the room. The challenge was cancelled.");}
        else if(roomState.wager.active()||room->cableConnected())outgoingChallenge.clear();
        else if(roomState.messageSequence!=fieldNoticeSequence){
            fieldNoticeSequence=roomState.messageSequence;const auto& message=roomState.message;
            if(message.find("declined")!=message.npos||message.find("cancelled")!=message.npos||message.find("expired")!=message.npos||message.find("pending")!=message.npos||message.find("money")!=message.npos||message.find("wager first")!=message.npos){outgoingChallenge.clear();queueFieldNotice("field-challenge-result",message);}
        }
    }
    if(fieldDialog.memory&&fieldDialog.prompt==FieldPrompt::Invitation&&(roomState.invitation.nonce!=fieldDialog.nonce||roomState.invitation.from<0)){
        if(fieldDialog.choiceWindow>=0){
            bool owned=false;for(unsigned i=0;i<16;++i){const auto task=0x03005090+40*i;if(r8(task+4)&&(r32(task)&~1u)==fieldSymbol("Task_MultichoiceMenu_HandleInput")&&r16(task+8+12)==fieldDialog.choiceWindow){callGuest(fieldSymbol("DestroyTask"),i);owned=true;}}
            if(owned)callGuest(fieldSymbol("DestroyScriptMenuWindow"),uint32_t(fieldDialog.choiceWindow));fieldDialog.choiceWindow=-1;
        }
        fieldDialog.prompt=FieldPrompt::Expired;callGuest(fieldSymbol("ScriptContext_SetupScript"),fieldDialog.memory+14);queueFieldNotice("field-challenge-expired","That invitation is no longer available.");
    }
    pollWagerAmount();
    if(fieldDialog.memory){
        if(now<=fieldDialog.began+3||!safeStory())return;
        const auto prompt=fieldDialog.prompt;const auto choice=r16(0x020370d0);callGuest(fieldSymbol("Free"),fieldDialog.memory);fieldDialog.memory=0;fieldDialog.prompt=FieldPrompt::None;
        const auto* target=fieldPeer(fieldDialog.target);
        if(prompt==FieldPrompt::Expired)return;
        if(prompt==FieldPrompt::CancelPending){if(choice==0){room->cancelInvitation();if(roomState.wager.active()&&(roomState.wager.phase==online::WagerPhase::Reserving||roomState.wager.phase==online::WagerPhase::Ready))room->wagerEvent(roomState.wager.id,5);outgoingChallenge.clear();}return;}
        if(prompt==FieldPrompt::Invitation){
            if(roomState.invitation.from<0||roomState.invitation.nonce!=fieldDialog.nonce)return;
            const bool accepted=choice==0&&target&&safeWallet()&&(roomState.invitation.activity==online::Activity::Trade||(usableFieldParty()&&nearbyTrainer(local,target->player)))&&(!roomState.invitation.stake||(roomState.invitation.stake<=shownBalance&&!walletHeld()));
            room->updateWallet(shownBalance,safeWallet());room->reply(accepted);
            if(accepted&&roomState.invitation.activity==online::Activity::Battle)queueFieldNotice("field-battle-agreed","Challenge accepted. Getting ready to battle!");return;
        }
        if(!target||!nearbyTrainer(local,target->player)||target->partner>=0){queueFieldNotice("field-challenge-gone","That trainer is no longer available nearby.");return;}
        if(prompt==FieldPrompt::Challenge){
            if(choice==1){fieldDialog.amount=std::min(100u,shownBalance);fieldDialog.digit=3;openFieldPrompt(FieldPrompt::Amount,"D-pad: set wager and digit.\nA: offer. B: cancel.",{});return;}
            if(choice!=0)return;fieldDialog.amount=0;
        }else if(prompt==FieldPrompt::Amount){if(choice!=0)return;}
        else if(prompt==FieldPrompt::Confirm){
            if(choice!=0)return;
            if(!safeWallet()||fieldDialog.amount>shownBalance||walletHeld()){queueFieldNotice("field-wager-money","Your money is not available for that wager.");return;}
            room->updateWallet(shownBalance,true);room->invite(target->slot,online::Activity::Battle,fieldDialog.amount);outgoingChallenge=target->id;fieldNoticeSequence=roomState.messageSequence;queueFieldNotice("field-challenge-sent","Challenge sent to "+target->name+".");return;
        }else return;
        const auto terms=fieldDialog.amount?"Each trainer stakes P"+std::to_string(fieldDialog.amount)+". The winner gets P"+std::to_string(fieldDialog.amount*2)+". Draws or cancellations refund deposits. This saves your game.":"A friendly battle. No money changes hands.";
        openFieldPrompt(FieldPrompt::Confirm,terms+" Challenge "+target->name+"?",{"Send challenge","Cancel"});return;
    }
    if(!safeStory()||noticeAllocation||fieldBattleActive||local.pixelX!=local.x*16||local.pixelY!=local.y*16)return;
    if(roomState.invitation.from>=0&&roomState.invitation.nonce!=seenFieldInvitation){
        const auto invitation=roomState.invitation;const online::Peer* from=nullptr;for(const auto& p:roomState.peers)if(p.slot==invitation.from)from=&p;
        if(!from)return;seenFieldInvitation=invitation.nonce;fieldDialog.target=from->id;fieldDialog.name=from->name;fieldDialog.nonce=invitation.nonce;
        const bool battle=invitation.activity==online::Activity::Battle;
        const auto terms=invitation.stake?"Each stakes P"+std::to_string(invitation.stake)+". Winner gets P"+std::to_string(invitation.stake*2)+". Draws or cancellations refund deposits. This saves your game.":battle?"No money changes hands.":"Meet in the upstairs Cable Club to trade.";
        if(battle&&(!nearbyTrainer(local,from->player)||!usableFieldParty())){room->reply(false);queueFieldNotice("field-invitation-busy","Battle declined. Both trainers need to be nearby and ready.");return;}
        openFieldPrompt(FieldPrompt::Invitation,from->name+(battle?" challenges you to battle! ":" wants to trade. ")+terms+" Accept?",{"Accept","Decline"});return;
    }
    if(!requestedChallenge.empty()){
        const auto id=std::move(requestedChallenge);requestedChallenge.clear();const auto* p=fieldPeer(id);
        if(!p||!nearbyTrainer(local,p->player)||!challengePrompt(*p,true))queueFieldNotice("field-challenge-unavailable","Approach an available trainer with a Pokemon ready to battle.");
    }
#ifdef FR_TEST_HARNESS
    if(now%30==0){std::ofstream f(diagnosticPath.parent_path()/"field-ui-check.txt");f<<"frame="<<now<<" prompt="<<unsigned(fieldDialog.prompt)<<" amount="<<fieldDialog.amount<<" menu="<<campMenuActive<<" count="<<unsigned(r8(0x020370f5))<<" order=";for(unsigned i=0;i<r8(0x020370f5)&&i<9;++i)f<<unsigned(r8(0x020370f6+i))<<',';f<<'\n';}
#endif
}
int campMenuSetupHook(uint32_t address,int,ArmCpuState* cpu){
    if(guestCall)return 0;callGuest(address);campMenuActive=false;
    unsigned count=r8(0x020370f5);if(count<9&&!nativeLinkScene()&&!replaying())for(unsigned i=1;i<count;++i)if(r8(0x020370f6+i)==3&&r8(0x020370f6+i-1)==2){
        bool retire=false;for(unsigned j=0;j<count;++j)retire|=r8(0x020370f6+j)==7;if(retire)break;
        for(unsigned j=count;j>i;--j)bus()->write8(0x020370f6+j,r8(0x020370f6+j-1));bus()->write8(0x020370f6+i,7);bus()->write8(0x020370f5,uint8_t(count+1));campMenuActive=true;break;
    }
    return guestReturn(cpu,0);
}
int campMenuTextHook(uint32_t,int,ArmCpuState* cpu){
    if(guestCall||!campMenuActive||cpu->R[1]!=fieldSymbol("gText_MenuRetire"))return 0;
    writeFieldText(cpu->R[0],camping()?"PACK UP":"CAMP");return guestReturn(cpu,cpu->R[0]);
}
int campMenuHelpHook(uint32_t address,int,ArmCpuState* cpu){
    if(guestCall||!campMenuActive||cpu->R[0]!=fieldSymbol("gStartMenuDesc_Retire"))return 0;
    const auto memory=callGuest(fieldSymbol("AllocZeroed"),96);writeFieldText(memory,"Set up or pack away your camp.");const auto result=callGuest(address,memory,cpu->R[1],cpu->R[2],cpu->R[3]);callGuest(fieldSymbol("Free"),memory);return guestReturn(cpu,result);
}
int campMenuActionHook(uint32_t,int,ArmCpuState* cpu){
    if(guestCall||!campMenuActive)return 0;callGuest(fieldSymbol("StartMenuExitCallback"));campMenuActive=false;toggleCamp();return guestReturn(cpu,1);
}

int campMenuWindowHook(uint32_t address,int,ArmCpuState* cpu){
    if(guestCall||!campMenuActive||cpu->R[0]!=8)return 0;return guestReturn(cpu,callGuest(address,7));
}
int campMenuRowsHook(uint32_t,int,ArmCpuState* cpu){
    if(guestCall||!campMenuActive||r8(0x020370f5)!=8)return 0;
    unsigned row=r8(cpu->R[0]),remaining=cpu->R[1];const auto window=callGuest(fieldSymbol("GetStartMenuWindowId"));const auto buffer=callGuest(fieldSymbol("AllocZeroed"),256);
    do{
        const auto entry=r8(0x020370f6+row);const auto text=r32(fieldSymbol("sStartMenuActionTable")+8*entry);
        if(entry==3||entry==8)callGuest(fieldSymbol("Menu_PrintFormatIntlPlayerName"),window,text,8,row*13);
        else{
            if(entry==7)writeFieldText(buffer,camping()?"PACK UP":"CAMP");else callGuest(fieldSymbol("StringExpandPlaceholders"),buffer,text);
            callGuestWords(fieldSymbol("AddTextPrinterParameterized"),{window,2,buffer,8,row*13,0xff,0});
        }
        ++row;
    }while(row<8&&--remaining);
    bus()->write8(cpu->R[0],uint8_t(row));callGuest(fieldSymbol("Free"),buffer);return guestReturn(cpu,row>=8);
}
int campMenuCursorHook(uint32_t address,int,ArmCpuState* cpu){
    if(guestCall||!campMenuActive||r8(0x020370f5)!=8||cpu->R[0]!=callGuest(fieldSymbol("GetStartMenuWindowId")))return 0;
    const auto sp=cpu->R[13];return guestReturn(cpu,callGuestWords(address,{cpu->R[0],cpu->R[1],cpu->R[2],cpu->R[3],13,r32(sp+4),r32(sp+8)}));
}
