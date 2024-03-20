#include <functional>
#include <mutex>

#include <sndfile.hh>

#include "Source.hh"

class Source::Impl {
    SndfileHandle sndfile_;
    using FillFunction = std::function<unsigned(const AudioBuffer&)>;
    FillFunction fillFunction_;
    std::mutex mutex_;

  public:
    Error load(const char* filename) noexcept {
        sndfile_ = SndfileHandle(filename);
        if (sndfile_.error() == SF_ERR_NO_ERROR) {
            auto fmt = sndfile_.format() & SF_FORMAT_SUBMASK;
            switch (fmt) {
                case SF_FORMAT_PCM_S8:
                case SF_FORMAT_PCM_U8:
                    fillFunction_ = [this](const AudioBuffer& buffer) {
                        auto channels = sndfile_.channels();
                        if (channels != 0) {
                            return sndfile_.readRaw(
                                buffer.data, buffer.frameCount / channels);
                        }
                        return 0L;
                    };
                    break;

                case SF_FORMAT_PCM_16:
                    fillFunction_ = [this](const AudioBuffer& buffer) {
                        return sndfile_.readf(static_cast<short*>(buffer.data),
                            buffer.frameCount);
                    };
                    break;

                case SF_FORMAT_PCM_24:
                case SF_FORMAT_PCM_32:
                    fillFunction_ = [this](const AudioBuffer& buffer) {
                        return sndfile_.readf(
                            static_cast<int*>(buffer.data), buffer.frameCount);
                    };
                    break;

                case SF_FORMAT_DOUBLE:
                    fillFunction_ = [this](const AudioBuffer& buffer) {
                        return sndfile_.readf(
                            static_cast<double*>(buffer.data),
                            buffer.frameCount);
                    };
                    break;

                default:
                    fillFunction_ = [this](const AudioBuffer& buffer) {
                        return sndfile_.readf(static_cast<float*>(buffer.data),
                            buffer.frameCount);
                    };
                    break;
            }
        }
        return static_cast<Error>(sndfile_.error());
    }

    unsigned fill(const AudioBuffer& buffer) noexcept {
        const std::unique_lock<std::mutex> lock(mutex_);
        return fillFunction_(buffer);
    }

    long seek(long frames) noexcept {
        if (!sndfile_) {
            return 0L;
        }
        const std::unique_lock<std::mutex> lock(mutex_);
        return sndfile_.seek(frames, SF_SEEK_CUR);
    }

    [[nodiscard]] StreamParams streamParams() const {
        auto format = [](auto fmt) {
            switch (fmt) {
                case SF_FORMAT_PCM_S8:
                    return SampleFormat::S8;

                case SF_FORMAT_PCM_U8:
                    return SampleFormat::U8;

                case SF_FORMAT_PCM_16:
                    return SampleFormat::S16;

                case SF_FORMAT_PCM_24:
                    return SampleFormat::S24;

                case SF_FORMAT_PCM_32:
                    return SampleFormat::S32;

                case SF_FORMAT_FLOAT:
                    return SampleFormat::F32;

                case SF_FORMAT_DOUBLE:
                    return SampleFormat::F64;

                default:
                    return SampleFormat::None;
            }
        };
        if (sndfile_) {
            return {format(sndfile_.format() & SF_FORMAT_SUBMASK),
                static_cast<unsigned>(sndfile_.channels()),
                static_cast<unsigned>(sndfile_.samplerate())};
        }
        return {SampleFormat::U8, 0, 0};
    }

    [[nodiscard]] long frames() const noexcept {
        if (sndfile_) {
            return sndfile_.frames();
        }
        return 0;
    }
};

Source::Error Source::load(const char* filename) noexcept {
    return impl_->load(filename);
}

unsigned Source::fill(const AudioBuffer& buffer) noexcept {
    return impl_->fill(buffer);
}

StreamParams Source::streamParams() const {
    return impl_->streamParams();
}

long Source::frames() const noexcept {
    return impl_->frames();
}

long Source::seek(long frames) noexcept {
    return impl_->seek(frames);
}

Source::Source() noexcept = default;
Source::~Source() = default;
