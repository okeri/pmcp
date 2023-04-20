#pragma once

#include <vector>

#include "Keymap.hh"
#include "Scrollable.hh"

class Help final : public ScrollableView {
    using Data = std::vector<std::pair<std::wstring, std::wstring>>;
    Data data_;

  public:
    explicit Help(const Keymap& keymap);

    [[nodiscard]] const Data& help() const noexcept;
};
