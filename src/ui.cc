#include <format>

#include "ui.hh"
#include "Theme.hh"
#include "Playlist.hh"
#include "PlayerView.hh"
#include "Lyrics.hh"
#include "Help.hh"
#include "Status.hh"
#include "Spectralizer.hh"

namespace {

enum class PlaylistItemState : unsigned {
    None = 0,
    Selected = 0x1,
    Playing = 0x2,
    PlaySelected = 0x3
};

constexpr PlaylistItemState operator|(
    const PlaylistItemState a, const PlaylistItemState b) {
    return static_cast<PlaylistItemState>(
        std::underlying_type_t<PlaylistItemState>(a) |
        std::underlying_type_t<PlaylistItemState>(b));
}

constexpr PlaylistItemState operator&(
    const PlaylistItemState a, const PlaylistItemState b) {
    return static_cast<PlaylistItemState>(
        std::underlying_type_t<PlaylistItemState>(a) &
        std::underlying_type_t<PlaylistItemState>(b));
}

constexpr PlaylistItemState& operator|=(
    PlaylistItemState& a, const PlaylistItemState b) {
    return a = a | b;
}

PlaylistItemState itemState(unsigned index, bool active,
    const std::optional<unsigned>& sel,
    const std::optional<unsigned>& play) noexcept {
    auto result = PlaylistItemState::None;
    if (active && sel && *sel == index) {
        result |= PlaylistItemState::Selected;
    }
    if (play && *play == index) {
        result |= PlaylistItemState::Playing;
    }
    return result;
}

}  // namespace

