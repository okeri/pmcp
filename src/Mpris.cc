#include "Mpris.hh"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdlib>
#include <ctime>
#include <format>
#include <mutex>
#include <string_view>

#include <sys/eventfd.h>
#include <unistd.h>
#include <systemd/sd-bus.h>

#include "Player.hh"

// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg,hicpp-vararg)

namespace {

constexpr auto BusName = "org.mpris.MediaPlayer2.pmcp";
constexpr auto ObjectPath = "/org/mpris/MediaPlayer2";
constexpr auto RootInterface = "org.mpris.MediaPlayer2";
constexpr auto PlayerInterface = "org.mpris.MediaPlayer2.Player";
constexpr auto NoTrack = "/org/mpris/MediaPlayer2/TrackList/NoTrack";
constexpr auto UsecPerSec = 1000000L;
constexpr auto UsecPerMsec = 1000L;
constexpr auto MaxSeekSteps = 64;
constexpr auto SeekThreshold = Player::SeekSeconds / 2;

std::string fileUrl(const std::string& path) {
    auto result = std::string("file://");
    for (auto sym : path) {
        auto byte = static_cast<unsigned char>(sym);
        auto unreserved = (byte >= '0' && byte <= '9') ||
                          (byte >= 'A' && byte <= 'Z') ||
                          (byte >= 'a' && byte <= 'z') || byte == '-' ||
                          byte == '_' || byte == '.' || byte == '~' ||
                          byte == '/';
        if (unreserved) {
            result += sym;
        } else {
            result += std::format("%{:02X}", byte);
        }
    }
    return result;
}

std::int64_t monotonicUsec() noexcept {
    auto now = timespec{};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec * UsecPerSec) + (now.tv_nsec / UsecPerMsec);
}

}  // namespace

class Mpris::Impl {
    Sender<Msg> sender_;
    sd_bus* bus_{nullptr};
    sd_bus_slot* rootSlot_{nullptr};
    sd_bus_slot* playerSlot_{nullptr};
    int notify_{-1};

    std::mutex mutex_;
    Snapshot pending_;
    Snapshot current_;
    Snapshot published_;
    std::optional<Action> action_;

    static const sd_bus_vtable rootVtable[];    // NOLINT(*-avoid-c-arrays)
    static const sd_bus_vtable playerVtable[];  // NOLINT(*-avoid-c-arrays)

    static Impl* self(void* userdata) noexcept {
        return static_cast<Impl*>(userdata);
    }

    static const char* statusName(Snapshot::State state) noexcept {
        if (state == Snapshot::State::Playing) {
            return "Playing";
        }
        return state == Snapshot::State::Paused ? "Paused" : "Stopped";
    }

    void dispatch(Action action) noexcept {
        sender_.send(Msg(action));
        if (!action_ || action == Action::Quit) {
            action_ = action;
        }
    }

    void seekBy(std::int64_t offset) noexcept {
        auto step = static_cast<std::int64_t>(Player::SeekSeconds) * UsecPerSec;
        auto steps = std::min<std::int64_t>(
            (std::abs(offset) + step - 1) / step, MaxSeekSteps);
        for (auto i = 0; i < steps; ++i) {
            dispatch(offset > 0 ? Action::FF : Action::Rew);
        }
    }

    void shutdown() noexcept {
        rootSlot_ = sd_bus_slot_unref(rootSlot_);
        playerSlot_ = sd_bus_slot_unref(playerSlot_);
        bus_ = sd_bus_flush_close_unref(bus_);
    }

    int appendMetadata(sd_bus_message* reply) const {
        auto append = [reply](const char* key, const char* type,
                          auto... value) {
            return sd_bus_message_append(reply, "{sv}", key, type, value...);
        };
        if (auto ret = sd_bus_message_open_container(reply, 'a', "{sv}");
            ret < 0) {
            return ret;
        }
        if (current_.state == Snapshot::State::Stopped) {
            if (auto ret = append("mpris:trackid", "o", NoTrack); ret < 0) {
                return ret;
            }
            return sd_bus_message_close_container(reply);
        }
        auto trackId = std::format("/org/pmcp/track/{}", current_.trackId);
        auto length = static_cast<std::int64_t>(current_.duration) * UsecPerSec;
        auto url = fileUrl(current_.path);
        auto rets = std::array{append("mpris:trackid", "o", trackId.c_str()),
            append("mpris:length", "x", length),
            append("xesam:title", "s", current_.title.c_str()),
            append("xesam:artist", "as", 1, current_.artist.c_str()),
            append("xesam:url", "s", url.c_str())};
        if (const auto* failed = std::ranges::find_if(rets, [](int ret) {
                return ret < 0;
            });
            failed != rets.end()) {
            return *failed;
        }
        return sd_bus_message_close_container(reply);
    }

