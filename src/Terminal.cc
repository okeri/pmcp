#include <iostream>
#include <cwchar>
#include <cstring>

#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>

#include "Theme.hh"
#include "Terminal.hh"
#include "input.hh"
#include "utf8.hh"

namespace {

static constexpr auto csiPrefix = L"\x1b[";

enum class CSI {
    Reset,
    ShowCursor,
    HideCursor,
    EnableWrap,
    DisableWrap,
    EnableAltScreen,
    DisableAltScreen
};

std::wostream& operator<<(std::wostream& os, CSI csi) noexcept {
    static std::vector<std::wstring> csiSequences = {
        L"0m", L"?25h", L"?25l", L"?7h", L"?7l", L"?1049h", L"?1049l"};
    os << csiPrefix
       << csiSequences[static_cast<std::underlying_type_t<CSI>>(csi)];
    return os;
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
                            return std::wstring(L"5;") + std::to_wstring(value);
                        } else if constexpr (std::is_same<ColorType,
                                                 unsigned>()) {
                            return std::wstring(L"2;") +
                                   std::to_wstring((value >> 16u) & 0xffu) +
                                   L';' +
                                   std::to_wstring((value >> 8u) & 0xffu) +
                                   L';' + std::to_wstring(value & 0xffu);
                        } else {
                            return std::wstring{};
                        }
                    },
                    color);
            };
            auto result = std::wstring{csiPrefix};
            auto fg = style.fg.index() != 0
                          ? colorString(style.fg)
                          : colorString(Theme::style(Element::Default).fg);
            if (!fg.empty()) {
                result += L";38;" + fg;
            }
            auto bg = style.bg.index() != 0
                          ? colorString(style.bg)
                          : colorString(Theme::style(Element::Default).bg);
            if (!bg.empty()) {
                result += L";48;" + bg;
            }
            for (auto i = 0u, v = 1u; i < decor.size(); ++i, v <<= 1) {
                if (style.hasDecoration(v) ||
                    Theme::style(Element::Default).hasDecoration(v)) {
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

enum class Flags : unsigned {
    None = 0,
    HasStyle = 0x1,
    Hidden = 0x2,
    Inverted = 0x4,
    NoInverted = 0x8,
    ClearDecoration = 0x10
};

constexpr Flags operator|(const Flags a, const Flags b) {
    return static_cast<Flags>(
        std::underlying_type_t<Flags>(a) | std::underlying_type_t<Flags>(b));
}

constexpr Flags& operator|=(Flags& a, const Flags b) {
    return a = a | b;
}

constexpr Flags operator&(const Flags a, const Flags b) {
    return static_cast<Flags>(
        std::underlying_type_t<Flags>(a) & std::underlying_type_t<Flags>(b));
}

constexpr Flags operator~(const Flags f) {
    return static_cast<Flags>(~std::underlying_type_t<Flags>(f));
}

constexpr Flags& operator&=(Flags& a, const Flags b) {
    return a = a & b;
}

class Cell {
    wchar_t data_{L' '};
    Element style_{Element::Default};
    Flags flags_{Flags::None};

    friend std::wostream& operator<<(
        std::wostream& os, const Cell& cell) noexcept;

  public:
    Cell() = default;

    void hide() noexcept {
        flags_ |= Flags::Hidden;
    }

    [[nodiscard]] bool has(const Flags f) const {
        return ((flags_ & f) == f);
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
        data_ = symbol;
        return *this;
    }

    [[nodiscard]] unsigned width() const noexcept {
        return wcwidth(data_);
    }

    [[nodiscard]] Element style() const noexcept {
        return style_;
    }

    static unsigned width(std::wstring_view s) noexcept {
        auto ret = wcswidth(s.begin(), s.length());
        if (ret == -1) {
            return s.length();
        }
        return ret;
    }
};

std::wostream& operator<<(std::wostream& os, const Cell& cell) noexcept {
    if (cell.has(Flags::ClearDecoration)) {
        os << csiPrefix << L"22;23;24;25;27;28;29;54;55;59;65m";
    }

    if (cell.has(Flags::HasStyle)) {
        os << styles()[cast(cell.style_)];
    }
    if (cell.has(Flags::Inverted)) {
        os << csiPrefix << L"7m";
    } else if (cell.has(Flags::NoInverted)) {
        os << csiPrefix << L"27m";
    }
    os << cell.data_;
    return os;
}

}  // namespace

class Terminal::Plane::Impl {
    unsigned left_;
    unsigned top_;
    Size size_;
    unsigned cursor_{0};
    std::vector<Cell> cells_;
    Pad pad_;
    friend class Terminal;

    [[nodiscard]] unsigned calcCellSize() const noexcept {
        return size_.rows * size_.cols;
    }

    void applyPad(unsigned len) {
        if (pad_.width > len) {
            auto count = pad_.width - len;
            for (; count != 0; --count) {
                cells_[cursor_++] = pad_.symbol;
            }
        }
    }

    unsigned putText(std::wstring_view s, unsigned cursor) noexcept {
        for (auto it = s.begin(); it < s.end(); ++it) {
            cells_[cursor] = *it;
            auto fin = cursor + cells_[cursor].width();
            for (++cursor; cursor < fin; ++cursor) {
                cells_[cursor].hide();
            }
        }
        return cursor;
    }

  public:
    explicit Impl(const Bounds& pos) noexcept :
        left_(pos.left),
        top_(pos.top),
        size_({pos.cols, pos.rows}),
        cells_(calcCellSize()),
        pad_(' ', 0) {
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
                std::fill(cells_.begin(), cells_.end(), Cell());
                break;

            case CSI::ClearDecoration:
                cells_[cursor_].clearDecoration();
                break;
        }
    }

    void operator<<(const Cursor& cursor) noexcept {
        cursor_ = cursor.y * size_.cols + cursor.x;
    }

    void operator<<(const Element element) noexcept {
        cells_[cursor_].setStyle(element);
    }

    void operator<<(unsigned value) noexcept {
        auto text = std::to_wstring(value);
        applyPad(text.length());
        for (auto c : text) {
            cells_[cursor_++] = c;
        }
    }

    void operator<<(wchar_t c) noexcept {
        cells_[cursor_++] = c;
    }

    void operator<<(std::wstring_view s) noexcept {
        cursor_ = putText(s, cursor_);
    }

    void operator<<(const Pad& pad) noexcept {
        pad_ = pad;
    }

    void box(std::wstring_view caption, Element captionStyle, const Bounds& pos,
        Element lineStyle) noexcept {
        auto width = size_.cols;
        auto calcCursor = [&width](int x, int y) { return y * width + x; };
        auto cursor = calcCursor(pos.left, pos.top);
        auto maxlen = pos.cols - 2;  // TODO: unsafe
        if (caption.length() > maxlen) {
            caption = caption.substr(0, maxlen);
        }

        auto len = Cell::width(caption);
        auto start = (maxlen - len) / 2 + pos.left;
        cells_[cursor] = Theme::lineChar(Theme::LineType::TopLeft);
        cells_[cursor].setStyle(lineStyle);
        auto end = calcCursor(start > 0 ? start - 1 : 0, pos.top);
        for (++cursor; cursor < end; ++cursor) {
            cells_[cursor] = Theme::lineChar(Theme::LineType::Horisontal);
        }

        auto right = pos.left + pos.cols;
        auto bottom = pos.top + pos.rows;
        cells_[cursor] = L' ';
        cells_[++cursor].setStyle(captionStyle);
        cursor = putText(caption, cursor);
        cells_[cursor] = L' ';
        cells_[cursor++].setStyle(lineStyle);

        end = calcCursor(right - 1, pos.top);
        for (; cursor < end; ++cursor) {
            cells_[cursor] = Theme::lineChar(Theme::LineType::Horisontal);
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
        end = calcCursor(right - 1, bottom - 1);
        for (++cursor; cursor < end; ++cursor) {
            cells_[cursor] = Theme::lineChar(Theme::LineType::Horisontal);
        }
        cells_[cursor] = Theme::lineChar(Theme::LineType::BottomRight);
    }
};

Terminal::Plane::~Plane() = default;

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

Terminal::Plane& Terminal::Plane::operator<<(const Pad& pad) noexcept {
    *impl_ << pad;
    return *this;
}

Terminal::Plane& Terminal::Plane::operator<<(wchar_t c) noexcept {
    *impl_ << c;
    return *this;
}

Terminal::Plane& Terminal::Plane::operator<<(unsigned value) noexcept {
    *impl_ << value;
    return *this;
}

Terminal::Plane& Terminal::Plane::operator<<(std::wstring_view s) noexcept {
    *impl_ << s;
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
    std::wcout << csiPrefix << top + 1 << 'H';
    const auto& cells = plane.impl_->cells_;
    for (const auto& cell : cells) {
        if (cell) {
            std::wcout << cell;
        }
    }
#endif
    return *this;
}

[[nodiscard]] Terminal::Size Terminal::size() const noexcept {
    auto ws = winsize();
    if (ioctl(0, TIOCGWINSZ, &ws) == 0) {  // NOLINT
        return {ws.ws_col, ws.ws_row};
    } else {
        return {0, 0};
    }
}

Terminal::Terminal() noexcept {
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

Terminal::Plane Terminal::createPlane(const Bounds& pos) {
    return Plane(pos);
}

void Terminal::render() const noexcept {
    std::wcout << CSI::Reset;
    std::wcout.flush();
}

Terminal& term() {
    static Terminal terminal;
    return terminal;
}

void loadTheme(const wchar_t* path) {
    Theme::load(path);
    styles();
}
