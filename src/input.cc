#include <string_view>
#include <unordered_map>

#include <unistd.h>

#include "input.hh"

namespace input {

Key read() noexcept {
    constexpr auto BufferSize = 8;
    char buffer[BufferSize] = {0};
    auto num = ::read(STDIN_FILENO, buffer, sizeof(buffer));
    static std::unordered_map<std::string_view, Key> seqTable = {
        {"A", Up},
        {"B", Down},
        {"C", Right},
        {"D", Left},
        {"F", End},
        {"H", Home},
        {"P", F1},
        {"Q", F2},
        {"R", F3},
        {"S", F4},
        {"[A", F1},
        {"[B", F2},
        {"[C", F3},
        {"[D", F4},
        {"[E", F5},
        {"1~", Home},
        {"2~", Insert},
        {"3~", Delete},
        {"4~", End},
        {"5~", PgUp},
        {"6~", PgDown},
        {"7~", Home},
        {"8~", End},
        {"10~", F0},
        {"11~", F1},
        {"12~", F2},
        {"13~", F3},
        {"14~", F4},
        {"15~", F5},
        {"17~", F6},
        {"18~", F7},
        {"19~", F8},
        {"20~", F9},
        {"21~", F10},
        {"23~", F11},
        {"24~", F12},
        {"25~", F13},
        {"26~", F14},
        {"28~", F15},
        {"29~", F16},
        {"31~", F17},
        {"32~", F18},
        {"33~", F19},
        {"34~", F20},
    };

    auto lookupCSI = [](std::string_view csi) {
        auto found = seqTable.find(csi);
        if (found != seqTable.end()) {
            return found->second;
        }
        return Null;
    };
    // NOLINTBEGIN(readability-magic-numbers)
    switch (buffer[0]) {
        case 0x1b:
            if (num == 1) {
                return Esc;
            }
            switch (buffer[1]) {
                case 'O':
                    return static_cast<Key>(F1 + buffer[2] - 'P');
                case '[':
                    return lookupCSI(&buffer[2]);
                default:
                    return static_cast<Key>(buffer[1]) | AltBase;
            }
        case '\r':
        case '\n':
            return Enter;
        case '\t':
            return Tab;
        case 0x7f:
            return Backspace;
        case 0:
            return Null;
        default:
            if (buffer[0] > 0 && buffer[0] < 0x1b) {
                return static_cast<Key>(buffer[0] - 1 + 'a') | CtrlBase;
            } else if (buffer[0] > 0x1b && buffer[0] < 0x20) {
                return static_cast<Key>(buffer[0] - 0x1c + '4') | CtrlBase;
            }
            return static_cast<Key>(buffer[0]);
    }
    // NOLINTEND(readability-magic-numbers)
}

}  // namespace input
