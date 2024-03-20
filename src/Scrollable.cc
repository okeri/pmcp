#include "Scrollable.hh"

ScrollWin::ScrollWin(
    unsigned from, unsigned contentSize, unsigned pageSize) noexcept :
    start(from) {
    auto bot = from + pageSize;
    if (bot > contentSize) {
        auto diff = bot - contentSize;
        if (diff < from) {
            start -= diff;
        } else {
            start = 0;
        }
    }
    end = contentSize < bot ? contentSize : bot;
}

ScrollWin ScrollableView::scroll(
    unsigned top, unsigned bottom, unsigned contentSize) noexcept {
    auto pageSize = bottom - top;
    if (contentSize > pageSize) {
        if (offset_ + pageSize > contentSize) {
            offset_ = contentSize - pageSize;
        }
    } else {
        offset_ = 0;
    }
    return {offset_, contentSize, pageSize};
}

void ScrollableView::up(unsigned offset) noexcept {
    if (offset > offset_) {
        offset_ = 0;
    } else {
        offset_ -= offset;
    }
}

void ScrollableView::down(unsigned offset) noexcept {
    offset_ += offset;
}

void ScrollableView::home([[maybe_unused]] bool force) noexcept {
    offset_ = 0;
}

void ScrollableView::end() noexcept {
    constexpr auto Bottom = 0xFFFFFFFF;
    offset_ = Bottom;
}

ScrollWin ScrollableElements::scroll(unsigned top, unsigned bottom,
    unsigned contentSize, unsigned element) noexcept {
    auto pageSize = bottom - top;
    if (element > offset_ + pageSize - 1) {
        offset_ = element - pageSize + 1;
    } else if (element < offset_) {
        offset_ = element;
    }
    return {offset_, contentSize, pageSize};
}
