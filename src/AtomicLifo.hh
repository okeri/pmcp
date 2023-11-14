#pragma once

#include <optional>
#include <atomic>

template <class Data>
class AtomicLifo {
    struct Node {
        Data data;
        Node* next;
    };

    std::atomic<Node*> head_{nullptr};

  public:
    void push(Data&& data) {
        auto node = new Node;
        node->data = std::move(data);
        node->next = head_.load(std::memory_order::acquire);
        while (!head_.compare_exchange_weak(node->next, node)) {
        }
    }

    std::optional<Data> pop() {
        if (auto head = head_.load()) {
            while (!head_.compare_exchange_weak(head, head->next)) {
            }
            auto data = std::move(head->data);
            delete head;
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
