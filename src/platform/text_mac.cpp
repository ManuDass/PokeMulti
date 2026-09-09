#include "platform/text.hpp"
#include <codecvt>
#include <locale>
#include <stdexcept>
namespace fr {
std::wstring widen(std::string_view text){try{return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.from_bytes(text.data(),text.data()+text.size());}catch(...){throw std::runtime_error("Invalid UTF-8 text.");}}
std::string narrow(std::wstring_view text){try{return std::wstring_convert<std::codecvt_utf8<wchar_t>>{}.to_bytes(text.data(),text.data()+text.size());}catch(...){throw std::runtime_error("Invalid Unicode text.");}}
}
