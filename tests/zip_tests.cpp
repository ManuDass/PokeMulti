#include "rom/zip.hpp"
#include <zlib.h>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <string>
using Bytes=std::vector<uint8_t>;
void put(Bytes& b,size_t p,uint32_t v,unsigned n) { for(unsigned i=0;i<n;++i)b.at(p+i)=uint8_t(v>>(8*i)); }
Bytes archive(bool compressed, const std::string& name="fixture.gba") {
    const Bytes source(4096,0x37);
    Bytes packed=source;
    if (compressed) {
        packed.resize(compressBound(uLong(source.size())));
        z_stream z{}; deflateInit2(&z,Z_DEFAULT_COMPRESSION,Z_DEFLATED,-MAX_WBITS,8,Z_DEFAULT_STRATEGY);
        z.next_in=const_cast<Bytef*>(source.data());z.avail_in=uInt(source.size());
        z.next_out=packed.data();z.avail_out=uInt(packed.size());
        if(deflate(&z,Z_FINISH)!=Z_STREAM_END)throw std::runtime_error("fixture deflate");
        packed.resize(z.total_out); deflateEnd(&z);
    }
    const uint32_t crc=uint32_t(crc32(0,source.data(),uInt(source.size())));
    const size_t dir=30+name.size()+packed.size(), end=dir+46+name.size();
    Bytes b(end+22);
    put(b,0,0x04034b50,4);put(b,4,20,2);put(b,8,compressed?8:0,2);
    put(b,14,crc,4);put(b,18,uint32_t(packed.size()),4);put(b,22,uint32_t(source.size()),4);put(b,26,uint32_t(name.size()),2);
    std::copy(name.begin(),name.end(),b.begin()+30);std::copy(packed.begin(),packed.end(),b.begin()+30+name.size());
    put(b,dir,0x02014b50,4);put(b,dir+4,20,2);put(b,dir+6,20,2);put(b,dir+10,compressed?8:0,2);
    put(b,dir+16,crc,4);put(b,dir+20,uint32_t(packed.size()),4);put(b,dir+24,uint32_t(source.size()),4);put(b,dir+28,uint32_t(name.size()),2);
    std::copy(name.begin(),name.end(),b.begin()+dir+46);
    put(b,end,0x06054b50,4);put(b,end+8,1,2);put(b,end+10,1,2);put(b,end+12,uint32_t(end-dir),4);put(b,end+16,uint32_t(dir),4);
    return b;
}
int main() {
    int checks=0;
    const auto reject=[&](const Bytes& b){try{fr::readZipRom(b);}catch(const std::exception&){++checks;return;}throw std::runtime_error("bad ZIP accepted");};
    try {
        for(bool compressed:{false,true}) {
            auto b=archive(compressed,"nested/UPPER.GBA");
            if(fr::readZipRom(b)!=Bytes(4096,0x37))throw std::runtime_error("decompression mismatch");
            ++checks;
            auto broken=b;broken[47]^=1;reject(broken);
            broken=b;broken.pop_back();reject(broken);
            const size_t end=b.size()-22, dir=end-46-16;
            broken=b;put(broken,dir+24,0x7fffffff,4);reject(broken);
            broken=b;put(broken,dir+8,1,2);put(broken,6,1,2);reject(broken);
            broken=b;put(broken,dir+42,0xfffffff0,4);reject(broken);
            broken=b;put(broken,end+4,1,2);reject(broken);
            broken=b;put(broken,end+8,2,2);put(broken,end+10,2,2);reject(broken);
        }
        reject(archive(false,"readme.txt"));
        // Duplicate central entry points at the same local data, still two ROMs.
        auto b=archive(false);const size_t end=b.size()-22,dir=end-57;
        Bytes extra(b.begin()+dir,b.begin()+end);b.insert(b.begin()+end,extra.begin(),extra.end());
        put(b,b.size()-22+8,2,2);put(b,b.size()-22+10,2,2);put(b,b.size()-22+12,114,4);reject(b);
        std::cout<<checks<<" ZIP checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
