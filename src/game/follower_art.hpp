#pragma once
#include "platform/image.hpp"
#include <array>
namespace fr::game {
// RPG Maker sheets: four columns, rows down/left/right/up. Exact duplicate
// 2x source pixels may be collapsed losslessly to the native GBA pixel grid.
struct FollowerSheet {Image image;int cellW=0,cellH=0;std::array<int,16> feet{};};
FollowerSheet decodeFollowerSheet(Image source);
inline unsigned followerRow(unsigned facing){return facing==2?3:facing==3?1:facing==4?2:0;}
}
