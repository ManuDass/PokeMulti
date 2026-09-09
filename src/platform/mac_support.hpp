#pragma once
#include <filesystem>
#include <string>
#include <cstdlib>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <stdexcept>
#include <Security/Security.h>
namespace fr {
inline void secureRandom(void* bytes,size_t count){if(SecRandomCopyBytes(kSecRandomDefault,count,static_cast<uint8_t*>(bytes))!=errSecSuccess)throw std::runtime_error("Cannot generate a secure identity.");}
inline void replaceMacFile(const std::filesystem::path& from,const std::filesystem::path& to){
    int fd=open(from.c_str(),O_RDONLY|O_CLOEXEC);if(fd<0)throw std::runtime_error("Cannot open pending save.");
    const int flushed=fsync(fd);close(fd);if(flushed||rename(from.c_str(),to.c_str()))throw std::runtime_error("Cannot commit the saved file.");
    fd=open(to.parent_path().c_str(),O_RDONLY|O_CLOEXEC);if(fd>=0){fsync(fd);close(fd);}
}
struct MacFileLock {
    int fd=-1;
    ~MacFileLock(){if(fd>=0)close(fd);}
    void acquire(const std::filesystem::path& path){
        fd=open(path.c_str(),O_RDWR|O_CREAT|O_CLOEXEC,0600);
        if(fd<0||flock(fd,LOCK_EX|LOCK_NB)){if(fd>=0)close(fd);fd=-1;throw std::runtime_error("This world or save is already open, or its folder is not writable.");}
    }
};
}
