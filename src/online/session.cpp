#include "online/session.hpp"
#include "online/campaign.hpp"
#include "frontend/world_store.hpp"
#include "platform/text.hpp"
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <bcrypt.h>
#include <iphlpapi.h>
#else
#include "platform/mac_network.hpp"
#include "platform/mac_support.hpp"
#endif
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <optional>
#include <sstream>
namespace fr::online {
namespace {
using Bytes=std::vector<uint8_t>;
#ifdef _WIN32
using SocketLength=int;
#else
using SocketLength=socklen_t;
#endif
using Clock=std::chrono::steady_clock;
enum Type:uint8_t {Hello=1,Welcome=2,State=3,Snapshot=4,Invite=5,Reply=6,CableOff=7,Ready=8,SerialStart=9,SerialReply=10,Notice=11,Motion=12,ChatSend=13,ChatBroadcast=14,WorldUpdate=15,WorldSnapshot=16,StoryUpdate=17,StorySnapshot=18,EncounterClaim=19,EncounterResult=20,EncounterRelease=21,EncounterSnapshot=22,WalletUpdate=23,WagerSnapshot=24,WagerEvent=25,CampSnapshot=26,BattleSnapshot=27,ReleasedOffer=28,ReleasedClaim=29,ReleasedFinish=30,ReleasedSnapshot=31,ReleasedResult=32,CampaignSnapshot=33,BattleStage=34,InvitationCancel=35,EncounterCheckpoint=36,Membership=37,DepartureEvent=38,PlayerCheckpoint=39,CheckpointAck=40,CheckpointRequest=41,JoinRejected=42,ShinyRate=43};
void byte(Bytes& b,unsigned v){b.push_back(uint8_t(v));}
void word(Bytes& b,unsigned v){byte(b,v);byte(b,v>>8);}
void dword(Bytes& b,uint32_t v){word(b,v);word(b,v>>16);}
void string(Bytes& b,const std::string& v){if(v.size()>128)throw std::runtime_error("Network string exceeds limit");byte(b,unsigned(v.size()));b.insert(b.end(),v.begin(),v.end());}
struct Reader {
    const Bytes& b;size_t p=0;
    unsigned u8(){if(p>=b.size())throw std::runtime_error("Truncated room packet");return b[p++];}
    bool boolean(){const auto v=u8();if(v>1)throw std::runtime_error("Invalid boolean in room packet");return v!=0;}
    unsigned u16(){auto a=u8();return a|(u8()<<8);}
    uint32_t u32(){auto a=u16();return a|(u16()<<16);}
    std::string str(){size_t n=u8();if(n>128||n>b.size()-p)throw std::runtime_error("Invalid room text");std::string s(b.begin()+p,b.begin()+p+n);p+=n;return s;}
    void end(){if(p!=b.size())throw std::runtime_error("Unexpected room packet data");}
};
void released(Bytes& b,const game::ReleasedMon& m){const auto value=game::encodeReleased(m);word(b,unsigned(value.size()));b.insert(b.end(),value.begin(),value.end());}
game::ReleasedMon released(Reader& r){const auto n=r.u16();if(n>1400||n>r.b.size()-r.p)throw std::runtime_error("Invalid release packet");std::string value(r.b.begin()+r.p,r.b.begin()+r.p+n);r.p+=n;return game::decodeReleased(value);}
void player(Bytes& b,const game::PlayerState& p){
    byte(b,p.active?1:0);byte(b,p.mapGroup);byte(b,p.mapNumber);byte(b,p.elevation);byte(b,p.facing);byte(b,p.graphics);
    word(b,uint16_t(p.x));word(b,uint16_t(p.y));word(b,p.follower);
    word(b,uint16_t(p.pixelX));word(b,uint16_t(p.pixelY));word(b,uint16_t(p.followerX));word(b,uint16_t(p.followerY));
    byte(b,uint8_t(p.offsetX));byte(b,uint8_t(p.offsetY));byte(b,p.spriteFrame);byte(b,p.flip);byte(b,p.followerVisible?1:0);byte(b,p.followerFacing);byte(b,p.followerFrame);
    dword(b,p.sequence);dword(b,p.sampleTime);byte(b,p.partyCount);byte(b,p.partyEggs);byte(b,p.followerShiny);dword(b,p.followerToken);word(b,p.followerEmoteSequence);byte(b,p.followerEmote);
}
game::PlayerState player(Reader& r){
    game::PlayerState p;const auto active=r.u8();p.active=active==1;
    p.mapGroup=uint8_t(r.u8());p.mapNumber=uint8_t(r.u8());p.elevation=uint8_t(r.u8());p.facing=uint8_t(r.u8());p.graphics=uint8_t(r.u8());
    p.x=int16_t(r.u16());p.y=int16_t(r.u16());p.follower=uint16_t(r.u16());
    p.pixelX=int16_t(r.u16());p.pixelY=int16_t(r.u16());p.followerX=int16_t(r.u16());p.followerY=int16_t(r.u16());
    p.offsetX=int8_t(r.u8());p.offsetY=int8_t(r.u8());p.spriteFrame=uint8_t(r.u8());p.flip=uint8_t(r.u8());const auto visible=r.u8();p.followerVisible=visible==1;p.followerFacing=uint8_t(r.u8());p.followerFrame=uint8_t(r.u8());
    p.sequence=r.u32();p.sampleTime=r.u32();p.partyCount=uint8_t(r.u8());p.partyEggs=uint8_t(r.u8());p.followerShiny=r.boolean();p.followerToken=r.u32();p.followerEmoteSequence=uint16_t(r.u16());p.followerEmote=uint8_t(r.u8());if(p.followerEmote>5)throw std::runtime_error("Invalid follower emote");
    if(p.pixelX< -32||p.pixelY< -32||p.pixelX>8224||p.pixelY>8224||p.followerX< -32||p.followerY< -32||p.followerX>8224||p.followerY>8224||std::abs(int(p.offsetX))>64||std::abs(int(p.offsetY))>64||p.spriteFrame>=64||p.flip>3||visible>1||p.followerFacing<1||p.followerFacing>4||p.followerFrame>3)
        throw std::runtime_error("Invalid sprite pose");
    if(p.partyCount>6 || (p.partyEggs>>p.partyCount)!=0 || active>1 || p.elevation>15 || p.facing<1 || p.facing>4 || p.graphics>=152 || p.x<0 || p.y<0 || p.x>511 || p.y>511 || p.follower>411)
        throw std::runtime_error("Invalid player update");
    return p;
}
void npc(Bytes& b,const NpcState& n){byte(b,n.localId);byte(b,n.owner);byte(b,n.anim);byte(b,n.command);word(b,uint16_t(n.oldX));word(b,uint16_t(n.oldY));byte(b,n.visible);dword(b,n.generation);player(b,n.pose);}
NpcState npc(Reader& r){NpcState n;n.localId=uint8_t(r.u8());n.owner=uint8_t(r.u8());n.anim=uint8_t(r.u8());n.command=uint8_t(r.u8());n.oldX=int16_t(r.u16());n.oldY=int16_t(r.u16());auto visible=r.u8();n.visible=visible!=0;n.generation=r.u32();n.pose=player(r);if(visible>1||(n.owner>=MaxRoomPlayers&&n.owner!=255))throw std::runtime_error("Invalid NPC owner");return n;}
void wildState(Bytes& b,const WildState& w){dword(b,w.id);word(b,w.species);byte(b,w.level);byte(b,w.elevation);word(b,uint16_t(w.x));word(b,uint16_t(w.y));word(b,uint16_t(w.pixelX));word(b,uint16_t(w.pixelY));byte(b,w.facing);byte(b,w.frame);byte(b,w.shiny);}
WildState wildState(Reader& r){WildState w;w.id=r.u32();w.species=uint16_t(r.u16());w.level=uint8_t(r.u8());w.elevation=uint8_t(r.u8());w.x=int16_t(r.u16());w.y=int16_t(r.u16());w.pixelX=int16_t(r.u16());w.pixelY=int16_t(r.u16());w.facing=uint8_t(r.u8());w.frame=uint8_t(r.u8());w.shiny=r.boolean();return w;}
void campState(Bytes& b,const game::CampState& c){
    dword(b,c.id);if(!c.id)return;dword(b,c.sequence);dword(b,c.sampleTime);dword(b,c.tick);byte(b,c.group);byte(b,c.map);byte(b,c.elevation);word(b,uint16_t(c.x));word(b,uint16_t(c.y));for(auto row:c.ground)word(b,row);byte(b,unsigned(c.party.size()));
    for(const auto& p:c.party){word(b,p.species);word(b,uint16_t(p.x));word(b,uint16_t(p.y));byte(b,p.facing);byte(b,p.frame);byte(b,p.mood);byte(b,p.shiny);}
}
game::CampState campState(Reader& r){
    game::CampState c;c.id=r.u32();if(!c.id)return c;c.sequence=r.u32();c.sampleTime=r.u32();c.tick=r.u32();c.group=uint8_t(r.u8());c.map=uint8_t(r.u8());c.elevation=uint8_t(r.u8());c.x=int16_t(r.u16());c.y=int16_t(r.u16());for(auto& row:c.ground)row=uint16_t(r.u16());auto n=r.u8();if(n>6)throw std::runtime_error("Too many camp Pokemon");
    while(n--){game::CampMon p;p.species=uint16_t(r.u16());p.x=int16_t(r.u16());p.y=int16_t(r.u16());p.facing=uint8_t(r.u8());p.frame=uint8_t(r.u8());p.mood=uint8_t(r.u8());p.shiny=r.boolean();c.party.push_back(p);}if(!game::validCamp(c))throw std::runtime_error("Invalid camp state");return c;
}
void battleState(Bytes& b,const game::BattlePresence& fight){dword(b,fight.id);if(!fight.id)return;dword(b,fight.tick);byte(b,(fight.settled?1:0)|(fight.returning?2:0));player(b,fight.trainer);byte(b,unsigned(fight.mons.size()));for(const auto& m:fight.mons){byte(b,m.position);word(b,m.species);word(b,uint16_t(m.x));word(b,uint16_t(m.y));byte(b,m.facing);byte(b,m.visible);byte(b,m.frame);byte(b,m.walking);byte(b,m.shiny);}}
game::BattlePresence battleState(Reader& r){game::BattlePresence b;b.id=r.u32();if(!b.id)return b;b.tick=r.u32();auto flags=r.u8();if(flags>3)throw std::runtime_error("Invalid battle phase");b.settled=flags&1;b.returning=flags&2;b.trainer=player(r);auto n=r.u8();if(n>4)throw std::runtime_error("Too many battle actors");while(n--){game::BattleMon m;m.position=uint8_t(r.u8());m.species=uint16_t(r.u16());m.x=int16_t(r.u16());m.y=int16_t(r.u16());m.facing=uint8_t(r.u8());auto visible=r.u8();if(visible>1)throw std::runtime_error("Invalid battle visibility");m.visible=visible!=0;m.frame=uint8_t(r.u8());auto walking=r.u8();if(walking>1)throw std::runtime_error("Invalid battle walking state");m.walking=walking!=0;m.shiny=r.boolean();b.mons.push_back(m);}if(!game::validBattle(b))throw std::runtime_error("Invalid battle presentation");return b;}
void worldReport(Bytes& b,const WorldReport& w){campState(b,w.camp);battleState(b,w.battle);byte(b,w.group);byte(b,w.map);byte(b,(w.field?1:0)|(w.started?2:0)|(w.wildEnabled?4:0));dword(b,w.sequence);byte(b,unsigned(w.npcs.size()));for(const auto& n:w.npcs)npc(b,n);byte(b,unsigned(w.wild.size()));for(const auto& m:w.wild)wildState(b,m);}
WorldReport worldReport(Reader& r){WorldReport w;w.camp=campState(r);w.battle=battleState(r);w.group=uint8_t(r.u8());w.map=uint8_t(r.u8());const auto bits=r.u8();w.field=(bits&1)!=0;w.started=(bits&2)!=0;w.wildEnabled=(bits&4)!=0;w.sequence=r.u32();auto count=r.u8();if(bits>7||count>15)throw std::runtime_error("Invalid world report");while(count--)w.npcs.push_back(npc(r));count=r.u8();if(count>12)throw std::runtime_error("Too many wild Pokemon");while(count--)w.wild.push_back(wildState(r));if(!validWorldReport(w))throw std::runtime_error("Invalid world semantics");return w;}
void storyValues(Bytes& b,const std::vector<game::StoryValue>& values){byte(b,unsigned(values.size()));for(auto v:values){word(b,v.id);word(b,v.value);dword(b,v.revision);}}
std::vector<game::StoryValue> storyValues(Reader& r,uint8_t policy){auto count=r.u8();if(count>128)throw std::runtime_error("Oversized story transaction");std::vector<game::StoryValue> out;while(count--){game::StoryValue v{uint16_t(r.u16()),uint16_t(r.u16()),r.u32()};if(!game::validStory(v,policy))throw std::runtime_error("Invalid story state");out.push_back(v);}return out;}
void encounterKey(Bytes& b,const EncounterKey& k){byte(b,k.group);byte(b,k.map);byte(b,unsigned(k.kind));dword(b,k.id);word(b,k.condition);word(b,k.expected);}
EncounterKey encounterKey(Reader& r){EncounterKey k;k.group=uint8_t(r.u8());k.map=uint8_t(r.u8());k.kind=EncounterKind(r.u8());k.id=r.u32();k.condition=uint16_t(r.u16());k.expected=uint16_t(r.u16());if(k.kind!=EncounterKind::Story&&k.kind!=EncounterKind::Npc&&k.kind!=EncounterKind::Wild&&k.kind!=EncounterKind::Unique)throw std::runtime_error("Invalid encounter kind");return k;}
void wager(Bytes& b,const Wager& w){
    byte(b,w.active());if(!w.active())return;string(b,w.id);string(b,w.host);for(const auto& id:w.players)string(b,id);
    dword(b,w.stake);byte(b,unsigned(w.phase));byte(b,w.winner<0?255:unsigned(w.winner));
    for(int i=0;i<2;++i){byte(b,(w.reserved[i]?1:0)|(w.began[i]?2:0)|(w.paid[i]?4:0));byte(b,w.outcome[i]);}
}
Wager wager(Reader& r){
    Wager w;const auto active=r.u8();if(!active)return w;if(active>1)throw std::runtime_error("Invalid wager");
    w.id=r.str();w.host=r.str();for(auto& id:w.players)id=r.str();w.stake=r.u32();const auto phase=r.u8(),winner=r.u8();w.phase=WagerPhase(phase);w.winner=winner==255?-1:int(winner);
    for(int i=0;i<2;++i){const auto bits=r.u8(),result=r.u8();if(bits>7||result>3)throw std::runtime_error("Invalid wager state");w.reserved[i]=(bits&1)!=0;w.began[i]=(bits&2)!=0;w.paid[i]=(bits&4)!=0;w.outcome[i]=uint8_t(result);}
    if(!validIdentity(w.id,"Wager")||!validIdentity(w.host,"Host")||!validIdentity(w.players[0],"Trainer")||!validIdentity(w.players[1],"Trainer")||w.players[0]==w.players[1]||!w.stake||w.stake>WagerLimit||phase<1||phase>5||(winner!=255&&winner>1))throw std::runtime_error("Invalid wager terms");
    return w;
}
Bytes packet(Type type,const Bytes& payload){
    if(payload.size()>16384)throw std::runtime_error("Room packet too large");
    Bytes b{'F','R','M','P',RoomProtocolVersion,uint8_t(type)};word(b,unsigned(payload.size()));b.insert(b.end(),payload.begin(),payload.end());return b;
}
bool textSafe(const std::string& value,size_t max){
    if(value.empty()||value.size()>max)return false;
    for(unsigned char c:value)if(c<32||c==127)return false;
    return true;
}
void nonblock(SOCKET s){u_long v=1;if(ioctlsocket(s,FIONBIO,&v))throw std::runtime_error("Cannot configure room socket");BOOL yes=TRUE;setsockopt(s,IPPROTO_TCP,TCP_NODELAY,reinterpret_cast<const char*>(&yes),sizeof(yes));}
void closeSocket(SOCKET& s){if(s!=INVALID_SOCKET){closesocket(s);s=INVALID_SOCKET;}}
}
uint64_t chatClock(){return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now().time_since_epoch()).count());}
bool validChat(const std::string& text){
    if(!textSafe(text,ChatLimit)||text.find_first_not_of(' ')==std::string::npos)return false;
    try{for(wchar_t c:fr::widen(text))if((c>=0x80&&c<=0x9f)||(c>=0x200b&&c<=0x200f)||(c>=0x2028&&c<=0x202e)||(c>=0x2060&&c<=0x206f)||c==0xfeff)return false;}catch(...){return false;}
    return true;
}
bool validIdentity(const std::string& id,const std::string& name){
    try{(void)fr::widen(name);}catch(...){return false;}
    return id.size()==32&&std::all_of(id.begin(),id.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');})&&textSafe(name,96);
}
std::string randomId(){
    std::array<uint8_t,16> bytes{};
#ifdef _WIN32
    if(BCryptGenRandom(nullptr,bytes.data(),ULONG(bytes.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)throw std::runtime_error("Could not generate room identity");
#else
    fr::secureRandom(bytes.data(),bytes.size());
#endif
    constexpr char hex[]="0123456789abcdef";std::string result;
    for(auto b:bytes){result+=hex[b>>4];result+=hex[b&15];}return result;
}
struct Session::Impl {
    struct Connection {WSAEVENT event=WSA_INVALID_EVENT;SOCKET socket=INVALID_SOCKET;int slot=-1;Bytes input,output;size_t sent=0;bool closing=false;Clock::time_point seen=Clock::now();};
    struct Command {Type type;Bytes body;};
    struct Serial {int from;uint32_t sequence;uint16_t word;uint32_t elapsed=0;};
    mutable std::mutex mutex;std::condition_variable changed;
    std::thread worker;std::atomic<bool> quit{false},incomingSerial{false};
    bool joinRejected=false;
    Status current;std::string identity,name,key,ip;game::PlayerState local{};
    std::vector<Connection> connections;SOCKET listener=INVALID_SOCKET;
    WSAEVENT listenerEvent=WSA_INVALID_EVENT;HANDLE wake=nullptr;
    std::deque<Command> commands;std::deque<Serial> requests;
    std::optional<Serial> activeRequest;
    std::map<uint32_t,uint16_t> responses;uint32_t sequence=0;
    std::array<Invitation,MaxRoomPlayers> pending{};std::array<Clock::time_point,MaxRoomPlayers> pendingSince{};
    std::array<uint64_t,MaxRoomPlayers> lastChat{};
    uint64_t submittedChat=0;
    uint32_t chatSequence=0;
    WorldAuthority worldAuthority;
    CampaignBook campaign;
    WagerBook wagers;std::filesystem::path accountFolder;
    game::ReleaseBook releaseBook;
    std::map<std::string,game::ReleasedMon> releaseBroadcasts;
    std::string releaseBroadcastCursor;
    std::map<uint32_t,bool> releaseReplies;
    uint32_t releaseSequence=0;uint64_t releaseFlushed=0;
    void emitReleased(const game::ReleasedMon& m){releaseBroadcasts[m.id]=m;}
    void emitReleased(){for(const auto& m:releaseBook.records())if(m.phase!=game::ReleasePhase::Caught)emitReleased(m);}

    struct Wallet {uint32_t balance=0;bool available=false;};
    std::array<Wallet,MaxRoomPlayers> wallets{};uint32_t invitationSequence=0;
    #include "session_world.inc"
    void emitWagers(){
        current.wager=wagers.forPlayer(identity);
        for(auto& c:connections)if(c.slot>=0)if(const auto* p=peer(c.slot)){Bytes b;wager(b,wagers.forPlayer(p->id));queue(c,WagerSnapshot,b);}
    }
    void pairCable(int a,int b,Activity activity=Activity::Battle){if(auto* first=peer(a))if(auto* second=peer(b)){if(first->partner==b&&second->partner==a)return;first->partner=b;second->partner=a;first->activity=second->activity=activity;first->battleStage=second->battleStage=0;}emitSnapshot();}

    WorldReport localWorld;
    uint32_t worldSent=0,claimSequence=0;
    std::map<uint32_t,bool> claims;
    uint32_t waitingClaim=0;
    void broadcast(Type type,const Bytes& bytes){for(auto& c:connections)if(c.slot>=0)queue(c,type,bytes);}
    void emitShinyRate(){Bytes b;dword(b,current.shinyRate);broadcast(ShinyRate,b);}
    void emitBattles(){current.world=worldAuthority.snapshot();Bytes b;for(const auto& battle:current.world.battles)battleState(b,battle);broadcast(BattleSnapshot,b);}
    void emitCamps(){current.world=worldAuthority.snapshot();Bytes b;for(unsigned i=0;i<MaxRoomPlayers;++i){dword(b,current.world.campDecisions[i]);campState(b,current.world.camps[i]);}broadcast(CampSnapshot,b);}
    void emitLeases(){
        current.world=worldAuthority.snapshot();Bytes b;byte(b,unsigned(current.world.leases.size()));
        for(const auto& l:current.world.leases){encounterKey(b,l.key);byte(b,l.owner);dword(b,l.token);}
        byte(b,unsigned(current.world.retries.size()));for(const auto& retry:current.world.retries){encounterKey(b,retry.key);byte(b,unsigned(retry.actors.size()));for(auto actor:retry.actors)byte(b,actor);}broadcast(EncounterSnapshot,b);
    }
    void emitWorld(uint8_t group,uint8_t number){
        current.world=worldAuthority.snapshot();
        for(const auto& m:current.world.maps)if(m.group==group&&m.map==number){
            for(size_t offset=0;offset<m.npcs.size()||offset==0;offset+=15){
                Bytes b;byte(b,m.group);byte(b,m.map);byte(b,m.wildOwner);dword(b,m.revision);dword(b,m.sampleTime);
                const auto count=std::min(size_t(15),m.npcs.size()-offset);byte(b,unsigned(count));for(size_t j=0;j<count;++j)npc(b,m.npcs[offset+j]);
                byte(b,unsigned(m.wild.size()));for(const auto& w:m.wild)wildState(b,w);
                const auto consumed=std::min(size_t(32),m.consumed.size());byte(b,unsigned(consumed));for(size_t j=m.consumed.size()-consumed;j<m.consumed.size();++j)dword(b,m.consumed[j]);
                broadcast(WorldSnapshot,b);
            }return;
        }
    }
    void emitStory(const std::vector<game::StoryValue>& values){
        current.world=worldAuthority.snapshot();
        for(size_t offset=0;offset<values.size()||offset==0;offset+=128){Bytes b;byte(b,current.world.storyReady);const auto end=std::min(values.size(),offset+128);storyValues(b,{values.begin()+offset,values.begin()+end});broadcast(StorySnapshot,b);}
    }
    void emitCampaign(){
        current.campaign=campaign.snapshot();
        const auto& state=current.campaign;
        for(size_t begin=0;begin<state.completed.size()||begin==0;begin+=16){
            Bytes b;string(b,state.id);dword(b,state.sequence);const auto end=std::min(state.completed.size(),begin+16);byte(b,unsigned(end-begin));
            for(size_t i=begin;i<end;++i){const auto& c=state.completed[i];word(b,c.objective);dword(b,c.sequence);string(b,c.actor);}broadcast(CampaignSnapshot,b);
        }
    }
    void remember(ChatMessage m){
        m.receivedAt=chatClock();
        if(current.chat.size()>=ChatHistoryLimit)current.chat.erase(current.chat.begin());
        current.chat.push_back(std::move(m));
    }
    std::string lastError;
    WSADATA wsa{};
    Impl(std::string i,std::string n,const std::filesystem::path& folder):identity(std::move(i)),name(std::move(n)),accountFolder(folder){
        if(!validIdentity(identity,name))throw std::runtime_error("Invalid trainer identity");
        if(WSAStartup(MAKEWORD(2,2),&wsa))throw std::runtime_error("Windows networking could not start");
        wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        if(!wake){WSACleanup();throw std::runtime_error("Cannot create network wake event");}
    }
    ~Impl(){shutdown();CloseHandle(wake);WSACleanup();}
    static WSAEVENT watch(SOCKET socket,long flags){
        WSAEVENT event=WSACreateEvent();
        if(event==WSA_INVALID_EVENT)throw std::runtime_error("Cannot create socket event");
        if(WSAEventSelect(socket,event,flags)){WSACloseEvent(event);throw std::runtime_error("Cannot watch socket events");}
        return event;
    }
    static void closeConnection(Connection& c){closeSocket(c.socket);if(c.event!=WSA_INVALID_EVENT){WSACloseEvent(c.event);c.event=WSA_INVALID_EVENT;}}
    void closeListener(){closeSocket(listener);if(listenerEvent!=WSA_INVALID_EVENT){WSACloseEvent(listenerEvent);listenerEvent=WSA_INVALID_EVENT;}}
    static void acknowledge(SOCKET socket,WSAEVENT event){WSANETWORKEVENTS events{};WSAEnumNetworkEvents(socket,event,&events);}
    Peer* peer(int slot){auto i=std::find_if(current.peers.begin(),current.peers.end(),[&](const Peer& p){return p.slot==slot;});return i==current.peers.end()?nullptr:&*i;}
    void queue(Connection& c,Type type,const Bytes& body){
        if(c.output.size()-c.sent>524288)throw std::runtime_error("Slow peer exceeded room queue limit");
        if(c.sent){c.output.erase(c.output.begin(),c.output.begin()+c.sent);c.sent=0;}
        auto p=packet(type,body);c.output.insert(c.output.end(),p.begin(),p.end());SetEvent(wake);
    }
    void rejectJoin(Connection& c,const std::string& message){
        Bytes b;string(b,message);queue(c,JoinRejected,b);c.closing=true;
    }
    void sendTo(int slot,Type type,const Bytes& body){
        if(slot==current.slot){receive(type,body,-1);return;}
        if(!current.hosting){if(!connections.empty())queue(connections[0],type,body);return;}
        for(auto& c:connections)if(c.slot==slot){queue(c,type,body);return;}
    }
    void emitMotion(int slot,const game::PlayerState& p){
        Bytes b;byte(b,unsigned(slot));player(b,p);
        for(auto& c:connections)if(c.slot>=0&&c.slot!=slot)queue(c,Motion,b);
    }
    void emitSnapshot(){
        Bytes b;byte(b,unsigned(current.peers.size()));
        for(const auto& p:current.peers){byte(b,p.slot);string(b,p.id);string(b,p.name);dword(b,p.chatAfter);player(b,p.player);byte(b,p.partner<0?255:unsigned(p.partner));byte(b,(p.cableReady?1:0)|(p.cableClock?2:0));byte(b,unsigned(p.activity));byte(b,p.battleStage);}
        for(auto& c:connections)if(c.slot>=0)queue(c,Snapshot,b);
    }
    void membership(const Peer& p,uint8_t kind){
        ChatMessage m{++chatSequence,p.slot,p.id,p.name,p.name+(kind==1?" joined server":" left server"),kind};
        Bytes b;dword(b,m.sequence);byte(b,kind);byte(b,p.slot);string(b,p.id);string(b,p.name);
        remember(m);for(auto& c:connections)if(c.slot>=0)queue(c,Membership,b);
    }
    void notice(int slot,const std::string& message){Bytes b;string(b,message);sendTo(slot,Notice,b);}
    void cancelPending(int slot,const std::string& reason){
        for(unsigned target=0;target<MaxRoomPlayers;++target)if(pending[target].from>=0&&(int(target)==slot||pending[target].from==slot)){
            const auto invitation=pending[target];pending[target]={};Bytes out;dword(out,invitation.nonce);sendTo(int(target),InvitationCancel,out);notice(int(target),reason);notice(invitation.from,reason);
        }
    }
    void clearCable(int slot){
        if(current.hosting)cancelPending(slot,"Invitation cancelled.");
        bool localAffected=slot==current.slot;
        if(auto* p=peer(slot)){const int other=p->partner;localAffected=localAffected||other==current.slot;p->partner=-1;p->battleStage=0;p->cableReady=false;p->cableClock=false;if(auto* q=peer(other)){q->partner=-1;q->battleStage=0;q->cableReady=false;q->cableClock=false;}}
        if(localAffected){requests.clear();responses.clear();activeRequest.reset();incomingSerial=false;}
        changed.notify_all();
    }
    void route(Type type,const Bytes& body,int from){
        Reader r{body};
        if(type==ShinyRate)throw std::runtime_error("Only the host can change the shiny rate.");
        if(type==PlayerCheckpoint){
            if(from==0&&worldPlayers){const auto serial=r.u32();Bytes save(r.b.begin()+r.p,r.b.end());worldPlayers->commit(identity,save);campaign.save();wagers.save();releaseBook.save();current.checkpointAck=serial;return;}
            readCheckpoint(r,from);return;
        }
        if(type==DepartureEvent){auto pose=player(r);r.end();if(auto* p=peer(from)){
            const auto& b=current.world.battles[from];const auto& anchor=b.id?b.trainer:p->player;
            if(pose.mapGroup!=anchor.mapGroup||pose.mapNumber!=anchor.mapNumber||std::abs(int(pose.pixelX)-anchor.pixelX)>32||std::abs(int(pose.pixelY)-anchor.pixelY)>32)return;
            if(!current.departures.empty()&&current.departures.back().slot==from&&chatClock()-current.departures.back().receivedAt<2000)return;
            Departure d{++sequence,uint8_t(from),pose,chatClock()};if(current.departures.size()>=64)current.departures.erase(current.departures.begin());current.departures.push_back(d);
            Bytes out;dword(out,d.sequence);byte(out,from);player(out,pose);broadcast(DepartureEvent,out);
        }return;}
        if(type==ReleasedOffer){const auto token=r.u32();auto m=released(r);r.end();const auto* p=peer(from);const bool accepted=p&&p->id==m.owner&&releaseBook.offer(m);if(accepted){current.released=releaseBook.records();emitReleased(*releaseBook.find(m.id));}Bytes b;dword(b,token);byte(b,accepted);sendTo(from,ReleasedResult,b);return;}
        if(type==ReleasedClaim){const auto token=r.u32();const auto id=r.str();r.end();const auto* p=peer(from);bool accepted=false;
            if(p&&p->player.active&&p->player.follower){game::ReleasePoint point{p->player.mapGroup,p->player.mapNumber,p->player.elevation,p->player.x,p->player.y};accepted=releaseBook.claim(id,p->id,point);}
            if(accepted){current.released=releaseBook.records();emitReleased(*releaseBook.find(id));}Bytes b;dword(b,token);byte(b,accepted);sendTo(from,ReleasedResult,b);return;}
        if(type==ReleasedFinish){const auto id=r.str();const auto caught=r.u8();r.end();if(caught>1)throw std::runtime_error("Invalid release battle result");if(const auto* p=peer(from);p&&releaseBook.finish(id,p->id,caught!=0)){current.released=releaseBook.records();emitReleased(*releaseBook.find(id));}return;}
        if(type==WalletUpdate){const auto balance=r.u32(),available=r.u8();r.end();if(balance>MoneyLimit||available>1)throw std::runtime_error("Invalid wallet report");wallets.at(from)={balance,available!=0};return;}
        if(type==WagerEvent){
            const auto id=r.str();const auto event=r.u8(),value=r.u8();r.end();const auto* author=peer(from);if(!author)return;
            if(wagers.event(author->id,id,uint8_t(event),uint8_t(value))){
                const auto* w=wagers.find(id);
                if(w&&w->phase==WagerPhase::Ready){
                    int slots[2]{-1,-1};for(const auto& p:current.peers)for(int side=0;side<2;++side)if(p.id==w->players[side])slots[side]=p.slot;
                    if(slots[0]>=0&&slots[1]>=0)pairCable(slots[0],slots[1]);else wagers.cancel(author->id);
                }
                emitWagers();
            }return;
        }
        if(type==WorldUpdate){auto w=worldReport(r);r.end();if(w.camp.id&&w.camp.id!=worldAuthority.snapshot().campDecisions[from])for(const auto& p:current.peers)if(p.player.active&&p.player.mapGroup==w.group&&p.player.mapNumber==w.map&&(game::campContains(w.camp,p.player.x,p.player.y)||game::campContains(w.camp,p.player.pixelX/16,p.player.pixelY/16)))worldAuthority.rejectCamp(unsigned(from),w.camp.id);
            const auto leases=worldAuthority.snapshot().leases.size();worldAuthority.report(unsigned(from),w,chatClock());if(leases!=worldAuthority.snapshot().leases.size())emitLeases();emitWorld(w.group,w.map);emitCamps();emitBattles();return;}
        if(type==StoryUpdate){const auto bits=r.u8();auto values=storyValues(r,current.rewardPolicy);r.end();if(bits>3)throw std::runtime_error("Invalid story mode");worldAuthority.story(unsigned(from),values,(bits&1)!=0,(bits&2)!=0);if(worldAuthority.snapshot().storyReady){const auto* who=peer(from);campaign.commit(worldAuthority.snapshot().story,(bits&1)?"Earlier adventure":who?who->name:"Trainer");emitCampaign();}std::vector<game::StoryValue> accepted;for(const auto& v:worldAuthority.snapshot().story)if(std::any_of(values.begin(),values.end(),[&](auto in){return in.id==v.id;}))accepted.push_back(v);if(bits&1)emitStory(accepted);else emitStory(worldAuthority.snapshot().story);emitLeases();return;}
        if(type==EncounterClaim){const auto token=r.u32();const auto key=encounterKey(r);r.end();const bool accepted=worldAuthority.claim(unsigned(from),key,token,chatClock());emitLeases();emitWorld(key.group,key.map);Bytes b;dword(b,token);byte(b,accepted);sendTo(from,EncounterResult,b);return;}
        if(type==EncounterRelease){const auto token=r.u32();const auto retry=r.u8();r.end();if(retry>1)throw std::runtime_error("Invalid encounter release");worldAuthority.release(unsigned(from),token,retry!=0);emitLeases();return;}
        if(type==EncounterCheckpoint){const auto token=r.u32();const auto count=r.u8();if(count>15)throw std::runtime_error("Invalid encounter checkpoint count");std::vector<NpcState> actors;for(unsigned i=0;i<count;++i)actors.push_back(npc(r));r.end();worldAuthority.checkpoint(unsigned(from),token,actors);if(!actors.empty())emitWorld(actors.front().pose.mapGroup,actors.front().pose.mapNumber);return;}
        if(type==ChatSend){
            auto text=r.str();r.end();
            if(!validChat(text))throw std::runtime_error("Invalid chat message");
            const auto* author=peer(from);if(!author)return;
            const auto tick=chatClock();
            if(lastChat[from]&&tick-lastChat[from]<500){notice(from,"Please wait a moment before sending again.");return;}
            lastChat[from]=tick;
            ChatMessage m{++chatSequence,uint8_t(from),author->id,author->name,std::move(text)};
            Bytes out;dword(out,m.sequence);byte(out,m.slot);string(out,m.id);string(out,m.name);string(out,m.text);
            remember(m);
            for(auto& c:connections)if(c.slot>=0)queue(c,ChatBroadcast,out);
            return;
        }
        if(type==State){auto p=player(r);r.end();if(auto* who=peer(from))who->player=p;emitMotion(from,p);return;}
        if(type==Ready){const unsigned ready=r.u8();r.end();if(ready>3)throw std::runtime_error("Invalid cable ready flag");if(auto* p=peer(from)){p->cableReady=(ready&1)!=0;p->cableClock=(ready&2)!=0;}emitSnapshot();changed.notify_all();return;}
        if(type==BattleStage){const unsigned stage=r.u8();r.end();if(stage<1||stage>3)throw std::runtime_error("Invalid field battle stage");if(auto* p=peer(from);p&&p->partner>=0&&p->activity==Activity::Battle&&stage>=p->battleStage&&stage<=p->battleStage+1){p->battleStage=uint8_t(stage);emitSnapshot();}return;}
        if(type==InvitationCancel){r.end();cancelPending(from,"Invitation cancelled.");return;}
        if(type==CableOff){r.end();cancelPending(from,"Invitation cancelled.");if(auto* p=peer(from))wagers.cancel(p->id);emitWagers();clearCable(from);emitSnapshot();return;}
        if(type==Invite){
            const int to=int(r.u8());const auto activity=Activity(r.u8());const auto stake=r.u32();r.end();
            auto* a=peer(from);auto* b=peer(to);
            if(!a||!b||from==to||a->partner>=0||b->partner>=0||(activity!=Activity::Battle&&activity!=Activity::Trade)||stake>WagerLimit||(activity==Activity::Trade&&stake))return;
            if(wagers.busy(a->id)||wagers.busy(b->id)){notice(from,"Finish the current wager first.");return;}
            for(unsigned slot=0;slot<MaxRoomPlayers;++slot)if(pending[slot].from>=0&&(int(slot)==from||int(slot)==to||pending[slot].from==from||pending[slot].from==to)){notice(from,"A battle or trade invitation is already pending.");return;}
            if(stake&&(!wallets[from].available||wallets[from].balance<stake)){notice(from,"Return to the overworld with enough money for this wager.");return;}
            pending[size_t(to)]={from,activity,stake,++invitationSequence};pendingSince[size_t(to)]=Clock::now();
            Bytes out;byte(out,unsigned(from));byte(out,unsigned(activity));dword(out,stake);dword(out,invitationSequence);sendTo(to,Invite,out);return;
        }
        if(type==Reply){
            const int to=int(r.u8());const unsigned accept=r.u8();const auto nonce=r.u32();r.end();
            if(from<0||from>=MaxRoomPlayers||to<0||to>=MaxRoomPlayers||accept>1||pending[size_t(from)].from!=to||pending[size_t(from)].nonce!=nonce)return;
            auto* a=peer(from);auto* b=peer(to);const auto invitation=pending[size_t(from)];pending[size_t(from)]={};
            if(a&&b&&accept&&a->partner<0&&b->partner<0&&!wagers.busy(a->id)&&!wagers.busy(b->id)){
                if(invitation.stake){
                    if(!wallets[from].available||!wallets[to].available||wallets[from].balance<invitation.stake||wallets[to].balance<invitation.stake){notice(from,"Wager cancelled: both trainers need enough available money.");notice(to,"Wager cancelled: both trainers need enough available money.");return;}
                    Wager w;w.id=randomId();w.host=identity;w.players={b->id,a->id};w.stake=invitation.stake;wagers.create(w);emitWagers();
                    notice(from,"Wager agreed. Saving each trainer's deposit.");notice(to,"Wager agreed. Saving each trainer's deposit.");
                }else{pairCable(from,to,invitation.activity);notice(from,invitation.activity==Activity::Battle?"Battle accepted. Getting ready...":"Trade connected. Visit the upstairs Cable Club together.");notice(to,invitation.activity==Activity::Battle?"Battle accepted. Getting ready...":"Trade connected. Visit the upstairs Cable Club together.");}
            }else notice(to,"Invitation declined or the trainer is busy.");
            emitSnapshot();return;
        }
        if(type==SerialStart||type==SerialReply){
            const int to=int(r.u8());const uint32_t seq=r.u32();const uint16_t value=uint16_t(r.u16());const uint32_t elapsed=type==SerialStart?r.u32():0;r.end();
            if(elapsed>33554432u)throw std::runtime_error("Invalid serial interval");
            auto* a=peer(from);auto* b=peer(to);
            if(!a||!b||a->partner!=to||b->partner!=from)return;
            if((type==SerialStart&&from>to)||(type==SerialReply&&from<to))return;
            Bytes out;byte(out,unsigned(from));dword(out,seq);word(out,value);if(type==SerialStart)dword(out,elapsed);sendTo(to,type,out);return;
        }
        throw std::runtime_error("Unexpected client room packet");
    }
    void receive(Type type,const Bytes& body,int from){
        if(current.hosting&&from>=0){route(type,body,from);return;}
        Reader r{body};
        if(type==ShinyRate){const auto rate=r.u32();r.end();if(rate<1||rate>8192)throw std::runtime_error("Invalid shiny rate.");current.shinyRate=rate;return;}

        if(type==PlayerCheckpoint){readCheckpoint(r,-1);return;}
        if(type==CheckpointAck){const auto serial=r.u32();r.end();if(!current.managedWorld||serial>saveSequence)throw std::runtime_error("Invalid checkpoint receipt.");current.checkpointAck=std::max(current.checkpointAck,serial);return;}
        if(type==CheckpointRequest){const auto serial=r.u32();r.end();if(current.managedWorld)current.checkpointRequest=std::max(current.checkpointRequest,serial);return;}
        if(type==DepartureEvent){Departure d;d.sequence=r.u32();d.slot=uint8_t(r.u8());d.origin=player(r);d.receivedAt=chatClock();r.end();if(!peer(d.slot))return;if(current.departures.size()>=64)current.departures.erase(current.departures.begin());current.departures.push_back(d);return;}
        if(type==ReleasedResult){const auto token=r.u32();const auto accepted=r.u8();r.end();if(accepted>1)throw std::runtime_error("Invalid release response");if(releaseReplies.size()>16)releaseReplies.clear();releaseReplies[token]=accepted!=0;changed.notify_all();return;}
        if(type==ReleasedSnapshot){auto m=released(r);r.end();auto i=std::find_if(current.released.begin(),current.released.end(),[&](const auto& old){return old.id==m.id;});if(i==current.released.end()){if(current.released.size()>=4096)throw std::runtime_error("Released population limit");current.released.push_back(m);}else if(m.revision>=i->revision)*i=m;return;}
        if(type==BattleSnapshot){auto battles=current.world.battles;for(auto& battle:battles)battle=battleState(r);r.end();current.world.battles=std::move(battles);return;}
        if(type==CampSnapshot){auto next=current.world.camps;auto decisions=current.world.campDecisions;for(unsigned i=0;i<MaxRoomPlayers;++i){decisions[i]=r.u32();next[i]=campState(r);}r.end();current.world.camps=std::move(next);current.world.campDecisions=decisions;return;}
        if(type==WagerSnapshot){auto w=wager(r);r.end();const auto* host=peer(0);if(w.active()&&(!host||w.host!=host->id||w.side(identity)<0))throw std::runtime_error("Wager is not for this trainer");current.wager=std::move(w);return;}
        if(type==EncounterResult){const auto token=r.u32();auto accepted=r.u8();r.end();if(accepted>1)throw std::runtime_error("Invalid encounter result");if(token==waitingClaim)claims[token]=accepted!=0;changed.notify_all();return;}
        if(type==EncounterSnapshot){auto count=r.u8();if(count>MaxRoomPlayers)throw std::runtime_error("Invalid encounter count");std::vector<EncounterLease> leases;while(count--){auto k=encounterKey(r);auto owner=r.u8();auto token=r.u32();if(owner>=MaxRoomPlayers||!token)throw std::runtime_error("Invalid encounter lease");leases.push_back({k,uint8_t(owner),token});}
            const auto retryCount=r.u8();if(retryCount>128)throw std::runtime_error("Invalid retry count");std::vector<EncounterRetry> retries;
            for(unsigned i=0;i<retryCount;++i){EncounterRetry retry;retry.key=encounterKey(r);const auto actors=r.u8();if((retry.key.kind!=EncounterKind::Story&&retry.key.kind!=EncounterKind::Npc)||!actors||actors>15)throw std::runtime_error("Invalid retry encounter");
                for(unsigned j=0;j<actors;++j){const auto actor=r.u8();if(!actor||actor>=255||std::find(retry.actors.begin(),retry.actors.end(),actor)!=retry.actors.end())throw std::runtime_error("Invalid retry actor");retry.actors.push_back(uint8_t(actor));}retries.push_back(std::move(retry));}
            r.end();current.world.leases=std::move(leases);current.world.retries=std::move(retries);return;}
        if(type==CampaignSnapshot){
            const auto id=r.str();const auto sequence=r.u32();const auto count=r.u8();
            if(!validIdentity(id,"Campaign")||count>16)throw std::runtime_error("Invalid campaign snapshot");
            auto next=current.campaign;if(next.id!=id)next={id,0,{}};
            for(unsigned i=0;i<count;++i){game::CampaignCompletion c{uint16_t(r.u16()),r.u32(),r.str()};
                if(!game::campaignObjective(c.objective)||!c.sequence||c.sequence>sequence||c.actor.empty()||c.actor.size()>32)throw std::runtime_error("Invalid campaign objective");
                auto existing=std::find_if(next.completed.begin(),next.completed.end(),[&](const auto& old){return old.objective==c.objective;});
                if(existing==next.completed.end())next.completed.push_back(c);else if(existing->sequence!=c.sequence||existing->actor!=c.actor)throw std::runtime_error("Campaign completion changed");
            }
            r.end();if(next.completed.size()>64)throw std::runtime_error("Campaign journal limit exceeded");
            next.sequence=std::max(next.sequence,sequence);std::sort(next.completed.begin(),next.completed.end(),[](const auto& a,const auto& b){return a.sequence<b.sequence;});current.campaign=std::move(next);return;
        }
        if(type==StorySnapshot){const auto ready=r.u8();auto values=storyValues(r,current.rewardPolicy);r.end();if(ready>1)throw std::runtime_error("Invalid story readiness");for(auto v:values){auto i=std::find_if(current.world.story.begin(),current.world.story.end(),[&](auto a){return a.id==v.id;});if(i==current.world.story.end())current.world.story.push_back(v);else *i=v;}current.world.storyReady=ready&&current.world.story.size()==game::storyKeys(current.rewardPolicy).size();return;}
        if(type==WorldSnapshot){
            MapState m;m.group=uint8_t(r.u8());m.map=uint8_t(r.u8());m.wildOwner=uint8_t(r.u8());m.revision=r.u32();m.sampleTime=r.u32();auto count=r.u8();if(count>15||(m.wildOwner>=MaxRoomPlayers&&m.wildOwner!=255))throw std::runtime_error("Invalid world snapshot");while(count--)m.npcs.push_back(npc(r));count=r.u8();if(count>12)throw std::runtime_error("Invalid wild count");while(count--)m.wild.push_back(wildState(r));count=r.u8();if(count>32)throw std::runtime_error("Invalid consumed count");while(count--)m.consumed.push_back(r.u32());r.end();WorldReport validate;validate.group=m.group;validate.map=m.map;validate.npcs=m.npcs;validate.wild=m.wild;if(!validWorldReport(validate))throw std::runtime_error("Invalid world snapshot semantics");
            auto i=std::find_if(current.world.maps.begin(),current.world.maps.end(),[&](const auto& a){return a.group==m.group&&a.map==m.map;});
            if(i==current.world.maps.end()){if(current.world.maps.size()>=512)throw std::runtime_error("World cache limit");current.world.maps.push_back(m);}else{
                i->revision=m.revision;i->sampleTime=m.sampleTime;i->wildOwner=m.wildOwner;i->wild=m.wild;i->consumed=m.consumed;
                for(const auto& n:m.npcs){auto a=std::find_if(i->npcs.begin(),i->npcs.end(),[&](const auto& b){return b.localId==n.localId;});if(a==i->npcs.end()){if(i->npcs.size()>=64)throw std::runtime_error("NPC cache limit");i->npcs.push_back(n);}else *a=n;}
            }return;
        }
        if(type==Membership){
            ChatMessage m;m.sequence=r.u32();m.kind=uint8_t(r.u8());m.slot=uint8_t(r.u8());m.id=r.str();m.name=r.str();r.end();
            if((m.kind!=1&&m.kind!=2)||m.slot>=MaxRoomPlayers||!validIdentity(m.id,m.name)||(!current.chat.empty()&&m.sequence<=current.chat.back().sequence))throw std::runtime_error("Invalid membership notice");
            m.text=m.name+(m.kind==1?" joined server":" left server");remember(std::move(m));return;
        }
        if(type==ChatBroadcast){
            ChatMessage m;m.sequence=r.u32();m.slot=uint8_t(r.u8());m.id=r.str();m.name=r.str();m.text=r.str();r.end();
            const auto* author=peer(m.slot);
            if(!author||author->id!=m.id||author->name!=m.name||!validChat(m.text))throw std::runtime_error("Invalid chat author or text");
            if(!current.chat.empty()&&m.sequence<=current.chat.back().sequence)throw std::runtime_error("Out-of-order chat message");
            remember(std::move(m));return;
        }
        if(type==JoinRejected){
            if(current.connected)throw std::runtime_error("Unexpected join rejection.");
            current.message=r.str();r.end();joinRejected=true;quit=true;changed.notify_all();return;
        }
        if(type==Welcome){current.slot=int(r.u8());current.rewardPolicy=uint8_t(r.u8());current.capacity=uint8_t(r.u8());const auto managed=r.u8();current.managedWorld=managed==1;current.worldId=r.str();current.worldName=r.str();r.end();if(managed>1||(!expectedRom.empty()&&!current.managedWorld)||(current.managedWorld&&!worldIdValid(current.worldId)))throw std::runtime_error("Invalid world session.");if(current.capacity<2||current.capacity>MaxRoomPlayers)throw std::runtime_error("Invalid player limit");if(current.rewardPolicy>game::AllRewardSharing)throw std::runtime_error("Invalid reward policy");if(current.slot<1||current.slot>=MaxRoomPlayers)throw std::runtime_error("Invalid room slot");current.connected=true;current.message="Connected to room.";return;}
        if(type==Motion){const int slot=int(r.u8());auto p=player(r);r.end();if(slot<0||slot>=MaxRoomPlayers)throw std::runtime_error("Invalid motion slot");if(auto* who=peer(slot))who->player=p;return;}
        if(type==Snapshot){
            const unsigned count=r.u8();if(!count||count>MaxRoomPlayers)throw std::runtime_error("Invalid room size");
            std::vector<Peer> peers;unsigned slots=0;
            for(unsigned i=0;i<count;++i){
                Peer p;p.slot=uint8_t(r.u8());p.id=r.str();p.name=r.str();p.chatAfter=r.u32();p.player=player(r);const unsigned partner=r.u8(),ready=r.u8();p.partner=partner==255?-1:int(partner);p.cableReady=(ready&1)!=0;p.cableClock=(ready&2)!=0;p.activity=Activity(r.u8());p.battleStage=uint8_t(r.u8());
                if(p.slot>=MaxRoomPlayers||(slots&(1u<<p.slot))||!validIdentity(p.id,p.name)|| (partner!=255&&partner>=MaxRoomPlayers)||ready>3||(p.activity!=Activity::Battle&&p.activity!=Activity::Trade)||p.battleStage>3)throw std::runtime_error("Invalid trainer in room");
                slots|=1u<<p.slot;peers.push_back(p);
            }r.end();
            const auto* oldSelf=peer(current.slot);const int oldPartner=oldSelf?oldSelf->partner:-1;
            current.peers=std::move(peers);
            if(current.invitation.from>=0&&!peer(current.invitation.from))current.invitation={};
            const auto* newSelf=peer(current.slot);
            if(oldPartner!=(newSelf?newSelf->partner:-1)){requests.clear();responses.clear();activeRequest.reset();incomingSerial=false;}
            changed.notify_all();return;
        }
        if(type==Invite){current.invitation={int(r.u8()),Activity(r.u8()),r.u32(),r.u32()};r.end();if(current.invitation.stake>WagerLimit||(current.invitation.activity!=Activity::Battle&&current.invitation.activity!=Activity::Trade)||(current.invitation.activity==Activity::Trade&&current.invitation.stake))throw std::runtime_error("Invalid invitation");return;}
        if(type==InvitationCancel){const auto nonce=r.u32();r.end();if(current.invitation.nonce==nonce)current.invitation={};return;}
        if(type==Notice){current.message=r.str();++current.messageSequence;r.end();return;}
        if(type==SerialStart||type==SerialReply){
            const int sender=int(r.u8());const auto seq=r.u32();const auto value=uint16_t(r.u16());const auto elapsed=type==SerialStart?r.u32():0;r.end();
            if(elapsed>33554432u)throw std::runtime_error("Invalid serial interval");
            auto* self=peer(current.slot);if(!self||self->partner!=sender)return;
            if(type==SerialStart){if(requests.size()>=8)throw std::runtime_error("Serial queue overflow");requests.push_back({sender,seq,value,elapsed});incomingSerial=true;changed.notify_all();}
            else{if(responses.size()>=16)responses.clear();responses[seq]=value;changed.notify_all();}
            return;
        }
        throw std::runtime_error("Unexpected server room packet");
    }
    void disconnect(size_t index){
        const int slot=connections[index].slot;closeConnection(connections[index]);
        connections.erase(connections.begin()+index);
        if(current.hosting){if(slot>=0){if(auto* p=peer(slot)){membership(*p,2);wagers.cancel(p->id);}emitWagers();wallets[slot]={};pending[slot]={};worldAuthority.leave(unsigned(slot));emitLeases();emitCamps();emitBattles();clearCable(slot);std::erase_if(current.peers,[&](const Peer& p){return p.slot==slot;});for(auto& p:pending)if(p.from==slot)p.from=-1;emitSnapshot();}}
        else{current.connected=false;if(!joinRejected)current.message="Disconnected from host.";current.peers.clear();changed.notify_all();quit=true;}
    }
    void consume(Connection& c,Type type,const Bytes& payload){
        if(current.hosting&&c.slot<0){
            if(type!=Hello)throw std::runtime_error("Join handshake required");
            Reader r{payload};const auto candidateKey=r.str(),id=r.str(),trainerName=r.str(),romHash=r.str(),secret=r.str();r.end();
            if(candidateKey!=key||!validIdentity(id,trainerName)){rejectJoin(c,"Room key or trainer identity is incorrect.");return;}
            if(std::any_of(current.peers.begin(),current.peers.end(),[&](const Peer& p){return p.id==id;})){rejectJoin(c,"This trainer is already in the room.");return;}
            int slot=1;while(slot<current.capacity&&peer(slot))++slot;if(slot>=current.capacity){rejectJoin(c,"This room is full.");return;}
            if(worldPlayers&&romHash!=expectedRom){rejectJoin(c,"ROM mismatch. Choose the same game and ROM revision as the host (FireRed or LeafGreen).");return;}
            if(worldPlayers&&!worldPlayers->authenticate(id,secret)){rejectJoin(c,"This trainer's world identity does not match. Use the original player profile.");return;}
            if(!worldPlayers&&!romHash.empty()){rejectJoin(c,"The host must select a saved world in the launcher.");return;}
            transfers[slot]={};c.slot=slot;lastChat[slot]=0;Peer p;p.slot=uint8_t(slot);p.id=id;p.name=trainerName;p.chatAfter=chatSequence;current.peers.push_back(p);
            Bytes b;byte(b,unsigned(slot));byte(b,current.rewardPolicy);byte(b,current.capacity);byte(b,current.managedWorld);string(b,current.worldId);string(b,current.worldName);queue(c,Welcome,b);emitShinyRate();emitSnapshot();emitStory(worldAuthority.snapshot().story);emitLeases();emitCamps();emitBattles();emitWagers();emitReleased();emitCampaign();membership(p,1);if(worldPlayers)sendCheckpoint(slot,1,worldPlayers->load(id));return;
        }
        receive(type,payload,c.slot);
    }
    void run(){
        try{
            Clock::time_point next=Clock::now();
            while(!quit){
                std::vector<HANDLE> events{wake};
                DWORD delay=16;
                {
                    std::lock_guard lock(mutex);
                    if(current.hosting)for(unsigned slot=0;slot<MaxRoomPlayers;++slot)if(pending[slot].from>=0&&Clock::now()-pendingSince[slot]>std::chrono::seconds(60))cancelPending(int(slot),"Invitation expired. Try again when both trainers are ready.");
                    if(listener!=INVALID_SOCKET){
                        acknowledge(listener,listenerEvent);
                        SOCKET accepted=accept(listener,nullptr,nullptr);
                        if(accepted!=INVALID_SOCKET){
                            if(connections.size()>=current.capacity+3)closesocket(accepted);
                            else{Connection c;c.socket=accepted;try{nonblock(accepted);c.event=watch(accepted,FD_READ|FD_WRITE|FD_CLOSE);connections.push_back(std::move(c));}catch(...){closeConnection(c);throw;}}
                        }
                    }
                    for(size_t i=0;i<connections.size();){
                        bool bad=false;auto& c=connections[i];
                        acknowledge(c.socket,c.event);
                        char buf[4096];int n=recv(c.socket,buf,sizeof(buf),0);
                        if(n>0){c.input.insert(c.input.end(),buf,buf+n);c.seen=Clock::now();}
                        else if(n==0||(n==SOCKET_ERROR&&WSAGetLastError()!=WSAEWOULDBLOCK))bad=true;
                        if(c.input.size()>65536||Clock::now()-c.seen>std::chrono::seconds(c.slot<0&&current.hosting?5:15))bad=true;
                        try{
                            unsigned handled=0;
                            while(!bad&&!c.closing&&c.input.size()>=8&&handled++<64){
                                if(c.input[0]!='F'||c.input[1]!='R'||c.input[2]!='M'||c.input[3]!='P'||c.input[4]!=RoomProtocolVersion)throw std::runtime_error("Incompatible room version. Update Pokemulti on both computers.");
                                const size_t length=size_t(c.input[6])|(size_t(c.input[7])<<8);if(length>16384)throw std::runtime_error("Oversized room packet");
                                if(c.input.size()<8+length)break;
                                const auto type=Type(c.input[5]);Bytes payload(c.input.begin()+8,c.input.begin()+8+length);c.input.erase(c.input.begin(),c.input.begin()+8+length);
                                consume(c,type,payload);
                            }
                            if(!bad&&c.sent<c.output.size()){
                                const int sent=send(c.socket,reinterpret_cast<const char*>(c.output.data()+c.sent),int(c.output.size()-c.sent),0);
                                if(sent>0)c.sent+=size_t(sent);else if(sent==SOCKET_ERROR&&WSAGetLastError()!=WSAEWOULDBLOCK)bad=true;
                                if(c.sent==c.output.size()){c.output.clear();c.sent=0;if(c.closing)bad=true;}
                            }
                        }catch(const std::exception& e){current.message=e.what();bad=true;}
                        if(bad)disconnect(i);else ++i;
                    }
                    while(!commands.empty()){
                        auto command=std::move(commands.front());commands.pop_front();
                        if(current.hosting)route(command.type,command.body,0);
                        else if(current.connected&&!connections.empty())queue(connections[0],command.type,command.body);
                    }
                    if(current.hosting){
                        unsigned count=0;while(!releaseBroadcasts.empty()&&count++<4){auto i=releaseBroadcasts.upper_bound(releaseBroadcastCursor);if(i==releaseBroadcasts.end())i=releaseBroadcasts.begin();releaseBroadcastCursor=i->first;Bytes b;released(b,i->second);broadcast(ReleasedSnapshot,b);releaseBroadcasts.erase(i);}
                    }
                    if(Clock::now()>=next){
                        next=Clock::now()+std::chrono::milliseconds(16);
                        if(localWorld.sequence!=worldSent&&current.connected){Bytes body;worldReport(body,localWorld);worldSent=localWorld.sequence;if(current.hosting)route(WorldUpdate,body,0);else if(!connections.empty())queue(connections[0],WorldUpdate,body);}
                        if(current.hosting){if(auto* p=peer(0))p->player=local;emitMotion(0,local);}
                        else if(current.connected&&!connections.empty()){Bytes body;player(body,local);queue(connections[0],State,body);}
                    }
                    if(listenerEvent!=WSA_INVALID_EVENT)events.push_back(listenerEvent);
                    for(const auto& c:connections)events.push_back(c.event);
                    delay=DWORD(std::clamp<int64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(next-Clock::now()).count(),1,16));
                }
                // Wake immediately for a new serial word or socket readiness. Sleep's
                // timer granularity otherwise adds latency to every link transfer.
                if(WaitForMultipleObjects(DWORD(events.size()),events.data(),FALSE,delay)==WAIT_FAILED)
                    throw std::runtime_error("Network event wait failed");
            }
        }catch(const std::exception& e){std::lock_guard lock(mutex);current.message=e.what();}
        std::lock_guard lock(mutex);
        if(current.hosting)try{for(const auto& p:current.peers)wagers.cancel(p.id);}catch(const std::exception& e){current.message=e.what();}
        for(auto& c:connections)closeConnection(c);connections.clear();closeListener();
        current.running=false;current.connected=false;current.hosting=false;current.slot=-1;current.peers.clear();current.chat.clear();current.world={};current.wager={};current.invitation={};requests.clear();responses.clear();activeRequest.reset();incomingSerial=false;changed.notify_all();
    }
    void shutdown(){quit=true;SetEvent(wake);changed.notify_all();if(worker.joinable())worker.join();}
    void start(bool host,const std::string& address,uint16_t port,const std::string& roomKey,uint8_t rewards=game::DefaultRewardSharing,bool freshCampaign=false,uint8_t capacity=4){
        if(capacity<2||capacity>MaxRoomPlayers)throw std::runtime_error("Choose a player limit from 2 to 32.");
        if(rewards>game::AllRewardSharing)throw std::runtime_error("Invalid reward policy");
        shutdown();std::lock_guard lock(mutex);
        if(!textSafe(roomKey,64)||roomKey.size()<8)throw std::runtime_error("Use a room key of 8 to 64 characters.");
        wallets={};invitationSequence=0;wagers.manualSaving(bool(worldPlayers));releaseBook.manualSaving(bool(worldPlayers));campaign.manualSaving(bool(worldPlayers));if(host)wagers.open(accountFolder.empty()?std::filesystem::path{}:accountFolder/"wagers-host.cfg");
        releaseBroadcasts.clear();releaseReplies.clear();releaseFlushed=0;if(host)releaseBook.open(accountFolder.empty()?std::filesystem::path{}:accountFolder/"released-world.cfg");
        current={};joinRejected=false;
        if(host){
            const auto settings=accountFolder/"shiny-rate.cfg";
            if(!accountFolder.empty()&&std::filesystem::exists(settings)){
                const auto bytes=readWorldFile(settings,32);std::istringstream input(std::string(bytes.begin(),bytes.end()));uint32_t rate=0;std::string extra;
                if(!(input>>rate)||rate<1||rate>8192||(input>>extra))throw std::runtime_error("Invalid world shiny rate.");current.shinyRate=rate;
            }
            current.released=releaseBook.records();
        }current.rewardPolicy=rewards;current.capacity=capacity;current.managedWorld=host&&bool(worldPlayers);if(worldPlayers){current.worldId=worldPlayers->world().id;current.worldName=worldPlayers->world().name;current.checkpointReady=true;}transfers={};committedRequests={};downloadedCheckpoint.clear();saveSequence=0;worldAuthority={};worldAuthority.rewardRules(rewards);localWorld={};worldSent=0;claims.clear();waitingClaim=0;lastChat={};submittedChat=0;chatSequence=0;current.hosting=host;current.localWorld=host&&address=="offline";current.port=port;current.running=true;key=roomKey;quit=false;
        if(host){
            campaign.open(accountFolder,freshCampaign);
            if(campaign.ready()){const auto saved=campaign.baseline(rewards);for(size_t i=0;i<saved.size();i+=128)worldAuthority.story(0,{saved.begin()+i,saved.begin()+std::min(saved.size(),i+128)},true,i+128>=saved.size());}
            current.world=worldAuthority.snapshot();current.campaign=campaign.snapshot();
        }
        connections.clear();commands.clear();requests.clear();responses.clear();activeRequest.reset();pending={};incomingSerial=false;
        SOCKET s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(s==INVALID_SOCKET)throw std::runtime_error("Cannot create room socket");
        sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_port=htons(port);
        if(host){
            addr.sin_addr.s_addr=htonl(INADDR_ANY);
#ifdef _WIN32
            BOOL exclusive=TRUE;setsockopt(s,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,reinterpret_cast<const char*>(&exclusive),sizeof(exclusive));
#endif
            if(!current.localWorld&&(bind(s,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))||listen(s,MaxRoomPlayers))){closesocket(s);current.running=false;throw std::runtime_error("Cannot host on this port. It may already be in use.");}
            SocketLength n=sizeof(addr);getsockname(s,reinterpret_cast<sockaddr*>(&addr),&n);current.port=ntohs(addr.sin_port);
            if(current.localWorld){closesocket(s);current.port=0;}else{listener=s;try{nonblock(listener);listenerEvent=watch(listener,FD_ACCEPT|FD_CLOSE);}catch(...){closeListener();current.running=false;throw;}}current.slot=0;current.connected=true;current.message="Hosting your world.";
            Peer p;p.slot=0;p.id=identity;p.name=name;current.peers={p};membership(p,1);
        }else{
            if(!port||inet_pton(AF_INET,address.c_str(),&addr.sin_addr)!=1){closesocket(s);current.running=false;throw std::runtime_error("Enter the host's IPv4 address and port.");}
            nonblock(s);
            const int connected=connect(s,reinterpret_cast<sockaddr*>(&addr),sizeof(addr));
            if(connected==SOCKET_ERROR&&WSAGetLastError()!=WSAEWOULDBLOCK){closesocket(s);current.running=false;throw std::runtime_error("Could not connect to host.");}
            fd_set writable;FD_ZERO(&writable);FD_SET(s,&writable);timeval timeout{3,0};
            if(select(int(s)+1,nullptr,&writable,nullptr,&timeout)<=0){closesocket(s);current.running=false;throw std::runtime_error("Host did not respond. Check address, VPN and firewall.");}
            int error=0;SocketLength n=sizeof(error);getsockopt(s,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&error),&n);
            if(error){closesocket(s);current.running=false;throw std::runtime_error("Host refused the connection.");}
            Connection c;c.socket=s;try{c.event=watch(s,FD_READ|FD_WRITE|FD_CLOSE);}catch(...){closeConnection(c);current.running=false;throw;}Bytes body;string(body,key);string(body,identity);string(body,name);string(body,expectedRom);string(body,credential);queue(c,Hello,body);connections.push_back(std::move(c));current.message="Joining room...";
        }
        worker=std::thread([this]{run();});
    }
    void command(Type type,Bytes body){if(commands.size()>=128)throw std::runtime_error("Room command queue is full");commands.push_back({type,std::move(body)});SetEvent(wake);}
};
Session::Session(std::string id,std::string name,const std::filesystem::path& accountFolder):impl_(std::make_unique<Impl>(std::move(id),std::move(name),accountFolder)){}
Session::~Session()=default;
void Session::host(uint16_t port,const std::string& key,uint8_t rewards,bool freshCampaign,uint8_t capacity){impl_->start(true,"",port,key,rewards,freshCampaign,capacity);}
void Session::playLocalWorld(){impl_->start(true,"offline",0,randomId());}
void Session::join(const std::string& ip,uint16_t port,const std::string& key){impl_->start(false,ip,port,key);}
void Session::stop(){impl_->shutdown();std::lock_guard lock(impl_->mutex);impl_->current={};impl_->current.message="You left the room.";}
Status Session::status() const{std::lock_guard lock(impl_->mutex);auto state=impl_->current;if(state.hosting&&state.managedWorld&&state.checkpointRequest)for(const auto& p:state.peers)if(p.slot&&impl_->committedRequests[p.slot]<state.checkpointRequest)++state.checkpointWaiting;return state;}
void Session::setShinyRate(uint32_t denominator){
    if(denominator<1||denominator>8192)throw std::runtime_error("Choose a shiny rate from 1 in 1 to 1 in 8192.");
    std::lock_guard lock(impl_->mutex);
    if(!impl_->current.running||!impl_->current.hosting)throw std::runtime_error("Only the world host can change the shiny rate.");
    if(!impl_->accountFolder.empty()){
        const auto value=std::to_string(denominator)+"\n";
        atomicWorldFile(impl_->accountFolder/"shiny-rate.cfg",{reinterpret_cast<const uint8_t*>(value.data()),value.size()});
    }
    impl_->current.shinyRate=denominator;impl_->emitShinyRate();SetEvent(impl_->wake);
}
std::string Session::id() const{return impl_->identity;}
std::string Session::connectionKey() const{std::lock_guard lock(impl_->mutex);return impl_->key;}
std::vector<std::string> Session::localAddresses() const {
#ifdef _WIN32
    ULONG bytes=15000;std::vector<uint8_t> storage(bytes);
    auto* adapters=reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data());
    ULONG result=GetAdaptersAddresses(AF_INET,GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER,nullptr,adapters,&bytes);
    if(result==ERROR_BUFFER_OVERFLOW&&bytes<=1024*1024){storage.resize(bytes);adapters=reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data());result=GetAdaptersAddresses(AF_INET,GAA_FLAG_SKIP_ANYCAST|GAA_FLAG_SKIP_MULTICAST|GAA_FLAG_SKIP_DNS_SERVER,nullptr,adapters,&bytes);}
    std::vector<std::string> addresses;
    if(result==NO_ERROR)for(auto* a=adapters;a;a=a->Next)if(a->OperStatus==IfOperStatusUp)for(auto* u=a->FirstUnicastAddress;u;u=u->Next){
        if(!u->Address.lpSockaddr||u->Address.lpSockaddr->sa_family!=AF_INET)continue;
        const auto* ip=reinterpret_cast<const sockaddr_in*>(u->Address.lpSockaddr);const auto value=ntohl(ip->sin_addr.s_addr);
        if((value>>24)==127||(value>>16)==0xa9fe)continue;
        char text[INET_ADDRSTRLEN]{};if(inet_ntop(AF_INET,&ip->sin_addr,text,sizeof(text))&&std::find(addresses.begin(),addresses.end(),text)==addresses.end())addresses.emplace_back(text);
    }
