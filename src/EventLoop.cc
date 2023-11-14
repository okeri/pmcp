#include <csignal>
#include <sys/epoll.h>

#include "Server.hh"
#include "EventLoop.hh"

namespace {

Sender<Msg>& gsender() noexcept {
    static Sender<Msg> data;
    return data;
}

void maskSigwinch() noexcept {
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGWINCH);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);
}

void sigact(int sig) noexcept {
    if (sig == SIGWINCH) {
        gsender().send(input::Key::Resize);
    }
}

}  // namespace

EventLoop::EventLoop(
    Sender<Msg> sender, const Keymap& keymap, const char* socketPath) :
    job_(
        [&keymap](Sender<Msg> msgSender, const char* sockPath) {
            signal(SIGWINCH, sigact);
            auto srv = Server(sockPath);
            auto poll = epoll_create1(EPOLL_CLOEXEC);

            constexpr auto maxEvents = 3;
            epoll_event evs[maxEvents] = {{EPOLLIN, {.fd = STDIN_FILENO}},
                {EPOLLIN, {.fd = srv.socket()}}, {0, {.fd = -1}}};

            for (auto i = 0; i < 2; ++i) {
                epoll_ctl(poll, EPOLL_CTL_ADD, evs[i].data.fd, &evs[i]);
            }

            auto closeClient = [&poll, &evs]() {
                if (evs[2].data.fd >= 0) {
                    epoll_ctl(poll, EPOLL_CTL_DEL, evs[2].data.fd, nullptr);
                    evs[2].data.fd = -1;
                }
            };

            while (true) {
                epoll_event events[maxEvents];
                auto action = std::optional<Action>{std::nullopt};
                auto eventCount = epoll_wait(poll, events, maxEvents, -1);

                for (auto i = 0; i < eventCount; ++i) {
                    if (events[i].data.fd == STDIN_FILENO) {
                        auto key = input::read();
                        msgSender.send(Msg(key));
                        action = keymap.map(key);
                    } else if (events[i].data.fd == srv.socket()) {
                        closeClient();
                        auto client = srv.accept();
                        if (client >= 0) {
                            evs[2].data.fd = client;
                            epoll_ctl(
                                poll, EPOLL_CTL_ADD, evs[2].data.fd, &evs[2]);
                        }
                    } else if (events[i].data.fd == evs[2].data.fd) {
                        action = srv.read(events[i].data.fd);
                        if (action) {
                            msgSender.send(Msg(*action));
                        } else {
                            closeClient();
                        }
                    }
                }

                if (action && *action == Action::Quit) {
                    break;
                }
            }
        },
        sender, socketPath) {
    gsender() = std::move(sender);
    maskSigwinch();
}

EventLoop::~EventLoop() {
    if (job_.joinable()) {
        job_.join();
    }
}
