#pragma once

#include <optional>
#include <vector>
#include <string>

#include "Scrollable.hh"

class Playlist final : public ScrollableElements {
  public:
    struct Entry {
        std::wstring title;
        std::string path;
        std::optional<unsigned> duration;

        Entry(const std::string& filePath, bool isDir);
        Entry(std::wstring songTitle, std::string filePath,
            std::optional<unsigned> songDuration);
        [[nodiscard]] bool isDir() const noexcept;
        auto operator<=>(const Entry& other) const;
    };

    explicit Playlist(std::vector<Entry> items);

    static Playlist scan(const std::string& path);
    static Playlist load(const std::string& path);
    static std::vector<Entry> collect(const std::string& path);
    void save(const std::string& path = "");

    void listDir(const std::string& path);
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

    const Entry& operator[](unsigned index) const noexcept;
    std::vector<Entry> recursiveCollect(unsigned index);

  private:
    std::optional<unsigned> selected_;
    std::optional<unsigned> playing_;
    std::vector<Entry> items_;
};
