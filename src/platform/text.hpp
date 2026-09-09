#pragma once
#include <string>
#include <string_view>
namespace fr {
std::wstring widen(std::string_view text);
std::string narrow(std::wstring_view text);
}
