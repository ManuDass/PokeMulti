#include "input/pokeball.hpp"
#include "frontend/ball_model.hpp"
#include "host_window.h"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <array>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
struct DispatchEntry { uint32_t addr; uint8_t thumb,resume; void(*fn)(); };
extern "C" const DispatchEntry kDispatchTable[]={{0,0,0,nullptr}};
extern "C" const unsigned kDispatchTableLen=0;
extern "C" int SDL_SendKeyboardKey(Uint8,SDL_Scancode);
using namespace fr::input;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
uint16_t injected=0x3ff;bool capture=false;
int main(int argc,char** argv){try{
    if(argc>1&&std::string(argv[1])=="--scan"){
        Pokeball ball;ball.scan();const auto end=ballTime()+12000;
        while(ballTime()<end){auto state=ball.status();if(state.phase!=BallPhase::Scanning){std::cout<<"scan phase="<<int(state.phase)<<" devices="<<state.devices.size()<<" message="<<state.message<<'\n';return 0;}std::this_thread::sleep_for(std::chrono::milliseconds(100));}
        throw std::runtime_error("Scan did not finish within its deadline");
    }
    const std::array<uint8_t,5> neutral{0x32,0,0,7,108},east{0x33,0,0,12,108},west{0x34,0,0,2,108},north{0x35,0,0,7,36},south{0x36,0,0,7,180};
    const auto n=*decodeBall(neutral);check(n.x==0&&n.y==0,"Neutral report decoded incorrectly");
    check(decodeBall(east)->x==1&&decodeBall(west)->x==-1,"Packed stick X incorrect");
    check(decodeBall(north)->y==-1&&decodeBall(south)->y==1,"Stick Y incorrect");
    check(!decodeBall(std::span(neutral).first(4)),"Short report accepted");
    std::array<uint8_t,513> tooLarge{};check(!decodeBall(tooLarge),"Oversized report accepted");
    BallControls controls;BallMapping mapping;
    check(controls.keys(*decodeBall(east),1,true,mapping)==0x3ff,"Held reconnect direction leaked");
    check(!controls.armed(),"Held stick unexpectedly armed");
    controls.keys({.2f,-.2f},5,true,mapping);
    check(controls.armed(),"Resting stick inside dead zone cannot arm");
    check(controls.keys({.2f,-.2f},6,true,mapping)==0x3ff,"Resting stick drifts after arming");
    controls.keys(n,10,true,mapping);
    check(controls.keys(*decodeBall(east),20,true,mapping)==0x3ef,"East binding");
    check(controls.keys(*decodeBall(west),30,true,mapping)==0x3df,"West binding");
    check(controls.keys(*decodeBall(north),40,true,mapping)==0x3bf,"North binding");
    check(controls.keys(*decodeBall(south),50,true,mapping)==0x37f,"South binding");
    check(controls.keys({.26f,0},60,true,mapping)==0x3ef,"Deadzone entry");
    check(controls.keys({.2f,0},70,true,mapping)==0x3ef,"Deadzone hysteresis");
    check(controls.keys({.1f,0},80,true,mapping)==0x3ff,"Deadzone release");
    BallSample click{0,0,false,true},top{0,0,true,false},both{0,0,true,true};
    check(controls.keys(click,100,true,mapping)==0x3ff,"Chord window leaked A");
    check(controls.keys(both,150,true,mapping)==0x3f7,"Start chord incorrect");
    check(controls.keys(both,250,true,mapping)==0x3ff,"Held chord repeats Start");
    check(controls.keys(top,300,true,mapping)==0x3ff,"Chord release leaked B");controls.keys(n,350,true,mapping);
    controls.keys(click,400,true,mapping);check(controls.keys(click,500,true,mapping)==0x3fe,"Held confirm missing");controls.keys(n,550,true,mapping);
    controls.keys(top,600,true,mapping);check(controls.keys(n,630,true,mapping)==0x3fd,"Short B tap lost");
    check(controls.keys(click,650,false,mapping)==0x3ff,"Lost focus/stale connection leaks input");
    check(controls.keys(click,700,true,mapping)==0x3ff,"Held input rearmed without release");controls.keys(n,710,true,mapping);
    mapping.swapButtons=true;mapping.invertY=true;controls.keys(click,750,true,mapping);check(controls.keys(click,850,true,mapping)==0x3fd,"Button swap missing");
    check(controls.keys(*decodeBall(north),900,true,mapping)==0x37f,"Y inversion missing");
    SDL_setenv("SDL_VIDEODRIVER","dummy",1);SDL_setenv("SDL_AUDIODRIVER","dummy",1);
    gbarecomp::HostWindow window;gbarecomp::HostShell shell;shell.wasd_movement=true;shell.controller_keys=[]{return injected;};shell.captures_input=[]{return capture;};
    check(window.open(),"SDL window failed");window.set_shell(shell);injected=0x3ef;
    check(window.pump().keyinput==0x3ef,"Accessory never reaches native input");
    SDL_SendKeyboardKey(SDL_PRESSED,SDL_SCANCODE_X);check(window.pump().keyinput==0x3ee,"Keyboard/accessory merge broken");
    capture=true;check(window.pump().keyinput==0x3ff,"Chat capture leaks accessory input");capture=false;
    injected=0x3ff;check(window.pump().keyinput==0x3fe,"Disconnect released keyboard input");SDL_SendKeyboardKey(SDL_RELEASED,SDL_SCANCODE_X);window.close();
    // Cancellation/lifetime test uses real Windows BLE APIs, without pairing.
    {Pokeball ball;check(ball.status().phase==BallPhase::Idle,"Unexpected initial connection");ball.scan();ball.disconnect();check(ball.status().phase==BallPhase::Idle,"Cancel did not clear state");}
    if(argc>1){fr::BallModel model;model.load(std::filesystem::path(argv[1]));check(model.triangles()>2000,"Missing supplied mesh parts");
        auto preview=model.render(argc>3?std::stof(argv[3]):-.24f,argc>4?std::stof(argv[4]):-.72f,560,312);size_t opaque=0;
        for(int y=0;y<preview.height;++y)for(int x=0;x<preview.width;++x)if(preview.pixels[size_t(y)*preview.width+x]>>24){++opaque;check(x>4&&y>4&&x<preview.width-5&&y<preview.height-5,"Model cut off at preview edge");}
        check(opaque>10000,"Preview unexpectedly blank");auto* surface=SDL_CreateRGBSurfaceWithFormatFrom(preview.pixels.data(),preview.width,preview.height,32,preview.width*4,SDL_PIXELFORMAT_RGBA32);
        check(surface!=nullptr,"Preview surface failed");if(argc>2)check(SDL_SaveBMP(surface,argv[2])==0,"Cannot save preview");SDL_FreeSurface(surface);
        std::cout<<"Model: "<<model.triangles()<<" triangles, "<<opaque<<" visible pixels\n";
    }
    std::cout<<"PASS: packet decoding, all directions, dead zone, Start chord, taps, remapping, focus/reconnect neutral, native SDL merge, BLE cancellation\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
