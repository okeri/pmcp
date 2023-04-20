#pragma once

#include <string>

namespace utf8 {

std::wstring convert(const char* str);
std::wstring convert(const char* str, size_t len);
std::string convert(const std::wstring& str);

}  // namespace utf8
