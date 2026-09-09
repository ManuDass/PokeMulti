#pragma once
namespace gbarecomp {
// Optional product shell, owned and called exclusively on the window/game thread.
// Opaque arguments are SDL_Window, SDL_Renderer, SDL_Event, SDL_Texture respectively.
struct HostShell {
    void (*open)(void*,void*) = nullptr;
    void (*event)(const void*) = nullptr;
    void (*render)(void*) = nullptr;
    bool (*captures_input)() = nullptr;
    void (*close)() = nullptr;
    bool wasd_movement = false;
    int (*volume)() = nullptr; // optional product volume, 0..100, same game thread
    unsigned short (*controller_keys)() = nullptr; // additive product accessory, active-low GBA bits
    bool (*allow_quit)() = nullptr; // Product may finish a world checkpoint before closing.
};
}
