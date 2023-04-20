#include <algorithm>
#include <random>

#include "Playqueue.hh"

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
        } else if (repeat) {
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
    } else if (repeat) {
        playing_ = items_.size() - 1;
        return true;
    }
    return false;
}

class HoldIndex {
    unsigned id_;
    unsigned& index_;
    const std::vector<Entry>& items_;

  public:
    explicit HoldIndex(const std::vector<Entry>& items, unsigned& index) :
        id_(items[index].id), index_(index), items_(items) {
    }

    HoldIndex(const HoldIndex&) = delete;
    HoldIndex(HoldIndex&&) = delete;
    HoldIndex& operator=(const HoldIndex&) = delete;
    HoldIndex& operator=(HoldIndex&&) = delete;

    ~HoldIndex() {
        auto found = std::find_if(items_.begin(), items_.end(),
            [this](const auto& item) { return item.id == id_; });
        if (found != items_.end()) {
            index_ = std::distance(items_.begin(), found);
        }
    }
};
void Playqueue::shuffle() noexcept {
    HoldIndex hold(items_, playing_);
    auto rng = std::default_random_engine{};
    std::shuffle(items_.begin(), items_.end(), rng);
}

void Playqueue::sort() noexcept {
    HoldIndex hold(items_, playing_);
    std::sort(items_.begin(), items_.end());
}
