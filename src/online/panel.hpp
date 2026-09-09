#pragma once
#include "online/session.hpp"
#include <filesystem>
#include <memory>
namespace fr::online {
class Panel {
public:
    Panel(Session& session,const std::filesystem::path& data,bool visible,bool hostImmediately,const std::string& name);
    ~Panel();
    void show();
    void capture();
    void applySettings();
    int volume() const;
    uint16_t controllerKeys();
    void open(void* window,void* renderer);
    void event(const void* event);
    void render(void* texture);
    void close();
    bool capturesInput() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
std::string loadIdentity(const std::filesystem::path& folder);
}
