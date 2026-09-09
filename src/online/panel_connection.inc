    void pollConnection(const Status& status){
        const bool hosting=status.running&&status.hosting;
        if(hosting&&(!inviteHosting||invitePort!=status.port)){
            inviteLocals=session.localAddresses();customPublic=false;publicOverride.fill(0);publicPort=status.port;
        }
        inviteHosting=hosting;invitePort=status.port;publicAddress.tick(hosting,status.port);
    }
    void pasteConnection(){
        char* clipboard=SDL_GetClipboardText();auto invite=parseConnectionInvite(clipboard?clipboard:"");SDL_free(clipboard);
        if(!invite){error="Copy an invitation from your host, then choose Paste invite.";return;}
        std::copy(invite->address.begin(),invite->address.end(),address.begin());address[invite->address.size()]=0;
        std::copy(invite->key.begin(),invite->key.end(),key.begin());key[invite->key.size()]=0;
        auto value=std::to_string(invite->port);std::copy(value.begin(),value.end(),port.begin());port[value.size()]=0;error.clear();
    }
    void connectionDetails(const Status& status){
        ImGui::SetNextWindowSize({450,0},ImGuiCond_Appearing);
        const auto display=ImGui::GetIO().DisplaySize;ImGui::SetNextWindowPos({display.x*.5f,display.y*.5f},ImGuiCond_Always,{.5f,.5f});
        ImGui::SetNextWindowSizeConstraints({450,0},{450,std::max(200.f,ImGui::GetIO().DisplaySize.y-32)});
        bool open=true;if(!ImGui::BeginPopupModal("Room connection",&open,ImGuiWindowFlags_AlwaysAutoResize))return;
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{8,5});
        if(!status.running){ImGui::CloseCurrentPopup();ImGui::PopStyleVar();ImGui::EndPopup();return;}
        auto copy=[&](const ConnectionInvite& invite,bool internet){auto text=connectionInvite(invite,internet);if(!text.empty()&&SDL_SetClipboardText(text.c_str())==0)inviteNotice="Copied. Friends can use Paste invite on Join a room.";};
        if(status.hosting){
            heading("INTERNET FRIENDS");
            const auto& detected=publicAddress.address();
            if(publicAddress.state()==PublicAddressLookup::State::Looking)paragraph("Finding your public IPv4 address...");
            else if(detected.empty())paragraph("Could not detect a public IPv4. Retry or enter one below.");
            else ImGui::Text("Public IPv4: %s",detected.c_str());
            if(ImGui::SmallButton("Refresh address")){publicAddress.refresh();inviteNotice.clear();}
            ImGui::Checkbox("Custom public address / port",&customPublic);
            if(customPublic){ImGui::SetNextItemWidth(-1);ImGui::InputTextWithHint("##publicAddress","Public IPv4",publicOverride.data(),publicOverride.size());ImGui::SetNextItemWidth(-110);ImGui::InputInt("Public port",&publicPort,0,0);}
            const std::string ip=customPublic?publicOverride.data():detected;
            const int external=customPublic?publicPort:status.port;
            const bool ready=validConnectionIPv4(ip,true)&&external>0&&external<=65535;
            if(customPublic&&!ready)paragraph("Enter a public IPv4 address and a port from 1 to 65535.");
            ImGui::Text("TCP port: %d",external);ImGui::TextWrapped("Room key: %s",key.data());
            ImGui::BeginDisabled(!ready);if(primary("Copy internet invite",{-1,30}))copy({ip,key.data(),uint16_t(external)},true);ImGui::EndDisabled();
            paragraph("Outside access is not verified. Your router and firewall must allow this room's TCP port.");
            if(ImGui::CollapsingHeader("Internet connection help")){
                ImGui::TextWrapped("Forward external TCP port %d to this PC on TCP port %u, and allow PokeMulti through Windows Firewall.",external,status.port);
                paragraph("Use this PC's LAN address listed below for the forwarding destination. Friends use your public address. Players on this PC use 127.0.0.1.");
                paragraph("If your provider uses shared addressing (CGNAT), a public-IP lookup cannot open inbound access. Ask your provider for a public IPv4, or use a VPN that supports incoming connections. A second router may need forwarding too.");
                ImGui::TextDisabled("Address lookup: ipify over HTTPS");
            }
            if(ImGui::CollapsingHeader("LAN / VPN / this PC")){
                paragraph("Only share these with players on that same network.");
                for(const auto& local:inviteLocals){if(local=="127.0.0.1")continue;ImGui::PushID(local.c_str());ImGui::TextUnformatted(local.c_str());ImGui::SameLine();if(ImGui::SmallButton("Copy LAN invite"))copy({local,key.data(),status.port},false);ImGui::PopID();}
                if(ImGui::Button("Copy same-PC invite",{-1,28}))copy({"127.0.0.1",key.data(),status.port},false);
            }
        }else{
            heading("YOUR HOST");ImGui::TextWrapped("%s : %u",address.data(),status.port);ImGui::TextWrapped("Room key: %s",key.data());
            if(ImGui::Button("Copy host connection",{-1,30}))copy({address.data(),key.data(),status.port},false);
            paragraph("Ask the host for their Internet invite when sharing with friends outside their network.");
        }
        if(!inviteNotice.empty())paragraph(inviteNotice.c_str());
        if(ImGui::Button("Done",{-1,30}))ImGui::CloseCurrentPopup();
        ImGui::PopStyleVar();ImGui::EndPopup();
    }
