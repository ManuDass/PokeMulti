#pragma once
#include <cstdint>
#include "game/rom_layout.hpp"
namespace fr::game {
struct UniquePokemon {uint32_t script;uint16_t claimed,hide,otherHide;bool gift;const char* name;};
// Canonical FireRed 1.0 scripts; the selected cartridge supplies each address.
inline constexpr UniquePokemon uniquePokemon[]{
 {0x08161ac8,0x246,0,0,true,"Lapras"},
 {0x0816c46d,0x263,0x57,0,true,"Eevee"},
 {0x0816ec00,0x278,0x60,0x61,true,"Dojo Pokemon"},
 {0x0816ec46,0x278,0x60,0x61,true,"Dojo Pokemon"},
 {0x081624f5,0x2bc,0x81,0,false,"Mewtwo"},
 {0x081631ac,0x2be,0x82,0,false,"Articuno"},
 {0x081637b8,0x2bf,0x5d,0,false,"Zapdos"},
 {0x08163b33,0x2bd,0x52,0,false,"Moltres"},
 {0x08168014,0x54,0x54,0,false,"Route 12 Snorlax"},
 {0x08168121,0x80,0x80,0,false,"Route 16 Snorlax"},
 {0x0816382f,0x2d0,0x85,0,false,"Power Plant Electrode"},
 {0x0816388d,0x2d1,0x86,0,false,"Power Plant Electrode"},
 {0x08164ffb,0x2f3,0x9c,0,false,"Ho-Oh"},
 {0x08165134,0x2f2,0x9b,0,false,"Lugia"},
 {0x081652c0,0x2e4,0x99,0,false,"Deoxys"}
};
inline const UniquePokemon* uniqueClaim(uint16_t flag){for(const auto& r:uniquePokemon)if(r.claimed==flag)return &r;return nullptr;}
inline const UniquePokemon* uniqueScript(uint32_t script,bool revision11){for(const auto& r:uniquePokemon)if(r.script+(revision11?0x78u:0u)==script)return &r;return nullptr;}
inline const UniquePokemon* uniqueScript(uint32_t script,FireRedRevision revision){for(const auto& r:uniquePokemon)if(romAddress(revision,r.script)==script)return &r;return nullptr;}
inline bool uniqueFlag(uint16_t flag){for(const auto& r:uniquePokemon)if(flag==r.claimed||(r.hide&&flag==r.hide)||(r.otherHide&&flag==r.otherHide))return true;return false;}
}
