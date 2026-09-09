#include "mod_function_hooks.h"
#include "runtime_arm.h"
#include <iostream>
#include <string>
#include <stdexcept>
int mark(uint32_t address,int,ArmCpuState* cpu){cpu->R[0]=address;return 1;}
int main(){try{
    for(unsigned i=0;i<80;++i){const auto temporary="owned-name-"+std::to_string(i);if(!gba_mod_register_function_entry_plugin(temporary.c_str(),0x08000000+i*4,1,mark))throw std::runtime_error("Valid runtime hook dropped");}
    // Names belong to the registry after temporary caller strings are gone.
    if(!gba_mod_set_function_hook_enabled("owned-name-70",1))throw std::runtime_error("Hook identifier lifetime");
    ArmCpuState cpu{};if(gba_mod_function_entry(0x08000000+69*4,1,&cpu))throw std::runtime_error("Enabled wrong hook");
    if(!gba_mod_function_entry(0x08000000+70*4,1,&cpu)||cpu.R[0]!=0x08000000+70*4)throw std::runtime_error("Late hook did not run");
    gba_mod_disable_all_function_hooks();if(gba_mod_function_entry(0x08000000+70*4,1,&cpu))throw std::runtime_error("Disable hooks");
    std::cout<<"Runtime hooks beyond 32 retain owned names and exact enable/disable behavior.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
