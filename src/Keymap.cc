#include <array>
#include <algorithm>
#include <utility>

#include "Toml.hh"
#include "Keymap.hh"

namespace {

const std::vector<std::wstring> keyNames = {L"backspace", L"left", L"right",
    L"up", L"down", L"home", L"end", L"pgup", L"pgdown", L"insert", L"delete",
    L"enter", L"tab", L"esc", L"f0", L"f1", L"f2", L"f3", L"f4", L"f5", L"f6",
    L"f7", L"f8", L"f9", L"f10", L"f11", L"f12", L"f13", L"f14", L"f15", L"f16",
    L"f17", L"f18", L"f19", L"f20"};

constexpr auto NamesStart = 0x110002;

constexpr input::Key operator&(const input::Key lhs, const input::Key rhs) {
    return static_cast<input::Key>(
        std::to_underlying(lhs) & std::to_underlying(rhs));
}

constexpr input::Key operator~(const input::Key key) {
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    return static_cast<input::Key>(~std::to_underlying(key));
}

constexpr input::Key& operator&=(input::Key& lhs, const input::Key rhs) {
    return lhs = lhs & rhs;
}

input::Key keyFromName(const std::wstring_view& name) {
    auto low = std::wstring(name);
    std::ranges::transform(
        low, low.begin(), [](wchar_t sym) { return std::tolower(sym); });
    if (low == L"alt") {
        return input::Key::AltBase;
    }
    if (low == L"ctrl") {
        return input::Key::CtrlBase;
    }
    if (auto found = std::ranges::find(keyNames, low);
        found != keyNames.end()) {
        return static_cast<input::Key>(
            NamesStart + std::distance(keyNames.begin(), found));
    }
    return input::key(name[0]);
}

template <typename Pred, typename IsSep>
void strBreak(std::wstring_view input, Pred handler, IsSep issep,
    const size_t start = 0, const size_t endpoint = std::wstring_view::npos) {
    const auto* tail = input.data() + start;
    const auto* end = endpoint == std::wstring_view::npos
                          ? input.data() + input.size()
                          : input.data() + endpoint;
    const auto* head = end;
    for (; tail < end; ++tail) {
        if (!issep(*tail)) {
            if (head == end) {
                head = tail;
            }
        } else {
            if (head != end) {
                if (!handler(head, tail)) {
                    return;
                }
                head = end;
            }
        }
    }
    if (head != end) {
        handler(head, tail);
    }
}

struct ActionDescription {
    std::string name;
    std::wstring description;
};

constexpr auto Count = static_cast<unsigned>(Action::Count);
const std::array<ActionDescription, Count> descriptions{{
    {.name = "quit", .description = L"Quit program"},
    {.name = "go",
        .description = L"Play a playlist entry or file or enter to directory"},
    {.name = "up", .description = L"Up"},
    {.name = "down", .description = L"Down"},
    {.name = "pgup", .description = L"Page up"},
    {.name = "pgdown", .description = L"Page down"},
    {.name = "home", .description = L"Home"},
    {.name = "end", .description = L"End"},
    {.name = "stop", .description = L"Stop playing"},
    {.name = "next", .description = L"Next song"},
    {.name = "prev", .description = L"Previous song"},
    {.name = "pause", .description = L"Toggle pause"},
    {.name = "moveup", .description = L"Move song up in playlist"},
    {.name = "movedown", .description = L"Move song down in playlist"},
    {.name = "toggle", .description = L"Switch between playlists"},
    {.name = "toggle_progress", .description = L"Toggle progress bar display"},
    {.name = "toggle_shuffle", .description = L"Toggle Shuffle option"},
    {.name = "toggle_repeat", .description = L"Toggle Repeat option"},
    {.name = "toggle_next", .description = L"Toggle Next option"},
    {.name = "lyrics", .description = L"Show/hide lyrics display"},
    {.name = "help", .description = L"Show/hide this help"},
    {.name = "toggle_visualization", .description = L"Show/hide visualization"},
    {.name = "add_to_playlist",
        .description = L"Add file/directory to playlist"},
    {.name = "delete", .description = L"Remove selected item from playlist"},
    {.name = "clear", .description = L"Clear playlist"},
    {.name = "ff", .description = L"Forward"},
    {.name = "rew", .description = L"Rewind"},
    {.name = "reset_view", .description = L"Exit to playlists view"},
    {.name = "volup1", .description = L"Volume up 1%"},
    {.name = "voldown1", .description = L"Volume down 1%"},
    {.name = "volup5", .description = L"Volume up 5%"},
    {.name = "voldown5", .description = L"Volume down 5%"},
    {.name = "volset10", .description = L"Set volume to 10%"},
    {.name = "volset20", .description = L"Set volume to 20%"},
    {.name = "volset30", .description = L"Set volume to 30%"},
    {.name = "volset40", .description = L"Set volume to 40%"},
    {.name = "volset50", .description = L"Set volume to 50%"},
    {.name = "volset60", .description = L"Set volume to 60%"},
    {.name = "volset70", .description = L"Set volume to 70%"},
    {.name = "volset80", .description = L"Set volume to 80%"},
    {.name = "volset90", .description = L"Set volume to 90%"},
    {.name = "volset100", .description = L"Set volume to 100%"},
}};

}  // namespace

