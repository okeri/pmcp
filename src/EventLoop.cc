#include <csignal>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include "Mpris.hh"
#include "EventLoop.hh"

namespace {

class Bus {
    [[maybe_unused]] Mpris* mpris_;

  public:
    explicit Bus(Mpris* mpris) noexcept : mpris_(mpris) {
    }

#ifdef ENABLE_MPRIS
    [[nodiscard]] int fd() const noexcept {
        return mpris_ != nullptr ? mpris_->fd() : -1;
    }

    [[nodiscard]] int notifyFd() const noexcept {
        return mpris_ != nullptr ? mpris_->notifyFd() : -1;
    }

    [[nodiscard]] unsigned events() const noexcept {
        return mpris_ != nullptr ? mpris_->events() : 0;
    }

    [[nodiscard]] int timeoutMs() const noexcept {
        return mpris_ != nullptr ? mpris_->timeoutMs() : -1;
    }

    std::optional<Action> process() const noexcept {
        return mpris_ != nullptr ? mpris_->process() : std::nullopt;
    }

    void flush() const noexcept {
        if (mpris_ != nullptr) {
            mpris_->flush();
        }
    }
#else
    [[nodiscard]] static int fd() noexcept {
        return -1;
    }

    [[nodiscard]] static int notifyFd() noexcept {
        return -1;
    }

    [[nodiscard]] static unsigned events() noexcept {
        return 0;
    }

    [[nodiscard]] static int timeoutMs() noexcept {
        return -1;
    }

    static std::optional<Action> process() noexcept {
        return std::nullopt;
    }

    static void flush() noexcept {
    }
#endif
};

bool isQuit(const std::optional<Action>& action) noexcept {
    return action && *action == Action::Quit;
}

}  // namespace

EventLoop::EventLoop(Sender<Msg> sender, const Keymap& keymap, Mpris* mpris) :
    job_(
        [&keymap](Sender<Msg> msgSender, Bus bus) {  // NOLINT
            sigset_t mask;
            sigemptyset(&mask);
            sigaddset(&mask, SIGWINCH);
            pthread_sigmask(SIG_BLOCK, &mask, nullptr);
            auto sigfd = signalfd(-1, &mask, SFD_CLOEXEC);

            auto poll = epoll_create1(EPOLL_CLOEXEC);

            constexpr auto MaxEvents = 4;
            constexpr auto BusSlot = 2;
            epoll_event evs[] = {
                {.events = EPOLLIN, .data = {.fd = STDIN_FILENO}},
                {.events = EPOLLIN, .data = {.fd = sigfd}},
                {.events = bus.events(), .data = {.fd = bus.fd()}},
                {.events = EPOLLIN, .data = {.fd = bus.notifyFd()}}};

            for (auto& event : evs) {
                if (event.data.fd >= 0) {
                    epoll_ctl(poll, EPOLL_CTL_ADD, event.data.fd, &event);
                }
            }

            auto rearmBus = [&poll, &evs, &bus]() {
                if (evs[BusSlot].data.fd < 0) {
                    return;
                }
                if (bus.fd() < 0) {
                    epoll_ctl(
                        poll, EPOLL_CTL_DEL, evs[BusSlot].data.fd, nullptr);
                    evs[BusSlot].data.fd = -1;
                } else if (auto flags = bus.events();
                           flags != evs[BusSlot].events) {
                    evs[BusSlot].events = flags;
                    epoll_ctl(poll, EPOLL_CTL_MOD, evs[BusSlot].data.fd,
                        &evs[BusSlot]);
                }
            };

            while (true) {
                epoll_event events[MaxEvents];
                auto action = std::optional<Action>{std::nullopt};
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
                auto eventCount =
                    epoll_wait(poll, events, MaxEvents, bus.timeoutMs());

                for (auto i = 0; i < eventCount; ++i) {
                    if (events[i].data.fd == STDIN_FILENO) {
                        auto key = input::read();
                        msgSender.send(Msg(key));
                        action = keymap.map(key);
                    } else if (events[i].data.fd == sigfd) {
                        signalfd_siginfo info{};
                        read(sigfd, &info, sizeof(info));
                        msgSender.send(Msg(input::Key::Resize));
                    } else if (events[i].data.fd == bus.notifyFd()) {
                        bus.flush();
                    }
                }

                auto request = bus.process();

                if (isQuit(action) || isQuit(request)) {
                    break;
                }
                rearmBus();
            }
            close(sigfd);
        },
        std::move(sender), Bus(mpris)) {
}

EventLoop::~EventLoop() {
    if (job_.joinable()) {
        job_.join();
    }
}
