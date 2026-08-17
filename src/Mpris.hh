#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "PImpl.hh"
#include "Action.hh"
#include "Msg.hh"
#include "channel.hh"

struct Snapshot {
    enum class State : std::uint8_t { Stopped, Paused, Playing };

    State state{State::Stopped};
    unsigned position{0};
    unsigned duration{0};
    unsigned trackId{0};
    std::string artist;
    std::string title;
    std::string path;
    bool shuffle{false};
    bool repeat{false};
    double volume{1.};

    bool operator==(const Snapshot&) const = default;
};

class Mpris {
    class Impl;

#if defined(__aarch64__) || defined(_M_ARM64)
    static constexpr auto MprisSize = 488;
#else
    static constexpr auto MprisSize = 480;
#endif
    static constexpr auto MprisAlign = 8;
    PImpl<Impl, MprisSize, MprisAlign> impl_;

  public:
    explicit Mpris(Sender<Msg> sender) noexcept;
    Mpris(const Mpris&) = delete;
    Mpris(Mpris&&) = delete;
    Mpris& operator=(const Mpris&) = delete;
    Mpris& operator=(Mpris&&) = delete;
    ~Mpris();

    [[nodiscard]] int fd() const noexcept;
    [[nodiscard]] int notifyFd() const noexcept;
    [[nodiscard]] unsigned events() const noexcept;
    [[nodiscard]] int timeoutMs() const noexcept;

    std::optional<Action> process() noexcept;
    void flush() noexcept;
    void publish(Snapshot&& snapshot) noexcept;
};
