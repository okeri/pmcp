#include <array>
#include <algorithm>

#include "Toml.hh"
#include "Keymap.hh"

namespace {

const std::vector<std::wstring> keyNames = {L"backspace", L"left", L"right",
    L"up", L"down", L"home", L"end", L"pgup", L"pgdown", L"insert", L"delete",
    L"enter", L"tab", L"esc", L"f0", L"f1", L"f2", L"f3", L"f4", L"f5", L"f6",
    L"f7", L"f8", L"f9", L"f10", L"f11", L"f12", L"f13", L"f14", L"f15", L"f16",
    L"f17", L"f18", L"f19", L"f20"};

constexpr auto NamesStart = 0x110002;

constexpr input::Key operator&(const input::Key a, const input::Key b) {
    return static_cast<input::Key>(std::underlying_type_t<input::Key>(a) &
                                   std::underlying_type_t<input::Key>(b));
}

constexpr input::Key operator~(const input::Key key) {
    return static_cast<input::Key>(~std::underlying_type_t<input::Key>(key));
}

constexpr input::Key& operator&=(input::Key& a, const input::Key b) {
    return a = a & b;
}

input::Key keyFromName(const std::wstring_view& name) {
    auto low = std::wstring(name);
    std::transform(low.begin(), low.end(), low.begin(),
        [](wchar_t sym) { return std::tolower(sym); });
    if (low == L"alt") {
        return input::Key::AltBase;
    }
    if (low == L"ctrl") {
        return input::Key::CtrlBase;
    }
    if (auto found = std::find(keyNames.begin(), keyNames.end(), low);
        found != keyNames.end()) {
        return static_cast<input::Key>(
            NamesStart + std::distance(keyNames.begin(), found));
    }
    return static_cast<input::Key>(name[0]);
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
                *const_cast<wchar_t*>(tail) = 0;  // NOLINT
                if (!handler(head, tail)) {
                    return;
                }
                head = end;
            }
        }
    }
    if (head != end) {
        *const_cast<wchar_t*>(tail) = 0;  // NOLINT
        handler(head, tail);
    }
}

struct ActionDescription {
    std::string name;
    std::wstring description;
};

constexpr auto Count = static_cast<unsigned>(Action::Count);
const std::array<ActionDescription, Count> descriptions{{
    {"quit", L"Quit program"},
    {"go", L"Play a playlist entry or file or enter to directory"},
    {"up", L"Move up"},
    {"down", L"Move down"},
    {"pgup", L"Page up"},
    {"pgdown", L"Page down"},
    {"home", L"Home"},
    {"end", L"End"},
    {"stop", L"Stop playing"},
    {"next", L"Next song"},
    {"prev", L"Previous song"},
    {"pause", L"Toggle pause"},
    {"toggle", L"Switch between playlists"},
    {"toggle_progress", L"Toggle progress bar display"},
    {"toggle_shuffle", L"Toggle Shuffle option"},
    {"toggle_repeat", L"Toggle Repeat option"},
    {"toggle_next", L"Toggle Next option"},
    {"lyrics", L"Show/hide lyrics display"},
    {"help", L"Show/hide this help"},
    {"toggle_visualization", L"Show/hide visualization"},
    {"add_to_playlist", L"Add file/directory to playlist"},
    {"delete", L"Remove selected item from playlist"},
    {"clear", L"Clear playlist"},
    {"ff", L"Forward"},
    {"rew", L"Rewind"},
    {"reset_view", L"Exit to playlists view"},
    {"volup1", L"Volume up 1%"},
    {"voldown1", L"Volume down 1%"},
    {"volup5", L"Volume up 5%"},
    {"voldown5", L"Volume down 5%"},
    {"volset10", L"Set volume to 10%"},
    {"volset20", L"Set volume to 20%"},
    {"volset30", L"Set volume to 30%"},
    {"volset40", L"Set volume to 40%"},
    {"volset50", L"Set volume to 50%"},
    {"volset60", L"Set volume to 60%"},
    {"volset70", L"Set volume to 70%"},
    {"volset80", L"Set volume to 80%"},
    {"volset90", L"Set volume to 90%"},
    {"volset100", L"Set volume to 100%"},
}};

}  // namespace

