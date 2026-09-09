#include "runtime_arm.h"
#include "runtime_bus_bridge.h"
#include "bios_hle.h"
#include "gba_bus.h"
#include "gba_ppu.h"
#include <iostream>
#include <stdexcept>
struct DispatchEntry { uint32_t addr; uint8_t thumb,resume; void(*fn)(); };
extern "C" const DispatchEntry kDispatchTable[]={{0,0,0,nullptr}};
extern "C" const unsigned kDispatchTableLen=0;
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){
    try {
        gba::GbaBus bus;gba::GbaPpu ppu;
        gbarecomp::set_active_bus(&bus);gbarecomp::set_active_ppu(&ppu);
        runtime_init(&bus);g_runtime_biosless=true;g_force_interp=1;
        gba::bios_hle_set_mode(gba::BiosHleMode::On);gba::bios_hle_boot_skip(0x08000000);
        check(g_cpu.R[13]==0x03007f00 && g_cpu.cpsr==0x1f,"boot stack/mode");
        bus.write32(0x02001000,0x12345678);bus.write32(0x03007e00,0x12345678);
        g_cpu.R[0]=3;runtime_swi(0x010000);
        check(bus.read32(0x02001000)==0,"EWRAM clear");
        check(bus.read32(0x03007e00)==0x12345678,"RegisterRamReset must preserve BIOS stack");
        // Original four-instruction guest IRQ fixture: acknowledge BIOS flag.
        bus.write32(0x03006000,0xe59f1008); // LDR r1,[pc,#8]
        bus.write32(0x03006004,0xe3a02001); // MOV r2,#1
        bus.write32(0x03006008,0xe1c120b0); // STRH r2,[r1]
        bus.write32(0x0300600c,0xe12fff1e); // BX lr
        bus.write32(0x03006010,0x03007ff8);
        bus.write32(0x03007ffc,0x03006000);
        g_cpu.R[0]=123;g_cpu.R[1]=456;g_cpu.R[2]=789;g_cpu.R[12]=999;
        const auto sp=g_cpu.R[13];g_cpu.R[15]=0x08001000;
        runtime_irq(0x08001000);
        check(bus.read16(0x03007ff8)==1,"guest IRQ ran");
        check(g_cpu.R[15]==0x08001000 && g_cpu.R[13]==sp && g_cpu.cpsr==0x1f,"IRQ return PC/stack/mode");
        check(g_cpu.R[0]==123 && g_cpu.R[1]==456 && g_cpu.R[2]==789 && g_cpu.R[12]==999,"IRQ volatile register restore");
        g_cpu.R[0]=0;g_cpu.R[1]=1;runtime_swi(0x040000);
        check(bus.read16(0x03007ff8)==0,"IntrWait consumes requested old flag");
        // A long BIOS copy must service periodic timer IRQs during its cost,
        // not collapse many timer events into one interrupt at the end.
        const uint32_t timerHandler[]={0xe59f1018,0xe5912000,0xe2822001,0xe5812000,0xe59f100c,0xe3a02040,0xe1c120b0,0xe12fff1e,0x02000100,0x04000202};
        for(unsigned i=0;i<10;++i)bus.write32(0x03006000+i*4,timerHandler[i]);
        bus.write32(0x02000100,0);bus.write16(0x04000200,0x40);bus.write16(0x04000208,1);
        bus.write16(0x0400010c,0xff00);bus.write16(0x0400010e,0xc0);
        g_cpu.R[0]=0x02002000;g_cpu.R[1]=0x02008000;g_cpu.R[2]=0x04001000;runtime_swi(0x0b0000);
        check(bus.read32(0x02000100)>=8,"long HLE service must remain interruptible");
        bus.write16(0x0400010e,0);bus.write16(0x04000200,0);bus.write16(0x04000208,0);
        bus.write32(0x03007e00,0x12345678);runtime_swi(0);
        check(bus.read32(0x03007e00)==0 && g_cpu.R[15]==0x08000000,"SoftReset state");
        runtime_shutdown();
        std::cout<<"BIOS boot, memory reset, IRQ ABI, IntrWait and SoftReset passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
