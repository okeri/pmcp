#include "Lyrics.hh"

const std::vector<std::wstring>& Lyrics::text() const noexcept {
    return text_;
}

void Lyrics::setSong(const wchar_t* song) noexcept {
    if (song != nullptr) {
        requested_ = song;
        if (active_) {
            update();
        }
    }
}

void Lyrics::activate(bool act) noexcept {
    active_ = act;
    if (act) {
        update();
    }
}

void Lyrics::update() {
    if (cached_ != requested_) {
    }
}

const wchar_t* Lyrics::title() const noexcept {
    if (cached_.empty()) {
        return L"Lyrics";
    }
    return cached_.c_str();
}
