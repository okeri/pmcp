#include "Help.hh"

Help::Help(const Keymap& keymap) noexcept {
    constexpr auto Count = static_cast<unsigned>(Action::Count);
    auto reversed = std::vector<std::vector<input::Key>>{Count};
    for (const auto& [k, v] : keymap.keymap_) {
        reversed[static_cast<unsigned>(v)].push_back(k);
    }

    data_.reserve(Count);
    auto hkline = [](const auto& items) {
        std::wstring result;
        std::wstring sep;
        for (const auto& item : items) {
            result += sep + Keymap::name(item);
            sep = L", ";
        }
        return result;
    };

    for (auto i = 0U; i < Count; ++i) {
        data_.emplace_back(hkline(reversed[i]), Keymap::description(i));
    }
}

const Help::Data& Help::help() const noexcept {
    return data_;
}
