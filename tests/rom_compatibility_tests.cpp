#include "rom/rom.hpp"
#include "frontend/profile.hpp"
#include "frontend/world_store.hpp"
#include "game/shiny.hpp"
#include <fstream>
#include <random>
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
    std::mt19937 random(8427);unsigned forms=0;
    for(unsigned i=0;i<4096;++i){
        const uint32_t p=random(),ot=random();
        for(bool unown:{false,true}){
            const auto shiny=fr::game::shinyPersonality(p,ot,true,unown,random());
            check(fr::game::isShiny(shiny,ot)&&(unown||shiny%25==p%25)&&(shiny&1)==(p&1),"Shiny traits changed");
            if(unown){check(fr::game::unownForm(shiny)==fr::game::unownForm(p),"Unown form changed");forms|=1u<<fr::game::unownForm(p);}
            else check((shiny&255)==(p&255),"Gender changed");
            const auto normal=fr::game::shinyPersonality(shiny,ot,false,unown,random());
            check(!fr::game::isShiny(normal,ot)&&normal%25==shiny%25&&(normal&1)==(p&1),"Non-shiny traits changed");
            if(unown)check(fr::game::unownForm(normal)==fr::game::unownForm(p),"Non-shiny Unown form changed");
            else check((normal&255)==(p&255),"Non-shiny gender changed");
        }
    }
    check(forms==0x0fffffff,"Missing Unown form coverage");
    const auto root=std::filesystem::temp_directory_path()/("pokemulti-profile-test-"+fr::worldRandomId());
    std::filesystem::create_directories(root);
    const fr::Profile original{root/"FireRed.zip",std::string(64,'a'),"Same Trainer"};
    fr::saveProfile(root/"profile.cfg",original);
    for(const auto* name:{"identity.cfg","identity.key","checkpoint.pmsv","friends.cfg"}){std::ofstream f(root/name,std::ios::binary);f<<"unchanged-personal-data";}
    const auto leaf=fr::updateProfileRom(root/"profile.cfg",root/"LeafGreen.zip",std::string(64,'b'));
    check(leaf.playerName==original.playerName&&leaf.romSha256==std::string(64,'b'),"ROM update changed username or failed selection");
    for(const auto* name:{"identity.cfg","identity.key","checkpoint.pmsv","friends.cfg"}){std::ifstream f(root/name,std::ios::binary);std::string value((std::istreambuf_iterator<char>(f)),{});check(value=="unchanged-personal-data","ROM update changed personal data");}
    bool rejected=false;try{fr::updateProfileRom(root/"profile.cfg",root/"broken.zip","bad");}catch(const std::exception&){rejected=true;}
    check(rejected&&fr::loadProfile(root/"profile.cfg")==leaf,"Rejected ROM update damaged profile");
    check(fr::updateProfileRom(root/"profile.cfg",original.romPath,original.romSha256)==original,"Switching back changed username");
    // Only this test's newly created, random temporary directory is removed.
    std::filesystem::remove_all(root);
    std::cout<<"PASS: platform hashes, ROM identity, shiny traits and profile-preserving ROM switching\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