#else
    std::vector<std::string> addresses;struct ifaddrs* list=nullptr;
    if(getifaddrs(&list)==0){for(auto* p=list;p;p=p->ifa_next){if(!p->ifa_addr||p->ifa_addr->sa_family!=AF_INET||!(p->ifa_flags&IFF_UP)||(p->ifa_flags&IFF_LOOPBACK))continue;char text[INET_ADDRSTRLEN]{};auto* a=reinterpret_cast<sockaddr_in*>(p->ifa_addr);if(inet_ntop(AF_INET,&a->sin_addr,text,sizeof(text)))addresses.emplace_back(text);}freeifaddrs(list);}
#endif
    if(addresses.empty())addresses.emplace_back("127.0.0.1");
    return addresses;
}
void Session::update(game::PlayerState p){std::lock_guard lock(impl_->mutex);impl_->local=p;}
void Session::updateWorld(WorldReport report){
    if(!validWorldReport(report)){
        std::ostringstream detail;detail<<"Invalid local world state: map="<<unsigned(report.group)<<','<<unsigned(report.map)<<" field="<<report.field<<" npcs="<<report.npcs.size()<<" wild="<<report.wild.size()<<" camp="<<report.camp.id<<" battle="<<report.battle.id;
        for(const auto& n:report.npcs)detail<<" [npc="<<unsigned(n.localId)<<" map="<<unsigned(n.pose.mapGroup)<<','<<unsigned(n.pose.mapNumber)<<" tile="<<n.pose.x<<','<<n.pose.y<<" old="<<n.oldX<<','<<n.oldY<<" pixel="<<n.pose.pixelX<<','<<n.pose.pixelY<<" anim="<<unsigned(n.anim)<<','<<unsigned(n.command)<<" visible="<<n.visible<<']';
        throw std::runtime_error(detail.str());
    }
    std::lock_guard lock(impl_->mutex);impl_->localWorld=std::move(report);
}
void Session::updateStory(const std::vector<game::StoryValue>& values,bool initial){
    std::lock_guard lock(impl_->mutex);
    for(const auto& v:values)if(!game::validStory(v,impl_->current.rewardPolicy))throw std::runtime_error("Invalid local story state");if(!impl_->current.connected)return;
    for(size_t offset=0;offset<values.size();offset+=128){const auto end=std::min(values.size(),offset+128);Bytes b;byte(b,(initial?1:0)|(end==values.size()?2:0));storyValues(b,{values.begin()+offset,values.begin()+end});impl_->command(StoryUpdate,std::move(b));}
}
uint32_t Session::claimEncounter(const EncounterKey& key){
    std::unique_lock lock(impl_->mutex);if(!impl_->current.connected||impl_->waitingClaim)return 0;
    const auto token=++impl_->claimSequence;impl_->waitingClaim=token;Bytes b;dword(b,token);encounterKey(b,key);impl_->command(EncounterClaim,std::move(b));
    const bool replied=impl_->changed.wait_for(lock,std::chrono::milliseconds(350),[&]{return impl_->claims.contains(token)||!impl_->current.connected;});
    const bool accepted=replied&&impl_->claims.contains(token)&&impl_->claims[token];impl_->claims.erase(token);impl_->waitingClaim=0;
    if(!accepted){Bytes cancel;dword(cancel,token);byte(cancel,0);impl_->command(EncounterRelease,std::move(cancel));}
    return accepted?token:0;
}
void Session::checkpointEncounter(uint32_t token,const std::vector<NpcState>& actors){if(!token||actors.empty()||actors.size()>15)return;std::lock_guard lock(impl_->mutex);Bytes b;dword(b,token);byte(b,unsigned(actors.size()));for(const auto& actor:actors)npc(b,actor);impl_->command(EncounterCheckpoint,std::move(b));}
void Session::releaseEncounter(uint32_t token,bool retry){if(!token)return;std::lock_guard lock(impl_->mutex);Bytes b;dword(b,token);byte(b,retry?1:0);impl_->command(EncounterRelease,std::move(b));}
bool Session::offerReleased(game::ReleasedMon mon){
    std::unique_lock lock(impl_->mutex);if(!impl_->current.connected||mon.owner!=impl_->identity)return false;
    const auto token=++impl_->releaseSequence;Bytes b;dword(b,token);released(b,mon);impl_->command(ReleasedOffer,std::move(b));
    const bool replied=impl_->changed.wait_for(lock,std::chrono::milliseconds(700),[&]{return impl_->releaseReplies.contains(token)||!impl_->current.connected;});
    const bool accepted=replied&&impl_->releaseReplies.contains(token)&&impl_->releaseReplies[token];impl_->releaseReplies.erase(token);return accepted;
}
void Session::advanceReleased(const std::vector<game::ReleasedMon>& states){
    std::lock_guard lock(impl_->mutex);if(!impl_->current.connected||!impl_->current.hosting)return;
    bool changed=false;const auto time=chatClock();for(const auto& m:states){const auto* old=impl_->releaseBook.find(m.id);const bool durable=old&&old->phase!=m.phase;
        if(impl_->releaseBook.advance(m,durable)){changed=true;impl_->emitReleased(*impl_->releaseBook.find(m.id));}}
    if(changed){if(time>impl_->releaseFlushed+2000){impl_->releaseBook.flush();impl_->releaseFlushed=time;}impl_->current.released=impl_->releaseBook.records();SetEvent(impl_->wake);}
}
bool Session::claimReleased(const std::string& id){
    std::unique_lock lock(impl_->mutex);if(!impl_->current.connected)return false;
    const auto token=++impl_->releaseSequence;Bytes b;dword(b,token);string(b,id);impl_->command(ReleasedClaim,std::move(b));
    const bool replied=impl_->changed.wait_for(lock,std::chrono::milliseconds(700),[&]{return impl_->releaseReplies.contains(token)||!impl_->current.connected;});
    const bool accepted=replied&&impl_->releaseReplies.contains(token)&&impl_->releaseReplies[token];impl_->releaseReplies.erase(token);
    if(!accepted){Bytes undo;string(undo,id);byte(undo,0);impl_->command(ReleasedFinish,std::move(undo));}return accepted;
}
void Session::finishReleased(const std::string& id,bool caught){std::lock_guard lock(impl_->mutex);if(!impl_->current.connected)return;Bytes b;string(b,id);byte(b,caught);impl_->command(ReleasedFinish,std::move(b));}
void Session::sendChat(const std::string& text){
    if(!validChat(text))throw std::runtime_error("Enter a message of up to 128 UTF-8 bytes, without control characters.");
    std::lock_guard lock(impl_->mutex);
    if(!impl_->current.connected)throw std::runtime_error("Join a room to chat.");
    const auto tick=chatClock();
    if(impl_->submittedChat&&tick-impl_->submittedChat<500)throw std::runtime_error("Please wait a moment before sending again.");
    Bytes b;string(b,text);impl_->command(ChatSend,std::move(b));impl_->submittedChat=tick;
}
void Session::invite(uint8_t target,Activity activity,uint32_t stake){if(stake>WagerLimit||(activity==Activity::Trade&&stake))throw std::runtime_error("Invalid battle wager");std::lock_guard lock(impl_->mutex);Bytes b;byte(b,target);byte(b,unsigned(activity));dword(b,stake);impl_->command(Invite,std::move(b));}
void Session::updateWallet(uint32_t balance,bool available){if(balance>MoneyLimit)return;std::lock_guard lock(impl_->mutex);if(!impl_->current.connected)return;Bytes b;dword(b,balance);byte(b,available);impl_->command(WalletUpdate,std::move(b));}
void Session::wagerEvent(const std::string& id,uint8_t event,uint8_t value){std::lock_guard lock(impl_->mutex);if(!impl_->current.connected)return;Bytes b;string(b,id);byte(b,event);byte(b,value);impl_->command(WagerEvent,std::move(b));}
void Session::reply(bool accept){
    std::lock_guard lock(impl_->mutex);if(impl_->current.invitation.from<0)return;
    Bytes b;byte(b,unsigned(impl_->current.invitation.from));byte(b,accept?1:0);dword(b,impl_->current.invitation.nonce);impl_->current.invitation.from=-1;impl_->command(Reply,std::move(b));
}
void Session::cancelInvitation(){std::lock_guard lock(impl_->mutex);impl_->command(InvitationCancel,{});}
void Session::battleStage(uint8_t stage){if(stage<1||stage>3)throw std::runtime_error("Invalid field battle stage");std::lock_guard lock(impl_->mutex);impl_->command(BattleStage,Bytes{stage});}
void Session::disconnectCable(){std::lock_guard lock(impl_->mutex);impl_->command(CableOff,{});}
bool Session::cableConnected() const {
    std::lock_guard lock(impl_->mutex);const auto* p=impl_->peer(impl_->current.slot);
    return impl_->current.connected && p && p->partner>=0;
}
uint16_t Session::cableControl(uint16_t value){
    std::lock_guard lock(impl_->mutex);auto* self=impl_->peer(impl_->current.slot);
    if(!self||self->partner<0)return value;
    const bool ready=(value&0x3000)==0x2000,clock=ready&&(value&0x4000)!=0;
    if(self->cableReady!=ready||self->cableClock!=clock){self->cableReady=ready;self->cableClock=clock;Bytes b;byte(b,(ready?1:0)|(clock?2:0));impl_->command(Ready,std::move(b));}
    if(!ready)return value;
    const auto* other=impl_->peer(self->partner);const unsigned role=self->slot<unsigned(self->partner)?0:1;
    return uint16_t((value&~0x3Cu)|(role?0x14u:0u)|(other&&other->cableReady?8u:0u));
}
bool Session::cableStart(uint16_t value,std::array<uint16_t,4>& out){return cableStartTimed(value,0,out);}
bool Session::cableStartTimed(uint16_t value,uint32_t elapsed,std::array<uint16_t,4>& out){
    std::unique_lock lock(impl_->mutex);auto* self=impl_->peer(impl_->current.slot);
    if(!self||self->partner<0||self->slot>self->partner)return false;
    const int partner=self->partner;const auto* other=impl_->peer(partner);
    if(!other||!other->cableReady)return false;
    const bool clockAtStart=other->cableClock;
    const uint32_t sequence=++impl_->sequence;
    Bytes b;byte(b,unsigned(partner));dword(b,sequence);word(b,value);dword(b,elapsed);impl_->command(SerialStart,std::move(b));
    const bool ok=impl_->changed.wait_for(lock,std::chrono::seconds(15),[&]{const auto* currentSelf=impl_->peer(impl_->current.slot);const auto* currentOther=impl_->peer(partner);return impl_->quit||impl_->responses.contains(sequence)||!impl_->current.connected||!currentSelf||currentSelf->partner!=partner||!currentOther||!currentOther->cableReady||(clockAtStart&&!currentOther->cableClock);});
    auto found=impl_->responses.find(sequence);
    if(!ok||found==impl_->responses.end()){impl_->current.message=ok?"Link port reset; waiting for the game to reconnect.":"Link timed out. The game will report a communication error.";return false;}
    out={value,found->second,0xffff,0xffff};impl_->responses.erase(found);
    if(impl_->current.message.starts_with("Link timed out")||impl_->current.message.starts_with("Link port reset"))impl_->current.message="Cable active; transfer recovered.";
    return true;
}
bool Session::cableClockReady() const {
    std::lock_guard lock(impl_->mutex);const auto* self=impl_->peer(impl_->current.slot);
    const auto* other=self?impl_->peer(self->partner):nullptr;
    return impl_->current.connected&&self&&other&&self->cableClock&&other->cableClock;
}
bool Session::cableTryRequest(uint32_t& elapsed){
    if(!impl_->incomingSerial.load())return false;
    std::lock_guard lock(impl_->mutex);const auto* self=impl_->peer(impl_->current.slot);
    if(!self||self->partner<0||self->slot<self->partner||impl_->requests.empty())return false;
    impl_->activeRequest=impl_->requests.front();impl_->requests.pop_front();impl_->incomingSerial=!impl_->requests.empty();
    elapsed=impl_->activeRequest->elapsed;return impl_->activeRequest->from==self->partner;
}
bool Session::cableRequest(uint32_t& elapsed){
    std::unique_lock lock(impl_->mutex);const auto* self=impl_->peer(impl_->current.slot);
    if(!self||self->partner<0||self->slot<self->partner)return false;
    const int partner=self->partner;
    if(!impl_->changed.wait_for(lock,std::chrono::seconds(15),[&]{const auto* other=impl_->peer(partner);const auto* currentSelf=impl_->peer(impl_->current.slot);return impl_->quit||!impl_->requests.empty()||!impl_->current.connected||!currentSelf||currentSelf->partner!=partner||!other||!other->cableClock;}))return false;
    if(impl_->requests.empty())return false;
    impl_->activeRequest=impl_->requests.front();impl_->requests.pop_front();impl_->incomingSerial=!impl_->requests.empty();
    elapsed=impl_->activeRequest->elapsed;return impl_->activeRequest->from==partner;
}
bool Session::cableRespond(uint16_t value,std::array<uint16_t,4>& out){
    std::lock_guard lock(impl_->mutex);const auto* self=impl_->peer(impl_->current.slot);
    if(!self||!impl_->activeRequest||impl_->activeRequest->from!=self->partner)return false;
    const auto request=*impl_->activeRequest;impl_->activeRequest.reset();
    Bytes b;byte(b,unsigned(request.from));dword(b,request.sequence);word(b,value);impl_->command(SerialReply,std::move(b));
    out={request.word,value,0xffff,0xffff};return true;
}
bool Session::cableAwait(uint16_t value,std::array<uint16_t,4>& out){
    {
        std::unique_lock lock(impl_->mutex);const auto* self=impl_->peer(impl_->current.slot);
        if(!self||self->partner<0||self->slot<self->partner)return false;
        const int partner=self->partner;
        if(!impl_->changed.wait_for(lock,std::chrono::seconds(15),[&]{const auto* currentSelf=impl_->peer(impl_->current.slot);return impl_->quit||!impl_->requests.empty()||!impl_->current.connected||!currentSelf||currentSelf->partner!=partner;})){
            impl_->current.message="Link timed out. The game will report a communication error.";return false;
        }
    }
    return cablePoll(value,out);
}
bool Session::cablePoll(uint16_t value,std::array<uint16_t,4>& out){
    if(!impl_->incomingSerial.load())return false;
    std::lock_guard lock(impl_->mutex);auto* self=impl_->peer(impl_->current.slot);
    if(!self||!self->cableReady||self->partner<0||self->slot<self->partner||impl_->requests.empty())return false;
    const auto request=impl_->requests.front();impl_->requests.pop_front();impl_->incomingSerial=!impl_->requests.empty();
    if(request.from!=self->partner)return false;
    Bytes b;byte(b,unsigned(request.from));dword(b,request.sequence);word(b,value);impl_->command(SerialReply,std::move(b));
    out={request.word,value,0xffff,0xffff};return true;
}
}

