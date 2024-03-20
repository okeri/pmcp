#pragma once

#include "Player.hh"

class Status {
    unsigned progressDone_{0};
    const Player::State& state_;
    const StreamParams& params_;

  public:
    Status(const Player::State& state, const StreamParams& params) noexcept :
        state_(state), params_(params) {
    }

    void setProgress(unsigned progress) noexcept {
        progressDone_ = progress;
    }

    [[nodiscard]] unsigned progress() const noexcept {
        return progressDone_;
    }

    [[nodiscard]] const StreamParams& streamParams() const noexcept {
        return params_;
    }

    [[nodiscard]] const Player::State& state() const noexcept {
        return state_;
    }
};
