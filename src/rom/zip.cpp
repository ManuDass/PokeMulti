#include "rom/zip.hpp"
#include <zlib.h>
#include <algorithm>
#include <string>
#include <stdexcept>
namespace fr {
namespace {
constexpr size_t MaxRom = 32*1024*1024;
void require(bool test, const char* message) { if (!test) throw std::runtime_error(message); }
uint16_t u16(std::span<const uint8_t> b,size_t p) {
    require(p<=b.size() && b.size()-p>=2,"Truncated ZIP metadata.");
    return uint16_t(b[p]) | uint16_t(uint16_t(b[p+1])<<8);
}
uint32_t u32(std::span<const uint8_t> b,size_t p) { return uint32_t(u16(b,p)) | (uint32_t(u16(b,p+2))<<16); }
struct Entry { uint16_t flags{},method{}; uint32_t crc{},compressed{},size{},offset{}; std::string name; };
}
std::vector<uint8_t> readZipRom(std::span<const uint8_t> b) {
    require(b.size()>=22 && b.size()<=64*1024*1024,"ZIP size is invalid (maximum 64 MiB).");
    size_t end=b.size();
    const size_t first=b.size()>65557?b.size()-65557:0;
    for(size_t p=b.size()-22;;--p) {
        if(u32(b,p)==0x06054b50 && p+22+u16(b,p+20)==b.size()) { end=p; break; }
        if(p==first) break;
    }
    require(end!=b.size(),"ZIP end directory is missing or truncated.");
    require(u16(b,end+4)==0 && u16(b,end+6)==0 && u16(b,end+8)==u16(b,end+10),"Split ZIP archives are not supported.");
    const auto count=u16(b,end+10); const auto dirSize=u32(b,end+12),dirOffset=u32(b,end+16);
    require(count>0 && count<=1024 && dirSize!=0xFFFFFFFF && dirOffset!=0xFFFFFFFF,"ZIP64 or excessive ZIP entries are not supported.");
    require(uint64_t(dirOffset)+dirSize==end,"ZIP central directory bounds are invalid.");
    size_t p=dirOffset; Entry selected; unsigned found=0;
    for(unsigned i=0;i<count;++i) {
        require(p<=end && end-p>=46 && u32(b,p)==0x02014b50,"Invalid ZIP central directory entry.");
        const auto nameSize=u16(b,p+28), extra=u16(b,p+30),comment=u16(b,p+32);
        const size_t entrySize=46u+nameSize+extra+comment;
        require(nameSize>0 && nameSize<=2048 && entrySize<=end-p,"Invalid ZIP filename or entry length.");
        std::string name(reinterpret_cast<const char*>(b.data()+p+46),nameSize);
        require(name.find('\0')==std::string::npos,"ZIP filename contains a null byte.");
        std::string lower=name;
        for(char& c:lower) if(c>='A'&&c<='Z') c=char(c-'A'+'a');
        if(lower.ends_with(".gba")) {
            require(++found==1,"This ZIP contains multiple GBA ROMs. Use a ZIP containing exactly one .gba file.");
            selected={u16(b,p+8),u16(b,p+10),u32(b,p+16),u32(b,p+20),u32(b,p+24),u32(b,p+42),name};
            require(u16(b,p+34)==0,"Split ZIP entries are unsupported.");
        }
        p+=entrySize;
    }
    require(p==end && found==1,"The ZIP must contain exactly one .gba ROM.");
    const auto& e=selected;
    require((e.flags & 0x2041)==0,"Encrypted ZIP files are not supported.");
    require(e.method==0 || e.method==8,"Unsupported ZIP compression. Use stored or Deflate compression.");
    require(e.size>0 && e.size<=MaxRom && e.compressed<=MaxRom,"ZIP ROM exceeds the 32 MiB decompression limit.");
    const size_t local=e.offset;
    require(local<=dirOffset && dirOffset-local>=30 && u32(b,local)==0x04034b50,"ZIP local header is invalid.");
    require(u16(b,local+6)==e.flags && u16(b,local+8)==e.method,"ZIP local and central metadata disagree.");
    const auto localName=u16(b,local+26),localExtra=u16(b,local+28);
    const uint64_t start=uint64_t(local)+30+localName+localExtra;
    require(start+e.compressed<=dirOffset && localName==e.name.size(),"ZIP compressed data overlaps its directory.");
    require(std::equal(e.name.begin(),e.name.end(),reinterpret_cast<const char*>(b.data()+local+30)),"ZIP filenames disagree.");
    if(!(e.flags&8)) require(u32(b,local+14)==e.crc && u32(b,local+18)==e.compressed && u32(b,local+22)==e.size,"ZIP local sizes or checksum disagree.");
    std::vector<uint8_t> output(e.size);
    if(e.method==0) {
        require(e.size==e.compressed,"Stored ZIP entry has inconsistent sizes.");
        std::copy_n(b.data()+static_cast<size_t>(start),e.size,output.begin());
    } else {
        z_stream stream{};
        require(inflateInit2(&stream,-MAX_WBITS)==Z_OK,"Could not initialize ZIP decompression.");
        struct Cleanup { z_stream* stream; ~Cleanup(){inflateEnd(stream);} } cleanup{&stream};
        stream.next_in=const_cast<Bytef*>(b.data()+static_cast<size_t>(start)); stream.avail_in=e.compressed;
        stream.next_out=output.data(); stream.avail_out=e.size;
        const auto status=inflate(&stream,Z_FINISH);
        require(status==Z_STREAM_END && stream.total_out==e.size && stream.total_in==e.compressed,"ZIP data is corrupt or exceeds its declared size.");
    }
    require(static_cast<uint32_t>(crc32(0,output.data(),static_cast<uInt>(output.size())))==e.crc,"ZIP ROM checksum failed. The archive is damaged.");
    return output;
}
}
