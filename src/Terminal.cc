#include <format>
#include <iostream>
#include <algorithm>
#include <cstdint>
#include <utility>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>

#include "Theme.hh"
#include "Terminal.hh"

namespace {

constexpr auto CSIPrefix = L"\x1b[";

enum class CSI : std::uint8_t {
    Reset,
    ShowCursor,
    HideCursor,
    EnableWrap,
    DisableWrap,
    EnableAltScreen,
    DisableAltScreen
};

std::wostream& operator<<(std::wostream& ostream, CSI csi) noexcept {
    static std::vector<std::wstring> csiSequences = {
        L"0m", L"?25h", L"?25l", L"?7h", L"?7l", L"?1049h", L"?1049l"};
    ostream << CSIPrefix << csiSequences[std::to_underlying(csi)];
    return ostream;
}

termios* terminfo() {
    static termios info;
    return &info;
}

const std::vector<std::wstring>& styles() {
    auto init = []() {
        std::vector<std::wstring> decor = {L"1", L"2", L"3", L"4", L"5", L"9"};
        auto stringify = [&decor](const Theme::Style& style) -> std::wstring {
            auto colorString = [](const Theme::Color& color) {
                return std::visit(
                    [&](auto&& value) {
                        using ColorType = std::decay_t<decltype(value)>;
                        if constexpr (std::is_same<ColorType,
                                          unsigned char>()) {
                            return std::format(L"5;{}", value);
                        } else if constexpr (std::is_same<ColorType,
                                                 unsigned>()) {
                            constexpr auto RedShift = 16U;
                            constexpr auto BlueShift = 8U;
                            constexpr auto ColorMask = 0xFFU;
                            auto red = [](unsigned colorVal) {
                                return (colorVal >> RedShift) & ColorMask;
                            };
                            auto blue = [](unsigned colorVal) {
                                return (colorVal >> BlueShift) & ColorMask;
                            };
                            auto green = [](unsigned colorVal) {
                                return (colorVal & ColorMask);
                            };
                            return std::format(L"2;{};{};{}", red(value),
                                blue(value), green(value));
                        } else {
                            return std::wstring{};
                        }
                    },
                    color);
            };
            auto result = std::wstring{CSIPrefix};
            auto foreground =
                style.fg.index() != 0
                    ? colorString(style.fg)
                    : colorString(Theme::style(Element::Default).fg);
            if (!foreground.empty()) {
                result += L";38;" + foreground;
            }
            auto background =
                style.bg.index() != 0
                    ? colorString(style.bg)
                    : colorString(Theme::style(Element::Default).bg);
            if (!background.empty()) {
                result += L";48;" + background;
            }
            for (auto i = 0U, value = 1U; i < decor.size(); ++i, value <<= 1) {
                if (style.hasDecoration(value) ||
                    Theme::style(Element::Default).hasDecoration(value)) {
                    result += std::wstring(L";") + decor[i];
                }
            }
            return result + L'm';
        };
        std::vector<std::wstring> styles;
        auto count = cast(Element::Count);
        styles.reserve(count);

        for (auto i = 0; i < count; ++i) {
            styles.emplace_back(stringify(Theme::style(cast(i))));
        }
        return styles;
    };
    static auto result = init();
    return result;
}

enum class Flags : std::uint8_t {
    None = 0,
    HasStyle = 0x1,
    Hidden = 0x2,
    Inverted = 0x4,
    NoInverted = 0x8,
    ClearDecoration = 0x10,
    Multi = 0x20
};

constexpr Flags operator|(const Flags lhs, const Flags rhs) {
    return static_cast<Flags>(
        std::to_underlying(lhs) | std::to_underlying(rhs));
}

constexpr Flags& operator|=(Flags& lhs, const Flags rhs) {
    return lhs = lhs | rhs;
}

constexpr Flags operator&(const Flags lhs, const Flags rhs) {
    return static_cast<Flags>(
        std::to_underlying(lhs) & std::to_underlying(rhs));
}

constexpr Flags operator~(const Flags flags) {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    return static_cast<Flags>(~std::to_underlying(flags));
}

constexpr Flags& operator&=(Flags& lhs, const Flags rhs) {
    return lhs = lhs & rhs;
}

class Cell {
    wchar_t data_[2]{L' ', 0};
    Element style_{Element::Default};
    Flags flags_{Flags::None};

