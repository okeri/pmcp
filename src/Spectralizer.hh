#include "Player.hh"

class Spectralizer {
    using SetBinCountFn = std::function<void(unsigned)>;
    const Player::State& state_;
    SetBinCountFn setBinCount_;
    std::vector<float> bins_;

  public:
    explicit Spectralizer(
        const Player::State& state, SetBinCountFn setBinCount) noexcept :
        state_(state),
        setBinCount_(std::move(setBinCount)),
        bins_(std::vector<float>(8u, 0.f)) {
    }

    [[nodiscard]] const Player::State& state() const noexcept {
        return state_;
    }

    [[nodiscard]] const std::vector<float>& bins() {
        return bins_;
    }

    void applyBins(std::vector<float> bins) {
        constexpr auto dropRate = 5.f;
        if (bins_.size() != bins.size()) {
            bins_ = std::move(bins);
        }
        for (auto i = 0ul; i < bins.size(); ++i) {
            auto diff = bins[i] - bins_[i];
            if (diff > 0) {
                bins_[i] = bins[i];
            } else {
                bins_[i] += diff / dropRate;
            }
        }
    }

    void setBinCount(unsigned binCount) {
        setBinCount_(binCount);
    }
};
