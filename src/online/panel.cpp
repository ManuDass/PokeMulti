#include "online/panel.hpp"
#include "online/connection.hpp"
#include "platform/text.hpp"
#include "platform/image.hpp"
#include "game/world.hpp"
#include "game/labels.hpp"
#include "input/pokeball.hpp"
#include "frontend/ball_model.hpp"
#include <imgui_internal.h>
#include <windows.h>
#include <shellapi.h>
#include <SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <fstream>
#include <future>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
namespace fr::online {
namespace {
void atomicText(const std::filesystem::path& path,const std::string& text){
    const auto temp=path.wstring()+L".tmp";
    {std::ofstream file(temp,std::ios::binary|std::ios::trunc);file<<text;file.flush();if(!file)throw std::runtime_error("Could not write profile settings");}
    if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Could not replace profile settings");
}
constexpr ImU32 ink=IM_COL32(28,34,44,255),muted=IM_COL32(123,132,142,255),cream=IM_COL32(246,244,237,255),red=IM_COL32(222,66,64,255),mint=IM_COL32(104,215,166,255);
ImVec4 color(ImU32 c){return ImGui::ColorConvertU32ToFloat4(c);}
void label(ImFont* font,float size,ImVec2 p,ImU32 c,const std::string& text){ImGui::GetWindowDrawList()->AddText(font,(size<14?8.f:std::round(size/4)*4),{std::floor(p.x),std::floor(p.y)},c,text.c_str());}

}
std::string loadIdentity(const std::filesystem::path& folder){
    std::filesystem::create_directories(folder);const auto path=folder/"identity.cfg";
    if(std::filesystem::exists(path)){std::ifstream f(path);std::string id;f>>id;if(!validIdentity(id,"Trainer"))throw std::runtime_error("Trainer identity file is invalid");return id;}
    const auto id=randomId();atomicText(path,id+"\n");return id;
}
struct Panel::Impl {
    Session& session;std::filesystem::path data;std::string name,gameLabel;
    bool leaf=false;ImU32 accent=red,accentHover=IM_COL32(236,82,77,255),accentPressed=IM_COL32(195,48,47,255);
    int shinyDraft=8192;uint32_t shownShinyRate=0;
    SDL_Texture* logo=nullptr;
    input::Pokeball ball;input::BallControls ballControls;input::BallMapping ballMapping;
    BallModel ballModel;SDL_Texture* ballTexture=nullptr;std::string ballModelError;
    bool ballModelTried=false,ballModelDirty=true;float ballYaw=-.95f,ballPitch=.35f;
    int optionsTab=0,ballSelected=0;uint64_t ballSelectedAddress=0;
    bool resumeAfterBallConnect=false;uint64_t ballTraceTime=0;
    uint16_t ballLastKeys=0x3ff;std::string ballInputHint;
    bool windowFocused()const{
#ifdef FR_TEST_HARNESS
        if(ballTest&&window&&std::string(SDL_GetCurrentVideoDriver())=="dummy")return ballTestWindowFocused;
#endif
        return window&&(SDL_GetWindowFlags(window)&SDL_WINDOW_INPUT_FOCUS);
    }
    void resumeGame(){
        resumeAfterBallConnect=false;focusChat=false;gameFocus=true;
        ImGui::ClearActiveID();ImGui::GetIO().ConfigFlags&=~ImGuiConfigFlags_NavEnableKeyboard;
        ballControls.reset();
    }
    void beginBallConnection(uint64_t address){
        resumeAfterBallConnect=true;ballControls.reset();
#ifdef FR_TEST_HARNESS
        if(ballTest){ballTestState={};ballTestState.phase=input::BallPhase::Connecting;return;}
#endif
        ball.connect(address);
    }
    void finishBallConnection(const input::BallStatus& state){
        if(!resumeAfterBallConnect)return;
        if(state.phase==input::BallPhase::Connected){
            resumeAfterBallConnect=false;
            // Only finish the connection interaction that the player requested.
            // Do not steal focus from chat, another app or a later UI action.
            if(windowFocused()&&!ImGui::GetIO().WantTextInput&&!ImGui::IsAnyItemActive()&&tab==2&&optionsTab==1)resumeGame();
        }else if(state.phase==input::BallPhase::Error||state.phase==input::BallPhase::Idle)resumeAfterBallConnect=false;
    }
#ifdef FR_TEST_HARNESS
    bool ballTest=false,ballTestWindowFocused=true;input::BallStatus ballTestState;float ballResumeX=0,ballResumeY=0;
#endif
    input::BallStatus ballState()const{
#ifdef FR_TEST_HARNESS
        if(ballTest)return ballTestState;
#endif
        return ball.status();
    }
    uint16_t controllerKeys(){
        const auto state=ballState();const auto now=input::ballTime();
        const bool foreground=context&&windowFocused();
        const bool typing=context&&ImGui::GetIO().WantTextInput;
        const bool focused=foreground&&gameFocus&&!typing;
        const bool fresh=state.phase==input::BallPhase::Connected&&state.lastReport&&now-state.lastReport<=750;
        const auto keys=ballControls.keys(state.sample,now,focused&&fresh,ballMapping);ballLastKeys=keys;
        ballInputHint=!fresh?"Waiting for controller input":!foreground?"Paused / select this game window":typing?"Paused while typing":!gameFocus?"Paused / resume the game":!ballControls.armed()?"Release the stick and both buttons":"Game controls active";
        // One bounded snapshot per second makes real controller reports and
        // focus/neutral gating diagnosable without a special test executable.
        if((state.phase!=input::BallPhase::Idle||ballTraceTime)&&now-ballTraceTime>=1000){
            ballTraceTime=now;std::ostringstream trace;
            trace<<"phase="<<int(state.phase)<<" reports="<<state.reportCount<<" age_ms="<<(state.lastReport?now-state.lastReport:0)
                <<" window_focus="<<foreground<<" game_focus="<<gameFocus<<" typing="<<typing<<" armed="<<ballControls.armed()
                <<" stick="<<state.sample.x<<","<<state.sample.y<<" top="<<state.sample.top<<" click="<<state.sample.stick<<" keys="<<keys
                <<" reason="<<ballInputHint<<"\n";
            try{atomicText(data/"controller-status.txt",trace.str());}catch(const std::exception&){}
        }
        return keys;
    }
    void saveBallMapping(){ballControls.reset();atomicText(data/"controller.cfg",std::to_string(ballMapping.deadzone)+" "+std::to_string(ballMapping.swapButtons)+" "+std::to_string(ballMapping.invertY)+"\n");}

    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;ImGuiContext* context=nullptr;
    ImFont* regular=nullptr;ImFont* bold=nullptr;ImFont* mono=nullptr;ImFont* chatFont=nullptr;
    struct Friend {std::string id,name;};std::vector<Friend> friends;
    bool follow=true,wild=true,sidebar=true,hostMode=true,autoHost=false,capture=false,gameFocus=true;
    int roomCapacity=4;
    bool focusChat=false;int gameVolume=100;
    std::array<char,ChatLimit+1> draft{};
    std::string chatError;
    uint32_t shownChat=0;
    bool wasConnected=false;
    int tab=0;uint8_t rewardPolicy=game::DefaultRewardSharing;std::string selected,error,notice;std::future<std::string> operation;
    PublicAddressLookup publicAddress;bool inviteHosting=false,customPublic=false;uint16_t invitePort=0;int publicPort=0;
    std::array<char,16> publicOverride{};std::vector<std::string> inviteLocals;std::string inviteNotice;
    std::array<char,65> address{},key{};std::array<char,8> port{};
    float viewportX=0,viewportY=0,viewportW=0,viewportH=0,sidebarScroll=0;
#ifdef FR_TEST_HARNESS
    uint64_t uiCommand=0,uiFrames=0;int uiPhase=0;std::string uiOperation,uiText;float uiX=0,uiY=0;
    void testInput(){
        auto& io=ImGui::GetIO();
        if(uiPhase==0){std::ifstream f(data/"test-ui-input.txt");uint64_t seq=0;std::string op;
            if(f>>seq>>op&&seq!=uiCommand){uiCommand=seq;uiOperation=op;uiPhase=1;
                if(op=="click"||op=="resize"||op=="drag")f>>uiX>>uiY;else if(op=="text"||op=="clipboard"||op=="ball-report")f>>std::quoted(uiText);
            }
        }
        if(uiPhase){
            if(uiOperation=="shiny-check"){
                game::requestFixture("shiny-check");uiPhase=-1;
            }else if(uiOperation=="ball-connect"){
                ballTest=true;tab=2;optionsTab=1;gameFocus=false;beginBallConnection(0);uiPhase=-1;
            }else if(uiOperation=="ball-blur"){
                ballTestWindowFocused=false;SDL_Event e{};e.type=SDL_WINDOWEVENT;e.window.event=SDL_WINDOWEVENT_FOCUS_LOST;event(&e);uiPhase=-1;
            }else if(uiOperation=="ball-focus"){
                ballTestWindowFocused=true;SDL_Event e{};e.type=SDL_WINDOWEVENT;e.window.event=SDL_WINDOWEVENT_FOCUS_GAINED;event(&e);uiPhase=-1;
            }else if(uiOperation=="ball-report"){
                std::vector<uint8_t> report;std::istringstream in(uiText);unsigned b;while(in>>std::hex>>b)report.push_back(uint8_t(b));
                if(auto sample=input::decodeBall(report)){ballTest=true;ballTestState.phase=input::BallPhase::Connected;ballTestState.message="Test input";ballTestState.sample=*sample;ballTestState.lastReport=input::ballTime();++ballTestState.reportCount;}
                uiPhase=-1;
            }else if(uiOperation=="ball-drop"){ballTest=true;ballTestState={};uiPhase=-1;}
            else if(uiOperation=="controller-page"){tab=2;optionsTab=1;gameFocus=false;uiPhase=-1;}
            else if(uiOperation=="click"){resumeAfterBallConnect=false;io.AddMousePosEvent(uiX,uiY);io.AddMouseButtonEvent(0,uiPhase==1);gameFocus=uiX>=viewportX&&uiX<viewportX+viewportW&&uiY>=viewportY&&uiY<viewportY+viewportH;if(uiPhase==2)uiPhase=-1;}
            else if(uiOperation=="drag"){
                SDL_Event e{};
                if(uiPhase==2){e.type=SDL_MOUSEMOTION;e.motion.xrel=int(uiX);e.motion.yrel=int(uiY);e.motion.x=int(viewportX+viewportW/2);e.motion.y=int(viewportY+viewportH/2);}
                else {e.type=uiPhase==1?SDL_MOUSEBUTTONDOWN:SDL_MOUSEBUTTONUP;e.button.button=SDL_BUTTON_RIGHT;e.button.x=int(viewportX+viewportW/2);e.button.y=int(viewportY+viewportH/2);}
                event(&e);if(uiPhase==3)uiPhase=-1;
            }
            else if(uiOperation=="text"){
                if(uiPhase==1){io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);}
                if(uiPhase==2){io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);}
                if(uiPhase==3){io.AddInputCharactersUTF8(uiText.c_str());uiPhase=-1;}
            }else if(uiOperation=="enter"){
                io.AddKeyEvent(ImGuiKey_Enter,uiPhase==1);if(uiPhase==2)uiPhase=-1;
            }else if(uiOperation=="chat"){focusChat=true;gameFocus=false;uiPhase=-1;}else if(uiOperation=="disconnect"){session.disconnectCable();uiPhase=-1;}
            else if(uiOperation=="clipboard"){SDL_SetClipboardText(uiText.c_str());uiPhase=-1;}
            else if(uiOperation=="clipboard-read"){char* value=SDL_GetClipboardText();atomicText(data/"test-clipboard.txt",value?value:"");SDL_free(value);uiPhase=-1;}
            else if(uiOperation=="resize"){SDL_SetWindowSize(window,int(uiX),int(uiY));uiPhase=-1;}
            else {SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=uiOperation=="camp"?SDLK_g:uiOperation=="trade"?SDLK_t:uiOperation=="escape"?SDLK_ESCAPE:uiOperation=="f2"?SDLK_F2:uiOperation=="f6"?SDLK_F6:uiOperation=="f11"?SDLK_F11:SDLK_F12;event(&e);uiPhase=-1;}
            ++uiPhase;
        }
    }
