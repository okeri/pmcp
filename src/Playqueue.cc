#include <algorithm>
#include <random>

#include "Playqueue.hh"

namespace {

template <class Action, class... Args>
unsigned modify(
    std::vector<Entry>& items, unsigned index, Action action, Args... args) {
    auto id = items[index].id;
    action(items.begin(), items.end(), std::forward<Args>(args)...);
    auto found = std::find_if(items.begin(), items.end(),
        [&id](const auto& item) { return item.id == id; });
    if (found != items.end()) {
        return std::distance(items.begin(), found);
    }
    return index;
}

}  // namespace

Playqueue::Playqueue(std::vector<Entry>&& items, unsigned playing) noexcept :
    playing_(playing), items_(std::move(items)) {
}

Entry Playqueue::current() const noexcept {
    return items_[playing_];
}

bool Playqueue::next(bool next, bool repeat) noexcept {
    if (next) {
        if (++playing_ < items_.size()) {
            return true;
        }
        if (repeat) {
            playing_ = 0;
            return true;
        }
    }
    return false;
}

bool Playqueue::prev(bool repeat) noexcept {
    if (playing_ > 0) {
        --playing_;
        return true;
    }
    if (repeat) {
        playing_ = items_.size() - 1;
        return true;
    }
    return false;
}

void Playqueue::shuffle() noexcept {
    auto rng = std::default_random_engine{};
    playing_ = modify(items_, playing_,
        std::shuffle<decltype(items_)::iterator, decltype(rng)>, rng);
}

void Playqueue::sort() noexcept {
    playing_ = modify(items_, playing_, std::sort<decltype(items_)::iterator>);
}