    friend std::wostream& operator<<(
        std::wostream& ostream, const Cell& cell) noexcept;

  public:
    Cell() = default;

    void hide() noexcept {
        flags_ |= Flags::Hidden;
    }

    [[nodiscard]] bool has(const Flags flags) const {
        return ((flags_ & flags) == flags);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return !has(Flags::Hidden);
    }

    void setStyle(Element style) noexcept {
        flags_ |= Flags::HasStyle;
        style_ = style;
    }

    void clearDecoration() noexcept {
        flags_ |= Flags::ClearDecoration;
    }

    void setInvert(bool inv) noexcept {
        if (inv) {
            flags_ |= Flags::Inverted;
            flags_ &= ~Flags::NoInverted;
        } else {
            flags_ |= Flags::NoInverted;
            flags_ &= ~Flags::Inverted;
        }
    }

    Cell& operator=(wchar_t symbol) noexcept {
        flags_ &= ~Flags::Multi;
        data_[0] = symbol;
        return *this;
    }

    void setSecond(wchar_t symbol) noexcept {
        flags_ |= Flags::Multi;
        data_[1] = symbol;
    }

    [[nodiscard]] unsigned width() const noexcept {
        return wcwidth(data_[0]) + (has(Flags::Multi) ? wcwidth(data_[1]) : 0);
    }

    [[nodiscard]] Element style() const noexcept {
        return style_;
    }

    static unsigned width(std::wstring_view str) noexcept {
        auto ret = wcswidth(str.begin(), str.length());
        if (ret == -1) {
            return str.length();
        }
        return ret;
    }
};

std::wostream& operator<<(std::wostream& ostream, const Cell& cell) noexcept {
    if (cell.has(Flags::ClearDecoration)) {
        ostream << CSIPrefix << L"22;23;24;25;27;28;29;54;55;59;65m";
    }

    if (cell.has(Flags::HasStyle)) {
        ostream << styles()[cast(cell.style_)];
    }
    if (cell.has(Flags::Inverted)) {
        ostream << CSIPrefix << L"7m";
    } else if (cell.has(Flags::NoInverted)) {
        ostream << CSIPrefix << L"27m";
    }
    ostream << cell.data_[0];
    if (cell.has(Flags::Multi)) {
        ostream << cell.data_[1];
    }
    return ostream;
}

}  // namespace

class Terminal::Plane::Impl {
    unsigned left_;
    unsigned top_;
    Size size_;
    unsigned cursor_{0};
    std::vector<Cell> cells_;
    friend class Terminal;

    [[nodiscard]] unsigned calcCellSize() const noexcept {
        return size_.rows * size_.cols;
    }

    void inc(unsigned& cursor) noexcept {
        if (cursor < cells_.size() - 1) {
            ++cursor;
        }
    }

    unsigned putText(std::wstring_view str, unsigned cursor) noexcept {
        for (const auto* it = str.begin(); it < str.end(); ++it) {
            cells_[cursor] = *it;
            auto width = cells_[cursor].width();
            if (width == 0) {
                cells_[cursor].setSecond(*(++it));
                inc(cursor);
                continue;
            }
            auto end = cursor + width;
            for (inc(cursor); cursor < end; inc(cursor)) {
                cells_[cursor].hide();
            }
        }
        return cursor;
    }

  public:
    explicit Impl(const Bounds& pos) noexcept :
        left_(pos.left),
        top_(pos.top),
        size_({.cols = pos.cols, .rows = pos.rows}),
        cells_(calcCellSize()) {
    }

    [[nodiscard]] const Size& size() const noexcept {
        return size_;
    }

    void resize(const Bounds& pos) noexcept {
        left_ = pos.left;
        top_ = pos.top;
        size_.cols = pos.cols;
        size_.rows = pos.rows;
        cells_.resize(calcCellSize(), Cell());
    }

