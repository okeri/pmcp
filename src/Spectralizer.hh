#include "Player.hh"

class Spectralizer {
    using SetBinCountFn = std::function<void(unsigned)>;
    static constexpr auto DefaultBinCount = 8U;

    const Player::State& state_;
    SetBinCountFn setBinCount_;
    std::vector<float> bins_;

  public:
    explicit Spectralizer(
        const Player::State& state, SetBinCountFn setBinCount) noexcept :
        state_(state),
        setBinCount_(std::move(setBinCount)),
        bins_(std::vector<float>(DefaultBinCount, 0.F)) {
    }

    [[nodiscard]] const Player::State& state() const noexcept {
        return state_;
    }

    [[nodiscard]] const std::vector<float>& bins() {
        return bins_;
    }

    void applyBins(std::vector<float> bins) {
        if (bins_.size() != bins.size()) {
            bins_ = std::move(bins);
            return;
        }
        constexpr auto DropRate = 5.F;
        for (auto i = 0UL; i < bins.size(); ++i) {
            auto diff = bins[i] - bins_[i];
            if (diff > 0) {
                bins_[i] = bins[i];
            } else {
                bins_[i] += diff / DropRate;
            }
        }
    }

    void setBinCount(unsigned binCount) {
        setBinCount_(binCount);
    }
};
