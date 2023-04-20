#pragma once

#include <vector>
#include <string>
#include <variant>

#include "Element.hh"

class Theme {
  public:
    enum class Decoration : unsigned {
        None = 0,
        Bold = 0x01,
        Dim = 0x02,
        Italic = 0x04,
        Underline = 0x08,
        Blink = 0x10,
        Strike = 0x20
    };

    enum class LineType {
        Vertical,
        Horisontal,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight
    };

    using Color = std::variant<std::monostate, unsigned char, unsigned>;

    struct Style {
        Color fg;
        Color bg;
        Decoration decoration;

        explicit operator bool() const {
            return fg.index() != 0 || bg.index() != 0 ||
                   decoration != Decoration::None;
        }

        [[nodiscard]] constexpr bool hasDecoration(unsigned flag) const {
            return ((flag & static_cast<std::underlying_type_t<Decoration>>(
                                decoration)) == flag);
        }
    };

    Theme(const Theme&) = delete;
    Theme(Theme&&) = delete;
    Theme& operator=(const Theme&) = delete;
    Theme& operator=(Theme&&) = delete;
    ~Theme() = default;

    [[nodiscard]] static const Style& style(Element element);
    [[nodiscard]] static wchar_t lineChar(LineType t);
    [[nodiscard]] static const std::wstring& state(unsigned index);

    static void load(const wchar_t* path);

  private:
    std::vector<Style> styles_;
    std::vector<wchar_t> lineChars_;
    std::vector<std::wstring> states_;

    explicit Theme();
    static Theme& instance();
};

inline constexpr Theme::Decoration operator|(
    const Theme::Decoration a, const Theme::Decoration b) {
    return static_cast<Theme::Decoration>(
        std::underlying_type_t<Theme::Decoration>(a) |
        std::underlying_type_t<Theme::Decoration>(b));
}

inline constexpr Theme::Decoration& operator|=(
    Theme::Decoration& a, const Theme::Decoration b) {
    return a = a | b;
}
