#include "input/pokeball.hpp"
namespace fr::input {
struct Pokeball::Impl {};
Pokeball::Pokeball():impl_(std::make_unique<Impl>()){}Pokeball::~Pokeball()=default;
void Pokeball::scan(){}void Pokeball::connect(uint64_t){}void Pokeball::disconnect(){}
BallStatus Pokeball::status()const{BallStatus result;result.phase=BallPhase::Error;result.message="Poké Ball Plus Bluetooth is not supported in this Mac preview. Keyboard and SDL controllers are available.";return result;}
}
