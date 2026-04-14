#include <filesystem>
#include <fstream>
#include <charconv>

#include <fileref.h>
#include <tag.h>

#include "Config.hh"
#include "Playlist.hh"
#include "utf8.hh"

namespace {

constexpr auto MaxSelected = 0xffffffff;

}  // namespace

namespace fs = std::filesystem;

Playlist::Entry::Entry(const std::string& filePath, bool isDir) {
    if (!isDir) {
        path = filePath;
        const TagLib::FileRef file(filePath.c_str());
        if (!file.isNull() && file.tag() != nullptr &&
            file.audioProperties() != nullptr) {
            title = file.tag()->artist().toWString() + L" - " +
                    file.tag()->title().toCWString();
            duration = file.audioProperties()->lengthInSeconds();
        } else {
            title = fs::path(filePath).stem().wstring();
            duration = 0;
        }
    } else {
        title = L"../";
        path = fs::path(filePath).parent_path().string();
    }
}

Playlist::Entry::Entry(std::wstring songTitle, std::string filePath,
    std::optional<unsigned> songDuration) :
    title(std::move(songTitle)),
    path(std::move(filePath)),
    duration(songDuration) {
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

Playlist::Playlist(std::vector<Entry> items) : items_(std::move(items)) {
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
    auto maxIndex = items_.size() - 1;
    auto index = selected_.value_or(playing_.value_or(MaxSelected));
    if (index != MaxSelected) {
        index += offset;
        if (index > maxIndex) {
            index = maxIndex;
        }
    } else {
        index = 0;
    }
    select(index);
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
    playing_ = std::nullopt;
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
    for (const auto& entry : fs::recursive_directory_iterator(entry.path)) {
        if (!entry.is_directory() &&
            (config().whiteList.empty() ||
                config().whiteList.contains(
                    entry.path().extension().string()))) {
            result.emplace_back(entry.path().string(), false);
        }
    }
    std::ranges::sort(result);
    return result;
}

std::vector<Playlist::Entry> Playlist::collect(const std::string& path) {
    std::vector<Playlist::Entry> result;
    if (path != "/") {
        result.emplace_back(path, true);
    }
    for (const auto& entry : fs::directory_iterator(path)) {
        if (entry.is_directory()) {
            result.emplace_back(entry.path().filename().wstring() + L'/',
                entry.path().string(), std::nullopt);
        } else if (config().whiteList.empty() ||
                   config().whiteList.contains(
                       entry.path().extension().string())) {
            result.emplace_back(entry.path().string(), false);
        }
    }
    std::ranges::sort(result);
    return result;
}

void Playlist::listDir(const std::string& path) {
    items_ = collect(path);
}

Playlist Playlist::scan(const std::string& path) {
    return Playlist(collect(path));
}

Playlist Playlist::load(const std::string& path) {
    std::vector<Entry> entries;
    if (auto input = std::ifstream(path)) {
        std::string line;
        unsigned dur{};
        Entry entry(path, true);
        while (std::getline(input, line)) {
            constexpr auto ExtInfLen = 8;
            if (line.length() > ExtInfLen &&
                line.compare(0, ExtInfLen, "#EXTINF:") == 0) {
                if (auto end = line.find(','); end != std::string::npos) {
                    std::from_chars(
                        line.c_str() + ExtInfLen, line.c_str() + end, dur);
                    entry.duration = dur;
                    entry.title = utf8::convert(line.substr(end + 1));
                }
            } else if (!line.empty() && line[0] != '#') {
                entry.path = line;
                entries.push_back(entry);
            }
        }
    }
    return Playlist(entries);
}

void Playlist::save(const std::string& path) {
    auto filename = path.empty() ? config().playlistPath : path;
    std::ofstream output(filename);
    output << "#EXTM3U"
           << "\n";
    for (const auto& item : items_) {
        if (item.duration.has_value()) {
            output << "#EXTINF:" << *item.duration << ","
                   << utf8::convert(item.title) << "\n";
            output << item.path << "\n";
        }
    }
}

std::optional<std::pair<unsigned, unsigned>> Playlist::move(bool moveUp) noexcept {
    if (items_.size() < 2) {
        return {};
    }
    auto oldPos = selected_.value_or(playing_.value_or(MaxSelected));
    if (oldPos == MaxSelected || (moveUp && oldPos == 0) ||
        (!moveUp && oldPos == items_.size() - 1)) {
        return {};
    }
    auto newPos = oldPos + (moveUp ? -1 : 1);
    std::swap(items_[oldPos], items_[newPos]);
    auto changeIf = [&oldPos, &newPos](auto& val) {
        if (val) {
            if (*val == oldPos) {
                val = newPos;
            } else if (*val == oldPos) {
                val = oldPos;
            }
        }
    };
    changeIf(selected_);
    changeIf(playing_);
    return std::make_pair(oldPos, newPos);
}