    void emitChanges() noexcept {
        // NOLINTBEGIN(cppcoreguidelines-pro-type-const-cast)
        auto names = std::array<char*, 6>{};
        auto count = 0U;
        auto changed = [&names, &count](const char* name) {
            names[count++] = const_cast<char*>(name);
        };
        // NOLINTEND(cppcoreguidelines-pro-type-const-cast)
        if (current_.state != published_.state) {
            changed("PlaybackStatus");
        }
        if (current_.trackId != published_.trackId ||
            current_.title != published_.title ||
            current_.artist != published_.artist ||
            current_.duration != published_.duration ||
            current_.path != published_.path) {
            changed("Metadata");
        }
        if (current_.volume != published_.volume) {
            changed("Volume");
        }
        if (current_.shuffle != published_.shuffle) {
            changed("Shuffle");
        }
        if (current_.repeat != published_.repeat) {
            changed("LoopStatus");
        }
        if (count > 0) {
            sd_bus_emit_properties_changed_strv(
                bus_, ObjectPath, PlayerInterface, names.data());
        }
        auto moved = current_.position > published_.position
                         ? current_.position - published_.position
                         : published_.position - current_.position;
        if (current_.trackId == published_.trackId &&
            current_.state == published_.state && moved >= SeekThreshold) {
            sd_bus_emit_signal(bus_, ObjectPath, PlayerInterface, "Seeked", "x",
                static_cast<std::int64_t>(current_.position) * UsecPerSec);
        }
        published_ = current_;
    }

    int call(sd_bus_message* msg, sd_bus_error* error) {
        const auto* name = sd_bus_message_get_member(msg);
        auto member = std::string_view(name != nullptr ? name : "");
        if (member == "Next") {
            dispatch(Action::Next);
        } else if (member == "Previous") {
            dispatch(Action::Prev);
        } else if (member == "Stop") {
            dispatch(Action::Stop);
        } else if (member == "Play") {
            if (current_.state == Snapshot::State::Paused) {
                dispatch(Action::Pause);
            } else if (current_.state == Snapshot::State::Stopped) {
                dispatch(Action::Next);
            }
        } else if (member == "Pause") {
            if (current_.state == Snapshot::State::Playing) {
                dispatch(Action::Pause);
            }
        } else if (member == "PlayPause") {
            dispatch(current_.state == Snapshot::State::Stopped
                         ? Action::Next
                         : Action::Pause);
        } else if (member == "Quit") {
            dispatch(Action::Quit);
        } else if (member == "Seek") {
            auto offset = std::int64_t{0};
            if (auto ret = sd_bus_message_read(msg, "x", &offset); ret < 0) {
                return ret;
            }
            seekBy(offset);
        } else if (member == "SetPosition") {
            const char* track = nullptr;
            auto position = std::int64_t{0};
            if (auto ret = sd_bus_message_read(msg, "ox", &track, &position);
                ret < 0) {
                return ret;
            }
            seekBy(position -
                   (static_cast<std::int64_t>(current_.position) * UsecPerSec));
        } else if (member == "OpenUri") {
            sd_bus_error_set_const(error,
                "org.freedesktop.DBus.Error.NotSupported",
                "pmcp cannot open uris");
            return -EOPNOTSUPP;
        }
        return sd_bus_reply_method_return(msg, "");
    }

