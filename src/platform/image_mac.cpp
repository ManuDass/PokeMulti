#include "platform/image.hpp"
#include <ImageIO/ImageIO.h>
#include <CoreGraphics/CoreGraphics.h>
#include <mach-o/dyld.h>
#include <stdexcept>
#include <memory>
#include <algorithm>
namespace fr {
std::filesystem::path executableFolder(){uint32_t size=0;_NSGetExecutablePath(nullptr,&size);std::vector<char> path(size);if(_NSGetExecutablePath(path.data(),&size))throw std::runtime_error("Cannot locate application.");return std::filesystem::canonical(path.data()).parent_path();}
std::filesystem::path localAsset(const std::filesystem::path& relative){auto root=assetFolder();for(int i=0;i<4&&!root.empty();++i){const auto path=root/relative;if(std::filesystem::is_regular_file(path))return path;root=root.parent_path();}return {};}
Image readImage(const std::filesystem::path& path){
    auto url=CFURLCreateFromFileSystemRepresentation(nullptr,reinterpret_cast<const UInt8*>(path.c_str()),path.native().size(),false);
    if(!url)throw std::runtime_error("Invalid image path.");
    auto source=CGImageSourceCreateWithURL(url,nullptr);CFRelease(url);
    if(!source)throw std::runtime_error("Cannot decode local image.");
    auto image=CGImageSourceCreateImageAtIndex(source,0,nullptr);CFRelease(source);
    if(!image)throw std::runtime_error("Cannot decode local image.");
    const size_t w=CGImageGetWidth(image),h=CGImageGetHeight(image);
    if(!w||!h||w>4096||h>4096){CGImageRelease(image);throw std::runtime_error("Image dimensions must be 1–4096 pixels.");}
    Image result{int(w),int(h),std::vector<uint32_t>(w*h)};
    auto color=CGColorSpaceCreateDeviceRGB();
    auto context=CGBitmapContextCreate(result.pixels.data(),w,h,8,w*4,color,kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big);CGColorSpaceRelease(color);
    if(!context){CGImageRelease(image);throw std::runtime_error("Cannot allocate image pixels.");}
    CGContextSetBlendMode(context,kCGBlendModeCopy);CGContextSetInterpolationQuality(context,kCGInterpolationNone);
    CGContextDrawImage(context,CGRectMake(0,0,w,h),image);CGContextRelease(context);CGImageRelease(image);
    // The software renderer uses straight RGBA, including transparent card edges.
    for(auto& p:result.pixels){const unsigned a=p>>24;if(a&&a!=255){unsigned r=std::min(255u,(p&255)*255/a),g=std::min(255u,((p>>8)&255)*255/a),b=std::min(255u,((p>>16)&255)*255/a);p=(a<<24)|(b<<16)|(g<<8)|r;}}
    return result;
}
std::vector<uint32_t> premultipliedBgra(const Image& image){auto out=image.pixels;for(auto& p:out){const auto a=p>>24,r=(p&255)*a/255,g=((p>>8)&255)*a/255,b=((p>>16)&255)*a/255;p=(a<<24)|(r<<16)|(g<<8)|b;}return out;}
}
