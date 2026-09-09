#include "platform/text.hpp"
#include <Windows.h>
#include <stdexcept>
#include <limits>
namespace fr {
std::wstring widen(std::string_view text) {
    if (text.empty()) return {};
    if (text.size() > static_cast<size_t>(std::numeric_limits<int>::max())) throw std::runtime_error("Text is too long.");
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (!size) throw std::runtime_error("Invalid UTF-8 text.");
    std::wstring result(size, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}
std::string narrow(std::wstring_view text) {
    if (text.empty()) return {};
    if (text.size() > static_cast<size_t>(std::numeric_limits<int>::max())) throw std::runtime_error("Text is too long.");
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (!size) throw std::runtime_error("Invalid Unicode text.");
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}
}
