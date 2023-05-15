#pragma once

#include <string_view>

#include "PImpl.hh"
#include "Element.hh"

class Terminal {
    Terminal() noexcept;
    friend Terminal& term();

  public:
    struct Cursor {
        unsigned x;
        unsigned y;

        Cursor(unsigned xx, unsigned yy) noexcept : x(xx), y(yy) {
        }

        explicit Cursor(unsigned yy) noexcept : x(0), y(yy) {
        }
    };

    struct Bounds {
        unsigned left;
        unsigned top;
        unsigned cols;
        unsigned rows;
    };

    struct Size {
        unsigned cols;
        unsigned rows;
    };

    class Plane {
        class Impl;
        PImpl<Impl, 48, 8> impl_;
        explicit Plane(const Bounds& pos) noexcept;
        friend class Terminal;

      public:
        enum class CSI { Reset, Clear, ClearDecoration, Invert, NoInvert };

        Plane(const Plane&) = delete;
        Plane(Plane&& other) noexcept = default;
        Plane& operator=(const Plane&) = delete;
        Plane& operator=(Plane&&) = delete;
        ~Plane();
        void resize(const Bounds& pos) noexcept;
        void box(std::wstring_view caption, Element captionStyle,
            const Bounds& pos, Element lineStyle) noexcept;
        [[nodiscard]] Size size() const noexcept;

        Plane& operator<<(CSI csi) noexcept;
        Plane& operator<<(const Cursor& cursor) noexcept;
        Plane& operator<<(const Element element) noexcept;
        Plane& operator<<(std::wstring_view s) noexcept;
        Plane& operator<<(wchar_t c) noexcept;
    };
    Terminal(const Terminal&) = delete;
    Terminal(Terminal&&) = delete;
    Terminal& operator=(const Terminal&) = delete;
    Terminal& operator=(Terminal&&) = delete;
    ~Terminal();

    Terminal& operator<<(const Plane& plane) noexcept;
    Plane createPlane(const Bounds& pos);
    void render() const noexcept;
    [[nodiscard]] Size size() const noexcept;
};

Terminal& term();
void loadTheme(const wchar_t* path);