    int getProperty(std::string_view name, sd_bus_message* reply) const {
        auto boolean = [reply](bool value) {
            return sd_bus_message_append(reply, "b", value ? 1 : 0);
        };
        if (name == "PlaybackStatus") {
            return sd_bus_message_append(
                reply, "s", statusName(current_.state));
        }
        if (name == "LoopStatus") {
            return sd_bus_message_append(
                reply, "s", current_.repeat ? "Playlist" : "None");
        }
        if (name == "Shuffle") {
            return boolean(current_.shuffle);
        }
        if (name == "Metadata") {
            return appendMetadata(reply);
        }
        if (name == "Volume") {
            return sd_bus_message_append(reply, "d", current_.volume);
        }
        if (name == "Position") {
            return sd_bus_message_append(reply, "x",
                static_cast<std::int64_t>(current_.position) * UsecPerSec);
        }
        if (name == "Rate" || name == "MinimumRate" || name == "MaximumRate") {
            return sd_bus_message_append(reply, "d", 1.);
        }
        if (name == "Identity") {
            return sd_bus_message_append(reply, "s", "pmcp");
        }
        if (name == "SupportedUriSchemes" || name == "SupportedMimeTypes") {
            return sd_bus_message_append(reply, "as", 0);
        }
        if (name == "CanRaise" || name == "HasTrackList") {
            return boolean(false);
        }
        return boolean(true);
    }

    int setProperty(std::string_view name, sd_bus_message* value) {
        if (name == "Volume") {
            auto volume = 0.;
            if (auto ret = sd_bus_message_read(value, "d", &volume); ret < 0) {
                return ret;
            }
            sender_.send(Msg(SetVolume{.value = std::clamp(volume, 0., 1.)}));
            return 0;
        }
        if (name == "Shuffle") {
            auto shuffle = 0;
            if (auto ret = sd_bus_message_read(value, "b", &shuffle); ret < 0) {
                return ret;
            }
            if (static_cast<bool>(shuffle) != current_.shuffle) {
                dispatch(Action::ToggleShuffle);
            }
            return 0;
        }
        if (name == "LoopStatus") {
            const char* status = nullptr;
            if (auto ret = sd_bus_message_read(value, "s", &status); ret < 0) {
                return ret;
            }
            if ((std::string_view(status) != "None") != current_.repeat) {
                dispatch(Action::ToggleRepeat);
            }
            return 0;
        }
        auto rate = 0.;
        return sd_bus_message_read(value, "d", &rate);
    }

    static int onCall(
        sd_bus_message* msg, void* userdata, sd_bus_error* error) {
        return self(userdata)->call(msg, error);
    }

    static int onGet(sd_bus* /*bus*/, const char* /*path*/,
        const char* /*interface*/, const char* property, sd_bus_message* reply,
        void* userdata, sd_bus_error* /*error*/) {
        return self(userdata)->getProperty(property, reply);
    }

    static int onSet(sd_bus* /*bus*/, const char* /*path*/,
        const char* /*interface*/, const char* property, sd_bus_message* value,
        void* userdata, sd_bus_error* /*error*/) {
        return self(userdata)->setProperty(property, value);
    }

  public:
    explicit Impl(Sender<Msg> sender) noexcept : sender_(std::move(sender)) {
        if (sd_bus_open_user(&bus_) < 0) {
            bus_ = nullptr;
            return;
        }
        if (sd_bus_add_object_vtable(bus_, &rootSlot_, ObjectPath,
                RootInterface, rootVtable, this) < 0 ||
            sd_bus_add_object_vtable(bus_, &playerSlot_, ObjectPath,
                PlayerInterface, playerVtable, this) < 0) {
            shutdown();
            return;
        }
        if (sd_bus_request_name(bus_, BusName, 0) < 0) {
            auto unique = std::format("{}.instance{}", BusName, getpid());
            if (sd_bus_request_name(bus_, unique.c_str(), 0) < 0) {
                shutdown();
                return;
            }
        }
        notify_ = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
        if (notify_ < 0) {
            shutdown();
        }
    }

