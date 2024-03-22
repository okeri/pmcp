#pragma once

#include <optional>
#include <atomic>

template <class Data>
class AtomicLifo {
    struct Node {
        Data data;
        Node* next{nullptr};
        explicit Node(Data&& newData) : data(std::move(newData)) {
        }
    };

    std::atomic<Node*> head_{nullptr};

  public:
    void push(Data&& data) {
        // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
        auto* node = new Node(std::move(data));
        node->next = head_.load();
        while (!head_.compare_exchange_weak(node->next, node)) {
        }
    }

    std::optional<Data> pop() {
        if (auto head = head_.load()) {
            while (!head_.compare_exchange_weak(head, head->next)) {
            }
            auto data = std::move(head->data);
            delete head;  // NOLINT(cppcoreguidelines-owning-memory)
            return data;
        }
       return {};
    }

    void waitNonEmpty() noexcept {
        head_.wait(nullptr);
    }

    void notify() noexcept {
        head_.notify_one();
    }
};