#endif
    Impl(Session& s,std::filesystem::path p,bool visible,bool host,std::string n,const std::string& code):session(s),data(std::move(p)),name(std::move(n)),gameLabel(code=="BPGE"?"LEAFGREEN / GAME BOY ADVANCE":"FIRERED / GAME BOY ADVANCE"),hostMode(!visible||host),autoHost(host){
        leaf=code=="BPGE";
        if(leaf){accent=IM_COL32(55,131,75,255);accentHover=IM_COL32(66,151,87,255);accentPressed=IM_COL32(40,102,55,255);}
        strcpy_s(address.data(),address.size(),"127.0.0.1");strcpy_s(port.data(),port.size(),"38475");
        strcpy_s(key.data(),key.size(),randomId().substr(0,12).c_str());
        if(session.status().connected){strcpy_s(key.data(),key.size(),session.connectionKey().c_str());strcpy_s(port.data(),port.size(),std::to_string(session.status().port).c_str());roomCapacity=session.status().capacity;}
        const auto file=data/"friends.cfg";
        if(std::filesystem::exists(file)){
            if(std::filesystem::file_size(file)>65536)throw std::runtime_error("Friends file is too large");
            std::ifstream f(file);Friend next;
            while(f>>std::quoted(next.id)>>std::quoted(next.name)){if(!validIdentity(next.id,next.name)||friends.size()>=200)throw std::runtime_error("Invalid friends file");friends.push_back(next);}
            if(!f.eof())throw std::runtime_error("Malformed friends file");
        }
        {std::ifstream audio(data/"audio.cfg");int value=100;if(audio>>value)gameVolume=std::clamp(value,0,100);}
        {std::ifstream controls(data/"controller.cfg");int zone=25,swap=0,invert=0;if(controls>>zone>>swap>>invert&&zone>=10&&zone<=60&&(swap==0||swap==1)&&(invert==0||invert==1))ballMapping={zone,swap!=0,invert!=0};}
        const auto settings=data/"world.cfg";
        if(std::filesystem::exists(settings)){
            std::ifstream f(settings);int a=1,b=1;
            if(!(f>>a>>b)||(a!=0&&a!=1)||(b!=0&&b!=1))throw std::runtime_error("Invalid world settings");follow=a!=0;wild=b!=0;
        }
    }
    ~Impl(){if(operation.valid())operation.wait();close();}
    void close(){ball.disconnect();ballControls.reset();if(ballTexture){SDL_DestroyTexture(ballTexture);ballTexture=nullptr;ballModelDirty=true;}if(logo){SDL_DestroyTexture(logo);logo=nullptr;}if(!context)return;ImGui::SetCurrentContext(context);ImGui_ImplSDLRenderer2_Shutdown();ImGui_ImplSDL2_Shutdown();ImGui::DestroyContext(context);context=nullptr;window=nullptr;renderer=nullptr;}
    void open(void* w,void* r){
        window=static_cast<SDL_Window*>(w);renderer=static_cast<SDL_Renderer*>(r);
        SDL_SetWindowTitle(window,("Pok\xc3\xa9Multi | "+name).c_str());
        const auto icon=readImage(executableFolder()/"Program_Icon.png");
        auto* iconSurface=SDL_CreateRGBSurfaceWithFormatFrom(const_cast<uint32_t*>(icon.pixels.data()),icon.width,icon.height,32,icon.width*4,SDL_PIXELFORMAT_RGBA32);
        if(!iconSurface)throw std::runtime_error("Cannot load application icon");
        SDL_SetWindowIcon(window,iconSurface);logo=SDL_CreateTextureFromSurface(renderer,iconSurface);SDL_FreeSurface(iconSurface);
        if(!logo)throw std::runtime_error("Cannot create application icon texture");
        SDL_SetTextureScaleMode(logo,SDL_ScaleModeNearest);
        SDL_SetWindowMinimumSize(window,940,650);
        SDL_Rect screen{};SDL_GetDisplayUsableBounds(SDL_GetWindowDisplayIndex(window),&screen);
        SDL_SetWindowSize(window,std::min(1140,std::max(940,screen.w-60)),std::min(760,std::max(650,screen.h-80)));
#ifdef FR_TEST_HARNESS
        if(std::string(SDL_GetCurrentVideoDriver())=="dummy")SDL_SetWindowSize(window,1140,760);
#endif
        SDL_SetWindowPosition(window,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED);
        context=ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
        io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
        ImFontConfig pixels;pixels.OversampleH=pixels.OversampleV=1;pixels.PixelSnapH=true;
        const auto dir=game::fontFolder();
        auto font=[&](const char* file,float size){auto* result=io.Fonts->AddFontFromFileTTF(narrow((dir/file).wstring()).c_str(),size,&pixels);if(!result)throw std::runtime_error("Cannot load supplied UI font");return result;};
        regular=font("PixelOperator.ttf",16);bold=font("PixelOperator-Bold.ttf",24);mono=font("PixelOperator8.ttf",16);
        chatFont=font("PixelOperator.ttf",20);
        if(!chatFont)throw std::runtime_error("Cannot load chat font");
        io.FontDefault=regular;
        ImGui::StyleColorsLight();auto& st=ImGui::GetStyle();st.WindowRounding=14;st.ChildRounding=10;st.FrameRounding=7;st.PopupRounding=10;st.ScrollbarRounding=6;st.GrabRounding=6;
        st.WindowPadding={14,14};st.FramePadding={10,6};st.ItemSpacing={8,7};st.WindowBorderSize=0;st.ChildBorderSize=0;st.FrameBorderSize=1;
        st.Colors[ImGuiCol_Text]=color(ink);st.Colors[ImGuiCol_TextDisabled]=color(muted);
        st.Colors[ImGuiCol_WindowBg]=color(leaf?IM_COL32(243,247,237,255):cream);st.Colors[ImGuiCol_ChildBg]=st.Colors[ImGuiCol_WindowBg];
        st.Colors[ImGuiCol_FrameBg]=color(IM_COL32(255,255,251,255));st.Colors[ImGuiCol_FrameBgHovered]=color(IM_COL32(242,230,220,255));st.Colors[ImGuiCol_FrameBgActive]=color(IM_COL32(242,230,220,255));
        st.Colors[ImGuiCol_Border]=color(IM_COL32(220,219,210,255));st.Colors[ImGuiCol_Button]=color(IM_COL32(227,226,217,255));st.Colors[ImGuiCol_ButtonHovered]=color(IM_COL32(218,217,207,255));st.Colors[ImGuiCol_ButtonActive]=color(IM_COL32(207,206,195,255));
        st.Colors[ImGuiCol_CheckMark]=color(accent);st.Colors[ImGuiCol_SliderGrab]=color(accent);st.Colors[ImGuiCol_SliderGrabActive]=color(accentPressed);st.Colors[ImGuiCol_Header]=color(IM_COL32(239,217,204,255));st.Colors[ImGuiCol_HeaderHovered]=color(IM_COL32(239,225,214,255));st.Colors[ImGuiCol_HeaderActive]=color(IM_COL32(235,211,200,255));
        if(leaf){
            st.Colors[ImGuiCol_FrameBgHovered]=st.Colors[ImGuiCol_FrameBgActive]=color(IM_COL32(223,237,216,255));
            st.Colors[ImGuiCol_Header]=color(IM_COL32(207,229,196,255));st.Colors[ImGuiCol_HeaderHovered]=color(IM_COL32(219,235,212,255));st.Colors[ImGuiCol_HeaderActive]=color(IM_COL32(192,216,180,255));
        }
        ImGui_ImplSDL2_InitForSDLRenderer(window,renderer);ImGui_ImplSDLRenderer2_Init(renderer);
    }
    bool busy(){return operation.valid();}
    template<class F> void async(F action){if(busy())return;error.clear();notice.clear();operation=std::async(std::launch::async,[action]{try{action();return std::string{};}catch(const std::exception& e){return std::string(e.what());}});}
    template<class F> void guard(F action){try{action();error.clear();}catch(const std::exception& e){error=e.what();}}
    void connect(){
        guard([&]{size_t used=0;unsigned value=std::stoul(port.data(),&used);if(!value||value>65535||used!=strlen(port.data()))throw std::runtime_error("Use a port from 1 to 65535.");
            const std::string host=address.data(),secret=key.data();const bool hosting=hostMode;const auto rewards=rewardPolicy;const bool fresh=false;const auto capacity=uint8_t(roomCapacity);
            async([this,host,secret,hosting,value,rewards,fresh,capacity]{if(hosting)session.host(uint16_t(value),secret,rewards,fresh,capacity);else session.join(host,uint16_t(value),secret);});});
    }
    void saveFriends(){std::ostringstream text;for(const auto& f:friends)text<<std::quoted(f.id)<<' '<<std::quoted(f.name)<<'\n';atomicText(data/"friends.cfg",text.str());}
    bool primary(const char* text,ImVec2 size={-1,38}){
        ImGui::PushStyleColor(ImGuiCol_Button,color(accent));ImGui::PushStyleColor(ImGuiCol_ButtonHovered,color(accentHover));ImGui::PushStyleColor(ImGuiCol_ButtonActive,color(accentPressed));ImGui::PushStyleColor(ImGuiCol_Text,color(cream));
        bool clicked=ImGui::Button(text,size);ImGui::PopStyleColor(4);return clicked;
    }
    void heading(const char* text){ImGui::PushFont(mono);ImGui::TextDisabled("%s",text);ImGui::PopFont();}
    void paragraph(const char* text){ImGui::PushStyleColor(ImGuiCol_Text,color(muted));ImGui::TextWrapped("%s",text);ImGui::PopStyleColor();}
    void rewardSettings(const Status& status){
        const bool editable=!status.running;
        if(!editable)rewardPolicy=status.rewardPolicy;
        if(ImGui::Button("Session rewards",{-1,28}))ImGui::OpenPopup("Session rewards");
        ImGui::SetNextWindowSize({380,0},ImGuiCond_Appearing);
        if(ImGui::BeginPopupModal("Session rewards",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
            paragraph(editable?"Choose which rewards every trainer can receive in this room.":"These settings were chosen by the host for this session.");
            ImGui::BeginDisabled(!editable);
            for(const auto& rule:std::array<std::pair<uint8_t,const char*>,3>{{{game::ShareTMs,"Gym and story TMs"},{game::ShareStoryItems,"Story items"},{game::ShareSpecialPokemon,"Pokemon"}}}){
                bool on=(rewardPolicy&rule.first)!=0;if(ImGui::Checkbox(rule.second,&on))rewardPolicy=uint8_t(on?rewardPolicy|rule.first:rewardPolicy&~rule.first);
            }
            ImGui::EndDisabled();
            paragraph("Unchecked optional rewards go only to their claimant. Essential quest tools stay available to everyone. Money is always individual. Battle wagers require both players' agreement.");
            if(primary("Done",{-1,30}))ImGui::CloseCurrentPopup();ImGui::EndPopup();
        }
    }
#include "panel_connection.inc"
    void room(const Status& status){
        if(status.localWorld||(!status.running&&status.managedWorld)){
            paragraph("Choose Host world or Join a friend from the launcher.");
            if(primary("Return to world selection")){SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);}return;
        }
        if(!status.running){
            ImGui::PushFont(bold);ImGui::TextUnformatted("Better together.");ImGui::PopFont();
            paragraph("Shared story. Personal teams and money.");ImGui::Spacing();
            const float half=(ImGui::GetContentRegionAvail().x-10)/2;
            if(ImGui::Selectable("Host a room",hostMode,0,{half,28}))hostMode=true;ImGui::SameLine();if(ImGui::Selectable("Join a room",!hostMode,0,{half,28}))hostMode=false;
            ImGui::BeginDisabled(busy());
            if(!hostMode){heading("HOST ADDRESS");ImGui::SameLine();if(ImGui::SmallButton("Paste invite"))pasteConnection();ImGui::SetNextItemWidth(-1);ImGui::InputText("##address",address.data(),address.size());}
            heading("ROOM KEY");ImGui::SetNextItemWidth(-1);ImGui::InputText("##key",key.data(),key.size());
            heading("PORT");ImGui::SetNextItemWidth(-1);ImGui::InputText("##port",port.data(),port.size(),ImGuiInputTextFlags_CharsDecimal);
            if(hostMode){ImGui::SetNextItemWidth(-1);ImGui::SliderInt("Players",&roomCapacity,2,int(MaxRoomPlayers));}
            if(primary(busy()?"Connecting...":hostMode?"Open room":"Join adventure"))connect();
            ImGui::EndDisabled();ImGui::Spacing();
            paragraph(hostMode?"Share your address, port and key with your friends.":"Use the same room key as your host. For two sessions on this PC, use 127.0.0.1.");
            ImGui::Separator();ImGui::TextWrapped("Trainer: %s",name.c_str());
        }else{
            ImGui::TextUnformatted(status.hosting?"Hosting your adventure":status.connected?"Connected to host":"Joining adventure...");
            if(ImGui::Button(status.hosting?"Invite friends":"Room connection",{-1,28})){inviteNotice.clear();ImGui::OpenPopup("Room connection");}
            ImGui::Spacing();heading("TRAINERS IN THIS ROOM");
            if(!status.world.storyReady)paragraph("Choose your own starter, then share the adventure.");
            if(ImGui::Button("Campaign journal",{-1,28})){game::showCampaignJournal();gameFocus=true;}
            const auto rewards=game::unclaimedStoryRewards();
            if(!rewards.empty())paragraph("Rewards are waiting for space in your Bag.");
            ImGui::BeginChild("Trainer list",{0,std::min(176.f,float(status.peers.size())*36.f)},ImGuiChildFlags_None);
            for(const auto& p:status.peers){
                ImGui::PushID(p.id.c_str());const auto start=ImGui::GetCursorScreenPos();
                const bool self=p.slot==status.slot;const bool chosen=selected==p.id;
                if(ImGui::Selectable("##trainer",chosen,0,{0,32})&&!self)selected=p.id;
                auto* d=ImGui::GetWindowDrawList();constexpr ImU32 dots[]{red,IM_COL32(78,142,218,255),IM_COL32(79,160,111,255),IM_COL32(172,115,204,255)};
                d->AddRectFilled({start.x+8,start.y+4},{start.x+32,start.y+28},dots[p.slot%4],8);
                label(mono,13,{start.x+16,start.y+9},cream,std::to_string(p.slot+1));
                // Clip long trainer names to the card rather than the neighboring game view.
                d->PushClipRect({start.x+42,start.y},{start.x+ImGui::GetContentRegionAvail().x-2,start.y+32},true);
                label(regular,16,{start.x+42,start.y},ink,p.name+(self?"  (you)":""));
                label(mono,11,{start.x+42,start.y+18},muted,status.world.battles[p.slot].id?"BATTLING":status.world.camps[p.slot].id?"CAMPING":p.partner>=0?"CABLE CONNECTED":p.player.active?"EXPLORING KANTO":"IN GAME");d->PopClipRect();ImGui::PopID();
            }
            ImGui::EndChild();
            const auto peer=std::find_if(status.peers.begin(),status.peers.end(),[&](const Peer& p){return p.id==selected&&p.slot!=status.slot;});
            const bool valid=peer!=status.peers.end();
            if(!status.wager.active()){
            ImGui::BeginDisabled(!valid||busy());
            if(primary("Add to friends",{-1,28})&&valid)guard([&]{if(std::none_of(friends.begin(),friends.end(),[&](const Friend& f){return f.id==peer->id;})){if(friends.size()>=200)throw std::runtime_error("Friends list is full.");friends.push_back({peer->id,peer->name});saveFriends();}notice="Friend saved.";});
            const float half=(ImGui::GetContentRegionAvail().x-10)/2;
            ImGui::BeginDisabled(valid&&peer->partner>=0);
            if(ImGui::Button("Battle",{half,30})&&valid){game::challengePlayer(peer->slot);gameFocus=true;}ImGui::SameLine();
            if(ImGui::Button("Trade",{half,30})&&valid)guard([&]{session.invite(peer->slot,Activity::Trade);notice="Trade invitation sent.";});
            ImGui::EndDisabled();ImGui::EndDisabled();}
            if(!valid&&!status.wager.active())paragraph("Select a trainer for battle or trade.");
            if(status.wager.active()){
                ImGui::Separator();ImGui::Text("Wager: P%u each",status.wager.stake);
                const auto phase=status.wager.phase;
                paragraph(phase==WagerPhase::Reserving?"Saving deposits...":phase==WagerPhase::Ready?"Deposits ready. Preparing your battle.":phase==WagerPhase::Battling?"Battle in progress":phase==WagerPhase::Refund?"Refund pending in the overworld":"Battle complete. Payout saves in the field");
                if((phase==WagerPhase::Reserving||phase==WagerPhase::Ready)&&ImGui::SmallButton("Cancel wager"))guard([&]{session.wagerEvent(status.wager.id,5);});
            }
            if(!game::walletNotice().empty()&&(!status.wager.active()||status.wager.terminal()))paragraph(game::walletNotice().c_str());
            if(session.cableConnected()){
                if(!status.wager.active())paragraph("Battles start in the field. Trading uses the upstairs Cable Club.");
                if(ImGui::Button("Disconnect cable",{-1,26}))guard([&]{session.disconnectCable();});
            }
            ImGui::Spacing();ImGui::BeginDisabled(busy());if(ImGui::Button(status.managedWorld?"Save & exit world":"Leave room",{-1,26})){if(status.managedWorld){SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);}else async([this]{session.stop();});}ImGui::EndDisabled();
        }
        connectionDetails(status);
    }
    void friendsPage(const Status& status){
        ImGui::PushFont(bold);ImGui::TextUnformatted("Your people.");ImGui::PopFont();paragraph("Friends are shown online when they are in this room.");ImGui::Spacing();
        if(friends.empty()){heading("A NEW JOURNEY");paragraph("Join a room, select another trainer and add them to your friends.");}
        for(size_t i=0;i<friends.size();++i){const auto f=friends[i];ImGui::PushID(f.id.c_str());
            auto peer=std::find_if(status.peers.begin(),status.peers.end(),[&](const Peer& p){return p.id==f.id;});const bool online=peer!=status.peers.end();
            ImGui::TextWrapped("%s",f.name.c_str());ImGui::TextColored(color(online?IM_COL32(43,133,96,255):muted),"%s",online?"In your room":"Offline");
            if(online&&ImGui::Button("View trainer")){selected=f.id;tab=0;}
            if(online)ImGui::SameLine();
            if(ImGui::SmallButton("Remove")){guard([&]{friends.erase(friends.begin()+i);saveFriends();});ImGui::PopID();break;}
            ImGui::Separator();ImGui::PopID();
        }
    }
    void controllerPage(){
        const auto state=ballState();finishBallConnection(state);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{8,4});
        if(!ballModelTried){ballModelTried=true;try{
            const auto path=localAsset(std::filesystem::path(L"Pok\u00e9 Ball Plus")/"ob0008_00_gadgets.dae");
            if(path.empty())ballModelError="Model folder not found";else ballModel.load(path);
        }catch(const std::exception& e){ballModelError=e.what();}}
        if(ballModelDirty&&!ballModel.empty()){
            ballModelDirty=false;const auto preview=ballModel.render(ballYaw,ballPitch,280,156);
            if(!ballTexture)ballTexture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,preview.width,preview.height);
            if(ballTexture){SDL_SetTextureBlendMode(ballTexture,SDL_BLENDMODE_BLEND);SDL_SetTextureScaleMode(ballTexture,SDL_ScaleModeLinear);SDL_UpdateTexture(ballTexture,nullptr,preview.pixels.data(),preview.width*4);}
        }
        const auto p=ImGui::GetCursorScreenPos();const float width=ImGui::GetContentRegionAvail().x;const float heroHeight=ImGui::GetIO().DisplaySize.y<720?118.f:156.f;auto* draw=ImGui::GetWindowDrawList();
        draw->AddRectFilled(p,{p.x+width,p.y+heroHeight},IM_COL32(228,232,226,255),10);
        if(ballTexture){const float scale=std::min(width/280.f,heroHeight/156.f),w=280*scale,h=156*scale;draw->AddImage(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(ballTexture)),{p.x+(width-w)/2,p.y+(heroHeight-h)/2},{p.x+(width+w)/2,p.y+(heroHeight+h)/2});}
        ImGui::InvisibleButton("Rotate Pokeball model",{width,heroHeight});
        if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(ImGuiMouseButton_Left)){
            const auto delta=ImGui::GetIO().MouseDelta;ballYaw=std::clamp(ballYaw+delta.x*.012f,-1.2f,1.2f);ballPitch=std::clamp(ballPitch+delta.y*.008f,-.8f,.8f);ballModelDirty=true;
        }
        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",ballModelError.empty()?"Drag to turn your model":ballModelError.c_str());
        ImGui::PushFont(bold);ImGui::TextUnformatted("Pok\xc3\xa9 Ball Plus");ImGui::PopFont();
        const bool connected=state.phase==input::BallPhase::Connected;
        ImGui::TextColored(color(connected?IM_COL32(38,130,91,255):muted),"%s",connected?"CONNECTED":"BLUETOOTH CONTROLLER");
        if(state.battery>=0){ImGui::SameLine();ImGui::TextDisabled("/ %d%%",state.battery);}
        const bool busy=state.phase==input::BallPhase::Scanning||state.phase==input::BallPhase::Connecting||state.phase==input::BallPhase::Waiting;
        if(connected||busy){if(ImGui::Button(connected?"Disconnect":"Cancel",{width*.49f,28})){ball.disconnect();ballControls.reset();}}
        else if(ImGui::Button("Find controller",{width*.49f,28})){ballSelected=0;ballSelectedAddress=0;ball.scan();}
        ImGui::SameLine();if(ImGui::Button("Windows setup",{-1,28})){
            if(reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",L"ms-settings:bluetooth",nullptr,nullptr,SW_SHOWNORMAL))<=32)error="Cannot open Windows Bluetooth settings.";
        }
        if(!state.devices.empty()&&!connected&&state.phase!=input::BallPhase::Connecting&&state.phase!=input::BallPhase::Waiting){
            ballSelected=std::clamp(ballSelected,0,int(state.devices.size())-1);ballSelectedAddress=state.devices[ballSelected].address;
            ImGui::SetNextItemWidth(width-80);
            if(ImGui::BeginCombo("##ballDevice",state.devices[ballSelected].label.c_str())){
                for(size_t i=0;i<state.devices.size();++i)if(ImGui::Selectable(state.devices[i].label.c_str(),int(i)==ballSelected)){ballSelected=int(i);ballSelectedAddress=state.devices[i].address;}
                ImGui::EndCombo();
            }
            ImGui::SameLine();if(ImGui::Button("Connect",{-1,0})){beginBallConnection(ballSelectedAddress);}
        }
        if(connected){
            ImGui::TextColored(color(gameFocus&&windowFocused()&&!ImGui::GetIO().WantTextInput&&ballControls.armed()?IM_COL32(38,130,91,255):muted),"%s",ballInputHint.c_str());
            if(!gameFocus||ImGui::GetIO().WantTextInput){
                if(ImGui::Button("Resume game",{-1,27}))resumeGame();
#ifdef FR_TEST_HARNESS
                ballResumeX=(ImGui::GetItemRectMin().x+ImGui::GetItemRectMax().x)*.5f;ballResumeY=(ImGui::GetItemRectMin().y+ImGui::GetItemRectMax().y)*.5f;
#endif
            }
        }
        if(!connected){ImGui::TextWrapped("%s",state.message.c_str());if(state.phase==input::BallPhase::Idle)ImGui::TextWrapped("Press the top button to wake your ball, then find it here.");}
        // Small live controls confirm the exact inputs arriving from this ball.
        const auto q=ImGui::GetCursorScreenPos();const bool fresh=connected&&state.lastReport&&input::ballTime()-state.lastReport<=750;
        draw->AddCircleFilled({q.x+17,q.y+17},15,IM_COL32(214,220,217,255));
        draw->AddCircleFilled({q.x+17+(fresh?state.sample.x:0)*10,q.y+17+(fresh?state.sample.y:0)*10},5,fresh?IM_COL32(38,130,91,255):muted);
        draw->AddCircleFilled({q.x+65,q.y+17},5,fresh&&state.sample.stick?red:muted);
        draw->AddCircleFilled({q.x+168,q.y+17},5,fresh&&state.sample.top?red:muted);
        draw->AddText({q.x+77,q.y+9},ink,"Stick click");draw->AddText({q.x+180,q.y+9},ink,"Top button");ImGui::Dummy({width,35});
        ImGui::TextUnformatted("Stick: move     Both buttons: menu");
        ImGui::TextUnformatted(ballMapping.swapButtons?"Top: A / confirm   Click: B / back":"Click: A / confirm   Top: B / back");
        if(ImGui::CollapsingHeader("Adjust controls")){
            ImGui::SetNextItemWidth(-1);ImGui::SliderInt("##ballDeadzone",&ballMapping.deadzone,10,60,"Stick dead zone: %d%%",ImGuiSliderFlags_AlwaysClamp);
            if(ImGui::IsItemDeactivatedAfterEdit())guard([&]{saveBallMapping();});
            bool changed=ImGui::Checkbox("Swap A and B",&ballMapping.swapButtons);ImGui::SameLine();changed=ImGui::Checkbox("Invert Y",&ballMapping.invertY)||changed;
            if(changed)guard([&]{saveBallMapping();});
            paragraph("Release the stick and buttons after reconnecting or returning from chat. Keyboard controls remain available.");
        }
        ImGui::PopStyleVar();
    }
    void worldOptions(const Status& status){
        heading("SESSION REWARDS");rewardSettings(status);ImGui::Spacing();
        heading("SHINY RATE");
        ImGui::Text("Current chance: 1 in %u",status.shinyRate);
        if(shownShinyRate!=status.shinyRate){shownShinyRate=status.shinyRate;shinyDraft=int(status.shinyRate);}
        const bool editable=status.hosting&&status.running;
        paragraph(editable?"Changes apply to everyone; saved with this world.":"The host chooses this setting; saved with the world.");
        ImGui::BeginDisabled(!editable);
        ImGui::TextUnformatted("One shiny in...");ImGui::SetNextItemWidth(-1);
        ImGui::InputInt("##shinyRate",&shinyDraft,0,0);
        const bool valid=shinyDraft>=1&&shinyDraft<=8192;
        ImGui::BeginDisabled(!valid||shinyDraft==int(status.shinyRate));
        if(primary("Apply shiny rate",{-1,30}))guard([&]{session.setShinyRate(uint32_t(shinyDraft));});
        ImGui::EndDisabled();
        if(ImGui::Button("Restore game default",{-1,28}))guard([&]{session.setShinyRate(8192);shinyDraft=8192;});
        ImGui::EndDisabled();
        if(!valid)paragraph("Enter a number from 1 to 8192.");
        paragraph("Default: 1 in 8,192. Lower means more shinies. Applies to new encounters and random gifts. Existing Pokemon stay unchanged.");
    }
    void options(const Status& status){
        const float third=(ImGui::GetContentRegionAvail().x-16)/3;
        if(ImGui::Selectable("General",optionsTab==0,0,{third,23}))optionsTab=0;ImGui::SameLine();
        if(ImGui::Selectable("World",optionsTab==2,0,{third,23}))optionsTab=2;ImGui::SameLine();
        if(ImGui::Selectable("Controller",optionsTab==1,0,{third,23}))optionsTab=1;
        ImGui::Spacing();if(optionsTab==1){controllerPage();return;}if(optionsTab==2){worldOptions(status);return;}

        ImGui::Spacing();heading("AUDIO");ImGui::SetNextItemWidth(-1);
        ImGui::SliderInt("##gameVolume",&gameVolume,0,100,"Game volume: %d%%",ImGuiSliderFlags_AlwaysClamp);
        if(ImGui::IsItemDeactivatedAfterEdit())guard([&]{atomicText(data/"audio.cfg",std::to_string(gameVolume)+"\n");});
        ImGui::Spacing();heading("OVERWORLD");
        bool changed=ImGui::Checkbox("Party follower",&follow);changed=ImGui::Checkbox("Visible wild Pokemon",&wild)||changed;
        if(changed)guard([&]{atomicText(data/"world.cfg",std::to_string(follow)+" "+std::to_string(wild)+"\n");});
        ImGui::Spacing();heading("DISPLAY");
        if(ImGui::Button("Focus on the game",{-1,30})){sidebar=false;gameFocus=true;}
        if(ImGui::CollapsingHeader("Keyboard controls"))paragraph("WASD / Arrows   Move\nX             Confirm / Nearby battle\nZ             Cancel / Run (with shoes)\nEnter     Game menu / Save\nR Shift   Select\nC / V      L / R\nG             Camp / Pack up\nT             Trade with facing trainer\nChat       Click input, Enter to send\nEsc        Return to game\nF2           Online sidebar\nF11        Fullscreen\nF12        Screenshot");
        heading("WORLD SAVE");paragraph(status.managedWorld?"Progress checkpoints automatically in the host world. The host can save everyone from the game menu.":"Save from the game's Start menu.");
        if(ImGui::Button("End session",{-1,30})){SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);}
    }
    void chatDock(const Status& status,ImVec2 pos,ImVec2 size){
        if(wasConnected&&!status.connected){draft.fill(0);chatError.clear();shownChat=0;focusChat=false;}
        wasConnected=status.connected;
        ImGui::SetCursorPos(pos);
        ImGui::PushStyleColor(ImGuiCol_ChildBg,color(IM_COL32(18,24,32,255)));
        ImGui::PushStyleColor(ImGuiCol_Text,color(cream));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{12,8});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{8,4});
        ImGui::BeginChild("Room chat",size,ImGuiChildFlags_AlwaysUseWindowPadding);
        ImGui::PushFont(mono);ImGui::TextColored(color(mint),"ROOM CHAT");ImGui::SameLine();
        ImGui::TextDisabled("%s",chatError.empty()?(status.connected?"Click to chat / Enter to send":"Join a room to chat"):chatError.c_str());ImGui::PopFont();
        const float history=std::max(12.f,ImGui::GetContentRegionAvail().y-36);
        ImGui::PushFont(chatFont);
        ImGui::BeginChild("Messages",{0,history},ImGuiChildFlags_None,ImGuiWindowFlags_NoBackground);
        const bool atBottom=ImGui::GetScrollY()>=ImGui::GetScrollMaxY()-2;
        if(status.chat.empty())ImGui::TextDisabled("%s",status.connected?"Say hello to your room.":"Your room's messages will appear here.");
        for(const auto& m:status.chat){
            if(m.kind){ImGui::PushStyleColor(ImGuiCol_Text,color(mint));ImGui::TextWrapped("%s",m.text.c_str());ImGui::PopStyleColor();continue;}
            ImGui::PushTextWrapPos(0);
            ImGui::TextColored(color(m.id==session.id()?mint:IM_COL32(143,191,243,255)),"%s",(m.name+":").c_str());
            // SameLine keeps ordinary messages compact; wrapping remains within the dock.
            if(ImGui::GetItemRectSize().x<ImGui::GetContentRegionAvail().x-80)ImGui::SameLine(0,6);
            ImGui::TextUnformatted(m.text.c_str());ImGui::PopTextWrapPos();
        }
        const uint32_t latest=status.chat.empty()?0:status.chat.back().sequence;
        if(latest!=shownChat&&atBottom)ImGui::SetScrollHereY(1.f);shownChat=latest;
        ImGui::EndChild();ImGui::PopFont();
        ImGui::BeginDisabled(!status.connected);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{9,5});
        ImGui::PushStyleColor(ImGuiCol_Text,color(ink));
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x-69);
        if(focusChat&&status.connected){ImGui::SetKeyboardFocusHere();focusChat=false;}
        bool send=ImGui::InputTextWithHint("##chat-message","Message your room...",draft.data(),draft.size(),ImGuiInputTextFlags_EnterReturnsTrue);
        if(ImGui::IsItemActive())gameFocus=false;
        ImGui::SameLine();send=primary("Send",{61,27})||send;
        if(send){try{session.sendChat(draft.data());draft.fill(0);chatError.clear();}catch(const std::exception& e){chatError=e.what();}focusChat=true;gameFocus=false;}
        ImGui::PopStyleColor();ImGui::PopStyleVar();ImGui::EndDisabled();
        ImGui::EndChild();ImGui::PopStyleVar(2);ImGui::PopStyleColor(2);
    }
    void render(void* texture){
        if(!context)return;ImGui::SetCurrentContext(context);
        if(operation.valid()&&operation.wait_for(std::chrono::seconds(0))==std::future_status::ready)error=operation.get();
        if(autoHost){autoHost=false;connect();}
        auto status=session.status();pollConnection(status);
        bool renamedFriend=false;
        for(auto& friendEntry:friends)for(const auto& peer:status.peers)if(friendEntry.id==peer.id&&friendEntry.name!=peer.name){friendEntry.name=peer.name;renamedFriend=true;break;}
        if(renamedFriend)guard([&]{saveFriends();});
        ImGui_ImplSDLRenderer2_NewFrame();ImGui_ImplSDL2_NewFrame();
#ifdef FR_TEST_HARNESS
        testInput();
#endif
        if(gameFocus)ImGui::GetIO().ConfigFlags&=~ImGuiConfigFlags_NavEnableKeyboard;else ImGui::GetIO().ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::NewFrame();
        const auto size=ImGui::GetIO().DisplaySize;
        ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize(size);
        ImGui::PushStyleColor(ImGuiCol_WindowBg,color(ink));ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,0);
        ImGui::Begin("Pok\xc3\xa9Multi",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoBringToFrontOnFocus|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
        auto* d=ImGui::GetWindowDrawList();
        d->AddImage(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(logo)),{24,21},{56,53});label(bold,24,{70,17},cream,"Pok\xc3\xa9Multi");label(mono,11,{71,45},IM_COL32(193,174,151,255),"T O G E T H E R");
        d->AddLine({230,22},{230,52},IM_COL32(61,65,72,255));label(mono,12,{252,29},IM_COL32(165,174,181,255),"KANTO / YOUR JOURNEY");
        const bool online=(status.connected||status.hosting)&&!status.localWorld;d->AddCircleFilled({size.x-334,36},4,online?mint:muted);
        label(mono,12,{size.x-320,29},online?mint:IM_COL32(170,178,184,255),online?std::to_string(status.peers.size())+" / "+std::to_string(status.capacity)+" CONNECTED":"SOLO ADVENTURE");
        ImGui::SetCursorPos({size.x-169,19});if(ImGui::Button(sidebar?"Hide sidebar  F2":"Online  F2",{145,35})){sidebar=!sidebar;gameFocus=true;}
        const float left=24,top=83,side=sidebar?(size.x<1060?300.f:328.f):0.f,gap=sidebar?20.f:0.f,right=size.x-24-side-gap,bottom=size.y-52;
        const float chatHeight=size.y<720?96.f:144.f,gameBottom=bottom-chatHeight-12;
        d->AddRectFilled({left,top},{right,gameBottom},IM_COL32(18,24,32,255),15);
        d->AddRect({left,top},{right,gameBottom},leaf?accent:IM_COL32(63,69,77,255),15,0,1);
        label(mono,12,{left+19,top+16},IM_COL32(146,157,164,255),gameLabel);
        std::string money=game::walletAvailable()?std::to_string(game::walletBalance()):"--";
        if(money.size()>3)money.insert(money.size()-3,",");
        d->AddRectFilled({right-142,top+9},{right-19,top+38},IM_COL32(33,48,44,255),7);
        label(regular,16,{right-132,top+15},mint,"Wallet P"+money);
        ImGui::SetCursorPos({right-142,top+9});ImGui::InvisibleButton("Wallet",{123,29});if(ImGui::IsItemHovered())ImGui::SetTooltip("Your personal money: P%u\nReserved in wagers: P%u",game::walletBalance(),game::walletHeld());
        // Reserve separate header and status rows, including the viewport border.
        const float contentTop=top+48,contentBottom=gameBottom-37;
        const float availW=right-left-32,availH=contentBottom-contentTop;
        const int scale=std::max(1,int(std::floor(std::min(availW/240,availH/160))));
        viewportW=float(240*scale);viewportH=float(160*scale);viewportX=std::floor((left+right-viewportW)/2);viewportY=std::floor(contentTop+(availH-viewportH)/2);
        d->AddRectFilled({viewportX-5,viewportY-5},{viewportX+viewportW+5,viewportY+viewportH+5},IM_COL32(7,11,15,255),4);
        d->AddImage(static_cast<ImTextureID>(reinterpret_cast<uintptr_t>(texture)),{viewportX,viewportY},{viewportX+viewportW,viewportY+viewportH});
        d->AddCircleFilled({left+22,gameBottom-21},3.5f,mint);label(mono,11,{left+34,gameBottom-27},IM_COL32(154,170,163,255),game::camping()?"CAMPING":status.managedWorld?"WORLD SAVE":"LOCAL SAVE");
        const auto releasedInfo=game::releaseNotice();const auto campInfo=releasedInfo.empty()?game::campNotice():releasedInfo;if(!campInfo.empty()){
            d->PushClipRect({left+145,gameBottom-30},{right-12,gameBottom-6},true);label(regular,14,{left+145,gameBottom-29},IM_COL32(181,194,187,255),campInfo);d->PopClipRect();
            ImGui::SetCursorPos({left+140,gameBottom-31});ImGui::InvisibleButton("Camp status",{right-left-152,24});if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",campInfo.c_str());
        }

        label(mono,12,{left,size.y-40},IM_COL32(173,180,184,255),"WASD Move  X Battle / A  Z Back  T Trade  ENTER Menu  G Camp");
        label(mono,12,{size.x-250,size.y-40},IM_COL32(173,180,184,255),gameFocus?"GAME CONTROLS ACTIVE":"CLICK GAME TO RESUME");
        for(auto m=status.chat.rbegin();m!=status.chat.rend();++m)if(m->kind&&chatClock()-m->receivedAt<6000){
            d->AddRectFilled({left+12,top+9},{right-155,top+36},IM_COL32(18,24,32,255));d->PushClipRect({left+12,top+10},{right-155,top+38},true);
            label(regular,14,{left+12,top+13},mint,m->text);d->PopClipRect();break;
        }
        chatDock(status,{left,gameBottom+12},{right-left,chatHeight});
        if(sidebar){
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{14,12});ImGui::SetCursorPos({size.x-24-side,top});ImGui::BeginChild("Adventure sidebar",{side,bottom-top},ImGuiChildFlags_AlwaysUseWindowPadding);
            ImGui::PushFont(mono);ImGui::TextDisabled("TRAINER NETWORK");ImGui::PopFont();
            const float third=(ImGui::GetContentRegionAvail().x-16)/3;
            const char* tabs[]{"Room","Friends","Options"};for(int i=0;i<3;++i){if(i)ImGui::SameLine();if(ImGui::Selectable(tabs[i],tab==i,0,{third,26}))tab=i;}
            ImGui::Separator();ImGui::Spacing();
            if(tab==0)room(status);else if(tab==1)friendsPage(status);else options(status);
            if(!error.empty()){ImGui::Spacing();ImGui::PushStyleColor(ImGuiCol_Text,color(red));ImGui::TextWrapped("%s",error.c_str());ImGui::PopStyleColor();}
            else if(!notice.empty()){ImGui::Spacing();paragraph(notice.c_str());}
            else if(!status.message.empty()&&status.message!="Connected to room."&&status.message!="Hosting a room for up to four trainers."&&!session.cableConnected()&&!status.wager.active()){ImGui::Spacing();paragraph(status.message.c_str());}
            sidebarScroll=ImGui::GetScrollMaxY();ImGui::EndChild();ImGui::PopStyleVar();
        }
        ImGui::End();ImGui::PopStyleVar(2);ImGui::PopStyleColor();ImGui::Render();ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(),renderer);
