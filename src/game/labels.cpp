#include "game/labels.hpp"
#include "platform/text.hpp"
#include "platform/image.hpp"
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <imgui.h>
#include <algorithm>
#include <array>
#include <map>
#include <cmath>
#include <stdexcept>
namespace fr::game {
std::filesystem::path fontFolder(){
    wchar_t path[32768]{};
    if(!GetModuleFileNameW(nullptr,path,32768))throw std::runtime_error("Cannot locate UI assets");
    return std::filesystem::path(path).parent_path()/"Fonts";
}
namespace {
using Microsoft::WRL::ComPtr;
void checked(HRESULT hr){if(FAILED(hr))throw std::runtime_error("Cannot load chat bubble tiles");}
struct Art {
    ImFontAtlas atlas;
    ImFont* font=nullptr;
    unsigned char* alpha=nullptr;
    int aw=0,ah=0;
    std::array<std::array<uint32_t,16*16>,12> tiles{};
    Art(){
        const auto folder=fontFolder();
        ImFontConfig config;config.OversampleH=config.OversampleV=1;config.PixelSnapH=true;
        font=atlas.AddFontFromFileTTF(narrow((folder/"PixelOperator8.ttf").wstring()).c_str(),8,&config,atlas.GetGlyphRangesDefault());
        if(!font)throw std::runtime_error("Cannot load Pixel Operator font");
        atlas.GetTexDataAsAlpha8(&alpha,&aw,&ah);
        const HRESULT init=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        struct Cleanup {bool active;~Cleanup(){if(active)CoUninitialize();}} cleanup{SUCCEEDED(init)};
        ComPtr<IWICImagingFactory> factory;checked(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
        constexpr const wchar_t* names[]{L"Upper Left Corner.png",L"Upper Middle Tile.png",L"Upper Right Corner.png",L"Left Middle Tile.png",L"Middle Tile.png",L"Right Middle Tile.png",L"Bottom Left Corner.png",L"Bottom Middle Tile.png",L"Bottom Right Corner.png",L"Thin Left Tile.png",L"Thin Middle Tile.png",L"Thin Right Tile.png"};
        for(unsigned i=0;i<12;++i){
            ComPtr<IWICBitmapDecoder> decoder;checked(factory->CreateDecoderFromFilename((folder/L"CHAT BUBBLE TILES"/names[i]).c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder));
            ComPtr<IWICBitmapFrameDecode> frame;checked(decoder->GetFrame(0,&frame));
            UINT w=0,h=0;checked(frame->GetSize(&w,&h));if(w!=16||h!=16)throw std::runtime_error("Chat tiles must be 16 by 16 pixels");
            ComPtr<IWICFormatConverter> converter;checked(factory->CreateFormatConverter(&converter));checked(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
            std::array<uint8_t,16*16*4> rgba{};checked(converter->CopyPixels(nullptr,64,UINT(rgba.size()),rgba.data()));
            // Copy each source texel exactly. No scaling, palette conversion,
            // recoloring, generated borders, pointers, or corner replacement.
            for(size_t pixel=0;pixel<256;++pixel){const auto* p=&rgba[pixel*4];tiles[i][pixel]=uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}

        }
    }
    int advance(wchar_t c) const {return int(std::lround(font->FindGlyph(ImWchar(c))->AdvanceX));}
    int width(const std::wstring& s) const {int w=0;for(auto c:s)w+=advance(c);return w;}
    void text(LabelImage& out,const std::wstring& s,int x,int y,uint32_t color){
        for(auto c:s){const auto* g=font->FindGlyph(ImWchar(c));
            const int sx=int(std::lround(g->U0*aw)),sy=int(std::lround(g->V0*ah));
            const int gw=int(std::lround((g->U1-g->U0)*aw)),gh=int(std::lround((g->V1-g->V0)*ah));
            for(int yy=0;yy<gh;++yy)for(int xx=0;xx<gw;++xx){const int dx=x+int(std::lround(g->X0))+xx,dy=y+int(std::lround(g->Y0))+yy;
                if(dx>=0&&dy>=0&&dx<out.width&&dy<out.height&&alpha[(sy+yy)*aw+sx+xx]>=128)out.pixels[dy*out.width+dx]=color;}
            x+=advance(c);
        }
    }
};
Art& art(){static Art a;return a;}
LabelImage blank(int w,int h){return {w,h,std::vector<uint32_t>(size_t(w*h))};}
}
const LabelImage& partyImage(unsigned count,unsigned eggs){
    static const LabelImage empty;
    if(!count||count>6||(eggs>>count))return empty;
    static const auto icons=[](){
        const auto folder=fr::executableFolder()/"UI Artwork";
        std::array<fr::Image,2> result{fr::readImage(folder/"Party Icon.png"),fr::readImage(folder/"Party Icon_Egg.png")};
        for(const auto& icon:result)if(icon.width>32||icon.height>32)throw std::runtime_error("Party icons must fit the overhead label area");
        return result;
    }();
    static std::map<unsigned,LabelImage> rows;
    const unsigned key=count|(eggs<<3);
    if(const auto it=rows.find(key);it!=rows.end())return it->second;
    const int cellWidth=std::max(icons[0].width,icons[1].width),height=std::max(icons[0].height,icons[1].height);
    auto row=blank(int(count)*(cellWidth+1)-1,height);
    // Original RGBA texels in party order, with one transparent pixel between
    // slots. Only the existing nearest-neighbor viewport scales the artwork.
    for(unsigned slot=0;slot<count;++slot){const auto& icon=icons[(eggs>>slot)&1];
        const int left=int(slot)*(cellWidth+1)+(cellWidth-icon.width)/2,top=height-icon.height;
        for(int y=0;y<icon.height;++y)std::copy_n(icon.pixels.begin()+y*icon.width,icon.width,row.pixels.begin()+(top+y)*row.width+left);
    }
    return rows.emplace(key,std::move(row)).first->second;
}
LabelImage nameImage(const std::string& text){
    auto& a=art();auto s=widen(text);
    if(a.width(s)>112){while(!s.empty()&&a.width(s+L"...")>112)s.pop_back();s+=L"...";}
    auto out=blank(a.width(s)+2,10);
    for(int y=0;y<3;++y)for(int x=0;x<3;++x)if(x!=1||y!=1)a.text(out,s,x,y,0xff000000);
    a.text(out,s,1,1,0xffffffff);return out;
}
LabelImage bubbleImage(const std::string& text){
    auto& a=art();const auto s=widen(text);std::vector<std::wstring> lines;std::wstring line;
    // Wrap on words, including a hard break for a single unbroken word.
    for(auto c:s){line+=c;if(a.width(line)>128){
        auto split=line.find_last_of(L' ');if(split==std::wstring::npos||split==0)split=line.size()-1;
        lines.push_back(line.substr(0,split));line=line.substr(split);if(!line.empty()&&line.front()==L' ')line.erase(0,1);
    }}
    if(!line.empty())lines.push_back(line);
    int width=0;for(const auto& l:lines)width=std::max(width,a.width(l));
    constexpr int tile=16,padding=3,leftInset=11,lineHeight=9;
    // Three clear pixels around the ink. The left inset also leaves three
    // clear pixels beside the original Poke Ball artwork; the artwork is never
    // painted over, moved or replaced.
    int minY=100,maxY=-100;
    for(const auto& l:lines)for(auto ch:l){const auto* g=a.font->FindGlyph(ImWchar(ch));if(g->Visible){minY=std::min(minY,int(std::floor(g->Y0)));maxY=std::max(maxY,int(std::ceil(g->Y1)));}}
    if(maxY<minY){minY=0;maxY=8;}
    const int textHeight=std::max(0,int(lines.size())-1)*lineHeight+maxY-minY;
    const int w=std::max(3*tile,(width+leftInset+padding+tile-1)/tile*tile);
    const int h=std::max(tile,(textHeight+2*padding+tile-1)/tile*tile);
    auto out=blank(w,h);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){
        const int column=x<tile?0:x>=w-tile?2:1;
        const int index=h==tile?9+column:(y<tile?0:y>=h-tile?6:3)+column;
        out.pixels[y*w+x]=a.tiles[index][(y%tile)*tile+x%tile];
    }
    const int textY=lines.size()>1?(h-textHeight)/2:padding;
    for(size_t i=0;i<lines.size();++i)a.text(out,lines[i],leftInset,textY-minY+int(i)*lineHeight,0xff000000);
    return out;
}
}
