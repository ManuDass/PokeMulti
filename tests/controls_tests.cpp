#include "host_window.h"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <iostream>
#include <stdexcept>
// Feed SDL's real keyboard-state machinery; SDL_PushEvent alone cannot test
// held-key polling. This private SDL entry point is used only by this test.
extern "C" int SDL_SendKeyboardKey(Uint8 state,SDL_Scancode scancode);
struct DispatchEntry { uint32_t addr; uint8_t thumb,resume; void(*fn)(); };
extern "C" const DispatchEntry kDispatchTable[]={{0,0,0,nullptr}};
extern "C" const unsigned kDispatchTableLen=0;
bool captured=false;
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){try{
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);SDL_setenv("SDL_AUDIODRIVER","dummy",1);
    gbarecomp::HostWindow window;gbarecomp::HostShell shell;shell.wasd_movement=true;shell.captures_input=[]{return captured;};
    check(window.open(),"Could not open test input window");window.set_shell(shell);
    auto input=[&]{return window.pump().keyinput;};
    check(input()==0x3ff,"Keys stuck at startup");
    const SDL_Scancode aliases[]{SDL_SCANCODE_D,SDL_SCANCODE_A,SDL_SCANCODE_W,SDL_SCANCODE_S};
    const SDL_Scancode arrows[]{SDL_SCANCODE_RIGHT,SDL_SCANCODE_LEFT,SDL_SCANCODE_UP,SDL_SCANCODE_DOWN};
    for(int i=0;i<4;++i){
        SDL_SendKeyboardKey(SDL_PRESSED,aliases[i]);check(input()==(0x3ff^(1<<(4+i))),"WASD direction is wrong");
        SDL_SendKeyboardKey(SDL_PRESSED,arrows[i]);SDL_SendKeyboardKey(SDL_RELEASED,aliases[i]);check(input()==(0x3ff^(1<<(4+i))),"Releasing WASD released a held arrow");
        SDL_SendKeyboardKey(SDL_RELEASED,arrows[i]);check(input()==0x3ff,"Released direction stuck");
    }
    SDL_SendKeyboardKey(SDL_PRESSED,SDL_SCANCODE_W);SDL_SendKeyboardKey(SDL_PRESSED,SDL_SCANCODE_D);check(input()==0x3af,"Simultaneous directions not additive");
    captured=true;check(input()==0x3ff,"Chat capture leaked WASD into the game");captured=false;check(input()==0x3af,"Game focus did not restore held keys");
    SDL_SendKeyboardKey(SDL_RELEASED,SDL_SCANCODE_W);SDL_SendKeyboardKey(SDL_RELEASED,SDL_SCANCODE_D);
    shell.wasd_movement=false;window.set_shell(shell);SDL_SendKeyboardKey(SDL_PRESSED,SDL_SCANCODE_W);check(input()==0x3ff,"Product opt-out still binds WASD");
    SDL_SendKeyboardKey(SDL_RELEASED,SDL_SCANCODE_W);window.close();
    std::cout<<"WASD/arrows, simultaneous holds, releases, UI capture and opt-out passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
