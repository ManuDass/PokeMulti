#include "rom/rom.hpp"
#include "game/world.hpp"
#include "frontend/world_store.hpp"
#include <thread>
#include <chrono>
#include <SDL.h>
#include "online/panel.hpp"
#include "runtime_bus_bridge.h"
#include "runtime_arm.h"
#include "gba_bus.h"
#include "platform/text.hpp"
#include "platform/version.hpp"
#include "runtime.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>
#ifdef FR_TEST_HARNESS
#include <SDL.h>
#endif

// The static corpus starts empty; native blocks are populated by the local
// compiler. Instruction fallback remains measurable during runtime bring-up.
struct DispatchEntry { uint32_t addr; uint8_t thumb, resume; void (*fn)(); };
extern "C" const DispatchEntry kDispatchTable[] = {{0,0,0,nullptr}};
extern "C" const unsigned kDispatchTableLen = 0;

namespace {
struct Cable final : gba::SerialLink {
    fr::online::Session& session;
    explicit Cable(fr::online::Session& s):session(s){}
    bool connected() const override {return session.cableConnected();}
    bool clocked() const override {return true;}
    bool clockReady() const override {return session.cableClockReady();}
    bool request(uint32_t& elapsed) override {return session.cableRequest(elapsed);}
    bool tryRequest(uint32_t& elapsed) override {return session.cableTryRequest(elapsed);}
    bool respond(uint16_t w,std::array<uint16_t,4>& r) override {
        const bool ok=session.cableRespond(w,r);
#ifdef FR_TEST_HARNESS
        static unsigned count=0;if((++count<6000&&(count<5||w!=0xb9a0||r[0]!=0xb9a0))||!ok)std::fprintf(stderr,"[sio] respond #%u send=%04x rx=%04x,%04x ok=%d pc=%08x\n",count,w,r[0],r[1],ok,g_cpu.R[15]);
#endif
        return ok;
    }
    bool startTimed(uint16_t w,uint32_t elapsed,std::array<uint16_t,4>& r) override {
        const bool ok=session.cableStartTimed(w,elapsed,r);
#ifdef FR_TEST_HARNESS
        static unsigned count=0;if((++count<6000&&(count<5||w!=0xb9a0||r[1]!=0xb9a0))||!ok)std::fprintf(stderr,"[sio] timed #%u span=%u send=%04x rx=%04x,%04x ok=%d pc=%08x\n",count,elapsed,w,r[0],r[1],ok,g_cpu.R[15]);
#endif
        return ok;
    }
    bool awaitWord(uint16_t w,std::array<uint16_t,4>& r) override {
        const bool ok=session.cableAwait(w,r);
#ifdef FR_TEST_HARNESS
        static unsigned count=0;if(++count<400)std::fprintf(stderr,"[sio] await #%u send=%04x rx=%04x,%04x ok=%d pc=%08x\n",count,w,r[0],r[1],ok,g_cpu.R[15]);
#endif
        return ok;
    }
    uint16_t control(uint16_t v) override {
        const auto result=session.cableControl(v);
#ifdef FR_TEST_HARNESS
        static uint16_t last=0;static unsigned count=0;
        if(last!=result&&(++count<200||((last^result)&0x7000u))){last=result;std::fprintf(stderr,"[sio] control=%04x pc=%08x\n",result,g_cpu.R[15]);}
#endif
        return result;
    }
    bool start(uint16_t w,std::array<uint16_t,4>& r) override {
        const bool ok=session.cableStart(w,r);
#ifdef FR_TEST_HARNESS
        static unsigned count=0;if(++count<400)std::fprintf(stderr,"[sio] start #%u send=%04x rx=%04x,%04x ok=%d pc=%08x\n",count,w,r[0],r[1],ok,g_cpu.R[15]);
#endif
        return ok;
    }
    bool poll(uint16_t w,std::array<uint16_t,4>& r) override {
        const bool ok=session.cablePoll(w,r);
#ifdef FR_TEST_HARNESS
        static unsigned count=0;if(ok&&++count<400)std::fprintf(stderr,"[sio] poll #%u send=%04x rx=%04x,%04x pc=%08x\n",count,w,r[0],r[1],g_cpu.R[15]);
#endif
        return ok;
    }
};
fr::online::Session* activeSession=nullptr;
fr::online::Panel* activePanel=nullptr;
Cable* activeCable=nullptr;
bool worldGuest=false,worldWasConnected=false,worldDisconnected=false;
std::filesystem::path launcherRoot;
#ifdef FR_TEST_HARNESS
uint32_t testStake=0;uint8_t testRewards=fr::game::DefaultRewardSharing;bool testManual=false;int testPort=0;bool testHost=false,testLink=false,testInvited=false,testBattle=false,testReport=false,testUi=false;
uint64_t testFrame=0,testFirst=0;
std::filesystem::path testProfile;
int testKey=-1;uint64_t testKeyUntil=0,testKeyCommand=0;
uint16_t testInput(uint16_t original){return fr::game::filterInput(testKey>=0&&testFrame<testKeyUntil?uint16_t(testKey):fr::game::testMovementInput(testKey<0?original:0x3ff));}

void testRender(uint8_t* rgb,uint32_t w,uint32_t h){
    const std::vector<uint8_t> before(rgb,rgb+w*h*3);
    fr::game::render(rgb,w,h);
    if(testReport){auto* memory=gbarecomp::active_bus();const auto q=memory->read8(0x0203adfa);const auto playback=memory->read32(0x03005e88);
        if(q==2||q==3||playback==1||playback==3){static std::ofstream recap(testProfile/"recap.csv");size_t changed=0;for(size_t i=0;i<before.size();++i)if(before[i]!=rgb[i])++changed;recap<<testFrame<<','<<unsigned(q)<<','<<playback<<','<<changed<<','<<unsigned(memory->read8(0x02024029))<<'\n';recap.flush();}}
    if(testReport&&testFrame%60==0){
        size_t changed=0;for(size_t i=0;i<before.size();i+=3)if(before[i]!=rgb[i]||before[i+1]!=rgb[i+1]||before[i+2]!=rgb[i+2])++changed;
        std::ofstream report(testProfile/"overlay-check.txt");
        report<<"frame="<<testFrame<<" changed_pixels="<<changed<<" follower="<<fr::game::followersEnabled()<<" wild="<<fr::game::visibleWildEnabled()<<'\n';
    }
    static uint64_t captured=0;
    if((!testLink&&!testReport)||testFrame/120==captured||w!=240||h!=160)return;
    captured=testFrame/120;
    BITMAPFILEHEADER header{};BITMAPINFOHEADER info{};
    header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info);header.bfSize=header.bfOffBits+w*h*3;
    info.biSize=sizeof(info);info.biWidth=LONG(w);info.biHeight=-LONG(h);info.biPlanes=1;info.biBitCount=24;
    std::vector<uint8_t> bgr(rgb,rgb+w*h*3);for(size_t i=0;i<bgr.size();i+=3)std::swap(bgr[i],bgr[i+2]);
    std::ofstream f(testProfile/"live.bmp",std::ios::binary);f.write(reinterpret_cast<char*>(&header),sizeof(header));f.write(reinterpret_cast<char*>(&info),sizeof(info));f.write(reinterpret_cast<char*>(bgr.data()),bgr.size());
    std::vector<uint8_t> native(before);for(size_t i=0;i<native.size();i+=3)std::swap(native[i],native[i+2]);
    std::ofstream original(testProfile/"native.bmp",std::ios::binary);original.write(reinterpret_cast<char*>(&header),sizeof(header));original.write(reinterpret_cast<char*>(&info),sizeof(info));original.write(reinterpret_cast<char*>(native.data()),native.size());
}
void testFrameStep(uint64_t number){
    testFrame=number;if(!testLink&&!testReport)return;
    if(activePanel&&number%120==0)activePanel->capture();
    if(!testFirst)testFirst=number;
    {std::ifstream f(testProfile/"test-keys.txt");uint64_t command=0,duration=0;std::string key;
     if(f>>command>>key>>duration && command!=testKeyCommand&&duration<=6000){const int value=std::stoi(key,nullptr,0);if(value>=-1&&value<=1023){testKeyCommand=command;testKey=value;testKeyUntil=number+duration;}}}
    auto status=activeSession->status();
    if(testHost&&!testManual&&!testInvited&&status.peers.size()==2&&(!testStake||number>testFirst+600)){activeSession->invite(1,testBattle?fr::online::Activity::Battle:fr::online::Activity::Trade,testStake);testInvited=true;}
    if(testLink&&!testHost&&!testManual&&status.invitation.from>=0)activeSession->reply(true);
    auto* bus=gbarecomp::active_bus();
    const auto elapsed=number-testFirst;
    // Controller replay owns input in these windowed tests.
    if(number%60==0){
        if(std::filesystem::exists(testProfile/"test-stop.txt")){SDL_Event event{};event.type=SDL_QUIT;SDL_PushEvent(&event);}
        {std::ofstream history(testProfile/"link-history.txt",std::ios::app);history<<number<<" "<<std::hex<<bus->read32(0x03003f20)<<" "<<bus->read32(0x03003eac)<<" "<<bus->read32(0x03003fb0)<<" "<<bus->read32(0x03003fb4)<<" "<<bus->read32(0x02022854)<<" "<<bus->read32(0x02022858)<<"\n";}
        std::ofstream f(testProfile/"link-check.txt");
        f<<"frame="<<number<<" slot="<<status.slot<<" peers="<<status.peers.size()<<" cable="<<activeSession->cableConnected()<<" message="<<status.message<<"\n";
        f<<std::hex<<"main="<<bus->read32(0x030030f4)<<" type="<<bus->read16(0x0202271a)<<" status="<<bus->read32(0x03003f20)<<" remote="<<bus->read32(0x03003f64)<<" error="<<bus->read32(0x03003eac)<<" sio="<<bus->read16(0x04000128)<<" send="<<bus->read16(0x0400012a)<<" battle="<<bus->read32(0x02022b4c)<<" outcome="<<unsigned(bus->read8(0x02023e8a))<<"\n";
        f<<std::dec<<"key="<<testKey<<" until="<<testKeyUntil<<" command="<<testKeyCommand<<"\n";
        for(unsigned i=0;i<16;++i){const auto a=0x02036e38+36*i;if(bus->read32(a)&1)f<<"object="<<i<<" xy="<<int(bus->read16(a+16))-7<<","<<int(bus->read16(a+18))-7<<" face="<<unsigned(bus->read8(a+24)&15)<<" flags="<<std::hex<<bus->read32(a)<<std::dec<<"\n";}
        static constexpr const char* orders[]={"GAEM","GAME","GEAM","GEMA","GMAE","GMEA","AGEM","AGME","AEGM","AEMG","AMGE","AMEG","EGAM","EGMA","EAGM","EAMG","EMGA","EMAG","MGAE","MGEA","MAGE","MAEG","MEGA","MEAG"};
        for(unsigned i=0;i<bus->read8(0x02024029)&&i<6;++i){const auto a=0x02024284+100*i;const auto personality=bus->read32(a),ot=bus->read32(a+4);unsigned g=0;while(orders[personality%24][g]!='G')++g;f<<"party="<<i<<" species="<<((bus->read32(a+32+12*g)^personality^ot)&65535)<<" ot="<<ot<<" pid="<<personality<<" hp="<<bus->read16(a+86)<<"\n";}
        f<<std::hex;
        for(unsigned i=0;i<16;++i){const auto a=0x03005090+40*i;if(bus->read8(a+4))f<<"task="<<bus->read32(a)<<"\n";}
    }
}
#endif
void ready(){gbarecomp::active_bus()->io().set_serial_link(activeCable);fr::game::ready();}
void frame(uint64_t number){
    if(worldGuest&&activeSession){const auto state=activeSession->status();
        if(!state.connected){worldDisconnected=true;SDL_Event exit{};exit.type=SDL_QUIT;SDL_PushEvent(&exit);return;}
    }
    if(activePanel)activePanel->applySettings();
    fr::game::frame(number);
    if(activeSession){
        activeSession->update(fr::game::player());
        auto status=activeSession->status();fr::game::localSlot(unsigned(status.slot));std::vector<fr::game::PlayerState> peers;
        for(const auto& p:status.peers)if(p.slot!=status.slot){auto state=p.player;state.netSlot=p.slot;state.identity=std::hash<std::string>{}(p.id);peers.push_back(state);}
        fr::game::peers(std::move(peers));
        std::vector<fr::game::Overhead> labels;const auto tick=fr::online::chatClock();
        for(const auto& p:status.peers){
            fr::game::Overhead label{p.slot,std::hash<std::string>{}(p.id),p.name,{}};
            for(auto m=status.chat.rbegin();m!=status.chat.rend();++m)if(!m->kind&&m->id==p.id){if(m->sequence>p.chatAfter&&tick-m->receivedAt<8000)label.message=m->text;break;}
            labels.push_back(std::move(label));
        }
        fr::game::overheads(std::move(labels));
    }
#ifdef FR_TEST_HARNESS
    testFrameStep(number);
#endif

}
}

