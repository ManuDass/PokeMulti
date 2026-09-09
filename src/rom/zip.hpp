#pragma once
#include <span>
#include <vector>
#include <cstdint>
namespace fr { std::vector<uint8_t> readZipRom(std::span<const uint8_t> archive); }
