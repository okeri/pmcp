#pragma once

#include <vector>
#include <string>

#include "Scrollable.hh"

class Lyrics final : public ScrollableView {
    std::vector<std::wstring> text_;
    std::wstring cached_;
    std::wstring requested_;
    bool active_{false};

    void update();

  public:
    [[nodiscard]] const std::vector<std::wstring>& text() const noexcept;
    void setSong(const wchar_t* song) noexcept;
    void activate(bool act) noexcept;
    [[nodiscard]] const wchar_t* title() const noexcept;
};