    void operator<<(CSI csi) noexcept {
        if (cursor_ > cells_.size()) {
            cursor_ = cells_.size() - 1;
        }
        switch (csi) {
            case CSI::Reset:
                cells_[cursor_].setStyle(Element::Default);
                break;

            case CSI::Invert:
                cells_[cursor_].setInvert(true);
                break;

            case CSI::NoInvert:
                cells_[cursor_].setInvert(false);
                break;

            case CSI::Clear:
                cursor_ = 0;
                std::ranges::fill(cells_, Cell());
                break;

            case CSI::ClearDecoration:
                cells_[cursor_].clearDecoration();
                break;
        }
    }

    void operator<<(const Cursor& cursor) noexcept {
        cursor_ = std::clamp((cursor.y * size_.cols) + cursor.x, 0U,
            static_cast<unsigned>(cells_.size() - 1));
    }

    void operator<<(Element element) noexcept {
        cells_[cursor_].setStyle(element);
    }

    void operator<<(wchar_t symbol) noexcept {
        cells_[cursor_] = symbol;
        inc(cursor_);
    }

    void operator<<(std::wstring_view str) noexcept {
        cursor_ = putText(str, cursor_);
    }

    void box(std::wstring_view caption, Element captionStyle, const Bounds& pos,
        Element lineStyle) noexcept {
        auto width = size_.cols;
        if (pos.cols < 4 || pos.rows < 4) {
            return;
        }
        auto calcCursor = [&width](unsigned col, unsigned row) {
            return (row * width) + col;
        };
        auto cursor = calcCursor(pos.left, pos.top);
        auto maxlen = pos.cols - 4;
        if (caption.length() > maxlen) {
            caption = caption.substr(0, maxlen);
        }

        cells_[cursor].setStyle(lineStyle);
        cells_[cursor] = Theme::lineChar(Theme::LineType::TopLeft);
        auto right = pos.left + pos.cols;
        auto bottom = pos.top + pos.rows;

        if (!caption.empty()) {
            auto len = Cell::width(caption);
            auto captionTruncated =
                len > maxlen ? caption.substr(0, maxlen) : caption;
            auto start = ((maxlen - len) / 2) + pos.left;
            auto end = calcCursor(start > 0 ? start - 1 : 0, pos.top);
            for (++cursor; cursor < end; ++cursor) {
                cells_[cursor] = Theme::lineChar(Theme::LineType::Horisontal);
            }

            cells_[cursor] = L' ';
            cells_[++cursor].setStyle(captionStyle);
            cursor = putText(captionTruncated, cursor);
            cells_[cursor] = L' ';
            cells_[cursor++].setStyle(lineStyle);

            end = calcCursor(right - 1, pos.top);
            for (; cursor < end; ++cursor) {
                cells_[cursor] = Theme::lineChar(Theme::LineType::Horisontal);
            }
        } else {
            for (++cursor; cursor < calcCursor(pos.cols - 1, pos.top);
                ++cursor) {
                cells_[cursor] = Theme::lineChar(Theme::LineType::Horisontal);
            }
        }
        cells_[cursor] = Theme::lineChar(Theme::LineType::TopRight);

        for (auto i = pos.top + 1; i < bottom - 1; ++i) {
            cursor = calcCursor(pos.left, i);
            cells_[cursor] = Theme::lineChar(Theme::LineType::Vertical);
            cells_[cursor].setStyle(lineStyle);
            cursor = calcCursor(right - 1, i);
            cells_[cursor] = Theme::lineChar(Theme::LineType::Vertical);
            cells_[cursor].setStyle(lineStyle);
        }

        cursor = calcCursor(pos.left, bottom - 1);
        cells_[cursor].setStyle(lineStyle);
        cells_[cursor] = Theme::lineChar(Theme::LineType::BottomLeft);
        auto end = calcCursor(right - 1, bottom - 1);
        for (++cursor; cursor < end; ++cursor) {
            cells_[cursor] = Theme::lineChar(Theme::LineType::Horisontal);
        }
        cells_[cursor] = Theme::lineChar(Theme::LineType::BottomRight);
    }
};

