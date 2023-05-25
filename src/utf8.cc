#include <cstring>

#include <unistr.h>

#include "utf8.hh"

namespace utf8 {

std::wstring convert(const char* str, size_t len) {
    auto result = std::wstring();
    result.resize(len + 1, L'\0');
    u8_to_u32(reinterpret_cast<const uint8_t*>(str), len++,
        reinterpret_cast<uint32_t*>(result.data()), &len);
    result.resize(len);
    return result;
}

std::wstring convert(const char* str) {
    return convert(str, strlen(str));
}

std::wstring convert(const std::string& str) {
    return convert(str.c_str(), str.length());
}

std::string convert(const std::wstring& str) {
    auto result = std::string();
    size_t len = str.length() << 2;
    result.resize(len, '\0');
    u32_to_u8(reinterpret_cast<const uint32_t*>(str.c_str()), str.length(),
        reinterpret_cast<uint8_t*>(result.data()), &len);
    result.resize(len);
    return result;
}

}  // namespace utf8
