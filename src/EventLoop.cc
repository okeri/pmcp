#include <csignal>

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
        signal(SIGWINCH, sigact);
    }
}

}  // namespace

EventLoop::EventLoop(Sender<Msg> sender, const Keymap& keymap) :
    job_(
        [&keymap](Sender<Msg> msgSender) {
            signal(SIGWINCH, sigact);
            while (true) {
                auto key = input::read();
                msgSender.send(Msg(key));
                if (auto action = keymap.map(key)) {
                    if (*action == Action::Quit) {
                        break;
                    }
                }
            }
        },
        sender) {
    gsender() = std::move(sender);
    maskSigwinch();
}

EventLoop::~EventLoop() {
    if (job_.joinable()) {
        job_.join();
    }
}