Keymap::Keymap(const std::string& path) :
    keymap_({{input::key('q'), Action::Quit}, {input::key('Q'), Action::Quit},
        {input::Up, Action::Up}, {input::Down, Action::Down},
        {input::Left, Action::Rew}, {input::Right, Action::FF},
        {input::PgUp, Action::PgUp}, {input::PgDown, Action::PgDown},
        {input::Home, Action::Home}, {input::End, Action::End},
        {input::Enter, Action::Play}, {input::Tab, Action::ToggleLists},
        {input::Up | input::Key::AltBase, Action::MoveUp},
        {input::Down | input::Key::AltBase, Action::MoveDown},
        {input::key('s'), Action::Stop}, {input::key('n'), Action::Next},
        {input::key('b'), Action::Prev}, {input::key(' '), Action::Pause},
        {input::key('p'), Action::Pause},
        {input::key('A'), Action::AddToPlaylist},
        {input::key('a'), Action::AddToPlaylist},
        {input::key('P'), Action::ToggleProgress},
        {input::key('S'), Action::ToggleShuffle},
        {input::key('R'), Action::ToggleRepeat},
        {input::key('N'), Action::ToggleNext},
        {input::key('X'), Action::ToggleNext},
        {input::key('L'), Action::ToggleLyrics},
        {input::key('h'), Action::ToggleHelp},
        {input::key('?'), Action::ToggleHelp},
#ifdef ENABLE_SPECTRALIZER
        {input::key('V'), Action::ToggleSpectralizer},
#endif
        {input::key('d'), Action::Delete}, {input::key('C'), Action::Clear},
        {input::key('x') | input::Key::CtrlBase, Action::ResetView},
        {input::Esc, Action::ResetView}, {input::key('>'), Action::VolUp1},
        {input::key('<'), Action::VolDn1}, {input::key('.'), Action::VolUp5},
        {input::key(','), Action::VolDn5},
        {input::key('1') | input::Key::AltBase, Action::VolSet10},
        {input::key('2') | input::Key::AltBase, Action::VolSet20},
        {input::key('3') | input::Key::AltBase, Action::VolSet30},
        {input::key('4') | input::Key::AltBase, Action::VolSet40},
        {input::key('5') | input::Key::AltBase, Action::VolSet50},
        {input::key('6') | input::Key::AltBase, Action::VolSet60},
        {input::key('7') | input::Key::AltBase, Action::VolSet70},
        {input::key('8') | input::Key::AltBase, Action::VolSet80},
        {input::key('9') | input::Key::AltBase, Action::VolSet90},
        {input::key('0') | input::Key::AltBase, Action::VolSet100}}) {
    auto parseKey = [](std::wstring_view strKey) {
        auto mods = input::Null;
        auto result = input::Null;
        if (strKey.length() == 1) {
            return input::key(strKey[0]);
        }
        strBreak(
            strKey,
            [&result, &mods](auto begin, auto end) {
                if (end - begin > 1 && *begin == L'^') {
                    ++begin;
                    mods |= input::CtrlBase;
                }
                auto key = keyFromName(std::wstring_view(begin, end));
                switch (key) {
                    case input::CtrlBase:
                    case input::AltBase:
                        mods |= key;
                        break;

                    default:
                        result = key;
                }

                return true;
            },
            [](const wchar_t symbol) {
                return isgraph(symbol) == 0 || symbol == L'+';
            });
        return result | mods;
    };

    auto parseAction = [](const auto& strAction) {
        if (auto found = std::ranges::find_if(descriptions,
                [&strAction](
                    const auto& entry) { return entry.name == strAction; });
            found != descriptions.end()) {
            return static_cast<Action>(
                std::distance(descriptions.begin(), found));
        }
        return Action::ResetView;
    };
    auto toml = Toml(path);
    toml.enumTable([&](std::string_view action, const Toml& val) {
        auto act = parseAction(action);
        if (!val.enumArray(
                [&](std::wstring_view key) { keymap_[parseKey(key)] = act; })) {
            if (auto key = val.as<std::wstring>()) {
                keymap_[parseKey(*key)] = act;
            }
        }
    });
}

std::optional<Action> Keymap::map(input::Key key) const noexcept {
    if (auto found = keymap_.find(key); found != keymap_.end()) {
        return found->second;
    }
    return {};
}

std::wstring Keymap::name(input::Key key) {
    auto result = std::wstring();
    if ((key & input::CtrlBase) != 0) {
        result = L"Ctrl+";
        key &= ~input::CtrlBase;
    }
    if ((key & input::AltBase) != 0) {
        result = L"Alt+";
        key &= ~input::AltBase;
    }
    if (key == input::Key::Null) {
    } else if (key == L' ') {
        result += L"space";
    } else if (key < input::Key::SpecialBase) {
        result += static_cast<wchar_t>(key);
    } else if (key >= NamesStart &&
               key < NamesStart + static_cast<int>(keyNames.size())) {
        result += keyNames[key - NamesStart];
    }
    return result;
}

const std::wstring& Keymap::description(unsigned index) {
    return descriptions[index].description;
}
