#pragma once

#include <functional>

#include "PImpl.hh"
#include "StreamParams.hh"
#include "AudioBuffer.hh"

class Sink {
    class Impl;
    PImpl<Impl, 56, 8> impl_;  // NOLINT(readability-magic-numbers)

  public:
    using BufferFillRoutine = std::function<unsigned(const AudioBuffer&)>;

    Sink(Sink::BufferFillRoutine fillBuffer, int argc, char* argv[]) noexcept;
    Sink(const Sink&) = delete;
    Sink(Sink&&) = delete;
    Sink& operator=(const Sink&) = delete;
    Sink& operator=(Sink&&) = delete;
    ~Sink();

    void start(const StreamParams& params) noexcept;
    void stop() noexcept;
    void activate(bool act) const noexcept;
};
