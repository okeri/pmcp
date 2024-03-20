#include <fstream>
#include <limits>
#include <cpptoml.h>

#include "utf8.hh"
#include "Toml.hh"

class Toml::Impl {
    std::shared_ptr<cpptoml::base> node_;
    friend class Toml;

  public:
    Impl() : node_(cpptoml::make_table()) {
    }

    explicit Impl(const std::string& filename) {
        try {
            node_ = cpptoml::parse_file(filename);
        } catch (cpptoml::parse_exception& e) {
        }
    }

    explicit Impl(std::shared_ptr<cpptoml::base>&& node) noexcept :
        node_(std::move(node)) {
    }

    explicit Impl(const std::shared_ptr<cpptoml::base>& node) noexcept :
        node_(node) {
    }

    explicit operator bool() const noexcept {
        return static_cast<bool>(node_);
    }

    [[nodiscard]] bool enumArray(EnumArrayCallbackW visit) const noexcept {
        if (!node_) {
            return false;
        }

        if (auto array = node_->as_array()) {
            for (const auto& value : *array) {
                auto str = value->as<std::string>()->get();
                visit(utf8::convert(str));
            }
            return true;
        }
        return false;
    }

    [[nodiscard]] bool enumArray(EnumArrayCallback visit) const noexcept {
        if (!node_) {
            return false;
        }

        if (auto array = node_->as_array()) {
            for (const auto& value : *array) {
                visit(value->as<std::string>()->get());
            }
            return true;
        }
        return false;
    }

    void enumTable(EnumTableCallback visit) const noexcept {
        if (!node_) {
            return;
        }
        for (const auto& element : *node_->as_table()) {
            visit(element.first, Toml(Node(element.second)));
        }
    }

    template <class R>
    [[nodiscard]] std::optional<R> as() const {
        if (node_) {
            if constexpr (std::is_same_v<std::wstring, R>) {
                if (auto result = node_->as<std::string>()->get();
                    !result.empty()) {
                    return utf8::convert(result);
                }
            } else {
                if (auto result = node_->as<std::decay_t<R>>()) {
                    return result->get();
                }
            }
        }
        return {};
    }

    [[nodiscard]] std::optional<Toml> get(const std::string& key) const {
        if (node_) {
            try {
                return Toml(Node(node_->as_table()->get(key)));
            } catch (std::out_of_range& e) {
            }
        }
        return {};
    }

    void push(
        const std::string& key, const std::shared_ptr<cpptoml::base>& item) {
        if (node_) {
            node_->as_table()->insert(key, item);
        }
    }
    void save(const std::string& filename) {
        std::ofstream file(filename);
        file << *node_;
    }
};

Toml::Toml() = default;

Toml::Toml(const std::string& filename) : impl_(filename) {
}

Toml::Toml(Node&& node) noexcept : impl_(std::move(node)) {
}

Toml::~Toml() = default;

bool Toml::enumArray(EnumArrayCallback visit) const noexcept {
    return impl_->enumArray(visit);
}

bool Toml::enumArray(EnumArrayCallbackW visit) const noexcept {
    return impl_->enumArray(visit);
}

bool Toml::enumArray(
    const std::string& key, EnumArrayCallback visit) const noexcept {
    if (auto values = impl_->get(key)) {
        return values->enumArray(visit);
    }
    return false;
}

bool Toml::enumArray(
    const std::string& key, EnumArrayCallbackW visit) const noexcept {
    if (auto values = impl_->get(key)) {
        return values->enumArray(visit);
    }
    return false;
}

void Toml::enumTable(EnumTableCallback visit) const noexcept {
    impl_->enumTable(visit);
}

std::optional<Toml> Toml::operator[](const std::string& key) const {
    return impl_->get(key);
}

Toml::operator bool() const noexcept {
    return impl_->operator bool();
}

void Toml::push(const std::string& key, const std::wstring& value) {
    impl_->push(key, cpptoml::make_value(utf8::convert(value)));
}

void Toml::push(const std::string& key, bool value) {
    impl_->push(key, cpptoml::make_value(value));
}

void Toml::save(const std::string& filename) {
    impl_->save(filename);
}

std::optional<bool> Toml::asBool() const noexcept {
    return impl_->as<bool>();
}
std::optional<std::string> Toml::asString() const noexcept {
    return impl_->as<std::string>();
}
std::optional<std::wstring> Toml::asWstring() const noexcept {
    return impl_->as<std::wstring>();
}
