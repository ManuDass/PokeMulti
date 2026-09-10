#include "game/world.hpp"
#include "game/performance.hpp"
#include "game/rom_layout.hpp"
#include "game/shiny.hpp"
#include <random>
#include "frontend/world_store.hpp"
#include "game/dialogue.hpp"
#include "game/wager_result.hpp"
#include <iomanip>
#include <SDL.h>
#include "game/released.hpp"
#include "game/motion.hpp"
#include "game/battle_staging.hpp"
#include "game/labels.hpp"
#include "game/follower.hpp"
#include "game/follower_effect.hpp"
#include "game/follower_art.hpp"
#include "game/presentation.hpp"
#include "game/camp_outline.hpp"
#include "online/session.hpp"
#include "game/rewards.hpp"
#include "platform/atomic_file.hpp"
#include <sstream>
#include <map>
#include <chrono>
#include "runtime_bus_bridge.h"
#include "runtime_arm.h"
#include "mod_function_hooks.h"
#include "gba_bus.h"
#include "gba_ppu.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <deque>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <stdexcept>
namespace fr::game {
namespace {
// Factual symbol addresses, restricted to the exact SHA-1 identities in rom.cpp.
// Layout evidence and provenance are recorded in docs/ROM_SUPPORT.md.
struct Addresses {
    uint32_t overworld,icons,iconIndices,iconPalettes,wild,graphics,palettes,encounter,createWild,startWild,repel;
};
Addresses addresses{};
FireRedRevision gameRevision=FireRedRevision::Unsupported;
uint32_t gameAddress(uint32_t canonical){return romAddress(gameRevision,canonical);}
PlayerState local{},previous{},completedPlayer{};
int completedOriginX=0,completedOriginY=0,completedOffsetX=0,completedOffsetY=0;
uint8_t completedAvatarFlags=0;
std::vector<PlayerState> remote;
struct Caption { Overhead info; LabelImage name,bubble; };
std::array<Caption,MaxRoomPlayers> captions{};
struct CaptionAnchor { unsigned slot; int x,top,bottom; uint8_t partyCount=0,partyEggs=0; };
std::vector<CaptionAnchor> captionAnchors;
std::array<MotionTimeline,MaxRoomPlayers> motion{};
std::array<PlayerState,MaxRoomPlayers> lastRemoteField{};
FollowerPath followerPath;
uint32_t speciesNames=0;
std::map<uint32_t,FollowerSheet> followerSheets;
double clockMs(){return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();}
struct Wild {int x,y;uint16_t species;uint8_t level;uint64_t born;int oldX=0,oldY=0;uint64_t moved=0;uint32_t id=0;int16_t sharedX=0,sharedY=0;uint8_t facing=1;bool shiny=false;};
constexpr unsigned WildStepFrames=32;
std::vector<Wild> wild;
std::filesystem::path diagnosticPath;
uint64_t now=0,lastSpawn=0;
uint32_t rng=0x41cb23d8;
bool followOn=true,wildOn=true,guestCall=false,haveOrigin=false;
int originX=0,originY=0;
uint8_t avatarFlags=0;
uint32_t elevationPriority=0,elevationSubpriority=0;
uint8_t roomSlot=0;
std::array<gba::HostObjPixel,240*160> hostPixels{};
uint64_t drawingKey=gba::kNoHostObject;
// Native OAM submission history bridges VBlank DMA without reading live
// sprite state against an older framebuffer. Attribute matching rejects stale
// records and includes every subsprite emitted for a field object.
struct NativeOrder { std::array<uint8_t,6> attributes{}; uint32_t order=0; bool valid=false; };
std::array<std::array<NativeOrder,128>,4> nativeOrders{};
unsigned nativeEpoch=0;
#ifdef FR_TEST_HARNESS
uint64_t nativeOrderMatches=0,nativeOrderMisses=0;
unsigned hostIntersections=0;
#endif
uint32_t pendingIndexPointer=0,pendingOrder=0;
unsigned pendingStart=128;
uint32_t depthOrder(unsigned subpriority,int ground,unsigned tie) {
    return (uint32_t(subpriority&255)<<24) |
        (uint32_t(32767-std::clamp(ground,-32768,32767))<<8) | (tie&255);
}
void flushNativeOrder();

gba::GbaBus* bus(){return gbarecomp::active_bus();}
bool validRom(uint32_t address,size_t count=1){
    return bus() && address>=0x08000000 && uint64_t(address)+count<=0x08000000ull+bus()->rom_size();
}
bool validRam(uint32_t address,size_t count=1){
    return (address>=0x02000000 && uint64_t(address)+count<=0x02040000) ||
           (address>=0x03000000 && uint64_t(address)+count<=0x03008000);
}
uint8_t r8(uint32_t a){return bus()->read8(a);}
uint16_t r16(uint32_t a){return bus()->read16(a);}
int16_t s16(uint32_t a){return static_cast<int16_t>(r16(a));}
uint32_t r32(uint32_t a){return bus()->read32(a);}
uint32_t random(){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
void flushNativeOrder(){
    if(pendingStart>=128 || !validRam(pendingIndexPointer))return;
    const unsigned end=std::min(128u,unsigned(r8(pendingIndexPointer)));
    for(unsigned i=pendingStart;i<end;++i){
        auto& entry=nativeOrders[nativeEpoch][i];
        for(unsigned j=0;j<6;++j)entry.attributes[j]=r8(0x03003128+i*8+j);
        entry.order=pendingOrder;entry.valid=true;
    }
    pendingStart=128;
}
int finishOamHook(uint32_t,int,ArmCpuState*){
    // CopyMatricesToOamBuffer follows AddSpritesToOamBuffer immediately. Its
    // entry still has the completed stack-local OAM count, before it is reused.
    if(!guestCall)flushNativeOrder();
    return 0;
}
int beginOamHook(uint32_t,int,ArmCpuState*){
    if(guestCall)return 0;
    flushNativeOrder();nativeEpoch=(nativeEpoch+1)%nativeOrders.size();
    for(auto& entry:nativeOrders[nativeEpoch])entry.valid=false;
    return 0;
}
int submitOamHook(uint32_t,int,ArmCpuState* cpu){
    if(guestCall)return 0;
    flushNativeOrder();
    const uint32_t sp=cpu->R[0],indexPointer=cpu->R[1];
    if(sp<0x0202063c || sp>=0x0202063c+68*64 || (sp-0x0202063c)%68 || !validRam(indexPointer))return 0;
    const unsigned sprite=(sp-0x0202063c)/68;
    const int ground=s16(sp+34)-int8_t(r8(sp+41))+((r8(sp+62)&2)?s16(0x02021bca):0);
    unsigned tie=128+sprite;
    for(unsigned i=0;i<16;++i){const auto object=0x02036e38+i*36;
        if((r32(object)&1)&&r8(object+4)==sprite){tie=i==r8(0x0203707d)?roomSlot:64+i;break;}}
    pendingIndexPointer=indexPointer;pendingStart=r8(indexPointer);
    pendingOrder=depthOrder(r8(sp+67),ground,tie);
    return 0;
}
uint32_t nativeOrder(unsigned index,const uint8_t* attributes){
    if(index>=128)return 0;
    for(unsigned age=0;age<nativeOrders.size();++age){
        const auto& entry=nativeOrders[(nativeEpoch+nativeOrders.size()-age)%nativeOrders.size()][index];
        if(entry.valid&&std::equal(entry.attributes.begin(),entry.attributes.end(),attributes)){
#ifdef FR_TEST_HARNESS
            ++nativeOrderMatches;
#endif
            return entry.order;
        }
    }
#ifdef FR_TEST_HARNESS
    ++nativeOrderMisses;
#endif
    // Unknown guest sprites (including UI) keep precedence within their layer.
    return 0;
}
uint64_t spriteKey(uint8_t elevation,int ground,unsigned tie){
    const unsigned priority=r8(elevationPriority+(elevation&15));
    const unsigned sub=uint8_t(r8(elevationSubpriority+(elevation&15))+1+2*(16-(((ground+8)&255)>>4)));
    return gba::host_obj_key(priority,depthOrder(sub,ground,tie));
}
void hostPixel(int x,int y,uint16_t color,uint32_t rgba=0){
    if(x<0||y<0||x>=240||y>=160)return;
    auto& dest=hostPixels[unsigned(y)*240+unsigned(x)];
#ifdef FR_TEST_HARNESS
    if(dest.key!=gba::kNoHostObject&&dest.key!=drawingKey)++hostIntersections;
#endif
    if(drawingKey<dest.key)dest={drawingKey,color,rgba};
}
bool sameMap(const PlayerState& a,const PlayerState& b){return a.active && b.active && a.mapGroup==b.mapGroup && a.mapNumber==b.mapNumber;}
uint16_t leadSpecies(bool& shiny,uint32_t& token){
    shiny=false;token=0;
    if(r8(0x02024029)>6)return 0;
    constexpr const char* order[]={"GAEM","GAME","GEAM","GEMA","GMAE","GMEA","AGEM","AGME","AEGM","AEMG","AMGE","AMEG","EGAM","EGMA","EAGM","EAMG","EMGA","EMAG","MGAE","MGEA","MAGE","MAEG","MEGA","MEAG"};
    for(unsigned slot=0;slot<r8(0x02024029);++slot){
        const auto base=0x02024284u+slot*100u;
        if(!r16(base+86))continue;
        const uint32_t personality=r32(base),key=personality^r32(base+4);
        uint16_t sum=0;std::array<uint32_t,12> data{};
        for(unsigned j=0;j<12;++j){data[j]=r32(base+32+j*4)^key;sum=uint16_t(sum+uint16_t(data[j])+uint16_t(data[j]>>16));}
        if(sum!=r16(base+28))continue;
        unsigned growth=0,misc=0;
        while(order[personality%24][growth]!='G')++growth;
        while(order[personality%24][misc]!='M')++misc;
        if(data[misc*3+1]&0x40000000u)continue;
        const auto species=uint16_t(data[growth*3]);
        if(species && species<=411){shiny=isShiny(personality,r32(base+4));token=personality^(r32(base+4)*16777619u);return species;}
    }
    return 0;
}
bool nativeLinkScene(){return bus()&&r32(0x03003f64)!=0;}
bool replaying(){return bus()&&questLogPlayback(r8(0x0203adfa),r32(0x03005e88));}
uint8_t partyEggMask(uint8_t count);
bool readPlayer(PlayerState& out){
    if(!bus() || nativeLinkScene() || replaying() || (r32(0x030030F4)&~1u)!=addresses.overworld || (r8(0x02037abf)&0x80))return false;
    const auto save=r32(0x03005008);
    const unsigned id=r8(0x0203707d);
    if(!validRam(save,16) || id>=16)return false;
    const uint32_t object=0x02036e38+id*36;
    const uint32_t flags=r32(object);
    if(!(flags&1) || !(flags&0x10000) || (flags&0x2000))return false;
    out.active=true;out.mapGroup=r8(save+4);out.mapNumber=r8(save+5);
    out.x=int16_t(s16(object+16)-7);out.y=int16_t(s16(object+18)-7);
    out.elevation=r8(object+11)&15;out.facing=r8(object+24)&15;out.graphics=r8(object+5);
    avatarFlags=r8(0x02037078);out.follower=leadSpecies(out.followerShiny,out.followerToken);
    const auto partyCount=r8(0x02024029);out.partyCount=partyCount<=6?partyCount:0;out.partyEggs=partyEggMask(out.partyCount);
    const uint32_t layout=r32(0x02036dfc);
    if(!validRom(layout,24) || out.x<0 || out.y<0 || out.x>=int(r32(layout)) || out.y>=int(r32(layout+4)) || out.facing<1 || out.facing>4)return false;
#ifdef FR_TEST_HARNESS
    if(!diagnosticPath.empty()){
        static std::ofstream probe(diagnosticPath.parent_path()/"motion-raw.csv");
        const auto sp=0x0202063c+68*r8(object+4);
        probe<<now<<','<<out.x<<','<<out.y<<','<<s16(object+20)-7<<','<<s16(object+22)-7
             <<','<<unsigned(out.facing)<<','<<unsigned(r8(0x0203707b))<<','<<s16(save)<<','<<s16(save+2);
        for(unsigned offset=32;offset<42;offset+=2)probe<<','<<s16(sp+offset);
        for(unsigned offset=42;offset<46;++offset)probe<<','<<unsigned(r8(sp+offset));
        probe<<','<<s16(0x0300506c)<<','<<s16(0x03005068)<<','<<int32_t(r32(0x03005060))<<','<<int32_t(r32(0x03005064))
             <<','<<s16(0x02021bc8)<<','<<s16(0x02021bca)<<','<<r32(sp+8)<<','<<r16(sp+2)<<'\n';
        if(now%60==0)probe.flush();
    }
#endif
    const auto sprite=r8(object+4);
    if(sprite>=64)return false;
    const uint32_t sp=0x0202063c+68*sprite;
    // Invert the ROM's SetSpritePosToMapCoords transform. Camera tile updates
    // lead movement; its signed sub-tile remainder compensates on both axes.
    const int cx=int32_t(r32(0x03005060)),cy=int32_t(r32(0x03005064));
    originX=(7-s16(save))*16-s16(0x0300506c)-cx+(cx>0?16:cx<0?-16:0);
    originY=(7-s16(save+2))*16-s16(0x03005068)-cy+(cy>0?16:cy<0?-16:0);
    out.pixelX=int16_t(s16(sp+32)-originX-8);
    out.pixelY=int16_t(s16(sp+34)-originY-16-int8_t(r8(sp+41)));
    out.offsetX=int8_t(std::clamp<int>(s16(sp+36),-64,64));out.offsetY=int8_t(std::clamp<int>(s16(sp+38),-64,64));
    out.flip=uint8_t((r16(sp+2)>>12)&3);
    const auto anims=r32(sp+8);const unsigned anim=r8(sp+42),command=r8(sp+43);
    out.spriteFrame=out.facing==2?1:out.facing>=3?2:0;
    if(anim<32&&validRom(anims,(anim+1)*4)){
        const auto commands=r32(anims+anim*4);
        if(command<64&&validRom(commands,(command+1)*4)){
            // END/JUMP commands keep the last displayed image, not a new pose.
            for(int i=int(command);i>=0;--i){const auto frame=r16(commands+unsigned(i)*4);if(frame<64){out.spriteFrame=uint8_t(frame);break;}}
        }
    }
    haveOrigin=true;
    return out.pixelX>=-32&&out.pixelY>=-32&&out.pixelX<=8224&&out.pixelY<=8224;
}
void captureNpcs();
int completedWorldHook(uint32_t,int,ArmCpuState*){
    if(guestCall)return 0;
    flushNativeOrder();
    PlayerState next{};if(!readPlayer(next))next={};
    static uint32_t sequence=0;next.sequence=++sequence;next.sampleTime=uint32_t(uint64_t(clockMs()));
    completedPlayer=next;completedAvatarFlags=avatarFlags;
    completedOriginX=originX;completedOriginY=originY;
    completedOffsetX=s16(0x02021bc8);completedOffsetY=s16(0x02021bca);
    captureNpcs();
    return 0;
}
bool occupied(int x,int y){
    for(unsigned i=0;i<16;++i){const auto a=0x02036e38+i*36;const auto flags=r32(a);
        if((flags&1)&&!(flags&0x2000)&&s16(a+16)-7==x&&s16(a+18)-7==y)return true;
    }return false;
}
bool grass(int x,int y,uint8_t elevation){
    const uint32_t layout=r32(0x02036dfc);
    if(!validRom(layout,24))return false;
    const uint32_t width=r32(layout),height=r32(layout+4),grid=r32(layout+12);
    if(width>512 || height>512 || x<0 || y<0 || uint32_t(x)>=width || uint32_t(y)>=height ||
       !validRom(grid,size_t(width)*height*2))return false;
    const uint16_t tile=r16(grid+2*(uint32_t(y)*width+uint32_t(x)));
    if((tile&0xC00) || (tile>>12)!=elevation)return false;
    const unsigned id=tile&1023;
    const uint32_t tileset=r32(layout+(id<640?16:20));
    if(!validRom(tileset,24))return false;
    const uint32_t attrs=r32(tileset+20)+4*(id<640?id:id-640);
    if(!validRom(attrs,4))return false;
    const uint32_t behavior=r32(attrs)&0x1ff;
    return behavior==2 || behavior==3;
}
PlayerState wildPose(const Wild& m){
    const auto elapsed=now>=m.moved?now-m.moved:0;const float t=std::min(1.f,float(elapsed)/WildStepFrames);
    PlayerState p;p.active=true;p.mapGroup=local.mapGroup;p.mapNumber=local.mapNumber;p.elevation=local.elevation;p.x=int16_t(m.x);p.y=int16_t(m.y);p.follower=m.species;p.followerShiny=m.shiny;
    p.pixelX=int16_t(std::lround((m.oldX+(m.x-m.oldX)*t)*16));p.pixelY=int16_t(std::lround((m.oldY+(m.y-m.oldY)*t)*16));p.followerFacing=m.facing;p.followerFrame=uint8_t((elapsed<WildStepFrames&&(m.oldX!=m.x||m.oldY!=m.y))?(elapsed/8)%4:0);return p;
}
Wild adoptWild(const online::WildState& w){
    Wild m{w.x,w.y,w.species,w.level,now>60?now-60:0,w.x,w.y,now>WildStepFrames?now-WildStepFrames:0,w.id,w.pixelX,w.pixelY,w.facing};
    m.shiny=w.shiny;
    const unsigned remaining=unsigned(std::abs(w.x*16-w.pixelX)+std::abs(w.y*16-w.pixelY));
    if(remaining&&remaining<=16){constexpr int dx[]{0,0,0,-1,1},dy[]{0,1,-1,0,0};m.oldX-=dx[w.facing];m.oldY-=dy[w.facing];const unsigned age=(16-remaining)*WildStepFrames/16;m.moved=now>age?now-age:0;}
    return m;
}
bool inView(int x,int y,int margin=0){
    const int center=x*16+completedOriginX+completedOffsetX+8;
    const int feet=y*16+completedOriginY+completedOffsetY+15;
    return center>=16-margin&&center<=224+margin&&feet>=16-margin&&feet<=152+margin;
}
bool chooseWild(uint16_t& species,uint8_t& level){
    for(unsigned i=0;i<134;++i){
        const auto h=addresses.wild+20*i;
        if(!validRom(h,20) || r8(h)==255)return false;
        if(r8(h)!=local.mapGroup || r8(h+1)!=local.mapNumber)continue;
        const auto land=r32(h+4);
        if(!validRom(land,8) || !r8(land))return false;
        const auto slots=r32(land+4);if(!validRom(slots,48))return false;
        constexpr unsigned weights[]{20,20,10,10,10,10,5,5,4,4,1,1};
        unsigned n=random()%100,slot=0;while(slot<11 && n>=weights[slot])n-=weights[slot++];
        const auto p=slots+slot*4;const auto lo=r8(p),hi=r8(p+1);
        species=r16(p+2);if(!species || species>411 || !lo || hi<lo || hi>100)return false;
        level=uint8_t(lo+random()%(hi-lo+1));return true;
    }
    return false;
}
uint32_t callGuestWords(uint32_t function,std::initializer_list<uint32_t> words,uint32_t maxSteps=2000000){
    performance::Scope timing("native-call",function);
    const ArmCpuState saved=g_cpu;const bool previousGuestCall=guestCall;guestCall=true;
    g_cpu.R[13]-=64;unsigned i=0;for(auto value:words){if(i<4)g_cpu.R[i]=value;else bus()->write32(g_cpu.R[13]+4*(i-4),value);++i;}
    g_cpu.R[15]=function;g_cpu.R[14]=0xFFFF0101;g_cpu.cpsr|=CPSR_T_BIT;uint32_t steps=0;
    try{while(g_cpu.R[15]!=0xFFFF0100){if(++steps>maxSteps)throw std::runtime_error("Native game call did not return");runtime_force_interp_step();}}
    catch(...){g_cpu=saved;guestCall=previousGuestCall;throw;}
    const auto result=g_cpu.R[0];g_cpu=saved;guestCall=previousGuestCall;return result;
}
uint32_t callGuest(uint32_t function,uint32_t a=0,uint32_t b=0,uint32_t c=0,uint32_t d=0,uint32_t e=0,uint32_t maxSteps=2000000){return callGuestWords(function,{a,b,c,d,e},maxSteps);}
uint8_t partyEggMask(uint8_t count){
    // Re-query native GetMonData only when a slot changes. Its header and
    // checksum change on deposit, trade, hatch, reordering or replacement;
    // steady walking does not repeatedly decrypt all six party records.
    struct Slot {std::array<uint32_t,4> key{};bool valid=false,egg=false;};
    static std::array<Slot,6> slots;uint8_t mask=0;
    for(unsigned i=0;i<6;++i){auto& cached=slots[i];if(i>=count){cached.valid=false;continue;}
        const auto mon=0x02024284+i*100;
        const std::array<uint32_t,4> key{r32(mon),r32(mon+4),r32(mon+16),r32(mon+28)};
        if(!cached.valid||cached.key!=key){cached.egg=callGuest(gameAddress(0x0803fbe8),mon,45)!=0;cached.key=key;cached.valid=true;}
        if(cached.egg)mask|=uint8_t(1u<<i);
    }
    return mask;
}
#include "world_field_symbols.inc"
bool fieldBattleActive=false;
bool fieldMoneyOfferReady();
uint32_t interactWithTrainer();
uint32_t interactWithFollower();

extern CampSimulation localCamp;
extern BattlePresence localBattle;
bool battleOccupied(int x,int y);
void createVisibleWild(uint16_t species,uint8_t level,bool shiny);
void beginBattlePresence(uint16_t enemy=0,uint32_t wildId=0,const PlayerState* encounterPose=nullptr);
bool campBlocked(int x,int y);
bool releasedOccupied(int x,int y);
#include "world_online.inc"
#include "world_shiny.inc"
#include "world_dialogue.inc"
#include "world_story_gates.inc"
void publishDeparture();
#include "world_story_retry.inc"
#include "world_field_ui.inc"
#include "world_campaign.inc"
#include "world_release.inc"
#ifdef FR_TEST_HARNESS
std::string fixtureRequest;
bool chaseRequested=false,walkRequested=false;int walkX=0,walkY=0;
void driveTestInput(){
    if(!chaseRequested&&!walkRequested)return;
    if(!local.active){bus()->io().set_keyinput(0x3ff);return;}
    int tx=walkX,ty=walkY;
    if(chaseRequested){
        if(wild.empty()){bus()->io().set_keyinput(0x3ff);return;}
        const auto closest=std::min_element(wild.begin(),wild.end(),[](const Wild& a,const Wild& b){return std::abs(a.x-local.x)+std::abs(a.y-local.y)<std::abs(b.x-local.x)+std::abs(b.y-local.y);});
        tx=closest->x;ty=closest->y;
    }
    const auto layout=r32(0x02036dfc);if(!validRom(layout,24))return;
    const int w=int(r32(layout)),h=int(r32(layout+4));const auto grid=r32(layout+12);
    if(w<=0||h<=0||w>512||h>512||tx<0||ty<0||tx>=w||ty>=h||!validRom(grid,size_t(w)*h*2))return;
    const int start=local.y*w+local.x,target=ty*w+tx;
    if(start==target){bus()->io().set_keyinput(0x3ff);if(!chaseRequested)walkRequested=false;return;}
    std::vector<int> parent(size_t(w)*h,-1);std::deque<int> queue{start};parent[start]=start;
    constexpr int dx[]{0,0,-1,1},dy[]{1,-1,0,0};
    while(!queue.empty()&&parent[target]<0){const int cell=queue.front();queue.pop_front();
        for(unsigned d=0;d<4;++d){int x=cell%w+dx[d],y=cell/w+dy[d];if(x<0||y<0||x>=w||y>=h)continue;
            const int next=y*w+x;const auto tile=r16(grid+2*next);
            if(parent[next]>=0||(tile&0xc00)||occupied(x,y))continue;
            parent[next]=cell;queue.push_back(next);
        }
    }
    if(parent[target]<0){bus()->io().set_keyinput(0x3ff);return;}
    int next=target;while(parent[next]!=start)next=parent[next];
    uint16_t mask=next%w>local.x?0x10:next%w<local.x?0x20:next/w<local.y?0x40:0x80;
    bus()->io().set_keyinput(0x3ff^mask);
}
int fixtureHook(uint32_t,int,ArmCpuState*){
    if(fixtureRequest.empty()||guestCall)return 0;
    const auto fixture=fixtureRequest;fixtureRequest.clear();
    if(fixture=="shiny-check"){
        const auto mon=0x0202402cu;unsigned randomShiny=0,natureShiny=0;bool valid=true;
        const auto rate=room?room->status().shinyRate:DefaultShinyRate;
        for(unsigned i=0;i<32;++i){
            callGuest(addresses.createWild,16,10,0);
            randomShiny+=isShiny(r32(mon),r32(mon+4));
            valid=valid&&callGuest(gameAddress(0x0803fbe8),mon,11)==16&&callGuest(gameAddress(0x0803fbe8),mon,4)==0;
            callGuestWords(gameAddress(0x0803dd98),{mon,16,10,32,i%25});
            natureShiny+=isShiny(r32(mon),r32(mon+4));valid=valid&&r32(mon)%25==i%25&&callGuest(gameAddress(0x0803fbe8),mon,11)==16;
        }
        constexpr uint32_t preserved=0x12345678;
        callGuestWords(gameAddress(0x0803da54),{mon,16,10,32,1,preserved,0,0});
        valid=valid&&r32(mon)==preserved&&callGuest(gameAddress(0x0803fbe8),mon,4)==0;
        for(bool expected:{false,true}){createVisibleWild(16,10,expected);valid=valid&&isShiny(r32(mon),r32(mon+4))==expected&&callGuest(gameAddress(0x0803fbe8),mon,4)==0;}
        std::ofstream report(diagnosticPath.parent_path()/"shiny-check.txt");report<<"rate="<<rate<<" random="<<randomShiny<<" nature="<<natureShiny<<" valid="<<valid<<"\n";
        return 0;
    }
    const auto save2=r32(0x0300500c);
    if(validRam(save2,16)){
        const std::string name=(fixture=="cable-b"||fixture=="wager-b"||fixture=="field-b"||fixture=="field-center-b")?"TEST B":"TEST A";
        for(unsigned i=0;i<8;++i)bus()->write8(save2+i,i<name.size()?uint8_t(name[i]==' '?0:0xBB+name[i]-'A'):0xff);
        bus()->write32(save2+10,(fixture=="cable-b"||fixture=="wager-b"||fixture=="field-b"||fixture=="field-center-b")?0x55667788:0x11223344);
    }
    const unsigned species=(fixture=="cable-b"||fixture=="wager-b"||fixture=="battle-char")?4:1;
    callGuest(addresses.createWild,species,10,0);
    for(unsigned i=0;i<100;++i)bus()->write8(0x02024284+i,r8(0x0202402c+i));
    callGuest(addresses.createWild,(fixture=="cable-b"||fixture=="wager-b"||fixture=="field-b"||fixture=="field-center-b")?19:16,10,0);
    for(unsigned i=0;i<100;++i)bus()->write8(0x02024284+100+i,r8(0x0202402c+i));
    bus()->write8(0x02024029,2);
    callGuest(gameAddress(0x0806e680),2088);callGuest(gameAddress(0x0806e680),2089);
    if(fixture.starts_with("wager")||fixture.starts_with("field-")){
        callGuest(addresses.createWild,1,(fixture=="wager-a"||fixture=="field-a"||fixture=="field-center-a")?50:5,0);
        for(unsigned i=0;i<100;++i)bus()->write8(0x02024284+i,r8(0x0202402c+i));
        for(unsigned i=100;i<600;++i)bus()->write8(0x02024284+i,0);
        bus()->write8(0x02024029,1);
        callGuest(gameAddress(0x0803e964),0x02024284,33,0); // Native SetMonMoveSlot: Tackle.
        callGuest(gameAddress(0x0809fd70),r32(0x03005008)+0x290,3000);
    }
    if(fixture=="spectate"){
        callGuest(addresses.createWild,1,5,0);for(unsigned i=0;i<100;++i)bus()->write8(0x02024284+i,r8(0x0202402c+i));
        bus()->write16(0x02024284+86,1); // Synthetic fixture: native damage will cause the faint.
        callGuest(gameAddress(0x0803e964),0x02024284,150,0);for(unsigned i=1;i<4;++i)callGuest(gameAddress(0x0803e964),0x02024284,0,i);
        callGuest(addresses.createWild,4,25,0);for(unsigned i=0;i<100;++i)bus()->write8(0x02024284+100+i,r8(0x0202402c+i));
    }
    if(fixture.starts_with("camp-")){
        constexpr unsigned team[]{1,4,7,25,133,16};
        for(unsigned slot=0;slot<6;++slot){callGuest(addresses.createWild,team[slot],10,0);for(unsigned i=0;i<100;++i)bus()->write8(0x02024284+slot*100+i,r8(0x0202402c+i));}
        bus()->write8(0x02024029,6);
    }
    if(fixture=="released-a"){
        for(unsigned i=0;i<3;++i){constexpr unsigned species[]{25,133,7};callGuest(addresses.createWild,species[i],10+i,0);callGuest(gameAddress(0x0808bbb4),0,i,0x0202402c);}
        callGuest(gameAddress(0x0809a084),1,10); // Isolated capture fixture: Master Balls.
    }
    if(fixture=="released-b")callGuest(gameAddress(0x0809a084),1,10);
    if(fixture=="unique-eevee"){callGuest(gameAddress(0x0806e6a8),0x263);callGuest(gameAddress(0x0806e6a8),0x57);}
    if(fixture=="wild-repel")callGuest(gameAddress(0x0806e584),0x4020,250);
    if(fixture=="campaign-start"||fixture.starts_with("campaign-gate-")){
        for(unsigned flag:{0x829u,0x2bu,0x3au,0x820u,0x254u,0x238u,0x23au,0x574u,0x674u,0x69eu,0x53au,0x555u,0x4b0u})storyWrite(uint16_t(flag),0);
        storyWrite(0x2d,1);storyWrite(0x4055,4);storyWrite(0x4057,0);storyWrite(0x4051,0);storyWrite(0x4054,0);storyWrite(0x4058,0);storyWrite(0x406c,0);storyWrite(0x2e,0);
        for(unsigned item:{327u,340u,342u,349u})while(callGuest(gameAddress(0x08099f40),item,1))callGuest(gameAddress(0x0809a1d8),item,1);
    }
    if(fixture.starts_with("campaign-")&&fixture!="campaign-start"&&campaignValue(0x4057)<2)storyWrite(0x829,0);
    if(fixture=="campaign-brock"||fixture=="campaign-trainer"){
        callGuest(addresses.createWild,1,50,0);for(unsigned i=0;i<100;++i)bus()->write8(0x02024284+i,r8(0x0202402c+i));
        for(unsigned i=100;i<600;++i)bus()->write8(0x02024284+i,0);bus()->write8(0x02024029,1);
        callGuest(gameAddress(0x0803e964),0x02024284,22,0);for(unsigned i=1;i<4;++i)callGuest(gameAddress(0x0803e964),0x02024284,0,i);
    }
    if(fixture.starts_with("campaign-retry-")){
        callGuest(gameAddress(0x080554cc),2); // Native Viridian heal checkpoint in the disposable fixture.
        storyWrite(0x828,1);storyWrite(0x829,1);storyWrite(0x4055,6);storyWrite(0x4057,2);storyWrite(0x4054,1);storyWrite(0x4f,1);
        for(uint16_t flag=0x500;flag<0x800;++flag)storyWrite(flag,0);
        callGuest(addresses.createWild,1,fixture=="campaign-retry-a"?5:80,0);
        for(unsigned i=0;i<100;++i)bus()->write8(0x02024284+i,r8(0x0202402c+i));
        for(unsigned i=100;i<600;++i)bus()->write8(0x02024284+i,0);bus()->write8(0x02024029,1);
        callGuest(gameAddress(0x0803e964),0x02024284,fixture=="campaign-retry-a"?150:33,0);for(unsigned i=1;i<4;++i)callGuest(gameAddress(0x0803e964),0x02024284,0,i);
        if(fixture=="campaign-retry-a")bus()->write16(0x02024284+86,1);
        callGuest(gameAddress(0x0809fd70),r32(0x03005008)+0x290,3000);
    }
    if(fixture=="campaign-oak")storyWrite(0x2b,0);
    if(fixture=="campaign-president"){storyWrite(0x053,1);storyWrite(0x4060,1);}
    if(fixture=="campaign-warden"){storyWrite(0x189,1);callGuest(gameAddress(0x0809a084),353,1);}
    if(fixture=="campaign-start")callGuest(gameAddress(0x0805538c),3,1,0xffffffff,26,27);
    else if(fixture=="campaign-gate-oldman-a")callGuest(gameAddress(0x0805538c),3,1,0xffffffff,21,12);
    else if(fixture=="campaign-gate-oldman-b")callGuest(gameAddress(0x0805538c),3,1,0xffffffff,22,12);
    else if(fixture=="campaign-gate-pewter-a")callGuest(gameAddress(0x0805538c),3,2,0xffffffff,41,21);
    else if(fixture=="campaign-gate-pewter-b")callGuest(gameAddress(0x0805538c),3,2,0xffffffff,41,22);
    else if(fixture=="campaign-retry-a"||fixture=="campaign-retry-b")callGuest(gameAddress(0x0805538c),3,41,0xffffffff,34,fixture=="campaign-retry-a"?5:6);
    else if(fixture=="campaign-mart")callGuest(gameAddress(0x0805538c),5,3,0xffffffff,4,7);
    else if(fixture=="campaign-oak")callGuest(gameAddress(0x0805538c),4,3,0xffffffff,6,4);
    else if(fixture=="campaign-brock")callGuest(gameAddress(0x0805538c),6,2,0xffffffff,6,6);
    else if(fixture=="campaign-warden")callGuest(gameAddress(0x0805538c),11,7,0xffffffff,3,6);
    else if(fixture=="campaign-trainer")callGuest(gameAddress(0x0805538c),3,21,0xffffffff,19,10);
    else if(fixture=="campaign-president")callGuest(gameAddress(0x0805538c),1,57,0xffffffff,9,10);
    else if(fixture=="campaign-oldman")callGuest(gameAddress(0x0805538c),3,1,0xffffffff,21,12);
    else if(fixture=="campaign-fly")callGuest(gameAddress(0x0805538c),25,0,0xffffffff,4,3);
    else if(fixture=="released-a")callGuest(gameAddress(0x0805538c),5,4,0xffffffff,11,2);
    else if(fixture=="released-b")callGuest(gameAddress(0x0805538c),3,1,0xffffffff,28,27);
    else if(fixture=="field-a"||fixture=="field-b")callGuest(gameAddress(0x0805538c),3,21,0xffffffff,64,fixture=="field-a"?11:12);
    else if(fixture=="field-center-a"||fixture=="field-center-b")callGuest(gameAddress(0x0805538c),5,4,0xffffffff,7,fixture=="field-center-a"?6:7);
    else if(fixture=="story-rival"){callGuest(gameAddress(0x0806e584),0x4052,0);callGuest(gameAddress(0x0806e680),0x3c);callGuest(gameAddress(0x0805538c),3,3,0xffffffff,23,8);}
    else if(fixture=="unique-eevee")callGuest(gameAddress(0x0805538c),10,11,0xffffffff,7,4);
    else if(fixture=="camp-route1"||fixture=="camp-route1-b")callGuest(gameAddress(0x0805538c),3,19,0xffffffff,fixture=="camp-route1-b"?10:8,13);
    else if(fixture.starts_with("camp"))callGuest(gameAddress(0x0805538c),3,21,0xffffffff,fixture=="camp-b"?67:64,11);
    else if(fixture=="spectate")callGuest(gameAddress(0x0805538c),3,19,0xffffffff,16,14);
    else if(fixture=="follower-edge")callGuest(gameAddress(0x0805538c),3,19,0xffffffff,10,0);
    else if(fixture.starts_with("wild")||fixture=="battle-char")callGuest(gameAddress(0x0805538c),3,19,0xffffffff,12,14);
    else callGuest(gameAddress(0x0805538c),5,5,0xffffffff,4,7);
    callGuest(gameAddress(0x0807e438));
    std::printf("TEST_FIXTURE=%s synthetic party and map setup; local validation only\n",fixture.c_str());
    return 0;
}
#endif
int encounterHook(uint32_t,int,ArmCpuState* cpu){
    if(guestCall || !local.active || !local.follower || (avatarFlags&0x1E))return 0;
    PlayerState live{};
    if(!readPlayer(live) || !grass(live.x,live.y,live.elevation))return 0;
    if(releasedEncounter(live))return guestReturn(cpu,1);
    if(!wildOn&&!roomState.connected)return 0;
    auto found=std::find_if(wild.begin(),wild.end(),[&](const Wild& w){return w.x==live.x && w.y==live.y && now>w.born+30;});
    bool contact=found!=wild.end();
    if(contact&&roomState.connected&&!claim(online::EncounterKind::Wild,found->id))contact=false;
    if(contact&&!callGuest(addresses.repel,found->level)){
        std::printf("VISIBLE_WILD_REPEL species=%u level=%u\n",found->species,found->level);
        wild.erase(found);contact=false;if(encounterToken){room->releaseEncounter(encounterToken);encounterToken=0;}
    }
    if(contact){
        const Wild chosen=*found;const auto encounterPose=controlsWild()?wildPose(chosen):wildMotion.contains(chosen.id)?wildMotion[chosen.id].sample(clockMs()):wildPose(chosen);wild.erase(found);
        std::printf("VISIBLE_WILD_CONTACT species=%u level=%u tile=%d,%d\n",chosen.species,chosen.level,chosen.x,chosen.y);
#ifdef FR_TEST_HARNESS
        chaseRequested=walkRequested=false;bus()->io().set_keyinput(0x3ff);
#endif
        createVisibleWild(chosen.species,chosen.level,chosen.shiny);
        beginBattlePresence(chosen.species,chosen.id,&encounterPose);
        callGuest(addresses.startWild);
        lastSpawn=now+120;
    }
    cpu->R[0]=contact?1:0;
    cpu->R[15]=cpu->R[14]&~1u;
    if(cpu->R[14]&1)cpu->cpsr|=CPSR_T_BIT;else cpu->cpsr&=~CPSR_T_BIT;
    return 1;
}
int blit(uint8_t* /*rgb*/,unsigned screenW,unsigned screenH,uint32_t pixels,uint32_t palette,int w,int h,int x,int y,bool flip=false,bool flipY=false,bool paint=true){
    if(w<=0||h<=0||w>64||h>64||!validRom(pixels,size_t(w*h/2))||!validRom(palette,32))return int(screenH);
    int top=int(screenH);
    for(int py=0;py<h;++py)for(int px=0;px<w;++px){
        const int dx=x+px,dy=y+py;if(dx<0||dy<0||dx>=int(screenW)||dy>=int(screenH))continue;
        const int sx=flip?w-1-px:px;
        const int sy=flipY?h-1-py:py;
        const unsigned index=(sy/8*(w/8)+sx/8)*32+(sy%8)*4+(sx%8)/2;
        const unsigned color=(r8(pixels+index)>>((sx&1)*4))&15;if(!color)continue;
        top=std::min(top,dy);
        const uint16_t c=r16(palette+2*color);
        if(paint)hostPixel(dx,dy,c);
    }
    return top;
}
void icon(uint8_t* rgb,unsigned w,unsigned h,uint16_t species,int x,int y){
    if(!species || species>411)return;
    const auto pixels=r32(addresses.icons+species*4);
    const unsigned palette=r8(addresses.iconIndices+species);
    // gMonIconPalettes is three inline 16-color RGB555 palettes, not a
    // SpritePalette descriptor table. Treating colors as pointers hid every icon.
    if(palette>=3||!validRom(pixels,1024))return;
    const uint32_t frame=pixels+uint32_t((now/16)&1)*512;
    // Party icons include transparent padding. Put their visible feet on the
    // tile's ground line, rather than floating at the trainer's head height.
    int bottom=-1;
    for(int py=31;py>=0&&bottom<0;--py)for(int px=0;px<32;++px){
        const unsigned index=(py/8*4+px/8)*32+(py%8)*4+(px%8)/2;
        if((r8(frame+index)>>((px&1)*4))&15){bottom=py;break;}
    }
    if(bottom>=0)blit(rgb,w,h,frame,addresses.iconPalettes+palette*32,32,32,x-16,y+15-bottom);
}
const FollowerSheet* followerSheet(uint16_t species,bool shiny=false){
    const uint32_t key=species|(uint32_t(shiny)<<16);
    auto found=followerSheets.find(key);if(found!=followerSheets.end())return found->second.image.empty()?nullptr:&found->second;
    std::string name;
    if(validRom(speciesNames+species*11,11))for(unsigned i=0;i<11;++i){const auto c=r8(speciesNames+species*11+i);if(c==255)break;if(c>=0xbb&&c<=0xd4)name+=char('A'+c-0xbb);else if(c>=0xa1&&c<=0xaa)name+=char('0'+c-0xa1);}
    if(species==29)name="NIDORANfE";if(species==32)name="NIDORANmA";
    FollowerSheet sheet;
    if(!name.empty())try{
        const auto folder=shiny?"Followers shiny":"Followers";
        auto path=diagnosticPath.parent_path()/folder/(name+".png");
        if(!std::filesystem::exists(path))path=localAsset(std::filesystem::path("LocalAssets")/folder/(name+".png"));
        if(path.empty())path=localAsset(std::filesystem::path("Following Pokemon EX/Graphics/Characters")/folder/(name+".png"));
        if(!path.empty())sheet=decodeFollowerSheet(readImage(path));
    }catch(const std::exception& e){std::fprintf(stderr,"Follower art: %s\n",e.what());}
    const auto& result=followerSheets.emplace(key,std::move(sheet)).first->second;
    return result.image.empty()?nullptr:&result;
}
void follower(uint8_t* rgb,unsigned w,unsigned h,const PlayerState& p,int x,int y){
    const auto* sheet=followerSheet(p.follower,p.followerShiny);
    if(!sheet){icon(rgb,w,h,p.follower,x,y);return;}
    const unsigned row=followerRow(p.followerFacing),frame=p.followerFrame%4;
    const int top=y+15-sheet->feet[row*4+frame],left=x-sheet->cellW/2;
    for(int yy=0;yy<sheet->cellH;++yy)for(int xx=0;xx<sheet->cellW;++xx){
        const auto rgba=sheet->image.pixels[(row*sheet->cellH+yy)*sheet->image.width+frame*sheet->cellW+xx];if(!(rgba>>24))continue;
        const auto c=uint16_t(((rgba&255)>>3)|((((rgba>>8)&255)>>3)<<5)|((((rgba>>16)&255)>>3)<<10));hostPixel(left+xx,top+yy,c,rgba);
    }
}
int trainer(uint8_t* rgb,unsigned w,unsigned h,const PlayerState& peer,int x,int y,bool paint=true){
    if(peer.graphics>=152)return int(h);
    const uint32_t info=r32(addresses.graphics+peer.graphics*4);
    if(!validRom(info,36))return int(h);
    const int sw=r16(info+8),sh=r16(info+10);const uint16_t tag=r16(info+2);
    uint32_t pal=0;
    for(unsigned i=0;i<19;++i)if(r16(addresses.palettes+i*8+4)==tag){pal=r32(addresses.palettes+i*8);break;}
    const auto frames=r32(info+28);
    const unsigned frame=peer.spriteFrame;
    if(!validRom(frames,8*(frame+1)))return int(h);
    return blit(rgb,w,h,r32(frames+frame*8),pal,sw,sh,x-sw/2+peer.offsetX,y+16-sh+peer.offsetY,(peer.flip&1)!=0,(peer.flip&2)!=0,paint);
}
void drawCampReaction(const CampMon&,const PlayerState&,unsigned,unsigned,uint32_t,int,int);
#include "world_camp.inc"
bool departingSlot(unsigned slot);
#include "world_battle.inc"
#include "world_field_battle.inc"
#include "world_session.inc"
#include "world_follower.inc"
void drawCaptions(){
    struct Box {int x,y,w,h;};
    auto overlap=[](Box a,Box b){return std::max(0,std::min(a.x+a.w,b.x+b.w)-std::max(a.x,b.x))*std::max(0,std::min(a.y+a.h,b.y+b.h)-std::max(a.y,b.y));};
    std::erase_if(captionAnchors,[](const auto& a){return a.slot>=MaxRoomPlayers||a.top<=0||a.top>=160||a.x<0||a.x>=240||captions[a.slot].info.name.empty();});
    std::sort(captionAnchors.begin(),captionAnchors.end(),[](auto a,auto b){return a.slot<b.slot;});
    std::vector<Box> protectedAreas,placed;
    for(const auto& b:followerEmoteBounds)protectedAreas.push_back({b[0],b[1],b[2],b[3]});
    struct Labels {Box name{},party{};};std::array<Labels,MaxRoomPlayers> boxes{};
    for(const auto& a:captionAnchors)protectedAreas.push_back({a.x-9,a.top,18,a.bottom-a.top+2});
    for(const auto& a:captionAnchors){const auto& n=captions[a.slot].name;auto& b=boxes[a.slot];
        const auto& row=partyImage(a.partyCount,a.partyEggs);const int nameY=std::max(0,a.top-n.height-1);
        // Keep each username and its icons together when nearby trainers crowd
        // the same label space. Native text boxes still occlude the whole stack.
        const bool icons=row.height&&nameY-row.height-2>=1;
        const int width=std::max(n.width,icons?row.width:0),extra=icons?row.height+2:0,height=n.height+extra;
        const int idealX=std::clamp(a.x-width/2,1,239-width),idealY=nameY-extra;
        std::vector<int> xs{idealX},ys{idealY};
        for(auto box:protectedAreas){xs.push_back(box.x-width-2);xs.push_back(box.x+box.w+2);ys.push_back(box.y-height-2);}
        Box best{idealX,idealY,width,height};int score=INT_MAX;
        for(int x:xs)for(int y:ys){if(x<1||x+width>239||y<0||y>idealY)continue;
            const Box candidate{x,y,width,height};int cost=std::abs(x-idealX)+2*std::abs(y-idealY);
            for(auto box:protectedAreas)cost+=1000*overlap(candidate,box);
            if(cost<score){score=cost;best=candidate;}
        }
        b.name={best.x+(width-n.width)/2,best.y+extra,n.width,n.height};
        // If there is no room above the original username at the viewport edge,
        // omit the entire row rather than cutting the supplied artwork in half.
        if(icons)b.party={best.x+(width-row.width)/2,best.y,row.width,row.height};
        protectedAreas.push_back(best);
    }
    auto draw=[](const LabelImage& image,int left,int y){
        for(int yy=0;yy<image.height;++yy)for(int xx=0;xx<image.width;++xx){
            const auto rgba=image.pixels[yy*image.width+xx];if(!(rgba>>24))continue;
            const auto color=uint16_t(((rgba&255)>>3)|((((rgba>>8)&255)>>3)<<5)|((((rgba>>16)&255)>>3)<<10));
            hostPixel(left+xx,y+yy,color,rgba);
        }
    };
#ifdef FR_TEST_HARNESS
    std::ofstream diagnostic;if(now%60==0&&!diagnosticPath.empty())diagnostic.open(diagnosticPath.parent_path()/"annotations-check.txt");
    if(diagnostic)diagnostic<<"names="<<captionAnchors.size()<<'\n';
#endif
    for(const auto& a:captionAnchors){const auto& c=captions[a.slot];
        if(c.info.message.empty())continue;
        const int w=c.bubble.width,h=c.bubble.height;
        const int idealX=std::clamp(a.x-w/2,1,239-w),idealY=(boxes[a.slot].party.h?boxes[a.slot].party.y:boxes[a.slot].name.y)-h-2;
        std::vector<int> xs{idealX,1,239-w},ys{idealY,a.bottom+4,1,159-h};
        for(const auto& box:protectedAreas){xs.push_back(box.x-w-3);xs.push_back(box.x+box.w+3);ys.push_back(box.y-h-3);ys.push_back(box.y+box.h+3);}
        Box best{idealX,std::clamp(idealY,1,159-h),w,h};int bestScore=INT_MAX;
        // A few stable candidate positions keep speech clear of every trainer
        // and username, including a speaker near a viewport edge.
        for(int x:xs)for(int y:ys){if(x<1||x+w>239||y<1||y+h>159)continue;Box candidate{x,y,w,h};
            int score=2*std::abs(x-idealX)+std::abs(y-idealY);
            for(auto box:protectedAreas)score+=1000*overlap(candidate,box);
            for(auto box:placed)score+=100*overlap(candidate,box);
            if(score<bestScore){bestScore=score;best=candidate;}
        }
        placed.push_back(best);
        // Priority one places annotations above the field, while native BG0
        // menus/dialogue, priority-zero objects and hardware windows still win.
        drawingKey=gba::host_obj_key(1,8+a.slot);
        draw(c.bubble,best.x,best.y);
#ifdef FR_TEST_HARNESS
        if(diagnostic){int obscured=0;for(auto box:protectedAreas)obscured+=overlap(best,box);diagnostic<<"bubble="<<a.slot<<" x="<<best.x<<" y="<<best.y<<" width="<<w<<" height="<<h<<" actor_overlap="<<obscured<<'\n';}
#endif
    }
    for(const auto& a:captionAnchors){const auto& n=captions[a.slot].name;drawingKey=gba::host_obj_key(1,1+a.slot);
        const auto& b=boxes[a.slot];draw(n,b.name.x,b.name.y);
        if(b.party.h)draw(partyImage(a.partyCount,a.partyEggs),b.party.x,b.party.y);
#ifdef FR_TEST_HARNESS
        if(diagnostic)diagnostic<<"party="<<a.slot<<" count="<<unsigned(a.partyCount)<<" eggs="<<unsigned(a.partyEggs)<<" visible="<<(b.party.h>0)<<" x="<<b.party.x<<" y="<<b.party.y<<" width="<<b.party.w<<" height="<<b.party.h<<" name_x="<<b.name.x<<" name_y="<<b.name.y<<" name_width="<<b.name.w<<" name_height="<<b.name.h<<'\n';
#endif
    }
}
}
void initialize(FireRedRevision revision,const std::filesystem::path& diagnostics,const std::filesystem::path& saveFile){
    campaignNativeSave=saveFile;
    gameRevision=revision;
    addresses={gameAddress(0x080565b4),gameAddress(0x083d37a0),gameAddress(0x083d3e80),gameAddress(0x083d3740),gameAddress(0x083c9cb8),
               gameAddress(0x0839fdb0),gameAddress(0x083a5158),gameAddress(0x080833b0),gameAddress(0x080a029c),gameAddress(0x0807f704),gameAddress(0x0808310c)};
    registerFieldHooks();
    registerShinyHooks();
    registerStoryRetryHooks();
    registerWorldSessionHooks();
    diagnosticPath=diagnostics;lastRemoteField={};battleVisualId={};localBattle={};battleMotion={};battleEntered=battleMonDataReady=false;battleIdentity=uint32_t(uint64_t(clockMs()));
    gba_mod_register_function_entry_plugin("pokemulti.release",gameAddress(0x08093218),1,releaseMonHook);
    gba_mod_set_function_hook_enabled("pokemulti.release",1);
    gba_mod_register_function_entry_plugin("pokemulti.battle-start",gameAddress(0x0807f690),1,battleStartHook);
    gba_mod_set_function_hook_enabled("pokemulti.battle-start",1);
    localCamp={};campFrames={};campMotion={};campBorders={};campRequested=campPending=false;campIdentity=uint32_t(uint64_t(clockMs()));
    gba_mod_register_function_entry_plugin("pokemulti.field-collision",gameAddress(0x080636ac),1,fieldCollisionHook);
    gba_mod_set_function_hook_enabled("pokemulti.field-collision",1);
    for(const auto& hook:std::vector<std::pair<uint32_t,decltype(&npcMovementHook)>>{{0x08069fb0,campaignGymGiftHook},{0x0806e6d0,campaignFlagHook},{0x080a011c,uniqueGiftHook},{0x08040b14,uniqueGiftHook},{0x08063db8,npcMovementHook},{0x0806cff4,interactedHook},{0x0806dd80,coordHook},{0x08081b84,trainerHook},{0x08069c74,onFrameHook}}){const auto name="firered.shared-"+std::to_string(hook.first);gba_mod_register_function_entry_plugin(name.c_str(),gameAddress(hook.first),1,hook.second);gba_mod_set_function_hook_enabled(name.c_str(),1);}
    speciesNames=gameAddress(0x08245ee0);followerSheets.clear();followerPath.clear();
    elevationPriority=gameAddress(0x083a707c);elevationSubpriority=gameAddress(0x083a706c);
    gba_mod_register_function_entry_plugin("firered.oam-begin",gameAddress(0x08006ba8),1,beginOamHook);
    gba_mod_set_function_hook_enabled("firered.oam-begin",1);
    gba_mod_register_function_entry_plugin("firered.oam-submit",gameAddress(0x08008a64),1,submitOamHook);
    gba_mod_set_function_hook_enabled("firered.oam-submit",1);
    gba_mod_register_function_entry_plugin("firered.oam-finish",gameAddress(0x08006eb8),1,finishOamHook);
    gba_mod_set_function_hook_enabled("firered.oam-finish",1);
    // CB2 entry observes the previous completed field update. A PPU frame yield
    // can interrupt CameraUpdate between its tile and pixel writes.
    gba_mod_register_function_entry_plugin("firered.presentation",addresses.overworld,1,completedWorldHook);
    gba_mod_set_function_hook_enabled("firered.presentation",1);
#ifdef FR_TEST_HARNESS
    gba_mod_register_function_entry_plugin("firered.test-fixture",addresses.overworld,1,fixtureHook);
    gba_mod_set_function_hook_enabled("firered.test-fixture",1);
#endif
    gba_mod_register_function_entry_plugin("firered.visible-wild",addresses.encounter,1,encounterHook);
    gba_mod_set_function_hook_enabled("firered.visible-wild",1);
}
void ready(){
    if(auto* ppu=gbarecomp::active_ppu())ppu->enable_obj_composition(nativeOrder);
}
void connect(online::Session* session){room=session;}
void localSlot(unsigned slot){roomSlot=uint8_t(slot<MaxRoomPlayers?slot:0);}
void frame(uint64_t frameNumber){
    now=frameNumber;if(guestCall)return;
#ifdef FR_TEST_HARNESS
    performance::frame=now;
    static bool profilingOpened=false;if(!profilingOpened){performance::open(diagnosticPath.parent_path());profilingOpened=true;}
#endif
    performance::Scope frameTiming("world-frame");
    PlayerState next=completedPlayer;
    static uint32_t publication=0;next.sequence=++publication;
    if(!bus()||nativeLinkScene()||replaying()||(r32(0x030030F4)&~1u)!=addresses.overworld||(r8(0x02037abf)&0x80)){next.active=false;next.sampleTime=uint32_t(uint64_t(clockMs()));}
    originX=completedOriginX;originY=completedOriginY;avatarFlags=completedAvatarFlags;haveOrigin=next.active;
    const bool battleMap=localBattle.id&&(!next.active||(next.mapGroup==localBattle.trainer.mapGroup&&next.mapNumber==localBattle.trainer.mapNumber&&next.elevation==localBattle.trainer.elevation));
    if(!sameMap(next,local)&&!battleMap){wild.clear();lastSpawn=now;}
    previous=local;local=next;
    {performance::Scope timing("syncWorldFrame");syncWorldFrame();}
    {performance::Scope timing("updateStoryGates");updateStoryGates();}
    {performance::Scope timing("updateStoryRetry");updateStoryRetry();}
    {performance::Scope timing("updateCamp");updateCamp();}
    {performance::Scope timing("updateBattlePresence");updateBattlePresence();}
    {performance::Scope timing("updateReleases");updateReleases();}
    {performance::Scope timing("updateFieldUi");updateFieldUi();}
    {performance::Scope timing("updateFieldBattle");updateFieldBattle();}
    {performance::Scope timing("updateFieldNotices");updateFieldNotices();}
    {performance::Scope timing("updateWorldSession");updateWorldSession();}
#ifdef FR_TEST_HARNESS
    if(!diagnosticPath.empty()){
        static std::ofstream sent(diagnosticPath.parent_path()/"motion-source.csv");
        sent<<now<<','<<local.sequence<<','<<local.sampleTime<<','<<local.active<<','<<local.pixelX<<','<<local.pixelY<<','<<unsigned(local.facing)<<','<<unsigned(local.spriteFrame)<<','<<unsigned(local.flip)<<'\n';if(now%60==0)sent.flush();
    }
#endif
    if(!local.active){
        publishWorld();
#ifdef FR_TEST_HARNESS
        driveTestInput();
#endif
        return;
    }
    updateFollower();
#ifdef FR_TEST_HARNESS
    if(!diagnosticPath.empty()){static std::ofstream jumps(diagnosticPath.parent_path()/"follower-jumps.csv");jumps<<now<<','<<local.pixelX<<','<<local.pixelY<<','<<unsigned(local.elevation)<<','<<int(local.offsetY)<<','<<local.followerX<<','<<local.followerY<<','<<int(local.followerOffsetY)<<','<<unsigned(local.followerFacing)<<'\n';if(now%15==0)jumps.flush();}
    if(!diagnosticPath.empty()){static std::ofstream poses(diagnosticPath.parent_path()/"follower-source.csv");
        poses<<now<<','<<local.pixelX<<','<<local.pixelY<<','<<unsigned(local.facing)<<','<<local.followerVisible<<','<<local.followerX<<','<<local.followerY<<','<<unsigned(local.followerFacing)<<','<<unsigned(local.followerFrame)<<'\n';if(now%60==0)poses.flush();}
#endif
    if((wildOn||roomState.connected) && controlsWild() && now>=lastSpawn+120 && local.follower && !(avatarFlags&0x1E)){
        lastSpawn=now;
        for(auto& m:wild){
            constexpr int dx[]{0,0,-1,1},dy[]{1,-1,0,0};const unsigned direction=random()%4;
            const int x=m.x+dx[direction],y=m.y+dy[direction];
            if(worldInView(x,y)&&!worldOccupied(x,y)&&grass(x,y,local.elevation)&&std::none_of(wild.begin(),wild.end(),[&](const Wild& other){return other.x==x&&other.y==y;})){
                m.oldX=m.x;m.oldY=m.y;m.moved=now;m.x=x;m.y=y;m.facing=uint8_t(direction+1);
            }
        }
        std::erase_if(wild,[&](const Wild& m){return now-m.born>1800 || !worldInView(m.x,m.y);});
        const auto viewers=worldViewers();
        for(unsigned tries=0;tries<80 && wild.size()<std::min(size_t(12),3*viewers.size());++tries){
            const auto& center=viewers[random()%viewers.size()];const int x=center.x+int(random()%13)-6,y=center.y+int(random()%11)-5;
            if(!worldInView(x,y) || std::abs(x-center.x)+std::abs(y-center.y)<2 || worldOccupied(x,y) || !grass(x,y,local.elevation))continue;
            if(std::any_of(wild.begin(),wild.end(),[&](const Wild& m){return m.x==x&&m.y==y;}))continue;
            uint16_t species;uint8_t level;if(chooseWild(species,level)){Wild spawn{x,y,species,level,now,x,y,now,(uint32_t(roomSlot)<<27)|(++wildIdentity&0x07ffffffu)};spawn.shiny=rollShiny(roomState.shinyRate);wild.push_back(spawn);}
        }
    }
#ifdef FR_TEST_HARNESS
    driveTestInput();
#endif
    publishWorld();
    if(!diagnosticPath.empty() && now%60==0){
        std::ofstream f(diagnosticPath);
        f<<"frame="<<now<<" map="<<unsigned(local.mapGroup)<<","<<unsigned(local.mapNumber)
         <<" tile="<<local.x<<","<<local.y<<" elevation="<<unsigned(local.elevation)
         <<" follower_xy="<<local.followerX<<","<<local.followerY<<" follower_facing="<<unsigned(local.followerFacing)<<" follower_frame="<<unsigned(local.followerFrame)
         <<" lead_species="<<local.follower<<" lead_shiny="<<local.followerShiny<<" wild="<<wild.size()<<" origin="<<originX<<","<<originY
         <<" offset="<<s16(0x02021bc8)<<","<<s16(0x02021bca)<<"\n";
        for(const auto& m:wild)f<<"wild="<<m.x<<","<<m.y<<" species="<<m.species<<" level="<<unsigned(m.level)<<" shiny="<<m.shiny<<" id="<<m.id<<"\n";
        const auto events=r32(0x02036e00);
        if(validRom(events,20)){
            const unsigned count=r8(events+1);const auto warps=r32(events+8);
            if(count<32&&validRom(warps,count*8))for(unsigned i=0;i<count;++i){const auto a=warps+i*8;f<<"warp="<<s16(a)<<","<<s16(a+2)<<" to="<<unsigned(r8(a+7))<<","<<unsigned(r8(a+6))<<"\n";}
        }
    }
}
void render(uint8_t* rgb,uint32_t w,uint32_t h){
    if(nativeLinkScene() || replaying() || !local.active || !haveOrigin || !rgb || w!=240 || h!=160)return;
    // Native normal fade coefficient remains 16 at the black endpoint even
    // after the active flag clears. Hardware effects are handled by the PPU.
    const unsigned fadeMode=r8(0x02037ac0)&3,fadeAmount=(r16(0x02037abc)>>6)&31;
    if((r8(0x02037abf)&0x80)||(fadeMode==0&&fadeAmount))return;
    if(std::all_of(rgb,rgb+w*h*3,[](uint8_t c){return c==0;}))return;
    performance::Scope timing("render");
    std::fill(hostPixels.begin(),hostPixels.end(),gba::HostObjPixel{});
    captionAnchors.clear();followerEmoteBounds.clear();
    const int dx=completedOriginX+completedOffsetX,dy=completedOriginY+completedOffsetY;
#ifdef FR_TEST_HARNESS
    hostIntersections=0;
#endif
    drawCamps(rgb,w,h,dx,dy);
    drawBattles(rgb,w,h,dx,dy);
    unsigned releaseIndex=160;for(const auto& p:releasePoses()){
        drawingKey=spriteKey(p.elevation,p.pixelY+dy+16,releaseIndex++);
        follower(rgb,w,h,p,p.pixelX+dx+8,p.pixelY+dy+p.offsetY);
    }
    unsigned wildIndex=0;
    if(wildOn)for(const auto& m:wild){
        const auto p=controlsWild()?wildPose(m):wildMotion.contains(m.id)?wildMotion[m.id].sample(clockMs()):PlayerState{};if(!p.active)continue;
        drawingKey=spriteKey(local.elevation,p.pixelY+dy+16,32+wildIndex++);
        follower(rgb,w,h,p,p.pixelX+dx+8,p.pixelY+dy);
#ifdef FR_TEST_HARNESS
        if(!diagnosticPath.empty()){static std::ofstream trace(diagnosticPath.parent_path()/"wild-motion.csv");trace<<now<<','<<m.id<<','<<p.pixelX<<','<<p.pixelY<<','<<unsigned(p.followerFacing)<<','<<unsigned(p.followerFrame)<<','<<(followerSheet(p.follower,p.followerShiny)!=nullptr)<<'\n';if(now%60==0)trace.flush();}
#endif
    }
    drawCompanion(rgb,w,h,local,roomSlot,dx,dy);
    if(!captions[roomSlot].info.name.empty()){
        const int x=local.pixelX+dx+8,y=local.pixelY+dy;
        drawingKey=spriteKey(local.elevation,y+16,roomSlot);
        captionAnchors.push_back({roomSlot,x+local.offsetX,trainer(rgb,w,h,local,x,y,false),y+16+local.offsetY,local.partyCount,local.partyEggs});
    }
    const double time=clockMs();
    for(const auto& received:remote)if(received.netSlot<MaxRoomPlayers&&!battlingSlot(received.netSlot)&&!departingSlot(received.netSlot)){
        const auto p=motion[received.netSlot].sample(time);
        if(!sameScene(local,p))continue;
        lastRemoteField[p.netSlot]=p;
        const int x=p.pixelX+dx+8,y=p.pixelY+dy;
        drawCompanion(rgb,w,h,p,p.netSlot,dx,dy);
        drawingKey=spriteKey(p.elevation,y+16,p.netSlot);
        const int top=trainer(rgb,w,h,p,x,y);
#ifdef FR_TEST_HARNESS
        static std::ofstream poses(diagnosticPath.parent_path()/"motion-render.csv");
        poses<<now<<','<<uint64_t(time)<<','<<unsigned(p.netSlot)<<','<<p.sequence<<','<<p.pixelX<<','<<p.pixelY<<','<<unsigned(p.facing)<<','<<unsigned(p.spriteFrame)<<','<<unsigned(p.flip)<<','<<dx<<','<<dy<<','<<local.pixelX<<','<<local.pixelY<<'\n';if(now%60==0)poses.flush();
#endif
        if(captions[p.netSlot].info.identity==p.identity)captionAnchors.push_back({p.netSlot,x+p.offsetX,top,y+16+p.offsetY,p.partyCount,p.partyEggs});

    }
    drawDepartures(rgb,w,h,dx,dy);
    drawCaptions();
    if(auto* ppu=gbarecomp::active_ppu()){
#ifdef FR_TEST_HARNESS
        if(now%60==0&&!diagnosticPath.empty())if(const auto* context=ppu->obj_composition_frame()){
            unsigned proposed=0,foreground=0,ui=0,objects=0,window=0;
            for(unsigned i=0;i<hostPixels.size();++i){const auto& obj=hostPixels[i];if(obj.key==gba::kNoHostObject)continue;
                ++proposed;const auto& native=context->pixels[i];
                if(!native.objects)++window;
                else if(native.top.key<obj.key){if(native.top.layer==0)++ui;else if(native.top.layer==4)++objects;else ++foreground;}}
            std::ofstream report(diagnosticPath.parent_path()/"composition-check.txt");
            report<<"frame="<<now<<" proposed="<<proposed<<" ui="<<ui<<" foreground="<<foreground<<" native_objects="<<objects<<" window="<<window<<" host_intersections="<<hostIntersections<<" native_matches="<<nativeOrderMatches<<" native_misses="<<nativeOrderMisses<<'\n';
        }
#endif
        ppu->compose_host_objects(rgb,hostPixels.data(),w,h);
#ifdef FR_TEST_HARNESS
        if(now%120==0&&!diagnosticPath.empty())if(const auto* context=ppu->obj_composition_frame()){
            std::ofstream pixels(diagnosticPath.parent_path()/"composition-pixels.csv");
            pixels<<"x,y,host_key,native_key,native_layer,obj_enabled,effect,before_r,before_g,before_b,after_r,after_g,after_b\n";
            const auto* original=ppu->latched_framebuffer();
            for(unsigned i=0;i<hostPixels.size();++i){const auto& obj=hostPixels[i];if(obj.key==gba::kNoHostObject)continue;
                const auto& native=context->pixels[i];
                pixels<<i%240<<','<<i/240<<','<<obj.key<<','<<native.top.key<<','<<unsigned(native.top.layer)<<','<<native.objects<<','<<((context->lines[i/240].control>>6)&3);
                for(unsigned c=0;c<3;++c)pixels<<','<<unsigned(original[i*3+c]);
                for(unsigned c=0;c<3;++c)pixels<<','<<unsigned(rgb[i*3+c]);pixels<<'\n';
            }
        }
#endif
    }
}
std::vector<std::string> unclaimedStoryRewards(){std::vector<std::string> out;if(roomState.connected&&storyApplied&&local.active&&!replaying())for(const auto& r:storyRewardRules)if(shareReward(r,roomState.rewardPolicy)&&campaignValue(r.progress)>=r.minimum&&!campaignRewardCollected(r))out.push_back(r.name);return out;}
void showCampaignJournal(){
    std::string text="Campaign journal";
    const auto& entries=roomState.campaign.completed;
    if(entries.empty())text+="\nYour shared adventure is beginning.";
    const size_t first=entries.size()>8?entries.size()-8:0;
    for(size_t i=first;i<entries.size();++i)if(const auto* r=campaignObjective(entries[i].objective))text+="\n"+std::string(r->title)+" - "+entries[i].actor;
    queueFieldNotice("journal",text);
}
void claimStoryRewards(){rewardRequested=true;}
uint32_t walletBalance(){return shownBalance;}
bool walletAvailable(){return walletKnown;}
uint32_t walletHeld(){return walletJournal.phase==1?walletJournal.stake:0;}
std::string walletNotice(){return walletMessage;}
std::string storyRewardNotice(){return rewardNotice;}
std::string releaseNotice(){
    if(releaseSaving||releaseBattleSaving)return "Saving your Pokemon...";
    if(!pendingReleases.empty())return safeWallet()?"Sharing released Pokemon...":"Log off the PC to save releases.";
    if(!releaseBattleId.empty()&&!roomState.connected&&!releaseBattleHost.empty())return "Rejoin the host to finish this encounter.";
    unsigned waiting=0,traveling=0;for(const auto& m:releasePopulation())if(room&&m.owner==room->id()){waiting+=m.phase==ReleasePhase::Waiting;traveling+=m.phase==ReleasePhase::Hello||m.phase==ReleasePhase::Leaving||m.phase==ReleasePhase::Goodbye||m.phase==ReleasePhase::Running;}
    if(waiting)return std::to_string(waiting)+(waiting==1?" Pokemon is waiting outside.":" Pokemon are waiting outside.");
    if(traveling)return "Released Pokemon heading to grass.";
    return {};
}
PlayerState player(){return local;}
void peers(std::vector<PlayerState> states){
    const double time=clockMs();
    for(const auto& p:states)if(p.netSlot<MaxRoomPlayers)motion[p.netSlot].push(p,time);
    for(unsigned i=0;i<MaxRoomPlayers;++i)if(std::none_of(states.begin(),states.end(),[&](const PlayerState& p){return p.netSlot==i;}))motion[i].clear();
    remote=std::move(states);
}
void overheads(std::vector<Overhead> labels){
    for(unsigned slot=0;slot<MaxRoomPlayers;++slot){
        const auto i=std::find_if(labels.begin(),labels.end(),[&](const Overhead& n){return n.slot==slot;});
        auto& c=captions[slot];
        if(i==labels.end()){c={};continue;}
        if(c.info.identity!=i->identity)c={};
        if(c.info.name!=i->name)c.name=nameImage(i->name);
        if(c.info.message!=i->message)c.bubble=i->message.empty()?LabelImage{}:bubbleImage(i->message);
        c.info=*i;
    }
}
bool requestWorldExit(){return beginWorldExit();}
bool worldSavePending(){
    if(!room)return false;const auto s=room->status();
    return s.managedWorld&&(worldSaveRequested||manualWorldSave||s.checkpointWaiting||s.checkpointAck<lastCheckpointSent);
}
void toggleCamp(){if(!fieldDialog.memory&&!fieldBattleActive)campRequested=true;}
void challengePlayer(uint8_t slot){requestFieldChallenge(slot);}
void tradeNearby(){
    if(!room||!roomState.connected||fieldUiBusy()||noticeAllocation||!safeWallet())return;
    constexpr int dx[]{0,0,0,-1,1},dy[]{0,1,-1,0,0};
    for(const auto& p:roomState.peers)if(p.slot!=roomSlot&&sameScene(local,p.player)&&p.player.x==local.x+dx[local.facing]&&p.player.y==local.y+dy[local.facing]){
        if(p.partner<0&&!room->cableConnected()){room->invite(p.slot,online::Activity::Trade);queueFieldNotice("trade-sent","Trade invitation sent to "+p.name+".");}return;
    }
}
bool camping(){return localCamp.state.id!=0;}
std::string campNotice(){return campMessage;}
uint16_t filterInput(uint16_t input){return worldExitRequested||campPending||localBattle.returning||gateWaitEvent?uint16_t(0x3ff):input;}
void followers(bool enabled){followOn=enabled;}
void visibleWild(bool enabled){wildOn=enabled;if(!enabled&&!roomState.connected)wild.clear();}
bool followersEnabled(){return followOn;}
bool visibleWildEnabled(){return wildOn;}
#ifdef FR_TEST_HARNESS
uint16_t testMovementInput(uint16_t original){if(chaseRequested||walkRequested){driveTestInput();return r16(0x04000130);}return original;}
void chaseWild(){chaseRequested=true;}
void walkTo(int x,int y){walkX=x;walkY=y;walkRequested=true;}
void requestFixture(const std::string& name){
    if(name!="shiny-check"&&name!="field-a"&&name!="field-b"&&name!="field-center-a"&&name!="field-center-b"&&!name.starts_with("campaign-")&&name!="battle-char"&&name!="released-a"&&name!="released-b"&&name!="spectate"&&name!="camp-route1"&&name!="camp-route1-b"&&name!="camp-a"&&name!="camp-b"&&name!="wild"&&name!="wild-repel"&&name!="cable-a"&&name!="cable-b"&&name!="story-rival"&&name!="unique-eevee"&&name!="wager-a"&&name!="wager-b")throw std::runtime_error("Unknown test fixture");
    fixtureRequest=name;
}
#endif
}
