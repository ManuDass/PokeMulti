#include "gba_io.h"
#include <iostream>
#include <stdexcept>
struct DispatchEntry {uint32_t addr;uint8_t thumb,resume;void(*fn)();};
extern "C" const DispatchEntry kDispatchTable[]={{0,0,0,nullptr}};
extern "C" const unsigned kDispatchTableLen=0;
struct Cable final:gba::SerialLink {
    bool slave=false,pending=false,fail=false,sync=false;
    bool connected() const override{return true;}
    bool synchronizeWrites() const override{return sync;}
    uint16_t control(uint16_t v)override{return uint16_t((v&~0x3c)|8|(slave?0x14:0));}
    bool start(uint16_t word,std::array<uint16_t,4>& result)override{result={word,0xabcd,0xffff,0xffff};return !fail;}
    bool poll(uint16_t word,std::array<uint16_t,4>& result)override{if(!pending)return false;pending=false;result={0x1234,word,0xffff,0xffff};return true;}
};
struct TimedCable final:gba::SerialLink {
    unsigned requested=0,completed=0;bool irqEnabled=true;
    bool connected() const override{return true;}
    bool clocked() const override{return true;}
    bool clockReady() const override{return irqEnabled;}
    uint16_t control(uint16_t v)override{return uint16_t((v&~0x3c)|0x1c);}
    bool start(uint16_t,std::array<uint16_t,4>&)override{return false;}
    bool poll(uint16_t,std::array<uint16_t,4>&)override{return false;}
    bool request(uint32_t& elapsed)override{if(requested==2)return false;elapsed=requested++?20000:0;return true;}
    bool tryRequest(uint32_t& elapsed)override{return request(elapsed);}
    bool respond(uint16_t w,std::array<uint16_t,4>& out)override{++completed;out={0x1234,w,0xffff,0xffff};return true;}
};
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
int main(){
    try{
        gba::GbaIo io;Cable cable;io.set_serial_link(&cable);
        io.write16(0x128,0x6003);io.write16(0x12a,0x1234);io.write16(0x128,0x6083);
        check(io.read16(0x128)&0x80,"master transfer is busy");
        check(!(io.if_reg()&0x80),"serial IRQ must wait for transfer completion");
        io.tick_sio(5755);
        check(!(io.read16(0x128)&0x80),"master completion clears busy");
        check(io.read16(0x120)==0x1234&&io.read16(0x122)==0xabcd&&io.read32(0x124)==0xffffffff,"multiplayer receive registers");
        check(io.if_reg()&0x80,"serial IRQ raised");
        io.write16(0x202,0x80);check(!(io.if_reg()&0x80),"serial IRQ acknowledgement");
        cable.slave=true;io.write16(0x128,0x6003);io.write16(0x12a,0x5678);cable.pending=true;
        io.tick_sio(1);check(io.read16(0x128)&0x80,"slave started by partner");
        io.tick_sio(5755);check(io.read16(0x122)==0x5678&&io.read16(0x120)==0x1234,"slave receives same ordered words");
        check((io.read8(0x128)&0x3c)==0x1c,"slave ID and SI/SD bits in byte reads");
        cable.sync=true;io.write16(0x128,0x6003);cable.pending=true;io.write16(0x12a,0x4321);
        check(io.read16(0x128)&0x80,"synchronized slave starts from a fresh send-word write");
        io.tick_sio(5755);check(io.read16(0x122)==0x4321,"synchronized slave received its submitted word");
        cable.sync=false;
        cable.slave=false;cable.fail=true;io.write16(0x128,0x6003);io.write16(0x128,0x6083);io.tick_sio(5755);
        check(io.read16(0x128)&0x40,"link failure exposed to guest");
        cable.fail=false;io.write16(0x128,uint16_t(io.read16(0x128)|0x80));io.tick_sio(5755);
        check(!(io.read16(0x128)&0x40),"a successful transfer clears the preceding hardware error");
        {
            gba::GbaIo timedIo;TimedCable timed;timedIo.set_serial_link(&timed);timedIo.write16(0x128,0x6003);timedIo.write16(0x12a,0x1111);
            timedIo.tick_sio(1);check(timed.completed==1&&(timedIo.read16(0x128)&0x80),"first timed transfer establishes the clock offset");
            timedIo.tick_sio(5755);timedIo.write16(0x202,0x80);
            timedIo.tick_sio(1);check(timedIo.cycles_until_next_sio_event()==14244,"next serial start follows master's elapsed cycles");
            timedIo.write16(0x12a,0x2222);timedIo.tick_sio(14244);
            check(timed.completed==2&&(timedIo.read16(0x128)&0x80)&&!(timedIo.if_reg()&0x80),"cycles before transfer start must not consume transfer duration");
            timedIo.tick_sio(5754);check(timedIo.read16(0x128)&0x80,"timed transfer retains its full duration");
            timedIo.tick_sio(1);check((timedIo.if_reg()&0x80)&&timedIo.read16(0x122)==0x2222,"timed transfer samples the fresh word and delivers its IRQ");
        }
        {
            gba::GbaIo polled;TimedCable wire;wire.irqEnabled=false;polled.set_serial_link(&wire);
            polled.write16(0x128,0x2003);polled.write16(0x12a,0x9876);
            polled.tick_sio(1);check(wire.completed==1&&(polled.read16(0x128)&0x80),"IRQ-disabled slave still shifts serial data");
            polled.tick_sio(5755);
            check(polled.read16(0x122)==0x9876&&!(polled.read16(0x128)&0x80)&&!(polled.if_reg()&0x80),"polled transfer completes without raising a disabled IRQ");
        }
        std::cout<<"Virtual cable register, role, timing, IRQ and failure checks passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
