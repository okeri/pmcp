#pragma GCC diagnostic ignored "-Wpedantic"
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#pragma GCC diagnostic warning "-Wpedantic"

#include "Sink.hh"

class Sink::Impl {
    pw_thread_loop* loop_{nullptr};
    pw_stream* stream_{nullptr};
    unsigned stride_{0};
    Sink::BufferFillRoutine fillBuffer_;

  public:
    Impl(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl& operator=(Impl&&) = delete;

    Impl(Sink::BufferFillRoutine fillBuffer, int argc, char* argv[]) noexcept :
        fillBuffer_(std::move(fillBuffer)) {
        pw_init(&argc, &argv);
        loop_ = pw_thread_loop_new(nullptr, nullptr);
    }

    void stop() noexcept {
        if (stream_ != nullptr) {
            pw_thread_loop_stop(loop_);
            pw_stream_destroy(stream_);
            stream_ = nullptr;
        }
    }

    void activate(bool active) const noexcept {
        if (stream_ != nullptr) {
            pw_thread_loop_lock(loop_);
            pw_stream_set_active(stream_, active);
            pw_thread_loop_unlock(loop_);
        }
    }

    void start(const StreamParams& streamParams) noexcept {
        const struct spa_pod* params[1];
        uint8_t buffer[1024];
        struct spa_pod_builder b = {buffer, sizeof(buffer), 0, {}, {}};
        auto format = [](auto fmt) {
            switch (fmt) {
                case SampleFormat::U8:
                    return SPA_AUDIO_FORMAT_U8;
                case SampleFormat::S8:
                    return SPA_AUDIO_FORMAT_S8;
                case SampleFormat::S16:
                    return SPA_AUDIO_FORMAT_S16;
                case SampleFormat::S32:
                    return SPA_AUDIO_FORMAT_S32;
                case SampleFormat::F32:
                    return SPA_AUDIO_FORMAT_F32;
                case SampleFormat::F64:
                    return SPA_AUDIO_FORMAT_F64;
                default:
                    return SPA_AUDIO_FORMAT_UNKNOWN;
            }
        };
        auto width = [](auto fmt) {
            switch (fmt) {
                case SampleFormat::U8:
                case SampleFormat::S8:
                    return 1;
                case SampleFormat::S16:
                    return 2;
                case SampleFormat::S32:
                case SampleFormat::F32:
                    return 4;
                case SampleFormat::F64:
                    return 8;
                default:
                    return 0;
            }
        };
        stop();
        stride_ = width(streamParams.format) * streamParams.channels;
        spa_audio_info_raw info = {.format = format(streamParams.format),
            .flags = 0,
            .rate = static_cast<unsigned>(streamParams.rate),
            .channels = streamParams.channels,
            .position = {0}};
        auto* props =
            pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY,
                "Playback", PW_KEY_MEDIA_ROLE, "Music", nullptr);

        static pw_stream_events stream_events = {PW_VERSION_STREAM_EVENTS,
            nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
            [](void* data) {
                auto* self = reinterpret_cast<Sink::Impl*>(data);
                auto* buffer = pw_stream_dequeue_buffer(self->stream_);
                auto buf = buffer->buffer;
                auto maxFrames = static_cast<uint64_t>(buf->datas[0].maxsize) /
                                 self->stride_;
                auto frames = std::min(buffer->requested, maxFrames);
                AudioBuffer audioBuffer{
                    buf->datas[0].data, static_cast<unsigned>(frames)};
                buf->datas[0].chunk->offset = 0;
                buf->datas[0].chunk->stride = self->stride_;
                buf->datas[0].chunk->size =
                    self->fillBuffer_(audioBuffer) * self->stride_;
                pw_stream_queue_buffer(self->stream_, buffer);
            },
            nullptr, nullptr, nullptr};

        stream_ = pw_stream_new_simple(pw_thread_loop_get_loop(loop_),
            "audio-src", props, &stream_events, this);

        params[0] = spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info);
        pw_stream_connect(stream_, PW_DIRECTION_OUTPUT, PW_ID_ANY,
            static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT |
                                         PW_STREAM_FLAG_MAP_BUFFERS |
                                         PW_STREAM_FLAG_RT_PROCESS),
            params, 1);
        pw_thread_loop_start(loop_);
    }

    ~Impl() {
        pw_thread_loop_destroy(loop_);
        pw_deinit();
    }
};

void Sink::start(const StreamParams& streamParams) noexcept {
    impl_->start(streamParams);
}

void Sink::stop() noexcept {
    impl_->stop();
}

void Sink::activate(bool act) const noexcept {
    impl_->activate(act);
}

Sink::Sink(BufferFillRoutine fillBuffer, int argc, char* argv[]) noexcept :
    impl_(std::move(fillBuffer), argc, argv) {
}

Sink::~Sink() = default;
