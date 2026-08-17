#include <unordered_map>
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <print>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>

#include <systemd/sd-bus.h>

// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg,hicpp-vararg)

namespace {

constexpr auto Destination = "org.mpris.MediaPlayer2.pmcp";
constexpr auto ObjectPath = "/org/mpris/MediaPlayer2";
constexpr auto RootInterface = "org.mpris.MediaPlayer2";
constexpr auto PlayerInterface = "org.mpris.MediaPlayer2.Player";
constexpr auto PropsInterface = "org.freedesktop.DBus.Properties";
constexpr auto SecPerMin = 60U;
constexpr auto UsecPerSec = 1000000L;

enum class Kind : std::uint8_t { Method, Status, Monitor };

struct Command {
    Kind kind{Kind::Method};
    const char* interface{nullptr};
    const char* method{nullptr};
};

struct Status {
    std::string title;
    unsigned position{0};
    unsigned duration{0};
    bool playing{false};
    bool paused{false};
};

class Error {
    sd_bus_error error_{};

  public:
    Error() = default;
    Error(const Error&) = delete;
    Error(Error&&) = delete;
    Error& operator=(const Error&) = delete;
    Error& operator=(Error&&) = delete;

    ~Error() {
        sd_bus_error_free(&error_);
    }

    sd_bus_error* ptr() noexcept {
        return &error_;
    }

    [[nodiscard]] std::string what() const {
        constexpr auto NotRunning = "pmcp is not running";
        if (sd_bus_error_has_name(&error_, SD_BUS_ERROR_SERVICE_UNKNOWN) != 0 ||
            sd_bus_error_has_name(&error_, SD_BUS_ERROR_NAME_HAS_NO_OWNER) !=
                0) {
            return NotRunning;
        }
        return error_.message != nullptr ? error_.message : NotRunning;
    }
};

class Reply {
    sd_bus_message* msg_{nullptr};

  public:
    Reply() = default;
    Reply(const Reply&) = delete;
    Reply(Reply&&) = delete;
    Reply& operator=(const Reply&) = delete;
    Reply& operator=(Reply&&) = delete;

    ~Reply() {
        sd_bus_message_unref(msg_);
    }

    sd_bus_message** ptr() noexcept {
        return &msg_;
    }

    sd_bus_message* get() noexcept {
        return msg_;
    }
};

void printStatus(const Status& status) {
    if (!status.playing && !status.paused) {
        std::println("stopped");
        return;
    }
    std::println("{}", status.title);
    std::println("{}{:02}:{:02} / {:02}:{:02}", status.paused ? "[paused] " : "",
        status.position / SecPerMin, status.position % SecPerMin,
        status.duration / SecPerMin, status.duration % SecPerMin);
}

class Client {
    sd_bus* bus_{nullptr};
    bool changed_{false};
    bool gone_{false};

    static int onChange(
        sd_bus_message* /*msg*/, void* userdata, sd_bus_error* /*error*/) {
        static_cast<Client*>(userdata)->changed_ = true;
        return 0;
    }

    static int onNameChange(
        sd_bus_message* msg, void* userdata, sd_bus_error* /*error*/) {
        const char* name = nullptr;
        const char* was = nullptr;
        const char* now = nullptr;
        if (sd_bus_message_read(msg, "sss", &name, &was, &now) < 0) {
            return 0;
        }
        if (std::string_view(name) == Destination &&
            (now == nullptr || *now == '\0')) {
            static_cast<Client*>(userdata)->gone_ = true;
        }
        return 0;
    }

    static void readMetadata(sd_bus_message* reply, Status& status) {
        auto artist = std::string{};
        auto title = std::string{};
        if (sd_bus_message_enter_container(reply, 'a', "{sv}") < 0) {
            return;
        }
        while (sd_bus_message_enter_container(reply, 'e', "sv") > 0) {
            const char* key = nullptr;
            if (sd_bus_message_read(reply, "s", &key) < 0) {
                break;
            }
            auto name = std::string_view(key);
            if (name == "xesam:artist" &&
                sd_bus_message_enter_container(reply, 'v', "as") > 0) {
                if (sd_bus_message_enter_container(reply, 'a', "s") > 0) {
                    const char* value = nullptr;
                    while (sd_bus_message_read(reply, "s", &value) > 0) {
                        if (artist.empty() && value != nullptr) {
                            artist = value;
                        }
                    }
                    sd_bus_message_exit_container(reply);
                }
                sd_bus_message_exit_container(reply);
            } else if (name == "xesam:title" &&
                       sd_bus_message_enter_container(reply, 'v', "s") > 0) {
                const char* value = nullptr;
                if (sd_bus_message_read(reply, "s", &value) > 0 &&
                    value != nullptr) {
                    title = value;
                }
                sd_bus_message_exit_container(reply);
            } else if (name == "mpris:length" &&
                       sd_bus_message_enter_container(reply, 'v', "x") > 0) {
                auto length = std::int64_t{0};
                if (sd_bus_message_read(reply, "x", &length) > 0) {
                    status.duration = static_cast<unsigned>(length / UsecPerSec);
                }
                sd_bus_message_exit_container(reply);
            } else {
                sd_bus_message_skip(reply, "v");
            }
            sd_bus_message_exit_container(reply);
        }
        sd_bus_message_exit_container(reply);
        status.title = artist.empty() ? title : artist + " - " + title;
    }

