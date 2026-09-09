#pragma once
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include "platform/mac_support.hpp"
#endif
namespace fr {
inline void replaceText(const std::filesystem::path& path,const std::string& content){
    std::filesystem::create_directories(path.parent_path());
    const auto temp=path.wstring()+L".tmp";
    {std::ofstream f(temp,std::ios::binary|std::ios::trunc);f<<content;f.flush();if(!f)throw std::runtime_error("Could not write the account record");}
#ifdef _WIN32
    DWORD error=0;
    for(unsigned retry=0;retry<50;++retry){
        if(MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))return;
        error=GetLastError();if(error!=ERROR_SHARING_VIOLATION&&error!=ERROR_ACCESS_DENIED)break;
        Sleep(10); // File scanners/readers may briefly hold the destination on Windows.
    }
    throw std::runtime_error("Could not commit the account record (Windows error "+std::to_string(error)+")");
#else
    replaceMacFile(temp,path);
#endif
}
}
