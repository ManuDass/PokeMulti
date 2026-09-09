#pragma once
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>
namespace fr {
class Log {
public:
    explicit Log(const std::filesystem::path& file);
    void write(std::string_view category, std::string_view message);
private:
    std::ofstream stream_;
    std::mutex mutex_;
};
}
