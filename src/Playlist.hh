#pragma once

#include <optional>
#include <vector>

#include "Scrollable.hh"
#include "Config.hh"

class Playlist final : public ScrollableElements {
  public:
    struct Entry {
        std::wstring title;
        std::wstring path;
        std::optional<unsigned> duration;

        Entry(const std::wstring& path, bool isDir);
        Entry(std::wstring t, std::wstring p, std::optional<unsigned> d);
        [[nodiscard]] bool isDir() const noexcept;
        auto operator<=>(const Entry&) const;
    };

    Playlist(std::vector<Entry> items, const Config& config);
    Playlist(const std::wstring& path, const Config& config);

    void listDir(const std::wstring& path);
    void select(unsigned index) noexcept;
    void setPlaying(const std::optional<unsigned>& index) noexcept;
    void clear() noexcept;
    void add(std::vector<Entry> entries);
    void remove(unsigned index);

    void up(unsigned offset) noexcept;
    void down(unsigned offset) noexcept;
    void home(bool force) noexcept;
    void end() noexcept;

    [[nodiscard]] unsigned topElement() const noexcept;
    [[nodiscard]] unsigned count() const noexcept;

    [[nodiscard]] std::optional<unsigned> selectedIndex() const noexcept;
    [[nodiscard]] std::optional<unsigned> playingIndex() const noexcept;
    const Entry& operator[](unsigned) const noexcept;
    std::vector<Entry> recursiveCollect(unsigned index);

  private:
    const Config& config_;
    std::optional<unsigned> selected_;
    std::optional<unsigned> playing_;
    std::vector<Entry> items_;
};