int wmain(int argc, wchar_t** argv) {
    try {
        std::cerr << "[PokeMulti] Runtime " << fr::BuildVersion << '\n';
        std::filesystem::path romPath, savePath, profilePath,worldPath,identityPath;
        std::string joinAddress,roomKey;unsigned roomPort=5544,capacity=4,rewardPolicy=fr::game::DefaultRewardSharing;
        std::string trainerName="Trainer",onlineMode;bool windowed=false;
        bool native=true;
        std::vector<std::string> arguments{"firered_game"};
        for (int i=1;i<argc;++i) {
            const std::wstring arg=argv[i];
            if (arg==L"--world-dir"&&i+1<argc)worldPath=argv[++i];
            else if(arg==L"--identity-dir"&&i+1<argc)identityPath=argv[++i];
            else if(arg==L"--join-address"&&i+1<argc)joinAddress=fr::narrow(argv[++i]);
            else if(arg==L"--room-key"&&i+1<argc)roomKey=fr::narrow(argv[++i]);
            else if(arg==L"--room-port"&&i+1<argc)roomPort=std::stoul(argv[++i]);
            else if(arg==L"--capacity"&&i+1<argc)capacity=std::stoul(argv[++i]);
            else if(arg==L"--rewards"&&i+1<argc)rewardPolicy=std::stoul(argv[++i]);
            else if (arg==L"--native") native=true;
            else if (arg==L"--interpreter") native=false;
            else if (arg==L"--rom" && i+1<argc) romPath=argv[++i];
#ifdef FR_TEST_HARNESS
            else if(arg==L"--test-host"&&i+1<argc){testPort=std::stoi(argv[++i]);testHost=true;testLink=true;}
            else if(arg==L"--test-join"&&i+1<argc){testPort=std::stoi(argv[++i]);testLink=true;}
            else if(arg==L"--test-battle")testBattle=true;
            else if(arg==L"--test-stake"&&i+1<argc)testStake=uint32_t(std::stoul(argv[++i]));
            else if(arg==L"--test-rewards"&&i+1<argc)testRewards=uint8_t(std::stoul(argv[++i]));
            else if(arg==L"--test-manual")testManual=true;
            else if(arg==L"--test-report")testReport=true;
            else if(arg==L"--test-ui")testUi=true;
            else if(arg==L"--chase-wild")fr::game::chaseWild();
            else if(arg==L"--walk-to"&&i+2<argc){const int x=std::stoi(argv[++i]);const int y=std::stoi(argv[++i]);fr::game::walkTo(x,y);}
            else if(arg==L"--fixture"&&i+1<argc)fr::game::requestFixture(fr::narrow(argv[++i]));
#endif
            else if(arg==L"--name"&&i+1<argc)trainerName=fr::narrow(argv[++i]);
            else if(arg==L"--online"&&i+1<argc)onlineMode=fr::narrow(argv[++i]);
            else if(arg==L"--profile-dir"&&i+1<argc)profilePath=argv[++i];
            else if (arg==L"--save" && i+1<argc) savePath=argv[++i];
            else if (arg==L"--input" && i+1<argc) _putenv_s("GBARECOMP_INPUT_REPLAY",fr::narrow(argv[++i]).c_str());
            else if ((arg==L"--frames" || arg==L"--dump-png" || arg==L"--save-state" || arg==L"--load-state")
                     && i+1<argc) {
                arguments.push_back(fr::narrow(arg)); arguments.push_back(fr::narrow(argv[++i]));
            } else if (arg==L"--window" || arg==L"--quiet") {arguments.push_back(fr::narrow(arg));if(arg==L"--window")windowed=true;}
            else throw std::runtime_error("Usage: firered_game --rom game.gba|game.zip --save profile.sav [--window] [--frames N] [--dump-png file.png]");
        }
        if (romPath.empty() || savePath.empty()) throw std::runtime_error("ROM and per-profile save path are required.");
        const auto bytes=fr::readRom(romPath);
        const auto report=fr::inspectRom(bytes);
        if (!report.supported()) throw std::runtime_error(report.error);
        if(profilePath.empty())profilePath=savePath.parent_path();if(identityPath.empty())identityPath=profilePath;launcherRoot=identityPath;
        const auto identity=fr::online::loadIdentity(identityPath);
        fr::online::Session online(identity,trainerName,worldPath.empty()?profilePath:worldPath);
        struct WorldLock {HANDLE handle=INVALID_HANDLE_VALUE;~WorldLock(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}} worldLock;
        if(!worldPath.empty()||!joinAddress.empty()){
            if(!roomPort||roomPort>65535||capacity<2||capacity>fr::MaxRoomPlayers||rewardPolicy>fr::game::AllRewardSharing)throw std::runtime_error("Invalid room settings.");
            auto secretBytes=fr::readWorldFile(identityPath/"identity.key",32);std::string secret(secretBytes.begin(),secretBytes.end());
            if(secret.empty()){secret=fr::worldRandomId();fr::atomicWorldFile(identityPath/"identity.key",{reinterpret_cast<const uint8_t*>(secret.data()),secret.size()});}
            online.configureWorld(worldPath,report.sha256,secret);
            if(!worldPath.empty()){
                worldLock.handle=CreateFileW((worldPath/L"session.lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
                if(worldLock.handle==INVALID_HANDLE_VALUE)throw std::runtime_error("This world is already open. Join its host instead.");
                profilePath=worldPath/"players"/identity/"runtime";savePath=profilePath/"trainer.sav";
                fr::WorldPlayers players(fr::loadWorld(worldPath));const auto checkpoint=players.load(identity);if(!checkpoint.empty())fr::restoreCheckpoint(profilePath,checkpoint);
                if(onlineMode=="host")online.host(uint16_t(roomPort),roomKey,uint8_t(rewardPolicy),false,uint8_t(capacity));else online.playLocalWorld();
            }else{
                online.join(joinAddress,uint16_t(roomPort),roomKey);
                const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
                while(!online.status().checkpointReady&&online.status().running&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(10));
                const auto status=online.status();if(!status.connected||!status.managedWorld||!status.checkpointReady)throw std::runtime_error("Could not load your trainer from the host: "+status.message);
                // A guest cache is never listed as an owned world and is replaced on every join.
                profilePath=identityPath/"guest-cache"/status.worldId/fr::worldRandomId();savePath=profilePath/"trainer.sav";
                const auto checkpoint=online.downloadedSave();if(!checkpoint.empty())fr::restoreCheckpoint(profilePath,checkpoint);worldGuest=true;
            }
        }
        if (!savePath.parent_path().empty()) std::filesystem::create_directories(savePath.parent_path());
        struct SaveLock { HANDLE h; ~SaveLock(){ if(h!=INVALID_HANDLE_VALUE) CloseHandle(h); } };
        SaveLock lock{CreateFileW((savePath.wstring()+L".lock").c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr)};
        if(lock.h==INVALID_HANDLE_VALUE) throw std::runtime_error("This save is already open in another game, or the save folder is not writable.");
        arguments.insert(arguments.end(),{"--rom",fr::narrow(romPath.wstring()),"--save",fr::narrow(savePath.wstring())});
        gbarecomp::RunOptions options;
        options.builtin_game_name="Pok\xc3\xa9Multi";
        options.builtin_rom_sha1=report.sha1.c_str();
        options.biosless_hle=true;
        options.rom_data=bytes.data(); options.rom_size=bytes.size();
        if(profilePath.empty())profilePath=savePath.parent_path();
        fr::game::initialize(report.revision,profilePath/"world-check.txt",savePath);

        fr::game::connect(&online);
        Cable cable(online);activeSession=&online;activeCable=&cable;
        std::unique_ptr<fr::online::Panel> panel;
        bool showPanel=windowed;
#ifdef FR_TEST_HARNESS
        if(testLink&&!testUi)showPanel=false;
#endif
        if(showPanel)panel=std::make_unique<fr::online::Panel>(online,identityPath,!onlineMode.empty(),onlineMode=="host"&&worldPath.empty(),trainerName);
        activePanel=panel.get();
#ifdef FR_TEST_HARNESS
        testProfile=profilePath;
        if(testLink){if(testHost)online.host(uint16_t(testPort),"local-test-key",testRewards);else online.join("127.0.0.1",uint16_t(testPort),"local-test-key");}
#endif
        options.game_ready=ready;
        options.game_frame=frame;
        options.game_render=fr::game::render;
        options.game_input=fr::game::filterInput;
#ifdef FR_TEST_HARNESS
        options.game_render=testRender;
        if(testLink||testReport)options.game_input=testInput;
#endif
        if(activePanel)options.shell={
            [](void* w,void* r){activePanel->open(w,r);},
            [](const void* e){activePanel->event(e);},
            [](void* t){activePanel->render(t);},
            []{return activePanel->capturesInput();},
            []{activePanel->close();},true,[]{return activePanel->volume();},[]{return activePanel->controllerKeys();},[]{return fr::game::requestWorldExit();}};
        options.freely_resizable_window=true;
        options.show_fps_by_default=false;
        std::vector<char*> pointers;
        for (auto& a:arguments) pointers.push_back(a.data());
        // Interpreter fallback is explicit until locally compiled blocks pass validation.
        _putenv_s("GBARECOMP_PRESENT_IN_PLACE","0");
        const auto cache=(!worldPath.empty()||worldGuest?identityPath:savePath.parent_path())/"native-cache";
        _putenv_s("GBARECOMP_HEAL_CACHE",fr::narrow(cache.wstring()).c_str());
        const auto diagnostics=cache/report.sha1/"diagnostics";
        std::filesystem::create_directories(diagnostics);
        _putenv_s("GBARECOMP_MISS_FRAG",fr::narrow((diagnostics/"native-misses.toml.frag").wstring()).c_str());
        _putenv_s("GBARECOMP_COVERAGE_JSON",fr::narrow((diagnostics/"native-coverage.json").wstring()).c_str());
        _putenv_s("GBARECOMP_FORCE_INTERP",native ? "0" : "1");
        _putenv_s("GBARECOMP_SELFHEAL_RECOMPILE",native ? "1" : "0");
        const auto result=gbarecomp::run_game(static_cast<int>(pointers.size()),pointers.data(),options);
        if(worldDisconnected){const std::string notice="Connection to the host ended. Your latest checkpoint is kept in the host world.";fr::atomicWorldFile(identityPath/"world-return.txt",{reinterpret_cast<const uint8_t*>(notice.data()),notice.size()});}
        return result;
    } catch (const std::exception& error) {
        std::cerr<<"PokeMulti runtime: "<<error.what()<<'\n';
        // Return startup failures to the launcher, including failed room joins.
        if(!launcherRoot.empty())try{
            const auto message=std::string(error.what()).substr(0,900);
            fr::atomicWorldFile(launcherRoot/"world-return.txt",{reinterpret_cast<const uint8_t*>(message.data()),message.size()});
        }catch(...){ }
        return 1;
    }
}