namespace ui {

using Cursor = Terminal::Cursor;
using CSI = Terminal::Plane::CSI;
using State = Player::State;

constexpr auto MinWidth = 42u;

void render(Playlist& pl, Terminal::Plane& plane, const wchar_t* caption,
    bool active, bool numbers, unsigned left, unsigned top, unsigned cols,
    unsigned rows) noexcept {
    auto sel = pl.selectedIndex();
    auto play = pl.playingIndex();

    auto entryStyle = [&active, &sel, &play](auto index) {
        auto st = itemState(index, active, sel, play);
        switch (st) {
            case PlaylistItemState::Playing:
                return Element::PlaylistPlaying;
            case PlaylistItemState::Selected:
                return Element::PlaylistSelected;
            case PlaylistItemState::PlaySelected:
                return Element::PlaylistPlayingSelected;
            default:
                return Element::PlaylistEntry;
        };
    };

    auto numStyle = [&active, &sel, &play](auto index) {
        if ((itemState(index, active, sel, play) &
                PlaylistItemState::Selected) != PlaylistItemState::None) {
            return Element::PlaylistNumberSelected;
        }
        return Element::PlaylistNumber;
    };

    auto timeStyle = [&active, &sel, &play](auto index) {
        if ((itemState(index, active, sel, play) &
                PlaylistItemState::Selected) != PlaylistItemState::None) {
            return Element::PlaylistTimeSelected;
        }
        return Element::PlaylistTime;
    };

    auto numWidth = [](unsigned number) {
        auto result = 1;
        while (number > 9) {
            number /= 10;
            ++result;
        }
        return result;
    };

    auto bottom = top + rows;
    auto right = left + cols;
    auto win = pl.scroll(top + 1, bottom - 1, pl.count(), pl.topElement());
    auto numLen = numbers ? numWidth(win.end) : 0u;
    for (auto itemIndex = win.start, yCursor = top + 1; itemIndex < win.end;
         ++itemIndex, ++yCursor) {
        plane << Cursor(left + 1, yCursor);

        if (numbers) {
            plane << numStyle(itemIndex)
                  << std::format(L"{:{}} ", itemIndex + 1, numLen);
        }
        plane << entryStyle(itemIndex);

        auto element = std::wstring_view(pl[itemIndex].title);
        auto minutes =
            pl[itemIndex].duration ? *pl[itemIndex].duration / 60 : 0u;
        auto minutesLen =
            pl[itemIndex].duration ? std::max(numWidth(minutes), 2) : 0u;

        auto maxElementLen = cols - numLen - minutesLen - 10;
        if (Terminal::width(element) < maxElementLen) {
            plane << element;
        } else {
            plane << element.substr(0, maxElementLen);
        }
        plane << CSI::ClearDecoration;
        if (pl[itemIndex].duration) {
            auto dur = pl[itemIndex].duration.value();
            plane << Cursor(right - 6 - minutesLen, yCursor)
                  << timeStyle(itemIndex)
                  << std::format(L"[{:02}:{:02}]", minutes, dur % 60);
        }
    }
    plane.box(caption, Element::Title, {left, top, cols, rows},
        active ? Element::SelectedFrame : Element::Frame);
}

void render(Status& status, Terminal::Plane& plane) {
    auto stateEntry = [](const auto& state) {
        return std::visit(
            [](auto&& value) -> const Entry* {
                using Type = std::decay_t<decltype(value)>;
                if constexpr (std::is_same<Type, Player::Playing>()) {
                    return &value.entry;
                } else if constexpr (std::is_same<Type, Player::Paused>()) {
                    return &value.entry;
                } else {
                    return nullptr;
                }
            },
            state);
    };
    constexpr auto MinWidthWithTitle = 70;
    const auto& state = status.state();
    const auto& config = status.config();
    const auto& params = status.streamParams();
    const auto* current = stateEntry(state);

    auto streamWidth = [](auto fmt) -> unsigned {
        switch (fmt) {
            case SampleFormat::U8:
            case SampleFormat::S8:
                return 8;
            case SampleFormat::S16:
                return 16;
            case SampleFormat::S24:
                return 24;
            case SampleFormat::S32:
            case SampleFormat::F32:
                return 32;
            case SampleFormat::F64:
                return 64;
            default:
                return 0;
        }
    };
    plane << CSI::Clear;
    const auto& size = plane.size();
    if (size.rows < 1 || size.cols < MinWidthWithTitle) {
        return;
    }

    auto renderStatusLine = [&]() {
        auto toMinSec = [](unsigned time) -> std::pair<unsigned, unsigned> {
            return {time / 60, time % 60};
        };
        constexpr auto enabledElement = [](bool enable) {
            return enable ? Element::Enabled : Element::Disabled;
        };

        auto curTime = current != nullptr ? toMinSec(status.progress())
                                          : std::make_pair(0u, 0u);
        auto ttlTime = current != nullptr ? toMinSec(current->duration)
                                          : std::make_pair(0u, 0u);

        plane << Element::StatusState << Theme::state(state.index()) << L' '
              << Element::StatusTimeBraces << L'[' << Element::StatusCurrentTime
              << std::format(L"{:02}:{:02}", curTime.first, curTime.second)
              << Element::StatusTimeBraces << L'/' << Element::StatusTotalTime
              << std::format(L"{:02}:{:02}", ttlTime.first, ttlTime.second)
              << Element::StatusTimeBraces << L"] ";
        if (!config.options.showProgress && current != nullptr) {
            plane << Element::StatusTitle
                  << std::wstring_view(current->title)
                         .substr(0, size.cols - MinWidthWithTitle)
                  << L' ';
        }

        if (params.format != SampleFormat::None) {
            plane << Element::Enabled << std::to_wstring(params.rate / 1000)
                  << Element::Disabled << L"kHz " << Element::Enabled
                  << std::to_wstring(streamWidth(params.format))
                  << Element::Disabled << L"bit ";
        }
        plane << enabledElement(config.options.shuffle) << L"[SHUFFLE] "
              << enabledElement(config.options.repeat) << L"[REPEAT] "
              << enabledElement(config.options.next) << L"[NEXT]";

        auto vol = status.streamParams().volume;
        auto volWidth = 13;
        auto start = plane.size().cols - volWidth;
        if (start < MinWidth) {
            return;
        }
        plane << Cursor(start, 0) << Element::VolumeCaption << L"volume: "
              << Element::VolumeValue
              << std::format(L"{: >3}%", static_cast<unsigned>(vol * 100));
    };
    auto stopped = std::get_if<Player::Stopped>(&state);
    if (stopped == nullptr || stopped->error.empty()) {
        renderStatusLine();
    } else {
        plane << Element::Error << stopped->error;
    }
    if (size.rows > 1) {
        plane << Cursor(1);
        if (current != nullptr) {
            auto progress = (size.cols) * status.progress() / current->duration;
            auto printProgress = [&plane](unsigned len) {
                for (auto i = 0u; i < len; ++i) {
                    plane << L'█';
                }
            };

            constexpr auto startOffset = 1u;
            auto before = std::min(progress, startOffset);
            plane << Element::ProgressBar;
            plane << CSI::Invert;
            printProgress(before);
            plane << CSI::NoInvert;
            for (auto i = before; i < startOffset; ++i) {
                plane << L' ';
            }

            auto intitle =
                std::min(static_cast<unsigned>(current->title.length()),
                    progress > startOffset ? progress - startOffset : 0u);
            if (intitle != 0) {
                plane << std::wstring_view(current->title).substr(0, intitle);
            }
            plane << Element::Default
                  << std::wstring_view(current->title).substr(intitle);
            if (progress > startOffset + Terminal::width(current->title)) {
                plane << Element::ProgressBar << CSI::Invert;
                progress -= startOffset + Terminal::width(current->title);
                printProgress(progress);
            }
        }
        plane << CSI::Reset;
    }
}

void render(PlayerView& view, Terminal::Plane& plane) {
    plane << CSI::Clear;
    const auto& size = plane.size();
    if (size.cols < MinWidth || size.rows < 3) {
        return;
    }

    auto center = size.cols / 2;
    render(view[0], plane, view.currentPath(), !view.playlistActive(), false, 0,
        0, center, size.rows);
    render(view[1], plane, L"Playlist", view.playlistActive(), true, center, 0,
        size.cols - center, size.rows);
}

void render(Help& help, Terminal::Plane& plane) {
    const auto& data = help.help();
    const auto& size = plane.size();
    plane << CSI::Clear;
    auto win = help.scroll(1, size.rows - 1, data.size());
    for (auto itemIndex = win.start, yCursor = 1u; itemIndex < win.end;
         ++itemIndex, ++yCursor) {
        plane << Cursor(1, yCursor) << data[itemIndex].first
              << Cursor(20, yCursor)
              << std::wstring_view(data[itemIndex].second)
                     .substr(0, size.cols - 21);
    }
    plane.box(L"Key Bindings", Element::Title, {0, 0, size.cols, size.rows},
        Element::Frame);
}

void render(Lyrics& lyrics, Terminal::Plane& plane) {
    const auto& data = lyrics.text();
    const auto& size = plane.size();
    plane << CSI::Clear;
    auto win = lyrics.scroll(1, size.rows - 1, data.size());
    auto maxWidth = size.cols - 2;
    auto yCursor =
        size.rows - 2 > data.size() ? (size.rows - 2 - data.size()) / 2 + 1 : 1;
    for (auto itemIndex = win.start; itemIndex < win.end;
         ++itemIndex, ++yCursor) {
        auto textLine = data[itemIndex].substr(0, maxWidth);
        plane << Cursor(1 + (maxWidth - Terminal::width(textLine)) / 2, yCursor)
              << textLine;
    }
    plane.box(lyrics.title(), Element::Title, {0, 0, size.cols, size.rows},
        Element::Frame);
}

void render(Spectralizer& spectres, Terminal::Plane& plane) {
    plane << CSI::Clear;
    const auto& size = plane.size();
    if (size.cols < MinWidth) {
        return;
    }
    if (std::get_if<Player::Playing>(&spectres.state()) != nullptr) {
        auto spectreWidth = size.cols - 2;
        constexpr auto MinWidthBar = 4u;
        auto barWidth = MinWidthBar;
        constexpr auto MaxBarCount = 32u;
        auto barCount = (spectreWidth + 1) / (barWidth + 1);
        while (barCount > MaxBarCount) {
            ++barWidth;
            barCount = (spectreWidth + 1) / (barWidth + 1);
        }
        static std::array<wchar_t, 8> BarChars = {
            L'▁', L'▂', L'▃', L'▄', L'▅', L'▆', L'▇', L'█'};

        auto drawBar = [&plane](unsigned xstart, unsigned width,
                           unsigned maxHeight, unsigned value) {
            auto fullRows = value >> 3;
            auto lastRow = value & 0x7;
            for (auto i = 0u; i < fullRows; ++i) {
                plane << Cursor(xstart, maxHeight - i);
                for (auto c = 0u; c < width; ++c) {
                    plane << BarChars[7];
                }
            }
            plane << Cursor(xstart, maxHeight - fullRows);
            for (auto c = 0u; c < width; ++c) {
                plane << BarChars[lastRow];
            }
        };
        auto extra = spectreWidth - ((barWidth + 1) * barCount - 1);
        auto xstart = 1u + extra / 2;
        auto maxHeight = size.rows - 2;
        auto maxValue = maxHeight << 3;
        if (spectres.bins().size() == barCount) {
            for (const auto& bin : spectres.bins()) {
                drawBar(xstart, barWidth, maxHeight, maxValue * bin);
                xstart += barWidth + 1;
            }
        } else {
            for (auto i = 0u; i < barCount; ++i) {
                drawBar(xstart, barWidth, maxHeight, 1);
                xstart += barWidth + 1;
            }
            spectres.setBinCount(barCount);
        }
    }
    plane.box(
        L"", Element::Title, {0, 0, size.cols, size.rows}, Element::Frame);
}

}  // namespace ui
