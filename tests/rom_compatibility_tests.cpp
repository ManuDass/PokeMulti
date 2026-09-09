#include "rom/rom.hpp"
#include "game/rom_layout.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try{
    const uint8_t abc[]{'a','b','c'};
    check(fr::digest(abc)=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","Platform SHA-256 mismatch");
    check(fr::digest(abc,false)=="a9993e364706816aba3e25717850c26c9cd0d89d","Platform SHA-1 mismatch");
    using R=fr::FireRedRevision;
    check(fr::identifyRevision("7862c67bdecbe21d1d69ce082ce34327e1c6ed5e",1)==R::LeafGreen_US_11,"LeafGreen identity");
    check(fr::identifyRevision("574fa542ffebb14be69902d1d36f1ec0a4afd71e",0)==R::LeafGreen_US_10,"LeafGreen original identity");
    check(fr::identifyRevision("7862c67bdecbe21d1d69ce082ce34327e1c6ed5e",0)==R::Unsupported,"Revision mismatch accepted");
    check(fr::game::romAddress(R::LeafGreen_US_11,0x080d87bc)==0x080d87a4,"LeafGreen battle text function");
    check(fr::game::romAddress(R::FireRed_US_11,0x08112450)==0x081124c8,"Late-code revision mapping");
    std::vector<uint8_t> forged(fr::FireRedRomSize);
    const std::string title="POKEMON LEAF",code="BPGE";
    std::copy(title.begin(),title.end(),forged.begin()+0xa0);std::copy(code.begin(),code.end(),forged.begin()+0xac);
    forged[0xb0]='0';forged[0xb1]='1';forged[0xb2]=0x96;forged[0xbc]=1;
    uint8_t sum=0;for(size_t i=0xa0;i<=0xbc;++i)sum=uint8_t(sum-forged[i]);forged[0xbd]=uint8_t(sum-0x19);
    check(fr::inspectRom(forged).error.find("Modified")!=std::string::npos,"Forged valid LeafGreen header bypassed hash validation");
    std::cout<<"PASS: platform hashes, LeafGreen identity, revision-specific hooks and forged-ROM rejection\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
