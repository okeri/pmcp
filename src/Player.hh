#pragma once

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
    [[nodiscard]] const StreamParams& streamParams() const noexcept;
    [[nodiscard]] const Entry* currentEntry() const noexcept;
    [[nodiscard]] std::optional<unsigned> currentId() const noexcept;

    void setVolume(double volume) noexcept;
    void clearQueue() noexcept;
    void updateShuffleQueue() noexcept;
    void ff() noexcept;
    void rew() noexcept;
    void setBinCount(unsigned count) noexcept;

  private:
    const Options& opts_;
    State state_;
    Source decoder_;
    StreamParams params_;
    Sink sink_;
    std::optional<Playqueue> queue_;
    long frames_{0};
    std::atomic_long framesDone_{0};
    std::atomic_uint binCount_{8};
    long seekFrames_{0};
    const State& start() noexcept;
    void stop() noexcept;
    [[nodiscard]] bool stopped() const noexcept;
};
