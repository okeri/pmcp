#include "PlayerView.hh"
#include "utf8.hh"

PlayerView::PlayerView(const Config& config) :
    path_(utf8::convert(config.home)),
    lists_({Playlist::scan(config.home, config),
        Playlist::load(config.playlistPath, config)}),
    playlistActive_(lists_[1].count() != 0) {
}

Playlist& PlayerView::activeList() noexcept {
    return lists_[playlistActive_ ? 1 : 0];
}

void PlayerView::up(unsigned offset) noexcept {
    activeList().up(offset);
}

void PlayerView::down(unsigned offset) noexcept {
    activeList().down(offset);
}

void PlayerView::home(bool force) noexcept {
    activeList().home(force);
}

void PlayerView::end() noexcept {
    activeList().end();
}

void PlayerView::toggleLists() noexcept {
    playlistActive_ = !playlistActive_;
}

void PlayerView::clear() noexcept {
    lists_[1].clear();
}

void PlayerView::delSelected() noexcept {
    if (playlistActive_) {
        if (auto sel = lists_[1].selectedIndex()) {
            lists_[1].remove(*sel);
        }
    }
}

void PlayerView::addToPlaylist() noexcept {
    if (!playlistActive_) {
        if (auto sel = lists_[0].selectedIndex()) {
            lists_[1].add(lists_[0].recursiveCollect(*sel));
        }
    }
}

std::optional<Playqueue> PlayerView::enter() noexcept {
    auto& list = activeList();
    if (auto sel = list.selectedIndex()) {
        const auto& selItem = list[*sel];
        if (selItem.isDir()) {
            if (playlistQueued_ == 0) {
                lists_[0].setPlaying(std::nullopt);
                playlistQueued_ = -1;
            }
            auto newsel =
                selItem.title == L"../"
                    ? std::make_optional<std::string>(utf8::convert(path_))
                    : std::nullopt;
            auto newpath = selItem.path;
            path_ = utf8::convert(newpath);
            list.listDir(newpath);
            if (newsel) {
                for (auto i = 0u; i < list.count(); ++i) {
                    if (list[i].path == *newsel) {
                        list.select(i);
                        break;
                    }
                }
            } else {
                list.home(true);
            }
        } else {
            std::vector<Entry> qi;
            auto index = 0;
            playlistQueued_ = playlistActive_ ? 1 : 0;
            for (auto i = 0u; i < list.count(); ++i) {
                const auto& item = list[i];
                if (!item.isDir()) {
                    qi.emplace_back(
                        i, item.duration.value(), item.title, item.path);
                    if (i == *sel) {
                        index = qi.size() - 1;
                    }
                }
            }
            return Playqueue(std::move(qi), index);
        }
    }
    return {};
}

void PlayerView::markPlaying(const std::optional<unsigned>& id) noexcept {
    lists_[0].setPlaying(std::nullopt);
    lists_[1].setPlaying(std::nullopt);
    if (playlistQueued_ > -1) {
        lists_[playlistQueued_ ? 1 : 0].setPlaying(id);
    }
}

const wchar_t* PlayerView::currentPath() const noexcept {
    return path_.c_str();
}

bool PlayerView::playlistActive() const noexcept {
    return playlistActive_;
}

Playlist& PlayerView::operator[](unsigned index) noexcept {
    return lists_[index];
}

PlayerView::~PlayerView() {
    lists_[1].save();
}
