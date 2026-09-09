#include "frontend/world_store.hpp"
#include "platform/text.hpp"
#include "rom/rom.hpp"
#include <Windows.h>
#include <bcrypt.h>
#include <array>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
namespace fr {
namespace {
constexpr std::array<const char*,4> files{"trainer.sav","wager-wallet.cfg","released-pending.cfg","released-battle.cfg"};
constexpr std::array<size_t,4> limits{131072,4096,256000,4096};
void put(std::vector<uint8_t>& out,uint32_t n){for(unsigned i=0;i<4;++i)out.push_back(uint8_t(n>>(8*i)));}
bool unpack(std::span<const uint8_t> bytes,std::array<std::vector<uint8_t>,4>* out){
    if(bytes.size()<8+16+64||bytes.size()>WorldCheckpointLimit||std::string_view(reinterpret_cast<const char*>(bytes.data()),8)!="PMWORLD1")return false;
    const auto body=bytes.first(bytes.size()-64);
    if(digest(body)!=std::string(reinterpret_cast<const char*>(bytes.data()+body.size()),64))return false;
    size_t pos=8;
    for(unsigned i=0;i<files.size();++i){
        if(body.size()-pos<4)return false;uint32_t n=0;for(unsigned j=0;j<4;++j)n|=uint32_t(body[pos++])<<(j*8);
        if(n>body.size()-pos||n>limits[i]||(i==0&&n!=131072))return false;
        if(out)(*out)[i]={body.begin()+pos,body.begin()+pos+n};pos+=n;
    }
    return pos==body.size();
}
std::filesystem::path playerFolder(const WorldInfo& world,const std::string& player){
    if(!worldIdValid(player))throw std::runtime_error("Invalid world player identity.");
    return world.folder/"players"/player;
}
}
bool worldIdValid(const std::string& id){return id.size()==32&&std::all_of(id.begin(),id.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});}
std::string worldRandomId(){std::array<uint8_t,16> data{};if(BCryptGenRandom(nullptr,data.data(),ULONG(data.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)throw std::runtime_error("Cannot create world identity.");std::string id;for(auto c:data){id+="0123456789abcdef"[c>>4];id+="0123456789abcdef"[c&15];}return id;}
void atomicWorldFile(const std::filesystem::path& file,std::span<const uint8_t> bytes){
    std::filesystem::create_directories(file.parent_path());const auto temp=file.wstring()+L".tmp";
    HANDLE h=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(h==INVALID_HANDLE_VALUE)throw std::runtime_error("Cannot write world checkpoint.");DWORD written=0;
    const bool ok=WriteFile(h,bytes.data(),DWORD(bytes.size()),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(h);CloseHandle(h);
    if(!ok||!MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("Cannot commit world checkpoint.");
}
std::vector<uint8_t> readWorldFile(const std::filesystem::path& file,size_t limit){
    if(!std::filesystem::exists(file))return {};const auto n=std::filesystem::file_size(file);if(n>limit)throw std::runtime_error("World file exceeds size limit.");
    std::ifstream in(file,std::ios::binary);std::vector<uint8_t> bytes(size_t(n),0);if(!in.read(reinterpret_cast<char*>(bytes.data()),std::streamsize(n)))throw std::runtime_error("Cannot read world file.");return bytes;
}
WorldInfo loadWorld(const std::filesystem::path& folder){
    const auto bytes=readWorldFile(folder/"world.cfg",4096);std::istringstream in(std::string(bytes.begin(),bytes.end()));std::string magic,extra;WorldInfo w;w.folder=folder;
    if(!(in>>magic>>std::quoted(w.id)>>std::quoted(w.name)>>std::quoted(w.romHash))||(in>>extra)||magic!="PMWORLD1"||!worldIdValid(w.id)||w.romHash.size()!=64||w.name.empty()||widen(w.name).size()>32)throw std::runtime_error("Invalid world description.");return w;
}
std::vector<WorldInfo> listWorlds(const std::filesystem::path& root,const std::string& hash){
    std::vector<WorldInfo> worlds;if(!std::filesystem::exists(root/"worlds"))return worlds;
    for(const auto& entry:std::filesystem::directory_iterator(root/"worlds"))if(entry.is_directory()&&std::filesystem::exists(entry.path()/"world.cfg")){auto w=loadWorld(entry.path());if(w.romHash==hash)worlds.push_back(std::move(w));}
    std::sort(worlds.begin(),worlds.end(),[](const auto& a,const auto& b){return a.name<b.name;});return worlds;
}
WorldInfo createWorld(const std::filesystem::path& root,const std::string& name,const std::string& hash){
    if(name.empty()||widen(name).size()>32||name.find_first_not_of(' ')==std::string::npos||std::any_of(name.begin(),name.end(),[](unsigned char c){return c<32;})||hash.size()!=64)throw std::runtime_error("Choose a world name with 1-32 characters.");
    WorldInfo w{worldRandomId(),name,hash,{}};w.folder=root/"worlds"/w.id;std::ostringstream out;out<<"PMWORLD1 "<<std::quoted(w.id)<<' '<<std::quoted(name)<<' '<<std::quoted(hash)<<'\n';const auto text=out.str();atomicWorldFile(w.folder/"world.cfg",{reinterpret_cast<const uint8_t*>(text.data()),text.size()});return w;
}
std::vector<uint8_t> captureCheckpoint(const std::filesystem::path& runtime,std::span<const uint8_t> flash){
    if(flash.size()!=131072)throw std::runtime_error("A world checkpoint requires a complete FireRed flash save.");
    std::vector<uint8_t> out{'P','M','W','O','R','L','D','1'};put(out,uint32_t(flash.size()));out.insert(out.end(),flash.begin(),flash.end());
    for(unsigned i=1;i<files.size();++i){const auto bytes=readWorldFile(runtime/files[i],limits[i]);put(out,uint32_t(bytes.size()));out.insert(out.end(),bytes.begin(),bytes.end());}
    const auto hash=digest(out);out.insert(out.end(),hash.begin(),hash.end());return out;
}
bool validCheckpoint(std::span<const uint8_t> bytes){return unpack(bytes,nullptr);}
void restoreCheckpoint(const std::filesystem::path& runtime,std::span<const uint8_t> bytes){
    std::array<std::vector<uint8_t>,4> parts;if(!unpack(bytes,&parts))throw std::runtime_error("World checkpoint is incomplete or damaged.");
    for(unsigned i=0;i<parts.size();++i)atomicWorldFile(runtime/files[i],parts[i]);
}
bool WorldPlayers::authenticate(const std::string& player,const std::string& secret){
    if(!worldIdValid(secret))return false;const auto file=playerFolder(world_,player)/"identity.sha256";
    const auto expected=digest({reinterpret_cast<const uint8_t*>(secret.data()),secret.size()});auto saved=readWorldFile(file,64);
    if(saved.empty()){atomicWorldFile(file,{reinterpret_cast<const uint8_t*>(expected.data()),expected.size()});return true;}
    return std::string(saved.begin(),saved.end())==expected;
}
std::vector<uint8_t> WorldPlayers::load(const std::string& player) const{
    const auto folder=playerFolder(world_,player);auto bytes=readWorldFile(folder/"checkpoint.pmsv");if(bytes.empty())return bytes;
    if(validCheckpoint(bytes))return bytes;bytes=readWorldFile(folder/"checkpoint.previous.pmsv");if(validCheckpoint(bytes))return bytes;throw std::runtime_error("No valid checkpoint remains for this trainer.");
}
void WorldPlayers::commit(const std::string& player,std::span<const uint8_t> checkpoint){
    if(!validCheckpoint(checkpoint))throw std::runtime_error("Invalid player checkpoint.");const auto folder=playerFolder(world_,player);const auto previous=readWorldFile(folder/"checkpoint.pmsv");
    if(validCheckpoint(previous))atomicWorldFile(folder/"checkpoint.previous.pmsv",previous);atomicWorldFile(folder/"checkpoint.pmsv",checkpoint);
}
}
