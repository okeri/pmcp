#pragma once

#include <thread>

#include "Keymap.hh"
#include "Msg.hh"
#include "channel.hh"

class EventLoop {
    std::thread job_;

  public:
    EventLoop(Sender<Msg> sender, const Keymap& keymap, const char* socketPath);
    EventLoop(const EventLoop&) = delete;
    EventLoop(EventLoop&&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;
    EventLoop& operator=(EventLoop&&) = delete;
    ~EventLoop();
};
