#include "platform/image.hpp"
#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <stdexcept>
namespace fr {
std::filesystem::path executableFolder(){wchar_t p[32768]{};if(!GetModuleFileNameW(nullptr,p,32768))throw std::runtime_error("Cannot locate application assets");return std::filesystem::path(p).parent_path();}
std::filesystem::path localAsset(const std::filesystem::path& relative){
    auto root=executableFolder();
    for(int i=0;i<4&&!root.empty();++i){const auto candidate=root/relative;if(std::filesystem::is_regular_file(candidate))return candidate;root=root.parent_path();}
    return {};
}
Image readImage(const std::filesystem::path& path){
    const auto init=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    struct Cleanup {bool active;~Cleanup(){if(active)CoUninitialize();}} cleanup{SUCCEEDED(init)};
    auto check=[](HRESULT hr){if(FAILED(hr))throw std::runtime_error("Cannot decode local image");};
    using Microsoft::WRL::ComPtr;
    ComPtr<IWICImagingFactory> factory;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)));
    ComPtr<IWICBitmapDecoder> decoder;check(factory->CreateDecoderFromFilename(path.c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnLoad,&decoder));
    ComPtr<IWICBitmapFrameDecode> frame;check(decoder->GetFrame(0,&frame));UINT w=0,h=0;check(frame->GetSize(&w,&h));
    if(!w||!h||w>4096||h>4096)throw std::runtime_error("Image dimensions must be between 1 and 4096 pixels");
    ComPtr<IWICFormatConverter> converter;check(factory->CreateFormatConverter(&converter));
    check(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,WICBitmapDitherTypeNone,nullptr,0,WICBitmapPaletteTypeCustom));
    Image out{int(w),int(h),std::vector<uint32_t>(size_t(w)*h)};
    check(converter->CopyPixels(nullptr,w*4,UINT(out.pixels.size()*4),reinterpret_cast<BYTE*>(out.pixels.data())));return out;
}
std::vector<uint32_t> premultipliedBgra(const Image& image){
    auto out=image.pixels;for(auto& p:out){const auto a=p>>24,r=(p&255)*a/255,g=((p>>8)&255)*a/255,b=((p>>16)&255)*a/255;p=(a<<24)|(r<<16)|(g<<8)|b;}return out;
}
}
