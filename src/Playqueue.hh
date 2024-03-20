#pragma once

#include <vector>
#include <string>

#include "Msg.hh"

struct Entry {
    unsigned id;
    unsigned duration;
    std::wstring title;
    std::string path;

    Entry(unsigned ident, unsigned dur, std::wstring songTitle,
        std::string filePath) :
        id(ident),
        duration(dur),
        title(std::move(songTitle)),
        path(std::move(filePath)) {
    }
    auto operator<=>(const Entry&) const = default;
};

class Playqueue {
  public:
    Playqueue(std::vector<Entry>&& items, unsigned playing) noexcept;
    Playqueue(Playqueue&&) = default;
    Playqueue& operator=(Playqueue&&) = default;
    Playqueue(const Playqueue&) = delete;
    Playqueue& operator=(const Playqueue&) = delete;
    ~Playqueue() = default;
    [[nodiscard]] Entry current() const noexcept;
    [[nodiscard]] bool next(bool next, bool repeat) noexcept;
    [[nodiscard]] bool prev(bool repeat) noexcept;
    void shuffle() noexcept;
    void sort() noexcept;

  private:
    unsigned playing_;
    std::vector<Entry> items_;
};
