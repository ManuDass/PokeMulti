#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace fr::game {
std::filesystem::path fontFolder();
struct LabelImage {
    int width=0,height=0;
    // Original RGBA bytes, packed R | G<<8 | B<<16 | A<<24.
    std::vector<uint32_t> pixels;
};
LabelImage nameImage(const std::string& text);
const LabelImage& partyImage(unsigned count,unsigned eggs=0);
LabelImage bubbleImage(const std::string& text);
}
