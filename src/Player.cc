#include "Player.hh"

Player::Player(Sender<Msg> progressSender, const Options& opts, int argc,
    char* argv[]) noexcept :
    opts_(opts),
    state_(Stopped()),
    params_({SampleFormat::None, 2, 44100}),
    sink_(
        [this, progressSender](const auto& buffer) {
            static unsigned long seconds = 0;
            auto result = decoder_.fill(buffer);
            framesDone_ += result;

            auto entry = currentEntry();
            if (!entry) {
                return result;
            }
            auto doneSec = result != 0
                               ? static_cast<unsigned>(
                                     framesDone_ * entry->duration / frames_)
                               : EndOfSong;

            if (doneSec != seconds) {
                seconds = doneSec;
                progressSender.send(Msg(static_cast<unsigned>(doneSec)));
            }
            return result;
        },
        argc, argv) {
}

const Player::State& Player::start() noexcept {
    sink_.stop();
    framesDone_ = 0;
    if (queue_) {
        auto entry = queue_->current();
        auto error = decoder_.load(entry.path.c_str());
        switch (error) {
            case Source::Error::BadFormat:
                state_ = Stopped{L"bad format"};
                break;

            case Source::Error::Open:
                state_ = Stopped{L"cannot open file"};
                break;

            case Source::Error::Malformed:
                state_ = Stopped{L"malformed"};
                break;

            case Source::Error::UnsupportedEncoding:
                state_ = Stopped{L"unsupported encoding"};
                break;

            case Source::Error::Ok:
                state_ = Playing{entry};
                frames_ = decoder_.frames();
                params_ = decoder_.streamParams();
                seekFrames_ = params_.rate * SeekSeconds;
                sink_.start(params_);
                break;
        }
    } else {
        state_ = Stopped();
    }
    return state_;
}

void Player::stop() noexcept {
    if (std::get_if<Stopped>(&state_) == nullptr) {
        sink_.stop();
        state_ = Stopped();
    }
    params_.format = SampleFormat::None;
}

const Player::State& Player::emit(
    Command cmd, std::optional<Playqueue>&& queue) {
    switch (cmd) {
        case Command::Pause:
            std::visit(
                [this](auto&& value) {
                    using Type = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same<Type, Player::Playing>()) {
                        sink_.activate(false);
                        state_ = Paused{value.entry};
                    } else if constexpr (std::is_same<Type, Player::Paused>()) {
                        sink_.activate(true);
                        state_ = Playing{value.entry};
                    }
                },
                state_);
            break;

        case Command::Stop:
            stop();
            break;

        case Command::Play:
            queue_ = std::move(queue);
            if (opts_.shuffle) {
                queue_->shuffle();
            }
            start();
            break;

        case Command::Next:
            if (queue_) {
                if (queue_->next(opts_.next, opts_.repeat)) {
                    return start();
                }
            }
            stop();
            break;

        case Command::Prev:
            if (queue_) {
                if (queue_->prev(opts_.repeat)) {
                    return start();
                }
            }
            stop();
            break;
    }

    return state_;
}

const Player::State& Player::state() const noexcept {
    return state_;
}

const StreamParams& Player::streamParams() const noexcept {
    return params_;
}

const Entry* Player::currentEntry() const noexcept {
    return std::visit(
        [](auto&& value) -> const Entry* {
            using Type = std::decay_t<decltype(value)>;
            if constexpr (std::is_same<Type, Player::Playing>()) {
                return &value.entry;
            } else if constexpr (std::is_same<Type, Player::Paused>()) {
                return &value.entry;
            } else {
                return nullptr;
            }
        },
        state_);
}

std::optional<unsigned> Player::currentId() const noexcept {
    const auto* entry = currentEntry();
    if (entry) {
        return entry->id;
    }
    return {};
}

const wchar_t* Player::currentSong() const noexcept {
    const auto* entry = currentEntry();
    if (entry) {
        return entry->title.c_str();
    }
    return nullptr;
}

bool Player::stopped() const noexcept {
    return state_.index() == 0;
}

void Player::ff() noexcept {
    if (!stopped()) {
        auto left = frames_ - framesDone_;
        framesDone_ = decoder_.seek(std::min(seekFrames_, left));
    }
}

void Player::rew() noexcept {
    if (!stopped()) {
        auto diff = framesDone_ - seekFrames_;
        framesDone_ = decoder_.seek(diff < 0 ? diff : -seekFrames_);
    }
}

Player::~Player() {
    stop();
}

void Player::clearQueue() noexcept {
    queue_ = std::nullopt;
}

void Player::updateShuffleQueue() noexcept {
    if (queue_) {
        opts_.shuffle ? queue_->shuffle() : queue_->sort();
    }
}