    Impl(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl& operator=(Impl&&) = delete;

    ~Impl() {
        shutdown();
        if (notify_ >= 0) {
            close(notify_);
        }
    }

    [[nodiscard]] int fd() const noexcept {
        return bus_ != nullptr ? sd_bus_get_fd(bus_) : -1;
    }

    [[nodiscard]] int notifyFd() const noexcept {
        return bus_ != nullptr ? notify_ : -1;
    }

    [[nodiscard]] unsigned events() const noexcept {
        if (bus_ == nullptr) {
            return 0;
        }
        auto ret = sd_bus_get_events(bus_);
        return ret > 0 ? static_cast<unsigned>(ret) : 0;
    }

    [[nodiscard]] int timeoutMs() const noexcept {
        auto deadline = std::uint64_t{0};
        if (bus_ == nullptr || sd_bus_get_timeout(bus_, &deadline) < 0 ||
            deadline == UINT64_MAX) {
            return -1;
        }
        auto left = static_cast<std::int64_t>(deadline) - monotonicUsec();
        if (left <= 0) {
            return 0;
        }
        return static_cast<int>((left + UsecPerMsec - 1) / UsecPerMsec);
    }

    std::optional<Action> process() noexcept {
        action_.reset();
        while (bus_ != nullptr) {
            auto ret = sd_bus_process(bus_, nullptr);
            if (ret < 0) {
                shutdown();
                break;
            }
            if (ret == 0) {
                break;
            }
        }
        return action_;
    }

    void flush() noexcept {
        auto value = std::uint64_t{0};
        if (notify_ >= 0) {
            eventfd_read(notify_, &value);
        }
        {
            const auto lock = std::lock_guard(mutex_);
            current_ = pending_;
        }
        if (bus_ != nullptr) {
            emitChanges();
        }
    }

    void publish(Snapshot&& snapshot) noexcept {
        {
            const auto lock = std::lock_guard(mutex_);
            pending_ = std::move(snapshot);
        }
        if (notify_ >= 0) {
            eventfd_write(notify_, 1);
        }
    }
};

// NOLINTBEGIN(*-avoid-c-arrays)
const sd_bus_vtable Mpris::Impl::rootVtable[] = {SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Raise", "", "", onCall, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Quit", "", "", onCall, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_PROPERTY("CanQuit", "b", onGet, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("CanRaise", "b", onGet, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY(
        "HasTrackList", "b", onGet, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("Identity", "s", onGet, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY(
        "SupportedUriSchemes", "as", onGet, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY(
        "SupportedMimeTypes", "as", onGet, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_VTABLE_END};

const sd_bus_vtable Mpris::Impl::playerVtable[] = {SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Next", "", "", onCall, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Previous", "", "", onCall, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Pause", "", "", onCall, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("PlayPause", "", "", onCall, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Stop", "", "", onCall, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Play", "", "", onCall, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("Seek", "x", "", onCall, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("SetPosition", "ox", "", onCall, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_METHOD("OpenUri", "s", "", onCall, SD_BUS_VTABLE_UNPRIVILEGED),
    SD_BUS_SIGNAL("Seeked", "x", 0),
    SD_BUS_PROPERTY("PlaybackStatus", "s", onGet, 0,
        SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_WRITABLE_PROPERTY("LoopStatus", "s", onGet, onSet, 0,
        SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_WRITABLE_PROPERTY(
        "Shuffle", "b", onGet, onSet, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_WRITABLE_PROPERTY(
        "Rate", "d", onGet, onSet, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY(
        "Metadata", "a{sv}", onGet, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_WRITABLE_PROPERTY(
        "Volume", "d", onGet, onSet, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE),
    SD_BUS_PROPERTY("Position", "x", onGet, 0, 0),
    SD_BUS_PROPERTY("MinimumRate", "d", onGet, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("MaximumRate", "d", onGet, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("CanGoNext", "b", onGet, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY(
        "CanGoPrevious", "b", onGet, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("CanPlay", "b", onGet, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("CanPause", "b", onGet, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("CanSeek", "b", onGet, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_PROPERTY("CanControl", "b", onGet, 0, SD_BUS_VTABLE_PROPERTY_CONST),
    SD_BUS_VTABLE_END};
// NOLINTEND(*-avoid-c-arrays)

Mpris::Mpris(Sender<Msg> sender) noexcept : impl_(std::move(sender)) {
}

int Mpris::fd() const noexcept {
    return impl_->fd();
}

int Mpris::notifyFd() const noexcept {
    return impl_->notifyFd();
}

unsigned Mpris::events() const noexcept {
    return impl_->events();
}

int Mpris::timeoutMs() const noexcept {
    return impl_->timeoutMs();
}

std::optional<Action> Mpris::process() noexcept {
    return impl_->process();
}

void Mpris::flush() noexcept {
    impl_->flush();
}

void Mpris::publish(Snapshot&& snapshot) noexcept {
    impl_->publish(std::move(snapshot));
}

// NOLINTEND(cppcoreguidelines-pro-type-vararg,hicpp-vararg)

Mpris::~Mpris() = default;
