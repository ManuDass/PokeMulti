#pragma once
#include <cstdint>
#include <span>
#include <optional>
#include <string>
#include <vector>
#include <memory>
namespace fr::input {
// Nintendo's PBP input characteristic: counter, buttons, packed stick, motion.
struct BallSample {float x=0,y=0;bool top=false,stick=false;}; // +Y is down
std::optional<BallSample> decodeBall(std::span<const uint8_t> report);
struct BallMapping {int deadzone=25;bool swapButtons=false;bool invertY=false;};
class BallControls {
public:
    uint16_t keys(const BallSample& sample,uint64_t now,bool usable,const BallMapping& mapping);
    void reset();
    bool armed()const{return armed_;}
private:
    int horizontal_=0,vertical_=0;
    bool armed_=false,chord_=false;
    uint64_t topAt_=0,stickAt_=0;
    bool topWas_=false,stickWas_=false;
};
enum class BallPhase {Idle,Scanning,Found,Connecting,Waiting,Connected,Error};
struct BallDevice {uint64_t address=0;std::string label;int signal=0;};
struct BallStatus {
    BallPhase phase=BallPhase::Idle;std::string message="Ready to connect";
    std::vector<BallDevice> devices;uint64_t address=0,lastReport=0,reportCount=0;
    int battery=-1;BallSample sample{};
};
// Owns a background Windows BLE worker; never blocks the native game on radio I/O.
class Pokeball {
public:
    Pokeball();~Pokeball();
    Pokeball(const Pokeball&)=delete;Pokeball& operator=(const Pokeball&)=delete;
    void scan();void connect(uint64_t address);void disconnect();
    BallStatus status() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
uint64_t ballTime();
}
