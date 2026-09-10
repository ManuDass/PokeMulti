#pragma once
#ifdef FR_TEST_HARNESS
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
namespace fr::game::performance {
inline std::ofstream output;
inline uint64_t frame=0;
inline void open(const std::filesystem::path& folder){if(std::getenv("POKEMULTI_PROFILE")){output.open(folder/"performance.csv");output<<"frame,section,milliseconds,address\n";}}
struct Scope {
    const char* name;uint32_t address;std::chrono::steady_clock::time_point start;
    Scope(const char* n,uint32_t a=0):name(n),address(a),start(output.is_open()?std::chrono::steady_clock::now():std::chrono::steady_clock::time_point{}){}
    ~Scope(){if(output.is_open()){const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();if(ms>=3){output<<frame<<','<<name<<','<<ms<<','<<std::hex<<address<<std::dec<<'\n';output.flush();}}}
};
}
#else
namespace fr::game::performance {struct Scope {constexpr Scope(const char*,unsigned=0){}};}
#endif
