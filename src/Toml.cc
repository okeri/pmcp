#include <fstream>
#include <toml++/toml.hpp>

#include "utf8.hh"
#include "Toml.hh"

namespace {

template <class Value>
void push(toml::node* node, std::string_view key, Value item) {
    if (node) {
        if (auto* table = node->as_table()) {
            table->insert(key, item);
        }
    }
}

template <class Value>
[[nodiscard]] std::optional<Value> as(toml::node* node) {
    if (node) {
        using Type = std::decay_t<Value>;
        if constexpr (std::is_same_v<std::wstring, Value> ||
                      std::is_same_v<std::wstring_view, Value>) {
            if (auto result = node->as<std::string>()->get(); !result.empty()) {
                return utf8::convert(result);
            }
        } else {
            if (auto result = node->as<Type>()) {
                return result->get();
            }
        }
    }
    return {};
}

template <class Visitor>
bool enumArray(toml::node* node, const Visitor& visit) noexcept {
    if (node) {
        if (auto* array = node->as_array()) {
            for (const auto& value : *array) {
                using Type = std::decay_t<Visitor>;
                if constexpr (std::is_same_v<Type,
                                  Toml::EnumArrayCallback<std::wstring>> ||
                              std::is_same_v<Type,
                                  Toml::EnumArrayCallback<std::wstring_view>>) {
                    visit(utf8::convert(value.as<std::string>()->get()));
                } else {
                    visit(value.as<std::string>()->get());
                }
            }
            return true;
        }
    }
    return false;
}

}  // namespace

Toml::Toml() : node_(std::make_unique<toml::table>()) {
}

Toml::Toml(std::string_view filename) {
    try {
        node_ = std::make_unique<toml::table>(toml::parse_file(filename));
    } catch (const toml::parse_error& e) {  // NOLINT
    }
}

Toml::Toml(Toml::Node* node) noexcept : node_(node), doFree_(false) {
}

Toml::~Toml() {
    if (node_ && !doFree_) {
        (void)node_.release();
    }
}

Toml::Toml(Toml&& other) noexcept : node_(std::move(other.node_)) {
    std::swap(doFree_, other.doFree_);
}

bool Toml::enumArray(
    const EnumArrayCallback<std::string_view>& visit) const noexcept {
    return ::enumArray(node_.get(), visit);
}

bool Toml::enumArray(
    const EnumArrayCallback<std::wstring_view>& visit) const noexcept {
    return ::enumArray(node_.get(), visit);
}

bool Toml::enumArray(std::string_view key,
    const EnumArrayCallback<std::string_view>& visit) const noexcept {
    if (auto values = operator[](key)) {
        return values->enumArray(visit);
    }
    return false;
}

bool Toml::enumArray(std::string_view key,
    const EnumArrayCallback<std::wstring_view>& visit) const noexcept {
    if (auto values = operator[](key)) {
        return values->enumArray(visit);
    }
    return false;
}

void Toml::enumTable(const EnumTableCallback& visit) const noexcept {
    if (node_) {
        if (auto* table = node_->as_table()) {
            for (const auto& [key, node] : *table) {
                visit(key.str(), Toml(&node));
            }
        }
    }
}

std::optional<Toml> Toml::operator[](std::string_view key) const {
    if (node_) {
        if (auto* table = node_->as_table()) {
            if (auto* value = table->get(key)) {
                return Toml(value);
            }
        }
    }
    return {};
}

Toml::operator bool() const noexcept {
    return static_cast<bool>(node_);
}

void Toml::save(const std::string& filename) {
    if (node_) {
        if (auto* table = node_->as_table()) {
            std::ofstream file(filename);
            file << *table;
        }
    }
}

template <>
std::optional<bool> Toml::as() const noexcept {
    return ::as<bool>(node_.get());
}

template <>
std::optional<std::string> Toml::as() const noexcept {
    return ::as<std::string>(node_.get());
}

template <>
std::optional<std::wstring> Toml::as() const noexcept {
    return ::as<std::wstring>(node_.get());
}

template <>
void Toml::push(std::string_view key, const std::wstring& value) {
    ::push(node_.get(), key, utf8::convert(value));
}

template <>
void Toml::push(std::string_view key, bool value) {
    ::push(node_.get(), key, value);
}