  public:
    Client() {
        if (sd_bus_open_user(&bus_) < 0) {
            throw std::runtime_error("cannot connect to the session bus");
        }
    }

    Client(const Client&) = delete;
    Client(Client&&) = delete;
    Client& operator=(const Client&) = delete;
    Client& operator=(Client&&) = delete;

    ~Client() {
        sd_bus_flush_close_unref(bus_);
    }

    void call(const Command& command) const {
        auto error = Error();
        auto reply = Reply();
        if (sd_bus_call_method(bus_, Destination, ObjectPath, command.interface,
                command.method, error.ptr(), reply.ptr(), "") < 0) {
            throw std::runtime_error(error.what());
        }
    }

    [[nodiscard]] Status status() const {
        auto result = Status();
        auto error = Error();
        char* state = nullptr;
        if (sd_bus_get_property_string(bus_, Destination, ObjectPath,
                PlayerInterface, "PlaybackStatus", error.ptr(), &state) < 0) {
            throw std::runtime_error(error.what());
        }
        result.playing = std::string_view(state) == "Playing";
        result.paused = std::string_view(state) == "Paused";
        // NOLINTNEXTLINE(cppcoreguidelines-no-malloc,hicpp-no-malloc,cppcoreguidelines-owning-memory)
        free(state);
        if (!result.playing && !result.paused) {
            return result;
        }

        auto position = std::int64_t{0};
        if (sd_bus_get_property_trivial(bus_, Destination, ObjectPath,
                PlayerInterface, "Position", error.ptr(), 'x', &position) >= 0) {
            result.position = static_cast<unsigned>(position / UsecPerSec);
        }

        auto reply = Reply();
        if (sd_bus_get_property(bus_, Destination, ObjectPath, PlayerInterface,
                "Metadata", error.ptr(), reply.ptr(), "a{sv}") < 0) {
            throw std::runtime_error(error.what());
        }
        readMetadata(reply.get(), result);
        return result;
    }

    void monitor() {
        sd_bus_match_signal(bus_, nullptr, Destination, ObjectPath,
            PropsInterface, "PropertiesChanged", onChange, this);
        sd_bus_match_signal(bus_, nullptr, Destination, ObjectPath,
            PlayerInterface, "Seeked", onChange, this);
        sd_bus_match_signal(bus_, nullptr, "org.freedesktop.DBus",
            "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameOwnerChanged",
            onNameChange, this);

        auto report = [](const Status& status) {
            printStatus(status);
            std::fflush(stdout);
        };

        report(status());
        while (true) {
            auto ret = sd_bus_process(bus_, nullptr);
            if (ret < 0) {
                throw std::runtime_error("lost the session bus");
            }
            if (gone_) {
                report(Status());
                return;
            }
            if (changed_) {
                changed_ = false;
                report(status());
            }
            if (ret == 0 && sd_bus_wait(bus_, UINT64_MAX) < 0) {
                throw std::runtime_error("lost the session bus");
            }
        }
    }
};

}  // namespace

int main(int argc, char* argv[]) try {
    const auto commands = std::unordered_map<std::string, Command>{
        {"quit", {.interface = RootInterface, .method = "Quit"}},
        {"stop", {.interface = PlayerInterface, .method = "Stop"}},
        {"pause", {.interface = PlayerInterface, .method = "PlayPause"}},
        {"prev", {.interface = PlayerInterface, .method = "Previous"}},
        {"next", {.interface = PlayerInterface, .method = "Next"}},
        {"play", {.interface = PlayerInterface, .method = "Play"}},
        {"status", {.kind = Kind::Status}},
        {"monitor", {.kind = Kind::Monitor}}};

    auto usage = [&commands](const char* name) {
        std::print("usage: {} <", name);
        std::string sep{};
        for (const auto& cmdName : std::views::keys(commands)) {
            std::print("{}{}", sep, cmdName);
            sep = "|";
        }
        std::println(">");
    };

    if (argc < 2) {
        usage(argv[0]);
        return -1;
    }
    auto cmd = std::string(argv[1]);
    std::ranges::transform(
        cmd, cmd.begin(), [](unsigned char sym) { return std::tolower(sym); });

    auto found = commands.find(cmd);
    if (found == commands.end()) {
        usage(argv[0]);
        return -2;
    }

    auto client = Client();
    switch (found->second.kind) {
        case Kind::Method:
            client.call(found->second);
            break;
        case Kind::Status:
            printStatus(client.status());
            break;
        case Kind::Monitor:
            client.monitor();
            break;
    }
    return 0;
} catch (const std::exception& error) {
    std::fputs(error.what(), stderr);
    std::fputc('\n', stderr);
    return -3;
}

// NOLINTEND(cppcoreguidelines-pro-type-vararg,hicpp-vararg)
