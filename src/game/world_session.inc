// Native, safe-field checkpoints. Guests never need to open the Save menu.
uint64_t nextWorldSave=0,nextFlashCheck=0,nextDeparture=0;
uint32_t seenWorldSaveRequest=0;
bool worldSaveRequested=false,manualWorldSave=false;
std::string lastWorldCheckpoint;
PlayerState lastLocalField;
bool worldExitRequested=false,worldExitReady=false;uint64_t worldExitBegan=0;
uint32_t lastCheckpointSent=0;bool exitCaptured=false;
bool beginWorldExit(){
    if(worldExitReady||!room||!room->status().managedWorld||(!room->status().connected&&!room->status().hosting))return true;
    if(!worldExitRequested){worldExitRequested=true;worldExitBegan=online::chatClock();worldSaveRequested=true;room->requestWorldSave();}
    return false;
}

struct DepartureVisual {uint32_t sequence=0;PlayerState origin;uint64_t began=0;};
std::array<DepartureVisual,MaxRoomPlayers> departures;
void publishDeparture(){
    if(guestCall||!room||!roomState.connected||replaying()||now<nextDeparture)return;
    auto origin=localBattle.id?localBattle.trainer:lastLocalField;
    if(!origin.active)return;nextDeparture=now+300;room->depart(origin);
}
int departureHook(uint32_t,int,ArmCpuState*){publishDeparture();return 0;}
int checkpointSaveHook(uint32_t,int,ArmCpuState*){
    if(!guestCall&&room&&room->status().managedWorld){manualWorldSave=true;worldSaveRequested=false;nextFlashCheck=now+2;}
    return 0;
}
bool departingSlot(unsigned slot){return slot<MaxRoomPlayers&&departures[slot].sequence&&online::chatClock()-departures[slot].began<5000&&sameScene(local,departures[slot].origin);}
void updateWorldSession(){
    if(local.active)lastLocalField=local;
    for(const auto& d:roomState.departures)if(d.slot<MaxRoomPlayers&&d.slot!=roomSlot&&online::chatClock()-d.receivedAt<1400&&d.sequence!=departures[d.slot].sequence){
        departures[d.slot]={d.sequence,d.origin,d.receivedAt};
    }
    if(!room)return;const auto status=room->status();if(!status.managedWorld)return;
    if(status.checkpointRequest!=seenWorldSaveRequest){seenWorldSaveRequest=status.checkpointRequest;if(!status.hosting)worldSaveRequested=true;}
    if(now>=nextWorldSave){worldSaveRequested=true;nextWorldSave=now+900;}
    if(worldExitRequested&&!worldExitReady){
        const bool flushed=exitCaptured&&status.checkpointAck>=lastCheckpointSent&&status.checkpointWaiting==0;
        if((flushed&&online::chatClock()-worldExitBegan>=250)||online::chatClock()-worldExitBegan>=5000){worldExitReady=true;SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);}
    }
    const bool fieldSafe=safeStory()&&validRam(r32(0x03005008),0x3d68)&&validRam(r32(0x0300500c),0xf24)&&r8(0x02024029)<=6&&!r32(0x03003f64);
    const bool safe=fieldSafe&&local.pixelX%16==0&&local.pixelY%16==0&&!fieldUiBusy()&&!noticeAllocation&&!encounterToken&&!walletPendingSave&&!campaignSaving&&pendingReleases.empty()&&!room->cableConnected();
    if(worldSaveRequested&&safe){
        prepareNativeFieldSave();
        if(callGuest(0x080da364+codeOffset(),0,0,0,0,0,20000000)!=1)throw std::runtime_error("World checkpoint failed; progress remains in memory.");
        worldSaveRequested=false;nextFlashCheck=now+2;
    }
    if(now<nextFlashCheck||bus()->save().dirty()||walletPendingSave||campaignSaving)return;
    nextFlashCheck=now+30;
    const auto flash=bus()->save().flash_bytes();
    // No checkpoint of a blank cartridge or incomplete native save sequence.
    if(flash.size()!=131072||std::none_of(flash.begin(),flash.end(),[](uint8_t v){return v!=255;}))return;
    if(!fieldSafe&&!manualWorldSave)return;
    const auto checkpoint=fr::captureCheckpoint(diagnosticPath.parent_path(),flash);const auto hash=fr::digest(checkpoint);
    if(hash!=lastWorldCheckpoint){if(const auto serial=room->storeCheckpoint(checkpoint)){lastCheckpointSent=serial;lastWorldCheckpoint=hash;if(status.hosting&&!worldExitRequested)room->requestWorldSave();}}
    if(worldExitRequested&&!worldSaveRequested)exitCaptured=true;
    manualWorldSave=false;
#ifdef FR_TEST_HARNESS
    std::ofstream f(diagnosticPath.parent_path()/"world-save-check.txt");f<<"frame="<<now<<" managed=1 request="<<status.checkpointRequest<<" ack="<<status.checkpointAck<<" pending="<<worldSaveRequested<<" world="<<status.worldId<<'\n';
#endif
}
void drawDepartures(uint8_t* rgb,unsigned w,unsigned h,int dx,int dy){
    for(unsigned slot=0;slot<MaxRoomPlayers;++slot){
        const auto& d=departures[slot];if(!departingSlot(slot)||!sameScene(local,d.origin))continue;
        auto p=d.origin;const auto elapsed=online::chatClock()-d.began;if(elapsed>=1400)continue;
        constexpr uint8_t turns[]{1,3,2,4};p.facing=turns[(elapsed/85)%4];p.spriteFrame=p.facing==2?1:p.facing>=3?2:0;p.flip=p.facing==4?1:0;p.offsetX=p.offsetY=0;
        const double rise=elapsed>350?double(elapsed-350)/1000.:0.;const int lift=int(360.*rise*rise);
        const int x=p.pixelX+dx+8,y=p.pixelY+dy-lift;
#ifdef FR_TEST_HARNESS
        {std::ofstream f(diagnosticPath.parent_path()/"departure-check.txt");f<<"sequence="<<d.sequence<<" slot="<<slot<<" elapsed="<<elapsed<<" facing="<<unsigned(p.facing)<<" lift="<<lift<<" x="<<x<<" y="<<y<<'\n';}
#endif
        drawingKey=spriteKey(p.elevation,p.pixelY+dy+16,slot);trainer(rgb,w,h,p,x,y);
    }
}
void registerWorldSessionHooks(){
    for(const auto& entry:std::vector<std::pair<uint32_t,decltype(&departureHook)>>{{0x080842e8,departureHook},{0x08085620,departureHook},{0x08085b64,departureHook},{0x0807e500,departureHook},{0x080da364,checkpointSaveHook}}){
        const auto key="pokemulti.world-session-"+std::to_string(entry.first);gba_mod_register_function_entry_plugin(key.c_str(),entry.first+codeOffset(),1,entry.second);gba_mod_set_function_hook_enabled(key.c_str(),1);
    }
}
