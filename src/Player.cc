#include "Player.hh"
#include "utf8.hh"

Player::Player(Sender<Msg> progressSender, const Options& opts, int argc,
    char* argv[]) noexcept :
    opts_(opts),
    state_(Stopped()),
    sink_(
        [this, progressSender](const auto& buffer) {
            static unsigned long seconds = 0;
            auto result = decoder_.fill(buffer);
            framesDone_ += result;
            if (result != 0) {
                auto doneSec =
                    framesDone_ *
                    std::get_if<Player::Playing>(&state_)->entry.duration /
                    frames_;
                if (doneSec != seconds) {
                    seconds = doneSec;
                    progressSender.send(Msg(static_cast<unsigned>(doneSec)));
                }
            } else {
                progressSender.send(Msg(EndOfSong));
            }
            return result;
        },
        argc, argv) {
}

const Player::State& Player::start() noexcept {
    framesDone_ = 0;
    sink_.stop();
    if (queue_) {
        auto entry = queue_->current();
        auto error = decoder_.load(utf8::convert(entry.path).c_str());
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
                auto params = decoder_.streamParams();
                seekFrames_ = params.rate * SeekSeconds;
                sink_.start(params);
                break;
        }
    } else {
        state_ = Stopped();
    }
    return state_;
}

void Player::stop() noexcept {
    if (std::get_if<Stopped>(&state_) == nullptr) {
        state_ = Stopped();
        sink_.stop();
    }
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

std::optional<unsigned> Player::currentId() const noexcept {
    return std::visit(
        [](auto&& value) -> std::optional<unsigned> {
            using Type = std::decay_t<decltype(value)>;
            if constexpr (std::is_same<Type, Player::Playing>()) {
                return value.entry.id;
            } else if constexpr (std::is_same<Type, Player::Paused>()) {
                return value.entry.id;
            } else {
                return std::nullopt;
            }
        },
        state_);
}

const wchar_t* Player::currentSong() const noexcept {
    return std::visit(
        [](auto&& value) -> const wchar_t* {
            using Type = std::decay_t<decltype(value)>;
            if constexpr (std::is_same<Type, Player::Playing>()) {
                return value.entry.title.c_str();
            } else if constexpr (std::is_same<Type, Player::Paused>()) {
                return value.entry.title.c_str();
            } else {
                return nullptr;
            }
        },
        state_);
}

bool Player::stopped() const noexcept {
    return state_.index() == 0;
}

void Player::ff() noexcept {
    if (!stopped()) {
        framesDone_ = decoder_.seek(seekFrames_);
    }
}

void Player::rew() noexcept {
    if (!stopped()) {
        framesDone_ = decoder_.seek(-seekFrames_);
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
