#pragma once
#include "game/world.hpp"
#include "online/world_sync.hpp"
#include "online/wager.hpp"
#include "game/released.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace fr::online {
inline constexpr uint8_t RoomProtocolVersion=19;
enum class Activity:uint8_t { Battle=1, Trade=2 };
struct Peer {
    uint8_t slot=0;
    std::string id,name;
    game::PlayerState player{};
    uint32_t chatAfter=0; // room sequence at this presence's join
    int partner=-1;
    bool cableReady=false,cableClock=false;
    Activity activity=Activity::Trade;uint8_t battleStage=0;
};
struct Invitation { int from=-1; Activity activity=Activity::Battle; uint32_t stake=0,nonce=0; };
inline constexpr size_t ChatLimit=128, ChatHistoryLimit=100;
struct ChatMessage {
    uint32_t sequence=0;
    uint8_t slot=0;
    std::string id,name,text;
    uint8_t kind=0; // 0: chat, 1: joined, 2: left (host generated only)
    uint64_t receivedAt=0; // receiver's monotonic milliseconds, never a remote clock
};
bool validChat(const std::string& text);
uint64_t chatClock();
struct Departure {uint32_t sequence=0;uint8_t slot=255;game::PlayerState origin;uint64_t receivedAt=0;};
struct Status {
    bool running=false,connected=false,hosting=false;
    uint16_t port=0;
    uint8_t capacity=4;
    bool managedWorld=false,checkpointReady=false,localWorld=false;
    unsigned checkpointWaiting=0;
    uint32_t checkpointRequest=0,checkpointAck=0;
    std::string worldId,worldName;
    std::vector<Departure> departures;
    uint8_t rewardPolicy=game::DefaultRewardSharing;
    int slot=-1;
    std::string message;uint32_t messageSequence=0;
    std::vector<Peer> peers;
    Invitation invitation;
    std::vector<ChatMessage> chat;
    SharedWorld world;
    game::CampaignState campaign;
    std::vector<game::ReleasedMon> released;
    Wager wager;
};
class Session {
public:
    Session(std::string id,std::string name,const std::filesystem::path& accountFolder={});
    ~Session();
    Session(const Session&)=delete;
    Session& operator=(const Session&)=delete;
    void host(uint16_t port,const std::string& key,uint8_t rewards=game::DefaultRewardSharing,bool freshCampaign=false,uint8_t capacity=4);
    void playLocalWorld();
    void join(const std::string& ipv4,uint16_t port,const std::string& key);
    void stop();
    void configureWorld(const std::filesystem::path& world,const std::string& romHash,const std::string& secret);
    std::vector<uint8_t> downloadedSave() const;
    uint32_t storeCheckpoint(std::vector<uint8_t> checkpoint);
    void requestWorldSave();
    void depart(game::PlayerState origin);

    Status status() const;
    void update(game::PlayerState player);
    void sendChat(const std::string& text);
    void updateWorld(WorldReport report);
    void updateStory(const std::vector<game::StoryValue>& values,bool initial=false);
    uint32_t claimEncounter(const EncounterKey& key);
    void checkpointEncounter(uint32_t token,const std::vector<NpcState>& actors);
    void releaseEncounter(uint32_t token,bool retry=false);
    bool offerReleased(game::ReleasedMon mon);
    void advanceReleased(const std::vector<game::ReleasedMon>& states);
    bool claimReleased(const std::string& id);
    void finishReleased(const std::string& id,bool caught);
    void invite(uint8_t target,Activity activity,uint32_t stake=0);
    void updateWallet(uint32_t balance,bool available);
    void wagerEvent(const std::string& id,uint8_t event,uint8_t value=0);
    void reply(bool accept);
    void cancelInvitation();
    void disconnectCable();
    void battleStage(uint8_t stage);
    bool cableConnected() const;
    uint16_t cableControl(uint16_t control);
    bool cableStart(uint16_t word,std::array<uint16_t,4>& result);
    bool cableStartTimed(uint16_t word,uint32_t elapsed,std::array<uint16_t,4>& result);
    bool cableClockReady() const;
    bool cableRequest(uint32_t& elapsed);
    bool cableTryRequest(uint32_t& elapsed);
    bool cableRespond(uint16_t word,std::array<uint16_t,4>& result);
    bool cablePoll(uint16_t word,std::array<uint16_t,4>& result);
    bool cableAwait(uint16_t word,std::array<uint16_t,4>& result);
    std::string id() const;
    std::string connectionKey() const;
    std::vector<std::string> localAddresses() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
std::string randomId();
bool validIdentity(const std::string& id,const std::string& name);
}
