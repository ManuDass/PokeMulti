#include "rom/rom.hpp"
#include "platform/text.hpp"
#include "thumb_decode.h"
#include <iostream>
#include <iomanip>
#include <stdexcept>
int wmain(int argc,wchar_t** argv){
 try{
  if(argc!=4)throw std::runtime_error("Usage: fr_rom_disasm <local.gba|zip> <hex-start> <hex-end>");
  const auto data=fr::readRom(argv[1]);if(!fr::inspectRom(data).supported())throw std::runtime_error("Unsupported ROM");
  const auto begin=std::stoul(argv[2],nullptr,16),end=std::stoul(argv[3],nullptr,16);
  if(begin<0x08000000||end>0x08000000+data.size()||end<begin||end-begin>65536||((begin|end)&1))throw std::runtime_error("Invalid bounded ROM range");
  for(auto a=begin;a<end;a+=2){auto i=a-0x08000000;const auto hw=uint16_t(data[i]|data[i+1]<<8);std::cout<<armv4t::format_ir(armv4t::ThumbDecoder::decode(hw,uint32_t(a)));
   if((hw&0xf800)==0x4800){auto literal=((a+4)&~3u)+((hw&255)*4);if(literal>=0x08000000&&literal+4<=0x08000000+data.size()){auto x=literal-0x08000000;std::cout<<" ; literal="<<std::hex<<(uint32_t(data[x])|uint32_t(data[x+1])<<8|uint32_t(data[x+2])<<16|uint32_t(data[x+3])<<24);}}
   std::cout<<'\n';
  }return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