namespace fr::online {
void Session::configureWorld(const std::filesystem::path& folder,const std::string& romHash,const std::string& secret){
    std::lock_guard lock(impl_->mutex);if(impl_->current.running)throw std::runtime_error("Choose a world before connecting.");
    if(romHash.size()!=64||!worldIdValid(secret))throw std::runtime_error("Invalid world credentials.");impl_->expectedRom=romHash;impl_->credential=secret;
    if(!folder.empty()){auto world=loadWorld(folder);if(world.romHash!=romHash)throw std::runtime_error("World uses another cartridge.");impl_->worldPlayers=std::make_unique<WorldPlayers>(world);if(!impl_->worldPlayers->authenticate(impl_->identity,secret))throw std::runtime_error("World belongs to another identity.");impl_->current.managedWorld=true;impl_->current.worldId=world.id;impl_->current.worldName=world.name;impl_->current.checkpointReady=true;}
}
std::vector<uint8_t> Session::downloadedSave() const{std::lock_guard lock(impl_->mutex);return impl_->downloadedCheckpoint;}
uint32_t Session::storeCheckpoint(std::vector<uint8_t> checkpoint){
    if(!validCheckpoint(checkpoint))throw std::runtime_error("Invalid world checkpoint.");std::lock_guard lock(impl_->mutex);
    const auto serial=++impl_->saveSequence;
    if(impl_->worldPlayers){if(!impl_->current.running){impl_->worldPlayers->commit(impl_->identity,checkpoint);impl_->current.checkpointAck=serial;}else{Bytes b;dword(b,serial);b.insert(b.end(),checkpoint.begin(),checkpoint.end());impl_->command(PlayerCheckpoint,std::move(b));}}
    else if(impl_->current.connected&&impl_->current.managedWorld)impl_->sendCheckpoint(0,serial,checkpoint);
    else return 0;
    return serial;
}
void Session::requestWorldSave(){std::lock_guard lock(impl_->mutex);impl_->requestWorldSave();}
void Session::depart(game::PlayerState origin){std::lock_guard lock(impl_->mutex);if(!impl_->current.connected)return;origin.active=true;origin.followerVisible=false;Bytes b;player(b,origin);impl_->command(DepartureEvent,std::move(b));}
}
