#pragma once

#include <utility>

struct ScrollWin {
    unsigned start;
    unsigned end;

    ScrollWin(unsigned from, unsigned contentSize, unsigned pageSize) noexcept;
};

template <class Object>
concept Scrollable = requires(Object& o) {
                         { o.up(std::declval<unsigned>()) };
                         { o.down(std::declval<unsigned>()) };
                         { o.home(std::declval<bool>()) };
                         { o.end() };
                     };

class ScrollableView {
    unsigned offset_{0};

  public:
    ScrollWin scroll(
        unsigned top, unsigned bottom, unsigned contentSize) noexcept;

    void up(unsigned offset) noexcept;
    void down(unsigned offset) noexcept;
    void home(bool force) noexcept;
    void end() noexcept;
};

class ScrollableElements {
    unsigned offset_{0};

  public:
    ScrollWin scroll(unsigned top, unsigned bottom, unsigned contentSize,
        unsigned element) noexcept;
};
