#import <AppKit/AppKit.h>
#include "frontend/profile.hpp"
#include "frontend/world_store.hpp"
#include "frontend/cartridge.hpp"
#include "online/connection.hpp"
#include "game/world.hpp"
#include "game/reward_policy.hpp"
#include "platform/image.hpp"
#include "platform/text.hpp"
#include "platform/version.hpp"
#include <SDL.h>
#include <imgui.h>
#include <backends/imgui_impl_sdl2.h>
#include <backends/imgui_impl_sdlrenderer2.h>
#include <spawn.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>
#include <future>
#include <algorithm>
#include <array>
#include <cstring>
extern char** environ;
namespace {
enum class Page {Welcome,Profile,Worlds,NewWorld,Host,Join,Settings,Name};
constexpr ImU32 cream=IM_COL32(244,240,228,255),paper=IM_COL32(255,252,241,255),ink=IM_COL32(35,47,64,255),muted=IM_COL32(102,115,124,255),red=IM_COL32(193,73,55,255);
std::filesystem::path chooseFile(NSArray<NSString*>* types){
    NSOpenPanel* panel=[NSOpenPanel openPanel];panel.canChooseDirectories=NO;panel.allowsMultipleSelection=NO;panel.allowedFileTypes=types;
    return [panel runModal]==NSModalResponseOK?std::filesystem::path(panel.URL.fileSystemRepresentation):std::filesystem::path{};
}
void openPath(const std::filesystem::path& path){[[NSWorkspace sharedWorkspace] openURL:[NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]]];}
struct Launcher {
    SDL_Window* window=nullptr;SDL_Renderer* renderer=nullptr;SDL_Texture* logo=nullptr;SDL_Texture* cartridge=nullptr;
    ImFont* font=nullptr;ImFont* bold=nullptr;
    std::filesystem::path data,romPath;std::optional<fr::Profile> profile;std::optional<fr::RomReport> report;
    std::future<std::pair<fr::RomReport,std::filesystem::path>> pending;
    std::vector<fr::WorldInfo> worlds;size_t selected=0,pageIndex=0;
    Page page=Page::Welcome;std::string status;std::array<char,128> name{},address{},key{};
    int gameMode=0;bool gameFailed=false;std::string previewCode;
    int port=38475,capacity=4;uint8_t rewards=fr::game::DefaultRewardSharing;bool quit=false,smoke=false,gameSeen=false;
    float scale=1,ox=0,oy=0,yaw=-.22f,pitch=.10f;fr::Image label;pid_t game=0;unsigned frames=0;
    ImVec2 point(float x,float y)const{return {ox+x*scale,oy+y*scale};}
    void string(std::string_view value,float x,float y,float size,ImU32 color=ink,bool strong=false){ImGui::GetWindowDrawList()->AddText(strong?bold:font,size*scale,point(x,y),color,value.data(),value.data()+value.size());}
    void box(float x,float y,float w,float h,ImU32 color,float radius=12){ImGui::GetWindowDrawList()->AddRectFilled(point(x,y),point(x+w,y+h),color,radius*scale);}
    bool button(const char* text,float x,float y,float w=488,float h=44,bool primary=false){
        ImGui::SetCursorScreenPos(point(x,y));ImGui::PushStyleColor(ImGuiCol_Button,ImGui::ColorConvertU32ToFloat4(primary?red:IM_COL32(240,244,237,255)));
        ImGui::PushStyleColor(ImGuiCol_Text,ImGui::ColorConvertU32ToFloat4(primary?IM_COL32_WHITE:ink));
        const bool pressed=ImGui::Button(text,{w*scale,h*scale});ImGui::PopStyleColor(2);return pressed;
    }
    void field(const char* id,char* value,size_t count,float x,float y,float w=488){ImGui::SetCursorScreenPos(point(x,y));ImGui::SetNextItemWidth(w*scale);ImGui::InputText(id,value,count);}
    void number(const char* id,int& value,float x,float y,float w=236){ImGui::SetCursorScreenPos(point(x,y));ImGui::SetNextItemWidth(w*scale);ImGui::InputInt(id,&value,0,0);}
    void setName(const std::string& value){name.fill(0);std::copy_n(value.data(),std::min(value.size(),name.size()-1),name.data());}
    void refresh(){
        if(!profile)return;worlds=fr::listWorlds(data,profile->romSha256);
        if(worlds.empty())worlds.push_back(fr::createWorld(data,"My first world",profile->romSha256));
        selected=std::min(selected,worlds.size()-1);pageIndex=selected/3;
    }
    SDL_Texture* texture(const fr::Image& image){
        auto* value=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,image.width,image.height);if(!value)throw std::runtime_error(SDL_GetError());
        SDL_SetTextureBlendMode(value,SDL_BLENDMODE_BLEND);SDL_SetTextureScaleMode(value,SDL_ScaleModeNearest);SDL_UpdateTexture(value,nullptr,image.pixels.data(),image.width*4);return value;
    }
    void updateCartridge(){if(cartridge)SDL_DestroyTexture(cartridge);cartridge=texture(fr::renderCartridge(label,fr::cartridgeStyle(report?report->gameCode:previewCode).color,yaw,pitch));}
    void validate(const std::filesystem::path& path){
        if(pending.valid())return;status="Checking your ROM...";
        pending=std::async(std::launch::async,[path]{return std::pair{fr::inspectRom(fr::readRom(path)),path};});
    }
    void chooseRom(){auto path=chooseFile(@[@"gba",@"zip"]);if(!path.empty())validate(path);}
    void poll(){
        if(pending.valid()&&pending.wait_for(std::chrono::seconds(0))==std::future_status::ready){
            auto [checked,path]=pending.get();if(!checked.supported())throw std::runtime_error(checked.error);
            romPath=path;report=checked;label=fr::cartridgeArtwork(path,data/"artwork"/checked.sha256,checked.gameCode);updateCartridge();
            if(profile){profile->romPath=path;profile->romSha256=checked.sha256;fr::saveProfile(data/"profile.cfg",*profile);refresh();page=Page::Worlds;}
            else{page=Page::Profile;setName("");}status="ROM verified. Stored locally.";
        }
        if(game){int code=0;const auto done=waitpid(game,&code,WNOHANG);if(done==game){game=0;SDL_ShowWindow(window);SDL_RaiseWindow(window);gameFailed=!WIFEXITED(code)||WEXITSTATUS(code)!=0;page=gameFailed&&gameMode?(gameMode==2?Page::Join:Page::Host):Page::Worlds;refresh();status=gameFailed?"The game closed with an error. Open Settings > Runtime log for details.":"Your world is closed.";
            const auto notice=data/"world-return.txt";if(std::filesystem::exists(notice)&&std::filesystem::file_size(notice)<=900){std::ifstream in(notice);status.assign(std::istreambuf_iterator<char>(in),{});std::filesystem::remove(notice);}}}
    }
    void launch(int mode){
        if(!profile||!report||game||pending.valid())return;
        if(mode&&(strlen(key.data())<8||strlen(key.data())>64))throw std::runtime_error("Use a room key of 8–64 characters.");
        if(port<1||port>65535||capacity<2||capacity>32)throw std::runtime_error("Choose a valid port and a player limit from 2 to 32.");
        if(mode==2&&!fr::online::validConnectionIPv4(address.data()))throw std::runtime_error("Enter your friend's IPv4 address.");
        const auto exe=fr::executableFolder()/"pokemulti_game";const auto save=data/"runtime-bootstrap"/"trainer.sav";
        std::filesystem::create_directories(save.parent_path());
        std::vector<std::string> args{exe.string(),"--rom",profile->romPath.string(),"--save",save.string(),"--window","--interpreter","--profile-dir",data.string(),"--identity-dir",data.string(),"--name",profile->playerName};
        if(mode!=2)args.insert(args.end(),{"--world-dir",worlds.at(selected).folder.string()});
        if(mode){args.insert(args.end(),{"--online",mode==1?"host":"join","--room-port",std::to_string(port),"--room-key",key.data(),"--capacity",std::to_string(capacity),"--rewards",std::to_string(rewards)});if(mode==2)args.insert(args.end(),{"--join-address",address.data()});}
        std::vector<char*> raw;for(auto& value:args)raw.push_back(value.data());raw.push_back(nullptr);
        posix_spawn_file_actions_t actions;posix_spawn_file_actions_init(&actions);
        posix_spawn_file_actions_addopen(&actions,STDIN_FILENO,"/dev/null",O_RDONLY,0);
        posix_spawn_file_actions_addopen(&actions,STDOUT_FILENO,(data/"runtime.log").c_str(),O_WRONLY|O_CREAT|O_TRUNC,0600);
        posix_spawn_file_actions_adddup2(&actions,STDOUT_FILENO,STDERR_FILENO);
        std::error_code ec;std::filesystem::remove(data/"world-return.txt",ec);
        pid_t child=0;const int result=posix_spawn(&child,exe.c_str(),&actions,nullptr,raw.data(),environ);posix_spawn_file_actions_destroy(&actions);
        if(result)throw std::runtime_error("Cannot start the bundled game.");gameMode=mode;gameFailed=false;game=child;gameSeen=true;SDL_HideWindow(window);
    }
    void draw(){
        int w=0,h=0;SDL_GetWindowSize(window,&w,&h);scale=std::min(w/1040.f,h/700.f);ox=(w-1040*scale)/2;oy=(h-700*scale)/2;
        auto& io=ImGui::GetIO();io.FontGlobalScale=scale;
        ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({float(w),float(h)});
        ImGui::Begin("PokéMulti",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollWithMouse|ImGuiWindowFlags_NoScrollbar);
        box(0,0,1040,700,cream,0);box(0,0,1040,8,red,0);
        ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)logo,point(40,36),point(88,84));
        string("PokéMulti",105,34,28,ink,true);string("YOUR GAMES. TOGETHER.",106,73,10,muted);
        string(std::string("MAC PREVIEW / ")+fr::BuildVersion,434,53,12,muted);
        if(profile){string("ONLINE USERNAME",678,30,12,muted);string(profile->playerName,678,53,16,ink,true);if(button("Edit",909,47,88,38)){setName(profile->playerName);page=Page::Name;}}
        box(40,121,370,490,IM_COL32(28,34,44,255),22);box(433,121,567,490,paper,22);
        string("ON YOUR SHELF",69,148,12,IM_COL32(187,198,203,255));string("Pick up your adventure.",69,183,20,IM_COL32_WHITE,true);
        ImGui::GetWindowDrawList()->AddImage((ImTextureID)(intptr_t)cartridge,point(40,205),point(410,525));
        ImGui::SetCursorScreenPos(point(60,235));ImGui::InvisibleButton("cartridge drag",{330*scale,250*scale});
        if(ImGui::IsItemActive()&&ImGui::IsMouseDragging(ImGuiMouseButton_Left)){yaw+=io.MouseDelta.x*.008f;pitch=std::clamp(pitch+io.MouseDelta.y*.004f,-.65f,.65f);updateCartridge();}
        string("DRAG TO ROTATE",168,470,10,IM_COL32(160,179,187,255));string(fr::narrow(fr::cartridgeStyle(report?report->gameCode:previewCode).title),69,506,22,IM_COL32_WHITE,true);
        if(button("Choose label art",70,550,310,36)&&report){auto file=chooseFile(@[@"png",@"jpg",@"jpeg"]);if(!file.empty()){label=fr::readImage(file);updateCartridge();auto target=data/"artwork"/report->sha256/"label.png";std::filesystem::create_directories(target.parent_path());std::filesystem::copy_file(file,target,std::filesystem::copy_options::overwrite_existing);}}
        if(pending.valid()){string("Checking your ROM...",472,199,24,ink,true);}
        else if(page==Page::Welcome){string("WELCOME",472,157,12,red);string("Your next adventure starts here.",472,198,24,ink,true);string("Choose English FireRed or LeafGreen US 1.0 / 1.1.\nA .gba file or ZIP with one .gba is supported.",472,290,18,muted);if(button("Select ROM",472,399,488,54,true))chooseRom();}
        else if(page==Page::Profile||page==Page::Name||page==Page::NewWorld){
            const bool world=page==Page::NewWorld,rename=page==Page::Name;
            string(world?"NEW WORLD":"YOUR PROFILE",472,157,12,red);string(world?"A fresh adventure.":rename?"A new name. Same adventure.":"Make it your adventure.",472,198,26,ink,true);
            string(world?"WORLD NAME":"ONLINE USERNAME",472,279,12,muted);field("##name",name.data(),name.size(),472,309);
            if(button(world?"Create world":rename?"Save username":"Create profile",472,390,488,54,true)){
                if(world){auto created=fr::createWorld(data,name.data(),profile->romSha256);refresh();for(size_t i=0;i<worlds.size();++i)if(worlds[i].id==created.id)selected=i;pageIndex=selected/3;}
                else if(rename)profile=fr::renameProfile(data/"profile.cfg",name.data());
                else{fr::Profile created{romPath,report->sha256,name.data()};fr::saveProfile(data/"profile.cfg",created);profile=created;refresh();}
                page=Page::Worlds;status=rename?"Username saved. Your in-game name and progress stay the same.":"Ready to play.";
            }
            if(profile&&button("Cancel",472,463))page=Page::Worlds;
            string(world?"Each world keeps its own trainers, teams and story.":"Used in rooms, chat and your friends list.\nYour game chooses its trainer name separately.",472,533,16,muted);
        }else if(page==Page::Worlds){
            string("YOUR WORLDS",472,153,12,red);string("Choose your next adventure.",472,192,27,ink,true);
            for(size_t row=0;row<3&&pageIndex*3+row<worlds.size();++row){const auto i=pageIndex*3+row;if(button((worlds[i].name+"##world"+std::to_string(i)).c_str(),472,244+row*48,488,41,i==selected))selected=i;}
            ImGui::BeginDisabled(!pageIndex);if(button("Previous",472,393,138,28))--pageIndex;ImGui::EndDisabled();
            string(std::to_string(pageIndex+1)+" / "+std::to_string((worlds.size()+2)/3),660,398,14,muted);
            ImGui::BeginDisabled((pageIndex+1)*3>=worlds.size());if(button("Next",822,393,138,28))++pageIndex;ImGui::EndDisabled();
            if(button("Play world",472,438,236,44,true))launch(0);if(button("Host world",724,438,236))page=Page::Host;
            if(button("New world",472,493,236,40)){setName("New adventure");page=Page::NewWorld;}if(button("Join a friend",724,493,236,40))page=Page::Join;
            if(button("Settings",472,545,236,36))page=Page::Settings;
            if(button("Program updates",724,545,236,36))SDL_OpenURL("https://github.com/ManuDass/PokeMulti/releases");
        }else if(page==Page::Host||page==Page::Join){
            const bool host=page==Page::Host;string(host?"HOST WORLD":"JOIN A FRIEND",472,153,12,red);string(host?worlds.at(selected).name:"Adventure together.",472,192,27,ink,true);
            if(!host){string("HOST IPv4 ADDRESS",472,238,12,muted);field("##address",address.data(),address.size(),472,265,314);if(button("Same PC",802,265,158,36))SDL_strlcpy(address.data(),"127.0.0.1",address.size());}
            string("ROOM KEY",472,host?239:311,12,muted);field("##key",key.data(),65,472,host?266:338);
            string("PORT",472,host?312:384,12,muted);number("##port",port,472,host?338:410);
            if(host){string("PLAYER LIMIT (2–32)",724,312,12,muted);number("##capacity",capacity,724,338);
                string("SHARED REWARDS / MONEY STAYS PERSONAL",472,397,12,muted);const char* labels[]{"TMs","Items","Pokémon"};for(int i=0;i<3;++i){bool checked=rewards&(1<<i);ImGui::SetCursorScreenPos(point(472+i*164,430));if(ImGui::Checkbox(labels[i],&checked))rewards^=1<<i;}
            }else if(button("Paste invite",724,410,236,36)){char* text=SDL_GetClipboardText();auto invite=fr::online::parseConnectionInvite(text?text:"");SDL_free(text);if(!invite)throw std::runtime_error("Clipboard does not contain a valid invite.");snprintf(address.data(),address.size(),"%s",invite->address.c_str());snprintf(key.data(),key.size(),"%s",invite->key.c_str());port=invite->port;}
            if(button(host?"Host selected world":"Join adventure",472,492,488,46,true))launch(host?1:2);if(button("Back to worlds",472,549,488,36))page=Page::Worlds;
        }else if(page==Page::Settings){
            string("SETTINGS",472,153,12,red);string("Your setup, at a glance.",472,192,28,ink,true);
            if(button("Change ROM",472,260))chooseRom();if(button("Open worlds folder",472,320))openPath(data/"worlds");if(button("Runtime log",472,380))openPath(data/"runtime.log");
            string("Mac preview uses the bundled interpreter.\nKeyboard and SDL controllers are supported.\nPoké Ball Plus Bluetooth support is Windows-only.",472,448,16,muted);
            if(button("Back to worlds",472,545))page=Page::Worlds;
        }
        if(gameFailed)box(40,627,833,63,IM_COL32(255,240,230,255),8);
        ImGui::GetWindowDrawList()->AddText(font,14*scale,point(52,635),gameFailed?red:muted,status.c_str(),nullptr,809*scale);if(button("Quit",894,639,102,37))quit=true;ImGui::End();
    }
    int run(){
        SDL_SetMainReady();if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_EVENTS)<0)throw std::runtime_error(SDL_GetError());
        window=SDL_CreateWindow("PokéMulti",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1040,700,SDL_WINDOW_RESIZABLE|SDL_WINDOW_ALLOW_HIGHDPI);
        if(!window)throw std::runtime_error(SDL_GetError());SDL_SetWindowMinimumSize(window,832,560);
        renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);if(!renderer)renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);if(!renderer)throw std::runtime_error(SDL_GetError());
        ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.ConfigFlags|=ImGuiConfigFlags_NavEnableKeyboard;
        ImFontConfig config;config.OversampleH=config.OversampleV=1;config.PixelSnapH=true;
        auto fonts=fr::assetFolder()/"Fonts";font=io.Fonts->AddFontFromFileTTF((fonts/"PixelOperator.ttf").c_str(),20,&config);bold=io.Fonts->AddFontFromFileTTF((fonts/"PixelOperator-Bold.ttf").c_str(),20,&config);
        if(!font||!bold)throw std::runtime_error("Included fonts are missing.");io.FontDefault=bold;
        auto& style=ImGui::GetStyle();style.WindowPadding={0,0};style.FramePadding={12,8};style.FrameRounding=6;style.FrameBorderSize=1;style.Colors[ImGuiCol_Text]=ImGui::ColorConvertU32ToFloat4(ink);style.Colors[ImGuiCol_FrameBg]=ImGui::ColorConvertU32ToFloat4(paper);style.Colors[ImGuiCol_Border]={.76,.80,.80,1};style.Colors[ImGuiCol_ButtonHovered]={.87,.92,.88,1};style.Colors[ImGuiCol_ButtonActive]={.77,.85,.81,1};style.Colors[ImGuiCol_CheckMark]=ImGui::ColorConvertU32ToFloat4(red);
        ImGui_ImplSDL2_InitForSDLRenderer(window,renderer);ImGui_ImplSDLRenderer2_Init(renderer);
        logo=texture(fr::readImage(fr::assetFolder()/"Program_Icon.png"));updateCartridge();
        std::filesystem::create_directories(data);profile=fr::loadProfile(data/"profile.cfg");
        if(!romPath.empty())validate(romPath);else if(profile)validate(profile->romPath);snprintf(address.data(),address.size(),"127.0.0.1");
        while(!quit){@autoreleasepool {
            SDL_Event event;while(SDL_PollEvent(&event)){ImGui_ImplSDL2_ProcessEvent(&event);if(event.type==SDL_QUIT&&!game)quit=true;}
            try{poll();}catch(const std::exception& e){status=e.what();}
            if(game){SDL_Delay(50);continue;}
            ImGui_ImplSDLRenderer2_NewFrame();ImGui_ImplSDL2_NewFrame();ImGui::NewFrame();
            try{draw();}catch(const std::exception& e){status=e.what();ImGui::End();}
            ImGui::Render();SDL_SetRenderDrawColor(renderer,244,240,228,255);SDL_RenderClear(renderer);ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(),renderer);
            if(smoke&&(++frames==5||frames==10)){int w,h;SDL_GetRendererOutputSize(renderer,&w,&h);auto* surface=SDL_CreateRGBSurfaceWithFormat(0,w,h,32,SDL_PIXELFORMAT_RGBA32);if(!surface||SDL_RenderReadPixels(renderer,nullptr,SDL_PIXELFORMAT_RGBA32,surface->pixels,surface->pitch)<0)throw std::runtime_error("Cannot capture launcher.");SDL_SaveBMP(surface,(data/(frames==5?"launcher-preview.bmp":"launcher-leafgreen.bmp")).c_str());SDL_FreeSurface(surface);if(frames==10)quit=true;else{previewCode="BPGE";label=fr::readImage(fr::assetFolder()/"Cart Art"/"Leaf Green Cart Art.png");updateCartridge();}}
            SDL_RenderPresent(renderer);
        }}
        SDL_DestroyTexture(logo);SDL_DestroyTexture(cartridge);ImGui_ImplSDLRenderer2_Shutdown();ImGui_ImplSDL2_Shutdown();ImGui::DestroyContext();SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
    }
};
}
int main(int argc,char** argv){@autoreleasepool {
    try{[NSApplication sharedApplication];[NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];Launcher app;
        app.data=std::filesystem::path(NSSearchPathForDirectoriesInDomains(NSApplicationSupportDirectory,NSUserDomainMask,YES).firstObject.fileSystemRepresentation)/"PokeMulti";
        for(int i=1;i<argc;++i){std::string_view arg=argv[i];if(arg=="--data-dir"&&i+1<argc)app.data=std::filesystem::absolute(argv[++i]);else if(arg=="--rom"&&i+1<argc)app.romPath=std::filesystem::absolute(argv[++i]);else if(arg=="--smoke-test")app.smoke=true;else throw std::runtime_error("Unknown launcher argument.");}
        return app.run();
    }catch(const std::exception& e){fprintf(stderr,"PokéMulti: %s\n",e.what());return 1;}
}}
