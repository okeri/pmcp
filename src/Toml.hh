#pragma once

#include <optional>
#include <string>
#include <functional>
#include <memory>

class Toml {
#ifdef TOMLPLUSPLUS_HPP
    using Node = toml::node;
#else
    class Node;
#endif
    std::unique_ptr<Node> node_;
    bool doFree_{true};

  public:
    Toml();
    explicit Toml(std::string_view filename);
    explicit Toml(Node* node) noexcept;
    Toml(Toml&& other) noexcept;

    Toml(const Toml&) = delete;
    Toml& operator=(Toml&&) = delete;
    Toml& operator=(const Toml&) = delete;
    ~Toml();

    template <class Value>
    using EnumArrayCallback = std::move_only_function<void(Value) const>;
    bool enumArray(
        const EnumArrayCallback<std::string_view>& visit) const noexcept;
    bool enumArray(
        const EnumArrayCallback<std::wstring_view>& visit) const noexcept;
    bool enumArray(std::string_view key,
        const EnumArrayCallback<std::string_view>& visit) const noexcept;
    bool enumArray(std::string_view key,
        const EnumArrayCallback<std::wstring_view>& visit) const noexcept;

    using EnumTableCallback =
        std::move_only_function<void(std::string_view, const Toml&) const>;
    void enumTable(const EnumTableCallback& visit) const noexcept;

    [[nodiscard]] std::optional<Toml> operator[](std::string_view key) const;

    explicit operator bool() const noexcept;
    void save(const std::string& filename);

    template <class Value>
    [[nodiscard]] std::optional<Value> as() const noexcept;

    template <class Value>
    [[nodiscard]] std::optional<Value> get(std::string_view key) const {
        return operator[](key).and_then(
            [](const Toml& t) { return t.as<Value>(); });
    }

    template <class Value>
    void push(std::string_view key, Value value);
};