Keymap::Keymap(const std::string& path) :
    keymap_({{static_cast<input::Key>('q'), Action::Quit},
        {static_cast<input::Key>('Q'), Action::Quit}, {input::Up, Action::Up},
        {input::Down, Action::Down}, {input::Left, Action::Rew},
        {input::Right, Action::FF}, {input::PgUp, Action::PgUp},
        {input::PgDown, Action::PgDown}, {input::Home, Action::Home},
        {input::End, Action::End}, {input::Enter, Action::Play},
        {input::Tab, Action::ToggleLists},
        {static_cast<input::Key>('s'), Action::Stop},
        {static_cast<input::Key>('n'), Action::Next},
        {static_cast<input::Key>('b'), Action::Prev},
        {static_cast<input::Key>(' '), Action::Pause},
        {static_cast<input::Key>('p'), Action::Pause},
        {static_cast<input::Key>('A'), Action::AddToPlaylist},
        {static_cast<input::Key>('a'), Action::AddToPlaylist},
        {static_cast<input::Key>('P'), Action::ToggleProgress},
        {static_cast<input::Key>('S'), Action::ToggleShuffle},
        {static_cast<input::Key>('R'), Action::ToggleRepeat},
        {static_cast<input::Key>('N'), Action::ToggleNext},
        {static_cast<input::Key>('X'), Action::ToggleNext},
        {static_cast<input::Key>('L'), Action::ToggleLyrics},
        {static_cast<input::Key>('h'), Action::ToggleHelp},
        {static_cast<input::Key>('?'), Action::ToggleHelp},
#ifdef ENABLE_SPECTRALIZER
        {static_cast<input::Key>('V'), Action::ToggleSpectralizer},
#endif
        {static_cast<input::Key>('d'), Action::Delete},
        {static_cast<input::Key>('C'), Action::Clear},
        {static_cast<input::Key>('x') | input::Key::CtrlBase,
            Action::ResetView},
        {input::Esc, Action::ResetView},
        {static_cast<input::Key>('>'), Action::VolUp1},
        {static_cast<input::Key>('<'), Action::VolDn1},
        {static_cast<input::Key>('.'), Action::VolUp5},
        {static_cast<input::Key>(','), Action::VolDn5},
        {static_cast<input::Key>('1') | input::Key::AltBase, Action::VolSet10},
        {static_cast<input::Key>('2') | input::Key::AltBase, Action::VolSet20},
        {static_cast<input::Key>('3') | input::Key::AltBase, Action::VolSet30},
        {static_cast<input::Key>('4') | input::Key::AltBase, Action::VolSet40},
        {static_cast<input::Key>('5') | input::Key::AltBase, Action::VolSet50},
        {static_cast<input::Key>('6') | input::Key::AltBase, Action::VolSet60},
        {static_cast<input::Key>('7') | input::Key::AltBase, Action::VolSet70},
        {static_cast<input::Key>('8') | input::Key::AltBase, Action::VolSet80},
        {static_cast<input::Key>('9') | input::Key::AltBase, Action::VolSet90},
        {static_cast<input::Key>('0') | input::Key::AltBase,
            Action::VolSet100}}) {
    auto parseKey = [](std::wstring_view strKey) {
        auto mods = input::Null;
        auto result = input::Null;
        if (strKey.length() == 1) {
            return static_cast<input::Key>(strKey[0]);
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
        if (auto found = std::find_if(descriptions.begin(), descriptions.end(),
                [&strAction](
                    const auto& entry) { return entry.name == strAction; });
            found != descriptions.end()) {
            return static_cast<Action>(
                std::distance(descriptions.begin(), found));
        }
        return Action::ResetView;
    };
    auto toml = Toml(path);
    toml.enumTable([&](const std::string& action, const Toml& val) {
        auto act = parseAction(action);
        if (!val.enumArray([&](const std::wstring& key) {
                keymap_[parseKey(key)] = act;
            })) {
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
    } else if (key < 0x110000) {  // NOLINT(readability-magic-numbers)
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
