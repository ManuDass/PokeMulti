#pragma once
#include "platform/image.hpp"
#include <string>
#include <atomic>
namespace fr {
struct CartridgeStyle {uint32_t color;std::wstring title;};
CartridgeStyle cartridgeStyle(const std::string& gameCode);
// Perspective-projected triangle mesh with a depth buffer and textured label.
Image renderCartridge(const Image& label,uint32_t shell,float yaw,float pitch,int width=370,int height=320);
Image cartridgeArtwork(const std::filesystem::path& rom,const std::filesystem::path& cache,const std::string& gameCode,const std::atomic_bool* cancelled=nullptr);
}
