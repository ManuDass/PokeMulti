// Included after world_online.inc; native hooks run only on the game thread.
struct ShinyCreation {uint32_t mon=0,trainer=0,rate=DefaultShinyRate;bool unown=false;} shinyCreation;
bool fixedWildCreation=false;
bool callerWithin(ArmCpuState* cpu,uint32_t canonical,uint32_t size){
    const auto caller=cpu->R[14]&~1u,entry=gameAddress(canonical);return caller>=entry&&caller<entry+size;
}
int shinyMonHook(uint32_t,int,ArmCpuState* cpu){
    // A nature/form-selected wild PID is newly generated. Fixed PIDs used by
    // eggs, existing roamers, trades and copied teams must remain unchanged.
    fixedWildCreation=callerWithin(cpu,0x0803dd98,0x66)||callerWithin(cpu,0x080829fc,0x8c);
    return 0;
}
int shinyBoxHook(uint32_t,int,ArmCpuState* cpu){
    shinyCreation={};const bool fixedWild=fixedWildCreation;fixedWildCreation=false;
    const auto rate=room?room->status().shinyRate:DefaultShinyRate;
    const uint32_t stack=cpu->R[13],save=r32(0x0300500c);
    if(rate==DefaultShinyRate||rate<1||rate>DefaultShinyRate||!validRam(stack,16)||!validRam(save,14)||!validRam(cpu->R[0],80))return 0;
    // OT_ID_PLAYER_ID only. Trainer opponents and preset gifts retain the ROM's rules.
    if(r32(stack+8)!=0||(r32(stack)!=0&&!fixedWild))return 0;
    shinyCreation={cpu->R[0],r32(save+10),rate,cpu->R[1]==201};return 0;
}
int shinyDataHook(uint32_t,int,ArmCpuState* cpu){
    if(!shinyCreation.mon||cpu->R[0]!=shinyCreation.mon||cpu->R[1]!=0||!callerWithin(cpu,0x0803dac4,0x2d4))return 0;
    const auto creation=shinyCreation;shinyCreation={};const auto value=cpu->R[2];if(!validRam(value,4))return 0;
    // Separate randomness leaves native encounter, IV and movement RNG untouched.
    static std::mt19937 generator(std::random_device{}());
    const bool shiny=std::uniform_int_distribution<uint32_t>(1,creation.rate)(generator)==1;
    const uint32_t before=r32(value),after=shinyPersonality(before,creation.trainer,shiny,creation.unown,generator());
    bus()->write32(value,after);
#ifdef FR_TEST_HARNESS
    std::printf("SHINY_CREATE rate=%u shiny=%u pid=%08x original=%08x ot=%08x\n",creation.rate,unsigned(isShiny(after,creation.trainer)),after,before,creation.trainer);
#endif
    return 0;
}
void registerShinyHooks(){
    shinyCreation={};fixedWildCreation=false;
    for(const auto& hook:std::vector<std::pair<uint32_t,decltype(&shinyMonHook)>>{{0x0803da54,shinyMonHook},{0x0803dac4,shinyBoxHook},{0x080404d0,shinyDataHook}}){
        const auto name="pokemulti.shiny-"+std::to_string(hook.first);
        gba_mod_register_function_entry_plugin(name.c_str(),gameAddress(hook.first),1,hook.second);gba_mod_set_function_hook_enabled(name.c_str(),1);
    }
}
