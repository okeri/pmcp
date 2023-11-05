#include "FFT.hh"
#include "Player.hh"
#include <cmath>

template <class Pred>
decltype(auto) bufferAction(
    SampleFormat format, const AudioBuffer& buffer, Pred pred) {
    switch (format) {
        case SampleFormat::S8:
            return pred(
                reinterpret_cast<int8_t*>(buffer.data), buffer.frameCount);

        case SampleFormat::U8:
            return pred(
                reinterpret_cast<uint8_t*>(buffer.data), buffer.frameCount);

        case SampleFormat::S16:
            return pred(
                reinterpret_cast<int16_t*>(buffer.data), buffer.frameCount);

        case SampleFormat::S24:
        case SampleFormat::S32:
            return pred(
                reinterpret_cast<int32_t*>(buffer.data), buffer.frameCount);

        case SampleFormat::F64:
            return pred(
                reinterpret_cast<double*>(buffer.data), buffer.frameCount);

        default:
            return pred(
                reinterpret_cast<float*>(buffer.data), buffer.frameCount);
    }
}

#ifdef ENABLE_SPECTRALIZER

void calculateBins(
    const AudioBuffer& buffer, const StreamParams& params, Player::Bins& bins) {
    constexpr auto highFreq = 16000;
    constexpr auto lowFreq = 100;
    constexpr auto MaxFFT = 4096u;

    auto fftSize = std::min(buffer.frameCount, MaxFFT);
    auto hamming = [](int N) {
        if (N == 0) {
            return std::vector<double>{};
        }
        if (N == 1) {
            return std::vector<double>(1, 1.);
        }

        auto result = std::vector<double>(N);
        for (auto i = 0, start = 1 - static_cast<int>(N); i < N; ++i) {
            result[i] = 0.54 + 0.46 * cos(M_PI * (start + (i << 1)) / (N - 1));
        }
        return result;
    };

    auto binSpace = [](unsigned fftSize) {
        std::vector<unsigned> result(Player::BinCount);
        constexpr auto K = 4.;
        constexpr auto sigma = 4.8 / Player::BinCount;
        auto hz2bin = [&fftSize](double freq) -> unsigned {
            return freq / (2 * highFreq) * fftSize;
        };
        auto lowest = hz2bin(lowFreq);
        for (auto i = 0u; i < result.size(); ++i) {
            auto bin = lowest + hz2bin(pow(M_E, K + sigma * i));
            if (i != 0 && result[i - 1] >= bin) {
                bin = result[i - 1] + 1;
            }
            result[i] = bin;
        }
        return result;
    };

    static auto audio = std::array<double, MaxFFT>{};
    static auto frequences = std::array<std::complex<double>, MaxFFT / 2 + 1>{};
    static auto window = hamming(fftSize);
    static auto space = binSpace(fftSize);
    static auto fft = FFT(fftSize, audio.data(), frequences.data());

    if (window.size() != fftSize) {
        window = hamming(fftSize);
        space = binSpace(fftSize);
        fft.resize(fftSize);
    }

    bufferAction(params.format, buffer,
        [&params](auto* frames, [[maybe_unused]] unsigned frameCount) {
            using SampleType = std::remove_pointer_t<decltype(frames)>;
            constexpr auto NormValue = std::numeric_limits<SampleType>::max();
            auto sum = [](SampleType v1, SampleType v2) {
                if constexpr (std::is_signed<SampleType>()) {
                    return std::abs(v1) + std::abs(v2);
                } else {
                    return v1 + v2;
                }
            };
            for (auto i = 0u; i < window.size(); ++i) {
                // 2 channels
                if (params.channelCount == 2) {
                    audio[i] = sum(frames[i * 2], frames[i * 2 + 1]) *
                               window[i] / 2 / NormValue;
                } else {
                    audio[i] = frames[i] * window[i] / NormValue;
                }
            }
        });

    fft.exec();

    for (auto bin = 0u; bin < bins.size() - 1; ++bin) {
        auto value = 0.f;
        for (auto c = space[bin]; c <= space[bin + 1] && c < fftSize / 2; ++c) {
            value += std::abs(frequences[c]);
        }
        value = log(value) / 5.f;
        bins[bin] = std::clamp(std::isnan(value) ? 0.f : value, 0.f, 1.f);
    }
}
#endif

Player::Player(Sender<Msg> progressSender, const Options& opts, int argc,
    char* argv[]) noexcept :
    opts_(opts),
    state_(Stopped()),
    sink_(
        [this, progressSender](const auto& buffer) {
            static unsigned long seconds = 0;
            auto sampleCount = decoder_.fill(buffer);
            bufferAction(params_.format, buffer,
                [this](auto* frames, unsigned frameCount) {
                    for (auto i = 0u; i < frameCount * params_.channelCount;
                         ++i) {
                        frames[i] *= params_.volume;
                    }
                });
            framesDone_ += sampleCount;

            auto entry = currentEntry();
            if (!entry) {
                return sampleCount;
            }
#ifdef ENABLE_SPECTRALIZER
            if (opts_.spectralizer) {
                calculateBins(buffer, params_, bins_);
            }
#endif
            auto doneSec = sampleCount != 0
                               ? static_cast<unsigned>(
                                     framesDone_ * entry->duration / frames_)
                               : EndOfSong;
            static auto endFired = false;
            if ((opts_.spectralizer && !endFired) || doneSec != seconds) {
                seconds = doneSec;
                progressSender.send(Msg(static_cast<unsigned>(doneSec)));
                endFired = doneSec == EndOfSong;
            }
            return sampleCount;
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

void Player::setVolume(double volume) noexcept {
    params_.volume = volume;
}

const Player::Bins& Player::bins() const noexcept {
    return bins_;
}
