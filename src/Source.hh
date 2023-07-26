#pragma once

#include <vector>

#include "PImpl.hh"
#include "StreamParams.hh"
#include "AudioBuffer.hh"

class Source {
    class Impl;
    PImpl<Impl, 88, 8> impl_;

  public:
    enum class Error { Ok, BadFormat, Open, Malformed, UnsupportedEncoding };
    using Buffer = std::vector<unsigned char>;
    Source() noexcept;
    Source(const Source&) = delete;
    Source(Source&&) = delete;
    Source& operator=(const Source&) = delete;
    Source& operator=(Source&&) = delete;
    unsigned fill(const AudioBuffer& buffer) noexcept;
    [[nodiscard]] long frames() const noexcept;
    Error load(const char* filename) noexcept;
    [[nodiscard]] StreamParams streamParams() const noexcept;
    long seek(long frames) noexcept;
    ~Source();
};
