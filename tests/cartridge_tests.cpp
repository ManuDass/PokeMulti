#include "frontend/cartridge.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(){try{
    const auto icon=fr::readImage(fr::executableFolder()/"Program_Icon.png");check(icon.width==16&&icon.height==16,"Supplied app icon dimensions changed");
    const auto red=fr::cartridgeStyle("BPRE"),green=fr::cartridgeStyle("BPGE");check(red.color!=green.color&&green.title.find(L"LeafGreen")!=std::wstring::npos,"Cartridge identity not game-specific");
    const auto a=fr::renderCartridge(icon,red.color,-.4f,-.18f),b=fr::renderCartridge(icon,red.color,3.14f,-.18f);
    check(a.pixels!=b.pixels,"Cartridge rotation has no effect");
    check(std::count(a.pixels.begin(),a.pixels.end(),0)>a.width,"Cartridge background is not transparent");
    check(std::count_if(a.pixels.begin(),a.pixels.end(),[](auto p){return (p>>24)!=0;})>20000,"Missing cartridge mesh");
    const fr::Image transparent{16,16,std::vector<uint32_t>(256,0)};
    check(fr::renderCartridge({},red.color,-.4f,-.18f).pixels==fr::renderCartridge(transparent,red.color,-.4f,-.18f).pixels,"Transparent label adds an unwanted backing or border");
    std::cout<<"Cartridge mesh, label, depth/rotation, game colors and exact 16px logo passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
