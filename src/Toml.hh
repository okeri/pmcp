#pragma once

#include <optional>
#include <string>
#include <functional>

#include "PImpl.hh"

class Toml {
    class Impl;
    using Node = PImpl<Impl, 16, 8>;  // NOLINT(readability-magic-numbers)
    Node impl_;

  public:
    using EnumArrayCallback =
        const std::function<void(const std::string& value)>&;
    using EnumArrayCallbackW =
        const std::function<void(const std::wstring& value)>&;
    using EnumTableCallback =
        const std::function<void(const std::string& value, const Toml& toml)>&;

    explicit Toml(Node&& node) noexcept;
    explicit Toml(const std::string& filename);

    Toml();
    Toml(const Toml&) = delete;
    Toml(Toml&&) = default;
    Toml& operator=(const Toml&) = delete;
    Toml& operator=(Toml&&) = delete;
    ~Toml();

    bool enumArray(EnumArrayCallback visit) const noexcept;   // NOLINT
    bool enumArray(EnumArrayCallbackW visit) const noexcept;  // NOLINT
    bool enumArray(                                           // NOLINT
        const std::string& key, EnumArrayCallback visit) const noexcept;
    bool enumArray(const std::string& key,  // NOLINT
        EnumArrayCallbackW visit) const noexcept;

    void enumTable(EnumTableCallback visit) const noexcept;

    [[nodiscard]] std::optional<Toml> operator[](const std::string& key) const;

    explicit operator bool() const noexcept;

    [[nodiscard]] std::optional<bool> asBool() const noexcept;
    [[nodiscard]] std::optional<std::string> asString() const noexcept;
    [[nodiscard]] std::optional<std::wstring> asWstring() const noexcept;

    template <class T>
    [[nodiscard]] std::optional<T> as() const noexcept {
        using R = std::decay_t<T>;
        if constexpr (std::is_same_v<R, bool>) {
            return asBool();
        } else if constexpr (std::is_same_v<R, std::wstring>) {
            return asWstring();
        } else if constexpr (std::is_same_v<R, std::string>) {
            return asString();
        } else {
            return {};
        }
    }

    template <class T>
    [[nodiscard]] std::optional<T> get(const std::string& key) const {
        auto val = operator[](key);
        if (val) {
            return val->as<T>();
        }
        return {};
    }

    void push(const std::string& key, const std::wstring& value);
    void push(const std::string& key, bool value);
    void save(const std::string& filename);
};
