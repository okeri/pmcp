#include <csignal>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <unistd.h>

#include "Server.hh"
#include "EventLoop.hh"

EventLoop::EventLoop(
    Sender<Msg> sender, const Keymap& keymap, const char* socketPath) :
    job_(
        [&keymap](Sender<Msg> msgSender, const char* sockPath) {  // NOLINT
            sigset_t mask;
            sigemptyset(&mask);
            sigaddset(&mask, SIGWINCH);
            pthread_sigmask(SIG_BLOCK, &mask, nullptr);
            auto sigfd = signalfd(-1, &mask, SFD_CLOEXEC);

            auto srv = Server(sockPath);
            auto poll = epoll_create1(EPOLL_CLOEXEC);

            constexpr auto MaxEvents = 4;
            epoll_event evs[] = {
                {.events = EPOLLIN, .data = {.fd = STDIN_FILENO}},
                {.events = EPOLLIN, .data = {.fd = srv.socket()}},
                {.events = EPOLLIN, .data = {.fd = sigfd}},
                {.events = 0, .data = {.fd = -1}}};

            for (auto i = 0; i < 3; ++i) {
                epoll_ctl(poll, EPOLL_CTL_ADD, evs[i].data.fd, &evs[i]);
            }

            auto closeClient = [&poll, &evs]() {
                if (evs[3].data.fd >= 0) {
                    epoll_ctl(poll, EPOLL_CTL_DEL, evs[3].data.fd, nullptr);
                    evs[3].data.fd = -1;
                }
            };

            while (true) {
                epoll_event events[MaxEvents];
                auto action = std::optional<Action>{std::nullopt};
                // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-array-to-pointer-decay)
                auto eventCount = epoll_wait(poll, events, MaxEvents, -1);

                for (auto i = 0; i < eventCount; ++i) {
                    if (events[i].data.fd == STDIN_FILENO) {
                        auto key = input::read();
                        msgSender.send(Msg(key));
                        action = keymap.map(key);
                    } else if (events[i].data.fd == srv.socket()) {
                        closeClient();
                        auto client = srv.accept();
                        if (client >= 0) {
                            evs[3].data.fd = client;
                            epoll_ctl(
                                poll, EPOLL_CTL_ADD, evs[3].data.fd, &evs[3]);
                        }
                    } else if (events[i].data.fd == sigfd) {
                        signalfd_siginfo info;
                        read(sigfd, &info, sizeof(info));
                        msgSender.send(Msg(input::Key::Resize));
                    } else if (events[i].data.fd == evs[3].data.fd) {
                        action = Server::read(events[i].data.fd);
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
            close(sigfd);
        },
        sender, socketPath) {
}

EventLoop::~EventLoop() {
    if (job_.joinable()) {
        job_.join();
    }
}
