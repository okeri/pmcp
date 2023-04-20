#pragma once

#include <string>
#include <optional>
#include <unordered_map>

#include "input.hh"

enum class Action {
    Quit,
    Play,
    Up,
    Down,
    PgUp,
    PgDown,
    Home,
    End,
    Stop,
    Next,
    Prev,
    Pause,
    ToggleLists,
    ToggleProgress,
    ToggleTags,
    ToggleShuffle,
    ToggleRepeat,
    ToggleNext,
    ToggleLyrics,
    ToggleHelp,
    AddToPlaylist,
    Delete,
    Clear,
    FF,
    Rew,
    ResetView,
    Count
};

class Keymap {
  public:
    explicit Keymap(const std::wstring& path);
    Keymap(const Keymap&) = delete;
    Keymap(Keymap&&) = delete;
    Keymap& operator=(const Keymap&) = delete;
    Keymap& operator=(Keymap&&) = delete;
    ~Keymap() = default;

    [[nodiscard]] std::optional<Action> map(input::Key key) const noexcept;

    static std::wstring name(input::Key key);
    static const std::wstring& description(unsigned index);

  private:
    std::unordered_map<input::Key, Action> keymap_;
    friend class Help;
};
