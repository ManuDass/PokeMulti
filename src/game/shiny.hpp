#pragma once
#include <cstdint>
#include <stdexcept>
namespace fr::game {
inline constexpr uint32_t DefaultShinyRate=8192;
constexpr bool isShiny(uint32_t personality,uint32_t trainer){
    return uint16_t(personality^(personality>>16)^trainer^(trainer>>16))<8;
}
constexpr unsigned unownForm(uint32_t p){return (((p>>18)&192)|((p>>12)&48)|((p>>6)&12)|(p&3))%28;}
// Used only before the native creator encrypts a brand-new Pokemon. Preserve
// nature, ability parity and gender (low byte); Unown instead preserves form.
// Existing records, OT IDs and the cartridge's shiny predicate never change.
inline uint32_t shinyPersonality(uint32_t original,uint32_t trainer,bool shiny,bool unown,uint32_t entropy){
    if(isShiny(original,trainer)==shiny)return original;
    auto matches=[&](uint32_t p){return p%25==original%25&&(p&1)==(original&1)&&(!unown||unownForm(p)==unownForm(original));};
    if(shiny){
        const unsigned count=unown?65536:256;
        uint32_t formFallback=0;bool haveFormFallback=false;
        for(unsigned i=0;i<count*8;++i){
            const uint16_t low=unown?uint16_t(entropy+i%count):uint16_t((original&255)|(((entropy+i%count)&255)<<8));
            const uint16_t high=uint16_t(low^trainer^(trainer>>16)^((entropy/65536+i/count)&7));
            const uint32_t p=uint32_t(low)|(uint32_t(high)<<16);
            if(matches(p))return p;
            if(unown&&!haveFormFallback&&(p&1)==(original&1)&&unownForm(p)==unownForm(original)){formFallback=p;haveFormFallback=true;}
        }
        // Some Gen III Unown letter/nature/OT combinations cannot be shiny.
        // For a NEW shiny Unown keep its chamber-specific letter, using a
        // compatible nature only when retaining both is mathematically impossible.
        if(haveFormFallback)return formFallback;
    }else{
        for(unsigned i=1;i<=65536;++i){
            const uint32_t p=(original&65535)|(uint32_t(uint16_t((original>>16)+i))<<16);
            if(matches(p)&&!isShiny(p,trainer))return p;
        }
    }
    throw std::runtime_error("No compatible shiny personality found.");
}
}
