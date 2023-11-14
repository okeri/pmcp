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

std::vector<float> calculateBins(
    const AudioBuffer& buffer, const StreamParams& params, unsigned binCount) {
    constexpr auto LowFreq = 100u;
    constexpr auto HighFreq = 20000u;
    constexpr auto MaxFFT = 4096u;

    auto fftSize = std::min(buffer.frameCount, MaxFFT);
    auto hanning = [](int N) {
        if (N == 0) {
            return std::vector<double>{};
        }
        if (N == 1) {
            return std::vector<double>(1, 1.);
        }

        auto result = std::vector<double>(N);
        for (auto i = 0, start = 1 - N; i < N; ++i) {
            result[i] = 0.5 + 0.5 * cos(M_PI * (start + (i << 1)) / (N - 1));
        }
        return result;
    };

    auto binSpace = [](unsigned binCount, unsigned fftSize,
                        unsigned sampleRate) {
        const auto start_power = std::log(LowFreq);
        const auto step =
            std::log(static_cast<double>(HighFreq) / LowFreq) / binCount;

        const auto freqResolution = static_cast<double>(sampleRate) / fftSize;
        auto hz2index = [&freqResolution](double freq) -> unsigned {
            return freq / freqResolution;
        };

        auto p = start_power;
        auto lowest = hz2index(LowFreq);
        auto result = std::vector<unsigned>(binCount);
        for (auto i = 0u; i < result.size(); ++i, p += step) {
            auto freq = std::pow(M_E, p);
            auto index = hz2index(freq);
            if (lowest >= index) {
                index = lowest + 1;
            }
            result[i] = lowest = index;
        }
        return result;
    };

    static auto audio = std::array<double, MaxFFT>{};
    static auto frequences = std::array<std::complex<double>, MaxFFT / 2 + 1>{};
    static auto window = hanning(fftSize);
    static auto scale = binSpace(binCount, fftSize, params.rate);
    static auto fft = FFT(fftSize, audio.data(), frequences.data());

    if (window.size() != fftSize || scale.size() != binCount) {
        scale = binSpace(binCount, fftSize, params.rate);
    }

    if (window.size() != fftSize) {
        window = hanning(fftSize);
        fft.resize(fftSize);
    }

    bufferAction(params.format, buffer,
        [&params](auto* frames, [[maybe_unused]] unsigned frameCount) {
            using SampleType = std::remove_pointer_t<decltype(frames)>;
            constexpr auto NormValue = std::numeric_limits<SampleType>::max();
            auto avg = [](SampleType v1, SampleType v2) {
                if constexpr (std::is_signed<SampleType>()) {
                    return (std::abs(static_cast<double>(v1)) +
                               std::abs(static_cast<double>(v2))) /
                           2;
                } else {
                    return (static_cast<double>(v1) + static_cast<double>(v2)) /
                           2;
                }
            };
            for (auto i = 0u; i < window.size(); ++i) {
                // 2 channels
                if (params.channelCount == 2) {
                    audio[i] = avg(frames[i * 2], frames[i * 2 + 1]) *
                               window[i] / NormValue;
                } else {
                    audio[i] =
                        static_cast<double>(frames[i]) * window[i] / NormValue;
                }
            }
        });

    auto chooseMagnitude = [&fftSize](unsigned low, unsigned high) {
        auto value = 0.;
        for (auto i = low; i < high && i < fftSize / 2; ++i) {
            value = std::max(value, std::abs(frequences[i]));
        }
        value = std::log(value * 2) / 5;  // 20 * log(2 * value) / 100
        return std::clamp(
            std::isnan(value) ? 0.f : static_cast<float>(value), 0.f, 1.f);
    };
    fft.exec();
    auto result = std::vector<float>(binCount);
    for (auto bin = 0u; bin < result.size() - 1; ++bin) {
        result[bin] = chooseMagnitude(scale[bin], scale[bin + 1]);
    }

    result.back() = chooseMagnitude(scale[binCount - 1], HighFreq);
    return result;
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
                progressSender.send(
                    Msg(calculateBins(buffer, params_, binCount_)));
            }
#endif
            auto doneSec = sampleCount != 0
                               ? static_cast<unsigned>(
                                     framesDone_ * entry->duration / frames_)
                               : EndOfSong;
            if (doneSec != seconds) {
                seconds = doneSec;
                progressSender.send(Msg(static_cast<unsigned>(doneSec)));
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

void Player::setBinCount(unsigned count) noexcept {
    binCount_ = count;
}