#ifdef FR_TEST_HARNESS
        if(++uiFrames%10==0){int w=0,h=0;SDL_GetWindowSize(window,&w,&h);std::ostringstream report;
            report<<"frames="<<uiFrames<<" command="<<uiCommand<<" phase="<<uiPhase<<" connected="<<status.connected<<" hosting="<<status.hosting<<" peers="<<status.peers.size()<<" invitation="<<status.invitation.from<<" cable="<<session.cableConnected()<<" tab="<<tab<<" sidebar="<<sidebar<<" keyboard="<<(!gameFocus||ImGui::GetIO().WantTextInput)<<" size="<<w<<","<<h<<" port="<<port.data()<<" key="<<key.data()<<" chat="<<status.chat.size()<<" chat_focus="<<ImGui::GetIO().WantTextInput<<" options_tab="<<optionsTab<<" ball_model="<<ballModel.triangles()<<" ball_phase="<<int(ballState().phase)<<" ball_resume_x="<<int(ballResumeX)<<" ball_resume_y="<<int(ballResumeY)<<" ball_resume="<<resumeAfterBallConnect<<" ball_armed="<<ballControls.armed()<<" ball_keys="<<ballLastKeys<<" scroll="<<sidebarScroll<<" public_lookup="<<int(publicAddress.state())<<" public_valid="<<validConnectionIPv4(publicAddress.address(),true)<<" shiny_rate="<<status.shinyRate<<" leaf_theme="<<leaf<<" volume="<<gameVolume<<" money="<<game::walletBalance()<<" held="<<game::walletHeld()<<" viewport="<<viewportX<<","<<viewportY<<","<<viewportW<<","<<viewportH<<" chrome_clear="<<(viewportY-5>=top+43&&viewportY+viewportH+5<=gameBottom-30)<<" error="<<error<<"\n";
            try{atomicText(data/"test-ui-status.txt",report.str());
                std::ostringstream transcript;for(const auto& m:status.chat)transcript<<m.sequence<<' '<<unsigned(m.slot)<<' '<<std::quoted(m.id)<<' '<<std::quoted(m.name)<<' '<<std::quoted(m.text)<<'\n';atomicText(data/"test-chat.txt",transcript.str());}catch(const std::exception&){}}
