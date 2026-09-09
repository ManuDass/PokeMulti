#pragma once
#include <array>
#include <cstdint>
#include <limits>

namespace gba {
// Optional host presentation data. Never written to guest OAM/VRAM or snapshots.
// Preserve the exact two native candidates and window/effect state for each
// scanline, so late network poses can join the original composition order.
constexpr uint64_t kNoHostObject = (std::numeric_limits<uint64_t>::max)();
inline uint64_t host_obj_key(unsigned priority, uint32_t order) {
    return (uint64_t(priority & 3) << 33) | order;
}
inline uint64_t native_composition_key(int key, unsigned layer, uint32_t order) {
    if (layer == 5) return kNoHostObject - 1;
    const auto priority = unsigned(key) / 256;
    return layer == 4 ? host_obj_key(priority, order)
        : (uint64_t(priority) << 33) | (uint64_t(1) << 32) | layer;
}
struct HostObjPixel {
    uint64_t key = kNoHostObject;
    uint16_t color = 0;
    uint32_t rgba = 0; // Optional original UI RGBA; zero uses the native RGB555 path.
};
struct CompositionSample {
    uint64_t key = kNoHostObject;
    uint16_t color = 0;
    uint8_t layer = 5;
    bool first = false, second = false, valid = false;
    uint16_t objTile = 0xffff; // Latched OAM attr2; presentation-only battle/HUD separation.
};
struct CompositionPixel {
    CompositionSample top, second;
    bool objects = false, effects = false;
};
struct CompositionLine {
    uint16_t control = 0, alpha = 0, brightness = 0;
};
struct ObjCompositionFrame {
    std::array<CompositionPixel, 240 * 160> pixels{};
    std::array<CompositionLine, 160> lines{};
    std::array<bool, 160> valid{};
};
using ObjOrderProvider = uint32_t (*)(unsigned oam_index, const uint8_t* attributes);
} // namespace gba
