#pragma once

namespace input {

enum Key : wchar_t {
    Null = 0,
    SpecialBase = 0x110000,
    Resize,
    Backspace,
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    PgUp,
    PgDown,
    Insert,
    Delete,
    Enter,
    Tab,
    Esc,
    F0,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    F13,
    F14,
    F15,
    F16,
    F17,
    F18,
    F19,
    F20,
    CtrlBase = 0x1000000,
    AltBase = 0x2000000,
    SuperBase = 0x4000000,
};

constexpr Key operator|(const Key key1, const Key key2) {
    return static_cast<Key>(static_cast<wchar_t>(key1) | static_cast<wchar_t>(key2));
}

constexpr Key& operator|=(Key& key1, const Key key2) {
    return key1 = key1 | key2;
}

// NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
constexpr Key key(wchar_t value) noexcept {
    return static_cast<Key>(value);
}

Key read() noexcept;

}  // namespace input