Terminal::Plane::~Plane() = default;
Terminal::Plane::Plane(Plane&&) noexcept = default;

[[nodiscard]] Terminal::Size Terminal::Plane::size() const noexcept {
    return impl_->size();
}

void Terminal::Plane::box(std::wstring_view caption, Element captionStyle,
    const Bounds& pos, Element lineStyle) noexcept {
    impl_->box(caption, captionStyle, pos, lineStyle);
}

void Terminal::Plane::resize(const Bounds& pos) noexcept {
    impl_->resize(pos);
}

Terminal::Plane::Plane(const Bounds& pos) noexcept : impl_(pos) {
}

Terminal::Plane& Terminal::Plane::operator<<(CSI csi) noexcept {
    *impl_ << csi;
    return *this;
}

Terminal::Plane& Terminal::Plane::operator<<(const Cursor& cursor) noexcept {
    *impl_ << cursor;
    return *this;
}

Terminal::Plane& Terminal::Plane::operator<<(const Element element) noexcept {
    *impl_ << element;
    return *this;
}

Terminal::Plane& Terminal::Plane::operator<<(wchar_t symbol) noexcept {
    *impl_ << symbol;
    return *this;
}

Terminal::Plane& Terminal::Plane::operator<<(std::wstring_view str) noexcept {
    *impl_ << str;
    return *this;
}

Terminal& Terminal::operator<<(const Plane& plane) noexcept {
    auto top = plane.impl_->top_;
#ifdef FREEPLANES
    auto left = plane.impl_->left_;
    const auto& size = plane.impl_->size_;
    for (auto line = 0u, cellIndex = 0u, cellEnd = size.cols; line < size.rows;
        ++line, cellEnd += size.cols) {
        if (left > 0) {
            std::wcout << csiPrefix << top + line + 1 << ';' << left + 1 << 'H';
        } else {
            std::wcout << csiPrefix << top + line + 1 << 'H';
        }

        for (; cellIndex < cellEnd; ++cellIndex) {
            if (const auto& cell = plane.impl_->cells_[cellIndex]) {
                std::wcout << cell;
            }
        }
    }
#else
    std::wcout << CSIPrefix << top + 1 << 'H';
    const auto& cells = plane.impl_->cells_;
    for (const auto& cell : cells) {
        if (cell) {
            std::wcout << cell;
        }
    }
#endif
    return *this;
}

Terminal::Size Terminal::size() noexcept {
    auto winSize = winsize();
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    if (ioctl(0, TIOCGWINSZ, &winSize) == 0) {
        return {.cols = winSize.ws_col, .rows = winSize.ws_row};
    }
    return {.cols = 0, .rows = 0};
}

Terminal::Terminal() noexcept {
    std::ios::sync_with_stdio(false);
    auto loc = std::locale{""};
    std::wcout.imbue(loc);
    std::locale::global(loc);
    tcgetattr(STDOUT_FILENO, terminfo());
    auto ios = termios();
    cfmakeraw(&ios);
    ios.c_iflag &= ~ICRNL;
    tcsetattr(STDOUT_FILENO, TCSANOW, &ios);
    std::wcout << CSI::EnableAltScreen << CSI::HideCursor;
#ifdef FREEPLANES
    std::wcout << CSI::DisableWrap;
#endif
}

Terminal::~Terminal() {
    tcsetattr(STDOUT_FILENO, TCSANOW, terminfo());
    std::wcout << CSI::ShowCursor << CSI::DisableAltScreen;
#ifdef FREEPLANES
    std::wcout << CSI::EnableWrap;
#endif
}

Terminal::Plane Terminal::createPlane(const Bounds& pos) noexcept {
    return Plane(pos);
}

void Terminal::render() noexcept {
    std::wcout << CSI::Reset;
    std::wcout.flush();
}

unsigned Terminal::width(std::wstring_view str) noexcept {
    return Cell::width(str);
}

void Terminal::loadTheme(const char* path) {
    Theme::load(path);
    styles();
}

Terminal& term() {
    static Terminal terminal;
    return terminal;
}
