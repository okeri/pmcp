#pragma once

#include "Config.hh"
#include "Player.hh"

class Status {
    unsigned progressDone_{0};
    const Config& config_;
    const Player::State& state_;
    const StreamParams& params_;

  public:
    Status(const Config& config, const Player::State& state,
        const StreamParams& params) noexcept :
        config_(config), state_(state), params_(params) {
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

    [[nodiscard]] const Config& config() const noexcept {
        return config_;
    }

    [[nodiscard]] const Player::State& state() const noexcept {
        return state_;
    }
};
