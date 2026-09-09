#include "game/labels.hpp"
#include "platform/image.hpp"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <stdexcept>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
    const auto icon=fr::readImage(fr::executableFolder()/"UI Artwork"/"Party Icon.png");
    check(fr::game::partyImage(0).pixels.empty()&&fr::game::partyImage(7).pixels.empty(),"Empty or invalid parties must not show icons");
    for(unsigned n=1;n<=6;++n){const auto& row=fr::game::partyImage(n);
        check(row.height==icon.height&&row.width==int(n)*(icon.width+1)-1,"Party row must preserve native artwork dimensions");
        for(unsigned i=0;i<n;++i)for(int y=0;y<icon.height;++y)for(int x=0;x<icon.width;++x)
            check(row.pixels[y*row.width+i*(icon.width+1)+x]==icon.pixels[y*icon.width+x],"Party icon texels or alpha were altered");
    }
    const auto egg=fr::readImage(fr::executableFolder()/"UI Artwork"/"Party Icon_Egg.png");
    check(fr::game::partyImage(1,2).pixels.empty(),"Egg outside the occupied party must be rejected");
    const auto& mixed=fr::game::partyImage(6,0x2a);
    for(unsigned i=0;i<6;++i){const auto& source=i%2?egg:icon;
        for(int y=0;y<source.height;++y)for(int x=0;x<source.width;++x)
            check(mixed.pixels[y*mixed.width+i*(icon.width+1)+x]==source.pixels[y*source.width+x],"Egg/party slot order or supplied texels changed");
    }
    auto name=fr::game::nameImage("LEAF");
    check(std::count(name.pixels.begin(),name.pixels.end(),0xffffffff)>8,"White username glyphs missing");
    check(std::count(name.pixels.begin(),name.pixels.end(),0xff000000)>8,"Black glyph outline missing");
    check(std::count(name.pixels.begin(),name.pixels.end(),0)>name.width,"Name background must be transparent");
    check(fr::game::nameImage(std::string(96,'W')).width<=114,"Long username exceeds screen budget");
    auto emit=[](const char* file,const fr::game::LabelImage& image){
        std::filesystem::create_directories("chat-label-fixtures");std::ofstream out(std::filesystem::path("chat-label-fixtures")/file,std::ios::binary);
        uint32_t size[]{uint32_t(image.width),uint32_t(image.height)};out.write(reinterpret_cast<const char*>(size),sizeof(size));out.write(reinterpret_cast<const char*>(image.pixels.data()),std::streamsize(image.pixels.size()*4));
    };
    auto shortBubble=fr::game::bubbleImage("Hi!");auto longBubble=fr::game::bubbleImage(std::string(128,'W'));
    auto two=fr::game::bubbleImage(std::string(23,'W')),three=fr::game::bubbleImage(std::string(50,'W'));
    check(shortBubble.width==48&&shortBubble.height==16,"Short messages must use a 1x3 frame of original 16px thin tiles");
    check(two.height==32&&three.height==48,"Wrapped messages must use two or three original tile rows");
    auto compact=fr::game::bubbleImage(std::string(40,'W'));
    check(compact.height==32,"Three compact text lines should fit two original tile rows");
    emit("compact-three-lines.rgba",compact);
    emit("thin.rgba",shortBubble);emit("two-row.rgba",two);emit("three-row.rgba",three);emit("long.rgba",longBubble);
    emit("thin-wide.rgba",fr::game::bubbleImage("Hi Leaf!"));emit("accents.rgba",fr::game::bubbleImage("\xc3\x89gjy!"));
    check(shortBubble.width<longBubble.width&&shortBubble.height<longBubble.height,"Nine-tile bubble must grow and wrap");
    check(longBubble.width<=144&&longBubble.height<=136,"Bubble exceeds viewport budget");
    check((shortBubble.pixels[0]>>24)==0&&(shortBubble.pixels[shortBubble.width-1]>>24)==0,"Rounded bubble corners must remain transparent");
    check(std::count(longBubble.pixels.begin(),longBubble.pixels.end(),0xff000000)>100,"Wrapped message glyphs missing");
    std::cout<<"Outlined names, transparent corners, supplied tiles, long-word wrapping and bounded bubbles, original 16px thin/multiple-row tiles passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
