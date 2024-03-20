#include <charconv>
#include <optional>
#include <array>

#include "Toml.hh"
#include "Theme.hh"

namespace {

std::optional<unsigned> parseInt(const std::string& strVal) {
    const auto* start = strVal.c_str();
    const auto* end = strVal.c_str() + strVal.size();
    // NOLINTBEGIN(readability-magic-numbers)
    int base = 10;
    while (isblank(*start) != 0 && start < end) {
        start++;
    }
    if (end - start > 1) {
        if (start[0] == '0' && (start[1] == 'x' || start[1] == 'X')) {
            base = 16;
            start += 2;
        } else if (start[0] == '#') {
            base = 16;
            ++start;
        }
    }

    unsigned uintVal{};
    auto err = std::from_chars(start, end, uintVal, base);
    if (err.ec == std::errc()) {
        return uintVal;
    }
    if (base == 10) {
        err = std::from_chars(start, end, uintVal, 16);
        return (
            err.ec == std::errc() ? std::make_optional(uintVal) : std::nullopt);
    }
    return {};
    // NOLINTEND(readability-magic-numbers)
}

}  // namespace

Theme::Theme() : styles_(cast(Element::Count)) {
}

void Theme::load(const char* path) {
    constexpr auto StdColorCount = 16;
    // support 8bit and 24bit format as well as some readable aliases
    std::array<std::string, StdColorCount> stdColors = {"black", "red", "green",
        "yellow", "blue", "magenta", "cyan", "white", "brightblack",
        "brightred", "brightgreen", "brightyellow", "brightblue",
        "brightmagenta", "brightcyan", "brightwhite"};

    auto parseColor = [&stdColors](const std::string& strVal) -> Color {
        if (auto* clrIt = std::find(stdColors.begin(), stdColors.end(), strVal);
            clrIt != stdColors.end()) {
            return static_cast<unsigned char>(
                std::distance(stdColors.begin(), clrIt));
        }

        if (auto uintValMaybe = parseInt(strVal)) {
            auto value = *uintValMaybe;
            constexpr auto MaxValueForByte = 0xFF;
            if (strVal.length() < 4 && value <= MaxValueForByte) {
                return static_cast<unsigned char>(value);
            }
            return value;
        }
        return {};
    };

    std::unordered_map<std::string, Decoration> decorations = {
        {"normal", Decoration::None}, {"default", Decoration::None},
        {"bold", Decoration::Bold}, {"dim", Decoration::Dim},
        {"italic", Decoration::Italic}, {"underline", Decoration::Underline},
        {"blink", Decoration::Bold}, {"strike", Decoration::Strike}};

    auto parseDecoration = [&decorations](const std::string& strVal) {
        if (auto found = decorations.find(strVal); found != decorations.end()) {
            return found->second;
        }
        auto value = parseInt(strVal);
        return value ? static_cast<Decoration>(*value) : Decoration::None;
    };

    auto parseStyle = [&parseColor, &parseDecoration](
                          const Toml& entry) -> Style {
        auto result = Style();
        if (auto foreground = entry.get<std::string>("fg")) {
            if (auto color = parseColor(*foreground); color.index() > 0) {
                result.fg = color;
            }
        }
        if (auto background = entry.get<std::string>("bg")) {
            if (auto color = parseColor(*background); color.index() > 0) {
                result.bg = color;
            }
        }

        if (auto style = entry["style"]) {
            if (!style->enumArray(
                    [&result, &parseDecoration](const std::string& value) {
                        result.decoration |= parseDecoration(value);
                    })) {
                if (auto styleVal = style->as<std::string>()) {
                    result.decoration = parseDecoration(*styleVal);
                }
            }
        }
        return result;
    };

    auto root = Toml(path);
    std::array<std::string, cast(Element::Count)> elementNames = {"default",
        "frame", "selected_frame", "window_title", "playlist_entry",
        "playlist_selected", "playlist_playing", "playlist_playing_selected",
        "playlist_number", "playlist_number_selected", "playlist_time",
        "playlist_time_selected", "volume_caption", "volume_value", "disabled",
        "enabled", "status_title", "status_state", "status_current_time",
        "status_total_time", "status_time_braces", "progress_bar", "error"};

    auto& self = instance();
    if (root) {
        for (auto i = cast(Element::Default); i < cast(Element::Count); ++i) {
            if (auto entry = root[elementNames[i]]) {
                self.styles_[i] = parseStyle(*entry);
            }
        }
    } else {
        // NOLINTBEGIN(readability-magic-numbers)
        self.styles_ = {{{}, {}, Decoration::None}, {{}, {}, Decoration::Dim},
            {static_cast<unsigned char>(4), {}, Decoration::None},
            {{}, {}, Decoration::Dim}, {{}, {}, Decoration::None},
            {{}, static_cast<unsigned char>(4), Decoration::Bold},
            {static_cast<unsigned char>(3), {},
                Decoration::Bold | Decoration::Underline},
            {{}, {}, Decoration::None},
            {static_cast<unsigned char>(6), {}, Decoration::Dim},
            {static_cast<unsigned char>(6), static_cast<unsigned char>(4),
                Decoration::None},
            {static_cast<unsigned char>(6), {}, Decoration::Dim},
            {static_cast<unsigned char>(6), static_cast<unsigned char>(4),
                Decoration::None},
            {{}, {}, Decoration::Dim},
            {static_cast<unsigned char>(3), {}, Decoration::Dim},
            {{}, {}, Decoration::Dim}, {{}, {}, Decoration::Bold},
            {static_cast<unsigned char>(4), {}, Decoration::Bold},
            {static_cast<unsigned char>(6), {}, Decoration::Bold},
            {static_cast<unsigned char>(3), {}, Decoration::Dim},
            {static_cast<unsigned char>(3), {}, Decoration::Dim},
            {{}, {}, Decoration::Dim},
            {{}, static_cast<unsigned char>(4), Decoration::Bold},
            {static_cast<unsigned char>(1), {}, Decoration::None}};
        // NOLINTEND(readability-magic-numbers)
    }

    if (auto line = root["line"]) {
        line->enumArray([&self](const std::wstring& value) {
            if (!value.empty()) {
                self.lineChars_.push_back(value[0]);
            }
        });
    }
    constexpr auto LineCharsCount = 6;
    if (self.lineChars_.size() != LineCharsCount) {
        self.lineChars_ = {L'│', L'─', L'╭', L'╮', L'╰', L'╯'};
    }

    if (auto states = root["states"]) {
        states->enumArray([&self](const std::wstring& value) {
            if (!value.empty()) {
                self.states_.push_back(value);
            }
        });
    }
    if (self.states_.size() != 3) {
        self.states_ = {L"  ", L"||", L" > "};
    }

    // some tricks
    auto inheritMaybe = [&self](Element result, Element strong, Element weak) {
        auto& resItem = self.styles_[cast(result)];
        const auto& strItem = self.styles_[cast(strong)];
        const auto& weakItem = self.styles_[cast(weak)];

        if (!resItem) {
            resItem.fg = strItem.fg;
            resItem.bg = weakItem.bg;
            resItem.decoration = strItem.decoration | weakItem.decoration;
        }
    };

    inheritMaybe(Element::PlaylistPlaying, Element::PlaylistEntry,
        Element::PlaylistEntry);
    inheritMaybe(Element::PlaylistPlayingSelected, Element::PlaylistPlaying,
        Element::PlaylistSelected);
    inheritMaybe(Element::PlaylistNumberSelected, Element::PlaylistNumber,
        Element::PlaylistSelected);
    inheritMaybe(Element::PlaylistTimeSelected, Element::PlaylistTime,
        Element::PlaylistSelected);

    // remove dim for progress, because we dont know how to proper handle it

    auto& progress = self.styles_[cast(Element::ProgressBar)];
    progress.decoration = static_cast<Decoration>(
        std::underlying_type_t<Decoration>(progress.decoration) &
        ~std::underlying_type_t<Decoration>(Decoration::Dim));
}

const std::wstring& Theme::state(unsigned index) {
    return instance().states_[index];
}

const Theme::Style& Theme::style(Element element) {
    return instance().styles_[cast(element)];
}

wchar_t Theme::lineChar(LineType lineType) {
    return instance()
        .lineChars_[static_cast<std::underlying_type_t<LineType>>(lineType)];
}

Theme& Theme::instance() {
    static Theme theme;
    return theme;
}
