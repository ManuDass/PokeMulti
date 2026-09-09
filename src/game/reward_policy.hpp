#pragma once
#include <cstdint>
namespace fr::game {
enum RewardSharing:uint8_t {ShareTMs=1,ShareStoryItems=2,ShareSpecialPokemon=4};
constexpr uint8_t DefaultRewardSharing=ShareTMs|ShareStoryItems,AllRewardSharing=7;
}
