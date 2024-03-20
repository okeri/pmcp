#pragma once

#include "ui.hh"
#include "Scrollable.hh"

template <class Object>
concept Renderable = requires(Object& obj) {
    { ui::render(obj, std::declval<Terminal::Plane&>()) };
};

struct IWidget {  // NOLINT(cppcoreguidelines-special-member-functions)
    virtual void render(Terminal::Plane& plane) = 0;
    virtual void up(unsigned offset) noexcept = 0;
    virtual void down(unsigned offset) noexcept = 0;
    virtual void home(bool force) noexcept = 0;
    virtual void end() noexcept = 0;
    virtual ~IWidget() = default;
};

template <class Object>
class Widget final : public IWidget {
    Object obj_;

  public:
    template <typename... Args>
    explicit Widget(Args&&... args) : obj_(std::forward<Args>(args)...) {
    }

    Widget(const Widget&) = delete;
    Widget(Widget&&) = delete;
    Widget& operator=(const Widget&) = delete;
    Widget& operator=(Widget&&) = delete;
    ~Widget() override = default;

    constexpr Object* ptr() noexcept {
        return &obj_;
    }

    constexpr Object& ref() noexcept {
        return obj_;
    }

    constexpr Object* operator->() noexcept {
        return ptr();
    }

    void render(Terminal::Plane& plane) override {
        static_assert(
            Renderable<Object>, "No render function for class Object");
        ui::render(obj_, plane);
    }

    void up(unsigned offset) noexcept override {
        if constexpr (Scrollable<Object>) {
            obj_.up(offset);
        }
    }

    void down(unsigned offset) noexcept override {
        if constexpr (Scrollable<Object>) {
            obj_.down(offset);
        }
    }

    void home(bool force) noexcept override {
        if constexpr (Scrollable<Object>) {
            obj_.home(force);
        }
    }

    void end() noexcept override {
        if constexpr (Scrollable<Object>) {
            obj_.end();
        }
    }
};
