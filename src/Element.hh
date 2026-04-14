#pragma once

#include <type_traits>
#include <utility>

enum class Element {
    Default,
    First = Default,
    Frame,
    SelectedFrame,
    Title,
    PlaylistEntry,
    PlaylistSelected,
    PlaylistPlaying,
    PlaylistPlayingSelected,
    PlaylistNumber,
    PlaylistNumberSelected,
    PlaylistTime,
    PlaylistTimeSelected,
    VolumeCaption,
    VolumeValue,
    Disabled,
    Enabled,
    StatusTitle,
    StatusState,
    StatusCurrentTime,
    StatusTotalTime,
    StatusTimeBraces,
    ProgressBar,
    Error,
    Count
};

inline constexpr auto cast(Element element) {
    return std::to_underlying(element);
}

inline constexpr auto cast(std::underlying_type_t<Element> element) {
    return static_cast<Element>(element);
}
