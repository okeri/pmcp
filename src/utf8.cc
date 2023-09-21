#include <cstring>
#include <codecvt>
#include <locale>

#include "utf8.hh"

namespace {
static std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
}  // namespace

namespace utf8 {

std::wstring convert(const char* str, size_t len) {
    return converter.from_bytes(str, str + len);
}

std::wstring convert(const char* str) {
    return converter.from_bytes(str);
}

std::wstring convert(const std::string& str) {
    return converter.from_bytes(str);
}

std::string convert(const std::wstring& str) {
    return converter.to_bytes(str);
}

}  // namespace utf8
