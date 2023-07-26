#pragma once

#include <memory>

#include "AtomicLifo.hh"

template <class Message>
using SyncState = AtomicLifo<Message>;

template <class Message>
class Sender;

template <class Message>
class Receiver
{
    std::shared_ptr<SyncState<Message>> state_;

    template <class M>
    friend std::pair<Sender<M>, Receiver<M>> channel();

    Receiver() : state_(std::make_shared<SyncState<Message>>())
    {}

  public:
    Receiver(Receiver&& other) noexcept : state_(std::move(other.state_))
    {}

    Receiver& operator=(Receiver&& other) noexcept
    {
        state_ = std::move(other.state_);
        return *this;
    }

    Receiver(const Receiver&) = delete;
    Receiver& operator=(const Receiver&) = delete;

    Message recv() noexcept
    {
        while (true)
        {
            auto result = state_->pop();
            if (result)
            {
                return *result;
            }
            state_->waitNonEmpty();
        }
    }

    std::optional<Message> try_recv() noexcept
    {
        return state_->pop();
    }
    ~Receiver() = default;
};

template <class Message>
class Sender
{
    std::shared_ptr<SyncState<Message>> state_;

    template <class M>
    friend std::pair<Sender<M>, Receiver<M>> channel();

    explicit Sender(const std::shared_ptr<SyncState<Message>>& state) :
        state_(state)
    {}

  public:
    Sender() = default;
    ~Sender() = default;
    Sender(Sender&& other) noexcept : state_(std::move(other.state_))
    {}

    Sender(const Sender& other) : state_(other.state_)
    {}

    Sender& operator=(Sender&& other) noexcept
    {
        state_ = std::move(other.state_);
        return *this;
    }

    Sender& operator=(const Sender& other)
    {
        state_ = other.state_;
        return *this;
    }

    void send(Message&& message) const noexcept
    {
        state_->push(std::move(message));
        state_->notify();
    }
};

template <class Message>
std::pair<Sender<Message>, Receiver<Message>> channel()
{
    auto receiver = Receiver<Message>();
    auto sender = Sender<Message>(receiver.state_);
    return {std::move(sender), std::move(receiver)};
}
