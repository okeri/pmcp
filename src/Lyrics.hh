#pragma once

#include <vector>
#include <string>

#ifdef ENABLE_GLYR
#include <mutex>
#endif

#include "channel.hh"
#include "Msg.hh"
#include "Scrollable.hh"

class Lyrics final : public ScrollableView {
    Sender<Msg> sender_;
    std::string provider_;
    std::wstring path_;
    std::vector<std::wstring> text_;
    std::wstring title_;

#ifdef ENABLE_GLYR
    std::mutex mutex_;
#endif
    bool active_{false};
    bool loaded_{false};

    void loadLyrics();

  public:
    Lyrics(Sender<Msg> progressSender, std::string provider,
        const std::string& path) noexcept;
    [[nodiscard]] const std::vector<std::wstring>& text() const noexcept;
    void setSong(const std::wstring& title) noexcept;
    void activate(bool act) noexcept;
    [[nodiscard]] const wchar_t* title() const noexcept;
};
