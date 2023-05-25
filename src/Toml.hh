#pragma once

#include <optional>
#include <string>
#include <functional>

#include "PImpl.hh"

class Toml {
    class Impl;
    using Node = PImpl<Impl, 16, 8>;
    Node impl_;

  public:
    using EnumArrayCallback = std::function<void(const std::string& value)>;
    using EnumArrayCallbackW = std::function<void(const std::wstring& value)>;
    using EnumTableCallback =
        std::function<void(const std::string& value, const Toml& toml)>;

    explicit Toml(Node&& node) noexcept;
    explicit Toml(const std::string& filename);

    Toml();
    Toml(const Toml&) = delete;
    Toml(Toml&&) = default;
    Toml& operator=(const Toml&) = delete;
    Toml& operator=(Toml&&) = delete;
    ~Toml();

    bool enumArray(EnumArrayCallback cb) const noexcept;   // NOLINT
    bool enumArray(EnumArrayCallbackW cb) const noexcept;  // NOLINT
    bool enumArray(                                        // NOLINT
        const std::string& key, EnumArrayCallback cb) const noexcept;
    bool enumArray(const std::string& key,  // NOLINT
        EnumArrayCallbackW cb) const noexcept;

    void enumTable(EnumTableCallback cb) const noexcept;

    [[nodiscard]] std::optional<Toml> operator[](const std::string& key) const;

    explicit operator bool() const noexcept;

    [[nodiscard]] std::optional<bool> as_bool() const noexcept;
    [[nodiscard]] std::optional<std::string> as_string() const noexcept;
    [[nodiscard]] std::optional<std::wstring> as_wstring() const noexcept;

    template <class T>
    std::optional<T> as() const noexcept {
        using R = std::decay_t<T>;
        if constexpr (std::is_same_v<R, bool>) {
            return as_bool();
        } else if constexpr (std::is_same_v<R, std::wstring>) {
            return as_wstring();
        } else if constexpr (std::is_same_v<R, std::string>) {
            return as_string();
        } else {
            return std::nullopt;
        }
    }

    template <class T>
    std::optional<T> get(const std::string& key) const {
        auto val = operator[](key);
        if (val) {
            return val->as<T>();
        }
        return std::nullopt;
    }

    void push(const std::string& key, const std::wstring& value);
    void push(const std::string& key, bool value);
    void save(const std::string& filename);
};
