#include "platform/log.hpp"
#include <chrono>
#include <iomanip>
#include <stdexcept>
namespace fr {
Log::Log(const std::filesystem::path& file) {
    std::filesystem::create_directories(file.parent_path());
    if (std::filesystem::exists(file) && std::filesystem::file_size(file) > 1024 * 1024) {
        auto previous = file; previous += ".previous";
        std::error_code ec;
        std::filesystem::remove(previous, ec);
        std::filesystem::rename(file, previous);
    }
    stream_.open(file, std::ios::app);
    if (!stream_) throw std::runtime_error("Cannot open local log.");
}
void Log::write(std::string_view category, std::string_view message) {
    std::lock_guard lock(mutex_);
    const auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm time{};
#ifdef _WIN32
    localtime_s(&time, &now);
#else
    localtime_r(&now, &time);
#endif
    stream_ << std::put_time(&time, "%Y-%m-%d %H:%M:%S") << " [" << category << "] " << message << '\n';
    stream_.flush();
    if (!stream_) throw std::runtime_error("Cannot write local log.");
}
}