#endif
        if(capture){
            capture=false;int w=0,h=0;SDL_GetRendererOutputSize(renderer,&w,&h);auto* surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_ARGB8888);
            if(surface){if(SDL_RenderReadPixels(renderer,nullptr,surface->format->format,surface->pixels,surface->pitch)==0){const auto temp=data/"game-ui.bmp.tmp";if(SDL_SaveBMP(surface,narrow(temp.wstring()).c_str())==0)MoveFileExW(temp.c_str(),(data/"game-ui.bmp").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);}SDL_FreeSurface(surface);}}
    }
    void event(const void* raw){
        if(!context)return;ImGui::SetCurrentContext(context);const auto& e=*static_cast<const SDL_Event*>(raw);
        ImGui_ImplSDL2_ProcessEvent(&e);
        if(e.type==SDL_MOUSEBUTTONDOWN||e.type==SDL_KEYDOWN||(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_FOCUS_LOST))resumeAfterBallConnect=false;
        if(e.type==SDL_KEYDOWN&&!e.key.repeat){
            if(e.key.keysym.sym==SDLK_g&&gameFocus&&!ImGui::GetIO().WantTextInput&&!(e.key.keysym.mod&(KMOD_CTRL|KMOD_ALT|KMOD_GUI)))game::toggleCamp();
            if(e.key.keysym.sym==SDLK_t&&gameFocus&&!ImGui::GetIO().WantTextInput&&!(e.key.keysym.mod&(KMOD_CTRL|KMOD_ALT|KMOD_GUI)))game::tradeNearby();
            if(e.key.keysym.sym==SDLK_ESCAPE&&!gameFocus){focusChat=false;gameFocus=true;ImGui::ClearActiveID();}
        }
        if(e.type==SDL_KEYDOWN&&!e.key.repeat){if(e.key.keysym.sym==SDLK_F2){sidebar=!sidebar;gameFocus=true;}if(e.key.keysym.sym==SDLK_F12)capture=true;if(e.key.keysym.sym==SDLK_F11)SDL_SetWindowFullscreen(window,(SDL_GetWindowFlags(window)&SDL_WINDOW_FULLSCREEN_DESKTOP)?0:SDL_WINDOW_FULLSCREEN_DESKTOP);}
        if(e.type==SDL_MOUSEBUTTONDOWN){const float x=float(e.button.x),y=float(e.button.y);gameFocus=x>=viewportX&&x<viewportX+viewportW&&y>=viewportY&&y<viewportY+viewportH;}
        if(e.type==SDL_WINDOWEVENT&&e.window.event==SDL_WINDOWEVENT_FOCUS_LOST){gameFocus=false;}
        // The launcher smoke capture message is consumed on the same SDL thread.
        if(e.type==SDL_SYSWMEVENT){} // no platform window or second message loop
    }
};
Panel::Panel(Session& s,const std::filesystem::path& data,bool visible,bool host,const std::string& name,const std::string& code):impl_(std::make_unique<Impl>(s,data,visible,host,name,code)){}
Panel::~Panel()=default;
int Panel::volume() const{return impl_->gameVolume;}
uint16_t Panel::controllerKeys(){return impl_->controllerKeys();}
void Panel::applySettings(){game::followers(impl_->follow);game::visibleWild(impl_->wild);}
void Panel::capture(){impl_->capture=true;}
void Panel::show(){impl_->sidebar=true;impl_->tab=0;}
void Panel::open(void* window,void* renderer){impl_->open(window,renderer);}
void Panel::event(const void* event){impl_->event(event);}
void Panel::render(void* texture){impl_->render(texture);}
void Panel::close(){impl_->close();}
bool Panel::capturesInput() const{return impl_->context&&(!impl_->gameFocus||ImGui::GetIO().WantTextInput);}
}
