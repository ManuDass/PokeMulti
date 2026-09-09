#pragma once
#include "host_obj_composition.h"
namespace fr::game {
inline constexpr int CampOutlineWidth=2; // Native pixels: 4px at the minimum 2x game scale.
// Between the ground BG2 and BG1: below all objects at that priority.
inline uint64_t campGroundKey(unsigned priority){return (uint64_t(priority&3)<<33)|(uint64_t(1)<<32)|1;}
inline bool campGroundVisible(const gba::CompositionPixel& p){return p.objects&&(p.top.layer==2||p.top.layer==3);}
}
