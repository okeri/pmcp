#include "Player.hh"

class Spectralizer {
    const Player::State& state_;
    const Player::Bins& bins_;

  public:
    explicit Spectralizer(
        const Player::State& state, const Player::Bins& bins) noexcept :
        state_(state), bins_(bins) {
    }

    [[nodiscard]] const Player::State& state() const noexcept {
        return state_;
    }

    [[nodiscard]] const Player::Bins& bins() const noexcept {
        return bins_;
    }
};
