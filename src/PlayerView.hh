#pragma once

#include <array>
#include <string>

#include "Playlist.hh"
#include "Config.hh"
#include "Playqueue.hh"
#include "Scrollable.hh"

class PlayerView {
    static constexpr auto ListCount = 2;

    std::wstring path_;
    std::array<Playlist, ListCount> lists_;
    bool playlistActive_;
    int playlistQueued_{-1};

    Playlist& activeList() noexcept;

  public:
    explicit PlayerView(const Config& config);
    PlayerView(const PlayerView&) = delete;
    PlayerView(PlayerView&&) = delete;
    PlayerView& operator=(const PlayerView&) = delete;
    PlayerView& operator=(PlayerView&&) = delete;
    ~PlayerView();

    void up(unsigned offset) noexcept;
    void down(unsigned offset) noexcept;
    void home(bool force) noexcept;
    void end() noexcept;
    void toggleLists() noexcept;
    void clear() noexcept;
    void delSelected() noexcept;
    void addToPlaylist() noexcept;
    void markPlaying(const std::optional<unsigned>& id) noexcept;
    std::optional<Playqueue> enter() noexcept;
    [[nodiscard]] const wchar_t* currentPath() const noexcept;
    [[nodiscard]] bool playlistActive() const noexcept;
    Playlist& operator[](unsigned index) noexcept;
};
