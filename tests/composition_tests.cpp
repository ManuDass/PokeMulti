#include "gba_ppu.h"
#include "game/camp_outline.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace gba;
namespace {
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
void store16(uint8_t* p,uint16_t value){p[0]=uint8_t(value);p[1]=uint8_t(value>>8);}
struct Fixture {
    GbaPpu ppu;
    std::array<uint8_t,0x400> io{},oam{},pal{};
    std::array<uint8_t,0x18000> vram{};
    std::array<uint8_t,GbaPpu::kFramebufferBytes> rgb{};
    std::array<HostObjPixel,240*160> host{};
    Fixture(){
        for(unsigned i=0;i<128;++i)store16(oam.data()+i*8,0x200);
        store16(pal.data(),0x03e0); // Green backdrop.
        store16(pal.data()+2,0x001f); // Red BG tile.
        store16(pal.data()+0x202,0x7c00); // Blue native object.
        std::fill_n(vram.data()+32,32,0x11);
        std::fill_n(vram.data()+0x10000,32,0x11);
        ppu.enable_obj_composition([](unsigned i,const uint8_t*){return uint32_t(100+i);});
    }
    void background(unsigned layer,unsigned priority){
        store16(io.data()+8+2*layer,uint16_t(priority|(1<<8)));
        for(unsigned i=0;i<1024;++i)store16(vram.data()+0x800+i*2,1);
    }
    void object(unsigned priority,bool semi=false){
        store16(oam.data(),uint16_t(64|(semi?0x400:0)));
        store16(oam.data()+2,96);store16(oam.data()+4,uint16_t(priority<<10));
    }
    void latch(uint16_t display){ppu.latch_framebuffer(display,io.data(),vram.data(),oam.data(),pal.data());
        std::copy_n(ppu.latched_framebuffer(),rgb.size(),rgb.begin());}
    void added(unsigned x,unsigned y,unsigned priority,unsigned order,uint16_t color=0x7fff){
        host[y*240+x]={host_obj_key(priority,order),color};}
    void compose(){ppu.compose_host_objects(rgb.data(),host.data(),240,160);}
    void pixel(unsigned x,unsigned y,unsigned red,unsigned green,unsigned blue,const char* why){
        const auto p=rgb.data()+(y*240+x)*3;check(p[0]==red&&p[1]==green&&p[2]==blue,why);}
};
}
int main(){try{
    auto f=std::make_unique<Fixture>();
    f->background(0,0);f->object(2);f->latch(0x1100);
    const auto native=f->rgb;f->compose();check(f->rgb==native,"empty host layer changed native pixels");
    f->added(98,66,2,50);f->compose();f->pixel(98,66,255,0,0,"sprite painted through dialogue/menu BG0");
    f=std::make_unique<Fixture>();f->latch(0x1000);f->added(98,66,1,1);
    f->host[66*240+98].rgba=0xffcf7b22;f->compose();f->pixel(98,66,34,123,207,"supplied PNG channels were quantized");
    f->background(0,0);f->latch(0x1100);f->compose();f->pixel(98,66,255,0,0,"RGBA tile painted through native menu");
    f=std::make_unique<Fixture>();f->background(1,1);f->latch(0x1200);
    f->added(98,66,2,50);f->compose();f->pixel(98,66,255,0,0,"sprite painted over higher-priority tree/roof");
    f->latch(0x1000);f->compose();f->pixel(98,66,255,255,255,"sprite missing through transparent foreground");
    f->background(1,2);f->latch(0x1200);f->compose();f->pixel(98,66,255,255,255,"same-priority BG incorrectly covered object");
    f=std::make_unique<Fixture>();f->object(2);f->latch(0x1000);
    f->added(98,66,2,150);f->compose();f->pixel(98,66,0,0,255,"farther host sprite covered nearer native trainer");
    f->added(98,66,2,50);f->compose();f->pixel(98,66,255,255,255,"nearer host sprite remained behind native trainer");
    f->added(98,66,2,100);f->latch(0x1000);f->compose();f->pixel(98,66,0,0,255,"equal depth did not retain stable precedence");
    f->added(98,66,3,0);f->latch(0x1000);f->compose();f->pixel(98,66,0,0,255,"depth overrode elevation priority");
    f=std::make_unique<Fixture>();
    store16(f->io.data()+0x40,(96<<8)|100);store16(f->io.data()+0x44,(64<<8)|72);
    store16(f->io.data()+0x48,0x2f);store16(f->io.data()+0x4a,0x3f);
    f->latch(0x3000);f->added(98,66,2,50);f->added(102,66,2,50);f->compose();
    f->pixel(98,66,0,255,0,"host sprite bypassed hardware window");f->pixel(102,66,255,255,255,"window clipped pixels outside its bounds");
    f->latch(0x1080);f->compose();f->pixel(98,66,255,255,255,"forced blank corrupted");
    f->added(98,66,2,50,0x001f);f->compose();f->pixel(98,66,255,255,255,"host sprite leaked into forced blank");
    f=std::make_unique<Fixture>();f->background(0,0);
    store16(f->io.data()+0x50,0x1041);store16(f->io.data()+0x52,0x0808);
    f->latch(0x1100);f->added(98,66,2,50,0x7c00);f->compose();
    f->pixel(98,66,132,0,132,"translucent foreground did not blend against inserted object");
    f=std::make_unique<Fixture>();store16(f->io.data()+0x50,0xd0);store16(f->io.data()+0x54,8);
    f->latch(0x1000);f->added(98,66,2,50,0x7fff);f->compose();f->pixel(98,66,132,123,132,"host object ignored native brightness/green-precision rules");
    f=std::make_unique<Fixture>();f->object(2,true);f->background(1,3);
    store16(f->io.data()+0x50,0x1240);store16(f->io.data()+0x52,0x0808);
    f->latch(0x1200);f->added(98,66,2,150);f->compose();f->pixel(98,66,0,0,255,"incorrect alpha blending between two object layers");
    f=std::make_unique<Fixture>();
    store16(f->io.data()+0x44,(64<<8)|72);store16(f->io.data()+0x48,0x2f);store16(f->io.data()+0x4a,0x3f);
    for(unsigned y=0;y<160;++y){store16(f->io.data()+0x40,uint16_t((96<<8)|(y==66?100:97)));
        f->ppu.render_scanline(y,0x3000,f->io.data(),f->vram.data(),f->oam.data(),f->pal.data());}
    f->ppu.mark_framebuffer_latched();std::copy_n(f->ppu.latched_framebuffer(),f->rgb.size(),f->rgb.begin());
    f->io.fill(0);f->added(98,66,2,50);f->added(98,67,2,50);f->compose();
    f->pixel(98,66,0,255,0,"lost latched per-scanline window state");f->pixel(98,67,255,255,255,"neighbor scanline inherited wrong mask");
    // Camp ground lines must sit above lawn and below native sprites/UI/trees.
    f=std::make_unique<Fixture>();f->background(2,2);f->latch(0x1400);
    f->host[66*240+98]={fr::game::campGroundKey(2),0x7fff};f->compose();f->pixel(98,66,255,255,255,"Camp outline missing over ground");
    check(fr::game::campGroundVisible(f->ppu.obj_composition_frame()->pixels[66*240+98]),"Plain ground allows perimeter");
    f->object(2);f->latch(0x1400);f->compose();f->pixel(98,66,0,0,255,"Camp outline covered native trainer");
    check(!fr::game::campGroundVisible(f->ppu.obj_composition_frame()->pixels[66*240+98]),"Native actor clips ground line");
    f->background(1,2);f->latch(0x1600);f->compose();f->pixel(110,66,255,0,0,"Foreground tree replaced by perimeter");
    f->host[66*240+110]={fr::game::campGroundKey(2),0x7fff};f->compose();f->pixel(110,66,255,0,0,"Camp outline painted above BG1 at equal priority");
    check(!fr::game::campGroundVisible(f->ppu.obj_composition_frame()->pixels[66*240+110]),"Foreground plane clips ground line");
    f->background(0,0);f->latch(0x1700);f->compose();f->pixel(98,66,255,0,0,"Camp perimeter covered native menu");
    f->ppu.reset();const auto resetRgb=f->rgb;f->compose();check(f->rgb==resetRgb,"reset reused stale composition context");
    std::cout<<"Sprite composition: native preservation, UI, terrain, depth, ties, elevation, windows, blanking, alpha, brightness and latched scanlines passed\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
