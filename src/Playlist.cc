#include <filesystem>

#include <fileref.h>
#include <tag.h>

#include "Playlist.hh"
#include "utf8.hh"

namespace fs = std::filesystem;

Playlist::Entry::Entry(const std::wstring& p, bool isDir) {
    if (!isDir) {
        path = p;
        TagLib::FileRef f(utf8::convert(p).c_str());
        if (!f.isNull() && f.tag() && f.audioProperties()) {
            title = f.tag()->artist().toWString() + L" - " +
                    f.tag()->title().toCWString();
            duration = f.audioProperties()->lengthInSeconds();
        } else {
            title = fs::path(p).stem().wstring();
            duration = 0;
        }
    } else {
        title = L"../";
        path = fs::path(p).parent_path().wstring();
    }
}

Playlist::Entry::Entry(
    std::wstring t, std::wstring p, std::optional<unsigned> d) :
    title(std::move(t)), path(std::move(p)), duration(d) {
}

bool Playlist::Entry::isDir() const noexcept {
    return !duration.has_value();
}

auto Playlist::Entry::operator<=>(const Entry& other) const {
    if (duration.has_value() != other.duration.has_value()) {
        return duration.has_value() ? std::strong_ordering::greater
                                    : std::strong_ordering::less;
    }
    return title <=> other.title;
}

Playlist::Playlist(std::vector<Entry> items, const Config& config) :
    config_(config), items_(std::move(items)) {
}

Playlist::Playlist(const std::wstring& path, const Config& config) :
    config_(config) {
    listDir(path);
    home(true);
}

void Playlist::select(unsigned index) noexcept {
    selected_ = index;
}

void Playlist::setPlaying(const std::optional<unsigned>& index) noexcept {
    playing_ = index;
}

void Playlist::down(unsigned offset) noexcept {
    if (items_.empty()) {
        return;
    }
    auto max = items_.size() - 1;
    auto index =
        selected_.value_or(playing_.value_or(static_cast<unsigned>(-1)));
    if (index != static_cast<unsigned>(-1)) {
        index += offset;
        if (index > max) {
            index = max;
        }
        select(index);
    } else {
        select(0);
    }
}

void Playlist::up(unsigned offset) noexcept {
    if (items_.empty()) {
        return;
    }
    auto index = selected_.value_or(playing_.value_or(0));
    if (index < offset) {
        index = 0;
    } else {
        index -= offset;
    }
    select(index);
}

void Playlist::home(bool force) noexcept {
    if ((force || !selected_) && !items_.empty()) {
        select(0);
    }
}

void Playlist::end() noexcept {
    if (!items_.empty()) {
        select(items_.size() - 1);
    }
}

void Playlist::clear() noexcept {
    selected_ = std::nullopt;
    items_.clear();
}

void Playlist::add(std::vector<Entry> entries) {
    items_.insert(items_.end(), entries.begin(), entries.end());
    home(false);
}

const Playlist::Entry& Playlist::operator[](unsigned index) const noexcept {
    return items_.at(index);
}

unsigned Playlist::count() const noexcept {
    return items_.size();
}

unsigned Playlist::topElement() const noexcept {
    return selected_.value_or(playing_.value_or(0));
}

std::optional<unsigned> Playlist::selectedIndex() const noexcept {
    return selected_;
}

std::optional<unsigned> Playlist::playingIndex() const noexcept {
    return playing_;
}

void Playlist::remove(unsigned index) {
    items_.erase(items_.begin() + index);
    auto len = items_.size();
    if (len > 0) {
        if (index >= len) {
            select(index - 1);
        }
    } else {
        selected_ = std::nullopt;
    }
}

std::vector<Playlist::Entry> Playlist::recursiveCollect(unsigned index) {
    const auto entry = items_[index];
    if (!entry.isDir()) {
        return {entry};
    }
    std::vector<Playlist::Entry> result;
    for (auto e : fs::recursive_directory_iterator(entry.path)) {
        if (!e.is_directory() &&
            (config_.whiteList.empty() ||
                config_.whiteList.contains(e.path().extension().wstring()))) {
            result.emplace_back(e.path().wstring(), false);
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

void Playlist::listDir(const std::wstring& path) {
    clear();
    if (path != L"/") {
        items_.emplace_back(path, true);
    }
    for (auto e : fs::directory_iterator(path)) {
        if (e.is_directory()) {
            items_.emplace_back(e.path().filename().wstring() + L'/',
                e.path().wstring(), std::nullopt);
        } else if (config_.whiteList.empty() ||
                   config_.whiteList.contains(e.path().extension().wstring())) {
            items_.emplace_back(e.path().wstring(), false);
        }
    }
    std::sort(items_.begin(), items_.end());
}
