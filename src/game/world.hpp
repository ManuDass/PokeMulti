#pragma once
#include "rom/rom.hpp"
#include "online/limits.hpp"
#include <cstdint>
#include <filesystem>
#include <vector>
namespace fr::online {class Session;}
namespace fr::game {
struct PlayerState {
    bool active=false;
    uint8_t mapGroup=0,mapNumber=0,elevation=0,facing=1,graphics=0;
    int16_t x=0,y=0;
    uint16_t follower=0;
    // Semantic presentation only: no sprite pointers, RAM, ROM art or save records.
    int16_t pixelX=0,pixelY=0,followerX=0,followerY=0;
    int8_t offsetX=0,offsetY=0;
    uint8_t spriteFrame=0,flip=0;
    bool followerVisible=false;
    uint8_t followerFacing=1,followerFrame=0;
    uint32_t sequence=0,sampleTime=0;
    uint64_t identity=0; // receiver-local identity; not serialized
    uint8_t netSlot=255;
    uint8_t partyCount=0; // Occupied party slots, including Eggs and fainted Pokemon.
    bool followerShiny=false; // Saved lead Pokemon identity; independent of room odds.
    uint32_t followerToken=0; // Individual identity, including same-species party switches.
    uint16_t followerEmoteSequence=0;
    uint8_t followerEmote=0; // 0 none, 1 heart, 2 happy, 3 music.
    uint8_t partyEggs=0; // Bit i is set for an Egg in occupied slot i.
};
void initialize(FireRedRevision revision,const std::filesystem::path& diagnostics,const std::filesystem::path& saveFile={});
void ready();
void connect(online::Session* session);
void localSlot(unsigned slot);
void frame(uint64_t frame);
void render(uint8_t* rgb,uint32_t width,uint32_t height);
PlayerState player();
void peers(std::vector<PlayerState> states);
struct Overhead { uint8_t slot=255; uint64_t identity=0; std::string name,message; };
void overheads(std::vector<Overhead> labels);
void toggleCamp();
void tradeNearby();
bool requestWorldExit();
void challengePlayer(uint8_t slot);
bool camping();
std::string campNotice();
uint16_t filterInput(uint16_t input);
void followers(bool enabled);
void visibleWild(bool enabled);
bool followersEnabled();
bool visibleWildEnabled();
std::vector<std::string> unclaimedStoryRewards();
void claimStoryRewards();
void showCampaignJournal();
uint32_t walletBalance();
bool walletAvailable();
uint32_t walletHeld();
std::string walletNotice();
std::string storyRewardNotice();
std::string releaseNotice();
#ifdef FR_TEST_HARNESS
void requestFixture(const std::string& name);
void chaseWild();
uint16_t testMovementInput(uint16_t original);
void walkTo(int x,int y);
#endif
}
