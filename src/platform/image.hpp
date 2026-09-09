#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>
namespace fr {
struct Image { int width=0,height=0; std::vector<uint32_t> pixels; bool empty() const {return pixels.empty();} };
std::filesystem::path executableFolder();
// User-supplied assets stay local; these reference directories are never packaged.
std::filesystem::path localAsset(const std::filesystem::path& relative);
Image readImage(const std::filesystem::path& path);
std::vector<uint32_t> premultipliedBgra(const Image& image);
}
