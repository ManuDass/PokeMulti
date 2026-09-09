#include "game/follower_art.hpp"
#include <stdexcept>
namespace fr::game {
FollowerSheet decodeFollowerSheet(Image source){
    if(source.width<16||source.height<16||source.width%4||source.height%4||source.width>1024||source.height>1024)throw std::runtime_error("Invalid 4 by 4 follower sheet");
    bool duplicated=source.width%8==0&&source.height%8==0&&source.width>=256;
    if(duplicated)for(int y=0;y<source.height&&duplicated;y+=2)for(int x=0;x<source.width;x+=2){const auto p=source.pixels[y*source.width+x];
        if(source.pixels[y*source.width+x+1]!=p||source.pixels[(y+1)*source.width+x]!=p||source.pixels[(y+1)*source.width+x+1]!=p){duplicated=false;break;}}
    if(duplicated){Image native{source.width/2,source.height/2,std::vector<uint32_t>(source.pixels.size()/4)};
        for(int y=0;y<native.height;++y)for(int x=0;x<native.width;++x)native.pixels[y*native.width+x]=source.pixels[(y*2)*source.width+x*2];source=std::move(native);}
    FollowerSheet out;out.cellW=source.width/4;out.cellH=source.height/4;out.image=std::move(source);
    // One foot anchor across all frames prevents bobbing as the outline changes.
    int bottom=0;
    for(int row=0;row<4;++row)for(int frame=0;frame<4;++frame)for(int y=0;y<out.cellH;++y)for(int x=0;x<out.cellW;++x)
        if(out.image.pixels[(row*out.cellH+y)*out.image.width+frame*out.cellW+x]>>24)bottom=std::max(bottom,y);
    out.feet.fill(bottom);return out;
}
}
