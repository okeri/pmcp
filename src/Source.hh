#pragma once

#include <cstdint>
#include <expected>
#include <vector>

#include "PImpl.hh"
#include "StreamParams.hh"
#include "AudioBuffer.hh"

class Source {
    class Impl;

#if defined(__aarch64__) || defined(_M_ARM64)
    static constexpr auto SourceSize = 96;
#else
    static constexpr auto SourceSize = 88;
#endif
    static constexpr auto SourceAlign = 8;
    PImpl<Impl, SourceSize, SourceAlign> impl_;

  public:
    enum class Error : std::uint8_t {
        Ok,
        BadFormat,
        Open,
        Malformed,
        UnsupportedEncoding
    };
    using Buffer = std::vector<unsigned char>;

    Source() noexcept;
    Source(const Source&) = delete;
    Source(Source&&) = delete;
    Source& operator=(const Source&) = delete;
    Source& operator=(Source&&) = delete;
    ~Source();

    unsigned fill(const AudioBuffer& buffer) noexcept;
    [[nodiscard]] long frames() const noexcept;
    [[nodiscard]] std::expected<StreamParams, Error> load(
        const char* filename) noexcept;
    long seek(long frames) noexcept;
};
