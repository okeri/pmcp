#pragma once

#include <variant>

#include "channel.hh"
#include "Options.hh"
#include "Playqueue.hh"
#include "Source.hh"
#include "Sink.hh"

enum class Command {
    Next,
    Prev,
    Stop,
    Pause,
    Play,
};

class Player {
  public:
    enum : unsigned { EndOfSong = 0xffffffff, SeekSeconds = 10 };
    struct Stopped {
        std::wstring error;
    };

    struct Paused {
        Entry entry;
    };

    struct Playing {
        Entry entry;
    };

    using State = std::variant<Stopped, Paused, Playing>;

    Player(Sender<Msg> progressSender, const Options& opts, int argc,
        char* argv[]) noexcept;

    Player(const Player&) = delete;
    Player(Player&&) = delete;
    Player& operator=(const Player&) = delete;
    Player& operator=(Player&&) = delete;
    ~Player();

    const State& emit(Command cmd, std::optional<Playqueue>&& = std::nullopt);
    [[nodiscard]] const State& state() const noexcept;
    [[nodiscard]] std::optional<unsigned> currentId() const noexcept;
    [[nodiscard]] const wchar_t* currentSong() const noexcept;

    void clearQueue() noexcept;
    void updateShuffleQueue() noexcept;
    void ff() noexcept;
    void rew() noexcept;

  private:
    const Options& opts_;
    State state_;
    Source decoder_;
    Sink sink_;
    std::optional<Playqueue> queue_;
    unsigned long frames_{0};
    unsigned long framesDone_{0};
    long seekFrames_{0};

    const State& start() noexcept;
    void stop() noexcept;
    [[nodiscard]] bool stopped() const noexcept;
};
