#include <array>
#include <algorithm>

#include "Toml.hh"
#include "Keymap.hh"

namespace {

static std::vector<std::wstring> keyNames = {L"backspace", L"left", L"right",
    L"up", L"down", L"home", L"end", L"pgup", L"pgdown", L"insert", L"delete",
    L"enter", L"tab", L"esc", L"f0", L"f1", L"f2", L"f3", L"f4", L"f5", L"f6",
    L"f7", L"f8", L"f9", L"f10", L"f11", L"f12", L"f13", L"f14", L"f15", L"f16",
    L"f17", L"f18", L"f19", L"f20"};

constexpr auto namesStart = 0x110002;

constexpr input::Key operator&(const input::Key a, const input::Key b) {
    return static_cast<input::Key>(std::underlying_type_t<input::Key>(a) &
                                   std::underlying_type_t<input::Key>(b));
}

constexpr input::Key operator~(const input::Key f) {
    return static_cast<input::Key>(~std::underlying_type_t<input::Key>(f));
}

constexpr input::Key& operator&=(input::Key& a, const input::Key b) {
    return a = a & b;
}

input::Key key(const std::wstring_view& name) {
    auto low = std::wstring(name);
    std::transform(low.begin(), low.end(), low.begin(),
        [](wchar_t c) { return std::tolower(c); });
    if (low == L"alt") {
        return input::Key::AltBase;
    } else if (low == L"ctrl") {
        return input::Key::CtrlBase;
    }
    if (auto found = std::find(keyNames.begin(), keyNames.end(), low);
        found != keyNames.end()) {
        return static_cast<input::Key>(
            namesStart + std::distance(keyNames.begin(), found));
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
                *const_cast<wchar_t*>(tail) = 0;
                if (!handler(head, tail)) {
                    return;
                }
                head = end;
            }
        }
    }
    if (head != end) {
        *const_cast<wchar_t*>(tail) = 0;
        handler(head, tail);
    }
}

struct ActionDescription {
    std::string name;
    std::wstring description;
};

constexpr auto count = static_cast<unsigned>(Action::Count);
static std::array<ActionDescription, count> descriptions{{{"quit",
                                                              L"Quit program"},
    {"go", L"Play a playlist entry or file or enter to directory"},
    {"up", L"Move up"}, {"down", L"Move down"}, {"pgup", L"Page up"},
    {"pgdown", L"Page down"}, {"home", L"Home"}, {"end", L"End"},
    {"stop", L"Stop playing"}, {"next", L"Next song"},
    {"prev", L"Previous song"}, {"pause", L"Toggle pause"},
    {"toggle", L"Switch between playlists"},
    {"toggle_progress", L"Toggle progress bar display"},
    {"toggle_shuffle", L"Toggle Shuffle option"},
    {"toggle_repeat", L"Toggle Repeat option"},
    {"toggle_next", L"Toggle Next option"},
    {"lyrics", L"Show/hide lyrics display"}, {"help", L"Show/hide this help"},
    {"add_to_playlist", L"Add file/directory to playlist"},
    {"delete", L"Remove selected item from playlist"},
    {"clear", L"Clear playlist"}, {"ff", L"Forward"}, {"rew", L"Rewind"},
    {"reset_view", L"Exit to playlists view"}}};

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
        {static_cast<input::Key>('d'), Action::Delete},
        {static_cast<input::Key>('C'), Action::Clear},
        {static_cast<input::Key>('x') | input::Key::CtrlBase,
            Action::ResetView},
        {input::Esc, Action::ResetView}}) {
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
                auto k = key(std::wstring_view(begin, end));
                switch (k) {
                    case input::CtrlBase:
                    case input::AltBase:
                        mods |= k;
                        break;

                    default:
                        result = k;
                }

                return true;
            },
            [](const wchar_t c) { return !isgraph(c) || c == L'+'; });
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
            if (auto k = val.as<std::wstring>()) {
                keymap_[parseKey(*k)] = act;
            }
        }
    });
}

std::optional<Action> Keymap::map(input::Key key) const noexcept {
    if (auto found = keymap_.find(key); found != keymap_.end()) {
        return found->second;
    }
    return std::nullopt;
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
    } else if (key < 0x110000) {
        result += static_cast<wchar_t>(key);
    } else if (key >= namesStart &&
               key < namesStart + static_cast<int>(keyNames.size())) {
        result += keyNames[key - namesStart];
    }
    return result;
}

const std::wstring& Keymap::description(unsigned index) {
    return descriptions[index].description;
}
