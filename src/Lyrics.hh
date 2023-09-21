#pragma once

#include <vector>
#include <string>

#include "channel.hh"
#include "Msg.hh"
#include "Scrollable.hh"

class Lyrics final : public ScrollableView {
    Sender<Msg> sender_;
    std::wstring path_;
    std::vector<std::wstring> text_;
    std::wstring title_;

    bool active_{false};
    bool loaded_{false};

    void loadLyrics();

  public:
    explicit Lyrics(Sender<Msg> progressSender, const std::string& path);
    [[nodiscard]] const std::vector<std::wstring>& text() const noexcept;
    void setSong(const std::wstring& title) noexcept;
    void activate(bool act) noexcept;
    [[nodiscard]] const wchar_t* title() const noexcept;
};
