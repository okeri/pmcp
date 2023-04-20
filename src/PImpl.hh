#pragma once

#include <cstddef>
#include <utility>

template <class T, size_t Len, size_t Align>
class PImpl {
    alignas(Align) std::byte data_[Len];

    T* ptr() noexcept {
        return reinterpret_cast<T*>(data_);
    }

    const T* ptr() const noexcept {
        return reinterpret_cast<const T*>(data_);
    }

  public:
    template <class... Args>
    explicit PImpl(Args&&... args) {  // NOLINT:hicpp-member-init
        static_assert(Len == sizeof(T), "Len and sizeof(T) mismatch");
        static_assert(Align == alignof(T), "Align and alignof(T) mismatch");
        new (ptr()) T(std::forward<Args>(args)...);
    }

    PImpl(PImpl&& other) noexcept {  // NOLINT:hicpp-member-init
        new (ptr()) T(std::move(*other.ptr()));
    }

    PImpl& operator=(PImpl&& other) = delete;
    PImpl(const PImpl&) = delete;
    PImpl& operator=(const PImpl&) = delete;

    T* operator->() noexcept {
        return ptr();
    }

    const T* operator->() const noexcept {
        return ptr();
    }

    T& operator*() noexcept {
        return *ptr();
    }

    const T& operator*() const noexcept {
        return *ptr();
    }

    ~PImpl() noexcept {
        ptr()->~T();
    }
};
