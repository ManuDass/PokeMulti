#include "frontend/cartridge.hpp"
#ifdef _WIN32
#include <windows.h>
#else

#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <limits>
namespace fr {
CartridgeStyle cartridgeStyle(const std::string& code){
    if(code.starts_with("BPR"))return {0xc8432b,L"Pok\u00e9mon FireRed"};
    if(code.starts_with("BPG"))return {0x82bc46,L"Pok\u00e9mon LeafGreen"};
    if(code.starts_with("AXV"))return {0xa1273a,L"Pok\u00e9mon Ruby"};
    if(code.starts_with("AXP"))return {0x3061a9,L"Pok\u00e9mon Sapphire"};
    if(code.starts_with("BPE"))return {0x239965,L"Pok\u00e9mon Emerald"};
    return {0x697580,L"Your next adventure"};
}
namespace {
struct V {float x,y,z,u=0,v=0;};
struct Raster {
    Image out;std::vector<float> depth;float yaw,pitch;
    Raster(int w,int h,float y,float p):out{w,h,std::vector<uint32_t>(size_t(w)*h)},depth(size_t(w)*h,1.e9f),yaw(y),pitch(p){}
    V rotate(V v){const float x=v.x*std::cos(yaw)+v.z*std::sin(yaw),z=-v.x*std::sin(yaw)+v.z*std::cos(yaw);
        const float y=v.y*std::cos(pitch)-z*std::sin(pitch),zz=v.y*std::sin(pitch)+z*std::cos(pitch);
        constexpr float tilt=-.065f;return {x*std::cos(tilt)-y*std::sin(tilt),x*std::sin(tilt)+y*std::cos(tilt),zz,v.u,v.v};}
    V project(V v){const float d=4.6f-v.z;return {out.width*.5f+v.x*(580.f*out.width/370.f)/d,out.height*.48f-v.y*(580.f*out.width/370.f)/d,1.f/d,v.u/d,v.v/d};}
    void triangle(V aa,V bb,V cc,uint32_t rgb,const Image* texture=nullptr){
        aa=rotate(aa);bb=rotate(bb);cc=rotate(cc);
        const V a=project(aa),b=project(bb),c=project(cc);
        auto edge=[](V p,V q,float x,float y){return (q.x-p.x)*(y-p.y)-(q.y-p.y)*(x-p.x);};
        const float area=edge(a,b,c.x,c.y);if(std::abs(area)<.0001f)return;
        const V e{bb.x-aa.x,bb.y-aa.y,bb.z-aa.z},f{cc.x-aa.x,cc.y-aa.y,cc.z-aa.z};
        V n{e.y*f.z-e.z*f.y,e.z*f.x-e.x*f.z,e.x*f.y-e.y*f.x};const float len=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
        const float lighting=texture?1.f:.62f+.38f*std::abs((-.35f*n.x+.5f*n.y+.8f*n.z)/std::max(.001f,len));
        const int x0=std::max(0,int(std::floor(std::min({a.x,b.x,c.x})))),x1=std::min(out.width-1,int(std::ceil(std::max({a.x,b.x,c.x}))));
        const int y0=std::max(0,int(std::floor(std::min({a.y,b.y,c.y})))),y1=std::min(out.height-1,int(std::ceil(std::max({a.y,b.y,c.y}))));
        for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
            const float wa=edge(b,c,x+.5f,y+.5f)/area,wb=edge(c,a,x+.5f,y+.5f)/area,wc=1-wa-wb;
            if(wa<-.00001f||wb<-.00001f||wc<-.00001f)continue;
            const float inv=wa*a.z+wb*b.z+wc*c.z,d=1.f/inv;const size_t i=size_t(y)*out.width+x;if(d>=depth[i])continue;
            uint32_t p=0xff000000|((rgb&255)<<16)|(rgb&0xff00)|((rgb>>16)&255);
            if(texture&&!texture->empty()){
                float u=(wa*a.u+wb*b.u+wc*c.u)*d,v=(wa*a.v+wb*b.v+wc*c.v)*d;
                const float labelAspect=1.64f/.84f,imageAspect=float(texture->width)/texture->height;
                if(imageAspect<labelAspect)u=(u-.5f)*(labelAspect/imageAspect)+.5f;else v=(v-.5f)*(imageAspect/labelAspect)+.5f;
                const int tx=std::clamp(int(u*texture->width),0,texture->width-1),ty=std::clamp(int(v*texture->height),0,texture->height-1);
                const uint32_t tex=(u<0||u>1||v<0||v>1)?0:texture->pixels[size_t(ty)*texture->width+tx],alpha=tex>>24;
                // Composite transparent label images onto the physical sticker.
                p=0xff000000;for(int shift:{0,8,16})p|=((((tex>>shift)&255)*alpha+((out.pixels[i]>>shift)&255)*(255-alpha))/255)<<shift;
            }
            uint32_t shaded=0xff000000;for(int shift:{0,8,16})shaded|=uint32_t(std::min(255.f,float((p>>shift)&255)*lighting))<<shift;
            depth[i]=d;out.pixels[i]=shaded;
        }
    }
    void quad(V a,V b,V c,V d,uint32_t color,const Image* texture=nullptr){triangle(a,b,c,color,texture);triangle(a,c,d,color,texture);}
    void box(float x0,float y0,float z0,float x1,float y1,float z1,uint32_t color){
        quad({x0,y1,z1},{x1,y1,z1},{x1,y0,z1},{x0,y0,z1},color);
        quad({x1,y1,z0},{x0,y1,z0},{x0,y0,z0},{x1,y0,z0},color);
        quad({x0,y1,z0},{x1,y1,z0},{x1,y1,z1},{x0,y1,z1},color);
        quad({x0,y0,z1},{x1,y0,z1},{x1,y0,z0},{x0,y0,z0},color);
        quad({x0,y1,z0},{x0,y1,z1},{x0,y0,z1},{x0,y0,z0},color);
        quad({x1,y1,z1},{x1,y1,z0},{x1,y0,z0},{x1,y0,z1},color);
    }
};
}
Image renderCartridge(const Image& label,uint32_t shell,float yaw,float pitch,int width,int height){
    Raster r(width,height,yaw,pitch);
    // GBA shell profile: clipped shoulders, raised top lip, beveled perimeter.
    const std::array<V,8> rim{{{-.94f,.60f,0},{.94f,.60f,0},{1.f,.54f,0},{1.f,-.51f,0},{.94f,-.59f,0},{-.94f,-.59f,0},{-1.f,-.51f,0},{-1.f,.54f,0}}};
    for(size_t i=0;i<rim.size();++i){const auto a=rim[i],b=rim[(i+1)%rim.size()];
        V af{a.x*.965f,a.y*.96f,.155f},bf{b.x*.965f,b.y*.96f,.155f},ab{a.x,a.y,-.12f},bb{b.x,b.y,-.12f};
        r.triangle({0,0,.155f},af,bf,shell);r.triangle({0,0,-.12f},bb,ab,shell);
        r.quad(af,{a.x,a.y,.095f},{b.x,b.y,.095f},bf,shell);
        r.quad({a.x,a.y,.095f},ab,bb,{b.x,b.y,.095f},shell);
    }
    r.box(-.99f,.43f,-.12f,.99f,.535f,.19f,shell);
    // Separate molded grip ribs, recessed sticker bed and lower notch.
    for(int i=0;i<3;++i)r.box(-.97f,.30f-i*.055f,.154f,-.90f,.323f-i*.055f,.172f,shell);
    for(int i=0;i<3;++i)r.box(.90f,.30f-i*.055f,.154f,.97f,.323f-i*.055f,.172f,shell);
    // The supplied artwork is the complete label. No extra plate or border;
    // its original transparent corners reveal the cartridge shell directly.
    if(!label.empty())r.quad({-.82f,.385f,.158f,0,0},{.82f,.385f,.158f,1,0},{.82f,-.455f,.158f,1,1},{-.82f,-.455f,.158f,0,1},shell,&label);
    r.box(-.22f,-.58f,.158f,.22f,-.54f,.181f,shell);
    r.box(-.80f,-.42f,-.137f,.80f,.35f,-.12f,shell);
    r.box(-.025f,-.12f,-.16f,.025f,.00f,-.137f,0x49515a);
    return std::move(r.out);
}
Image cartridgeArtwork(const std::filesystem::path& rom,const std::filesystem::path& cache,const std::string& gameCode,const std::atomic_bool* cancelled){
    const auto custom=cache/"label.png",title=cache/"title.png";
    if(std::filesystem::exists(custom))return readImage(custom);
    const char* labelName=gameCode.starts_with("BPR")?"Fire Red Cart Art.png":gameCode.starts_with("BPG")?"Leaf Green Cart Art.png":gameCode.starts_with("AXV")?"Ruby Cart Art.png":gameCode.starts_with("AXP")?"Sapphire Cart Art.png":gameCode.starts_with("BPE")?"Emerald Cart Art.png":nullptr;
    if(labelName){const auto supplied=localAsset(std::filesystem::path("Cart Art")/labelName);if(!supplied.empty())return readImage(supplied);}
    if(std::filesystem::exists(title))return readImage(title);
#ifdef _WIN32
    std::filesystem::create_directories(cache);
    const auto input=cache/"preview-input.csv";
    {std::ofstream f(input);f<<"# gbarecomp-keyinput-v1\n0,0x3ff\n1700,0x3f7\n1710,0x3ff\n";}
    const auto exe=executableFolder()/"pokemulti_game.exe";
    const auto quote=[](const std::filesystem::path& p){return L"\""+p.wstring()+L"\"";};
    // A disposable, headless boot reads artwork from the owner's ROM. Never
    // touch their game save or ship extracted pixels in the distribution.
    std::wstring cmd=quote(exe)+L" --rom "+quote(rom)+L" --save "+quote(cache/"preview.sav")+L" --interpreter --input "+quote(input)+L" --frames 1850 --dump-png "+quote(title)+L" --quiet";
    SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
    HANDLE output=CreateFileW((cache/"preview.log").c_str(),GENERIC_WRITE,FILE_SHARE_READ,&sa,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    HANDLE in=CreateFileW(L"NUL",GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,&sa,OPEN_EXISTING,0,nullptr);
    STARTUPINFOW startup{sizeof(startup)};startup.dwFlags=STARTF_USESTDHANDLES;startup.hStdInput=in;startup.hStdOutput=output;startup.hStdError=output;
    PROCESS_INFORMATION process{};const BOOL ok=CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,cache.c_str(),&startup,&process);
    if(output!=INVALID_HANDLE_VALUE)CloseHandle(output);if(in!=INVALID_HANDLE_VALUE)CloseHandle(in);
    if(!ok)return {};
    CloseHandle(process.hThread);DWORD wait=WAIT_TIMEOUT;
    for(int elapsed=0;elapsed<45000;elapsed+=100){wait=WaitForSingleObject(process.hProcess,100);if(wait!=WAIT_TIMEOUT||(cancelled&&cancelled->load()))break;}
    if(wait!=WAIT_OBJECT_0){TerminateProcess(process.hProcess,1);WaitForSingleObject(process.hProcess,5000);CloseHandle(process.hProcess);return {};}
    DWORD code=1;GetExitCodeProcess(process.hProcess,&code);CloseHandle(process.hProcess);
    if(code||!std::filesystem::exists(title))return {};
    return readImage(title);
#else
    return {}; // The supported cartridge label is bundled; no external preview process is needed.
#endif
}
}
