#pragma once

#include <memory>
#include <optional>
#include <condition_variable>
#include <mutex>

template <class Message>
struct SyncState {
    std::mutex mutex_;
    std::condition_variable cv_;
    std::optional<Message> message_;
};

template <class Message>
class Sender;

template <class Message>
class Receiver {
    std::shared_ptr<SyncState<Message>> state_;

    template <class M>
    friend std::pair<Sender<M>, Receiver<M>> channel();

    Receiver() : state_(std::make_shared<SyncState<Message>>()) {
    }

  public:
    Receiver(Receiver&& other) noexcept : state_(std::move(other.state_)) {
    }

    Receiver& operator=(Receiver&& other) noexcept {
        state_ = std::move(other.state_);
        return *this;
    }

    Receiver(const Receiver&) = delete;
    Receiver& operator=(const Receiver&) = delete;

    Message recv() noexcept {
        std::unique_lock lock(state_->mutex_);
        state_->cv_.wait(lock, [this] { return state_->message_.has_value(); });
        auto result = std::move(state_->message_.value());
        state_->message_ = std::nullopt;
        return result;
    }

    std::optional<Message> try_recv() noexcept {
        std::lock_guard lock(state_->mutex_);
        auto result = std::move(state_->message_);
        state_->message_ = std::nullopt;
        return result;
    }
    ~Receiver() = default;
};

template <class Message>
class Sender {
    std::shared_ptr<SyncState<Message>> state_;

    template <class M>
    friend std::pair<Sender<M>, Receiver<M>> channel();

    explicit Sender(const std::shared_ptr<SyncState<Message>>& state) :
        state_(state) {
    }

  public:
    Sender() = default;
    ~Sender() = default;
    Sender(Sender&& other) noexcept : state_(std::move(other.state_)) {
    }

    Sender(const Sender& other) : state_(other.state_) {
    }

    Sender& operator=(Sender&& other) noexcept {
        state_ = std::move(other.state_);
        return *this;
    }

    Sender& operator=(const Sender& other) {
        state_ = other.state_;
        return *this;
    }

    bool send(Message&& message) const noexcept {
        {
            std::lock_guard lock(state_->mutex_);
            if (state_->message_) {
                // TODO:
            }
            state_->message_ = std::move(message);
        }
        state_->cv_.notify_all();
        return true;
    }
};

template <class Message>
std::pair<Sender<Message>, Receiver<Message>> channel() {
    auto receiver = Receiver<Message>();
    auto sender = Sender<Message>(receiver.state_);
    return {std::move(sender), std::move(receiver)};
}
