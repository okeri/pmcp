#pragma once

#include "Config.hh"
#include "Player.hh"

class Status {
    unsigned progressDone_{0};
    const Config& config_;
    const Player::State& state_;

  public:
    Status(const Config& config, const Player::State& state) noexcept :
        config_(config), state_(state) {
    }

    void setProgress(unsigned progress) noexcept {
        progressDone_ = progress;
    }

    [[nodiscard]] unsigned progress() const noexcept {
        return progressDone_;
    }

    [[nodiscard]] const Config& config() const noexcept {
        return config_;
    }

    [[nodiscard]] const Player::State& state() const noexcept {
        return state_;
    }
};
