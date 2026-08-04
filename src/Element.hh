#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>

enum class Element : std::uint8_t {
    Default,
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

constexpr auto cast(Element element) {
    return std::to_underlying(element);
}

constexpr auto cast(std::underlying_type_t<Element> element) {
    return static_cast<Element>(element);
}
