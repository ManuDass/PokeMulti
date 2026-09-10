void Launcher::refreshWorlds(){
    if(!profile_||!validated_||profile_->romSha256!=validated_->report.sha256){worlds_.clear();return;}
    worlds_=listWorlds(dataRoot_,profile_->romSha256);
    if(worlds_.empty()){
        auto world=createWorld(dataRoot_,"My first world",profile_->romSha256);
        const auto oldSave=dataRoot_/"saves"/profile_->romSha256/"trainer.sav";
        if(std::filesystem::exists(oldSave)){
            auto identityBytes=readWorldFile(dataRoot_/"identity.cfg",128);std::istringstream in(std::string(identityBytes.begin(),identityBytes.end()));std::string id;in>>id;
            if(id.empty()){id=worldRandomId();atomicWorldFile(dataRoot_/"identity.cfg",{reinterpret_cast<const uint8_t*>(id.data()),id.size()});}
            if(!worldIdValid(id))throw std::runtime_error("Invalid trainer identity.");
            WorldPlayers players(world);players.commit(id,captureCheckpoint(dataRoot_,readWorldFile(oldSave)));
            // Copy shared campaign/transaction records; keep every original in place.
            for(const auto* name:{"wagers-host.cfg","released-world.cfg","campaign-current.cfg"})if(std::filesystem::exists(dataRoot_/name))std::filesystem::copy_file(dataRoot_/name,world.folder/name);
            if(std::filesystem::exists(dataRoot_/"campaigns"))std::filesystem::copy(dataRoot_/"campaigns",world.folder/"campaigns",std::filesystem::copy_options::recursive);
        }
        worlds_.push_back(std::move(world));
    }
    selectedWorld_=std::min(selectedWorld_,worlds_.size()-1);worldPage_=selectedWorld_/3;
}
void Launcher::worldEdit(unsigned index,const std::wstring& value,D2D1_RECT_F rect,bool number){
    auto& edit=worldEdits_[index];
    if(!edit){edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_TABSTOP|ES_AUTOHSCROLL|(number?ES_NUMBER:0),0,0,0,0,window_,reinterpret_cast<HMENU>(INT_PTR(230+index)),instance_,nullptr);SendMessageW(edit,EM_SETLIMITTEXT,index==1?64:64,0);SetWindowTextW(edit,value.c_str());}
    SendMessageW(edit,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE);
    MoveWindow(edit,int(offsetX_+rect.left*scale_),int(offsetY_+rect.top*scale_),int((rect.right-rect.left)*scale_),int((rect.bottom-rect.top)*scale_),TRUE);ShowWindow(edit,SW_SHOW);
}
std::wstring Launcher::worldEditText(unsigned index){wchar_t value[128]{};GetWindowTextW(worldEdits_[index],value,128);return value;}
void Launcher::layoutWorlds(){
    if(page_==Page::Menu){
        if(worlds_.empty()||worlds_.front().romHash!=profile_->romSha256)refreshWorlds();
        for(size_t row=0;row<3&&worldPage_*3+row<worlds_.size();++row){const auto index=worldPage_*3+row;button(240+int(row),widen(worlds_[index].name),D2D1::RectF(472,244+row*48,960,285+row*48),index==selectedWorld_);}
        button(203,L"Previous",D2D1::RectF(472,393,610,421),false,worldPage_>0);button(204,L"Next",D2D1::RectF(822,393,960,421),false,(worldPage_+1)*3<worlds_.size());
        button(Continue,L"Play world",D2D1::RectF(472,438,708,482),true,!gameProcess_);button(Host,L"Host world",D2D1::RectF(724,438,960,482),false,!gameProcess_);
        button(205,L"New world",D2D1::RectF(472,493,708,533));button(Join,L"Join a friend",D2D1::RectF(724,493,960,533));
        button(Settings,L"Settings & ROM",D2D1::RectF(472,545,708,581));button(116,L"Update program",D2D1::RectF(724,545,960,581));return;
    }
    if(page_==Page::NewWorld){
        MoveWindow(nameEdit_,int(offsetX_+472*scale_),int(offsetY_+309*scale_),int(488*scale_),int(48*scale_),TRUE);SendMessageW(nameEdit_,WM_SETFONT,reinterpret_cast<WPARAM>(font_),TRUE);SendMessageW(nameEdit_,EM_SETLIMITTEXT,32,0);ShowWindow(nameEdit_,SW_SHOW);
        button(206,L"Create world",D2D1::RectF(472,390,960,444),true);button(Back,L"Back to worlds",D2D1::RectF(472,463,960,505));return;
    }
    const bool host=page_==Page::HostWorld;
    if(!host){worldEdit(0,L"127.0.0.1",D2D1::RectF(472,266,788,302));button(209,L"Same PC",D2D1::RectF(802,266,960,302));button(208,L"Paste invite",D2D1::RectF(724,410,960,446));}
    worldEdit(1,L"",D2D1::RectF(472,host?266:338,960,host?302:374));
    worldEdit(2,L"38475",D2D1::RectF(472,host?338:410,708,host?374:446),true);
    if(host){worldEdit(3,L"4",D2D1::RectF(724,338,960,374),true);
        const wchar_t* names[]{L"TMs",L"Items",L"Pokemon"};for(unsigned i=0;i<3;++i)button(210+i,std::wstring((worldRewards_&(1<<i))?L"[x] ":L"[ ] ")+names[i],D2D1::RectF(472+i*164,430,624+i*164,467));
    }
    button(207,host?L"Host selected world":L"Join adventure",D2D1::RectF(472,492,960,538),true,!gameProcess_);button(Back,L"Back to worlds",D2D1::RectF(472,549,960,585));
}
void Launcher::paintWorlds(){
    if(page_==Page::Menu){
        text(validated_&&validated_->report.gameCode=="BPGE"?L"YOUR LEAFGREEN WORLDS":L"YOUR FIRERED WORLDS",D2D1::RectF(472,153,960,181),12,accent(),true);text(L"Choose your next adventure.",D2D1::RectF(472,191,960,235),27,Ink,true);
        text(std::to_wstring(worldPage_+1)+L" / "+std::to_wstring((worlds_.size()+2)/3),D2D1::RectF(660,397,780,421),14,Muted);return;
    }
    if(page_==Page::NewWorld){text(L"NEW WORLD",D2D1::RectF(472,153,960,181),12,accent(),true);text(L"A fresh adventure.",D2D1::RectF(472,191,960,235),28,Ink,true);text(L"WORLD NAME",D2D1::RectF(472,278,960,306),12,Muted,true);text(L"Each world keeps its own trainers, teams and story.",D2D1::RectF(472,530,950,580),16,Muted);return;}
    const bool host=page_==Page::HostWorld;
    text(host?L"HOST WORLD":L"JOIN A FRIEND",D2D1::RectF(472,153,960,181),12,accent(),true);
    text(host?widen(worlds_.at(selectedWorld_).name):L"Adventure together.",D2D1::RectF(472,191,960,235),27,Ink,true);
    if(!host)text(L"HOST IPv4 ADDRESS",D2D1::RectF(472,240,960,264),12,Muted,true);
    text(L"ROOM KEY",D2D1::RectF(472,host?240:312,960,host?264:336),12,Muted,true);
    text(L"PORT",D2D1::RectF(472,host?312:384,708,host?336:408),12,Muted,true);
    if(host){text(L"PLAYER LIMIT (2-32)",D2D1::RectF(724,312,960,336),12,Muted,true);text(L"SHARED REWARDS / MONEY STAYS PERSONAL",D2D1::RectF(472,397,960,425),12,Muted,true);}
    if(!host)text(L"Same PC uses 127.0.0.1. Start Host world in the other session.",D2D1::RectF(472,589,960,620),14,Muted);
}
bool Launcher::worldCommand(int id){
    if(id>=240&&id<243&&page_==Page::Menu){selectedWorld_=worldPage_*3+size_t(id-240);layout();return true;}
    if(id==203||id==204){worldPage_=id==203?worldPage_-1:worldPage_+1;layout();return true;}
    if(id==205){page_=Page::NewWorld;SetWindowTextW(nameEdit_,L"New adventure");layout();SetFocus(nameEdit_);return true;}
    if(id==206){wchar_t name[33]{};GetWindowTextW(nameEdit_,name,33);auto created=createWorld(dataRoot_,narrow(name),profile_->romSha256);worlds_.clear();refreshWorlds();for(size_t i=0;i<worlds_.size();++i)if(worlds_[i].id==created.id)selectedWorld_=i;worldPage_=selectedWorld_/3;page_=Page::Menu;layout();return true;}
    if(id>=210&&id<213){worldRewards_^=uint8_t(1u<<(id-210));layout();return true;}
    if(id==207){playGame(page_==Page::HostWorld?1:2);return true;}
    if(id==209&&page_==Page::JoinWorld){SetWindowTextW(worldEdits_[0],L"127.0.0.1");gameFailed_=false;status_=L"Use the same room key and port as the host session on this PC.";layout();return true;}
    if(id==208&&page_==Page::JoinWorld){
        std::wstring value;
        if(OpenClipboard(window_)){if(auto data=GetClipboardData(CF_UNICODETEXT)){if(auto text=static_cast<const wchar_t*>(GlobalLock(data))){value=text;GlobalUnlock(data);}}CloseClipboard();}
        const auto invite=online::parseConnectionInvite(narrow(value));if(!invite)throw std::runtime_error("Copy your friend's PokéMulti invite first.");
        SetWindowTextW(worldEdits_[0],widen(invite->address).c_str());SetWindowTextW(worldEdits_[1],widen(invite->key).c_str());SetWindowTextW(worldEdits_[2],std::to_wstring(invite->port).c_str());return true;
    }
    return false;
}
