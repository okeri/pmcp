#include <thread>
#include <filesystem>
#include <fstream>

#include "utf8.hh"
#include "Lyrics.hh"

#ifdef ENABLE_GLYR
#include "glyr/glyr.h"

namespace glyr {

class Query {
    GlyrQuery query_;

  public:
    Query() {  // NOLINT: hicpp-member-init
        glyr_query_init(&query_);
    }
    Query(const Query&) = delete;
    Query(Query&&) = delete;
    Query& operator=(const Query&) = delete;
    Query& operator=(Query&&) = delete;

    ~Query() {
        glyr_query_destroy(&query_);
    }

    operator GlyrQuery*() noexcept {  // NOLINT:hicpp-explicit-convertions
        return &query_;
    }
};

class Fetcher {
  public:
    Fetcher() {
        glyr_init();
    }

    Fetcher(const Fetcher&) = delete;
    Fetcher(Fetcher&&) = delete;
    Fetcher& operator=(const Fetcher&) = delete;
    Fetcher& operator=(Fetcher&&) = delete;

    std::optional<std::string> fetchLyrics(
        const char* artist, const char* title) {
        Query query;
        glyr_opt_title(query, title);
        glyr_opt_artist(query, artist);
        glyr_opt_type(query, GLYR_GET_LYRICS);

        auto* cache = glyr_get(query, nullptr, nullptr);
        if (cache != nullptr && cache->size > 0) {
            for (auto i = 0ull; i < cache->size; ++i) {
                if (cache->data[i] == '\r') {
                    cache->data[i] = '\n';
                }
            }
            return cache->data;
        }
        return {};
    }
    ~Fetcher() {
        glyr_cleanup();
    }
};

}  // namespace glyr

#endif  // ENABLE_GLYR

namespace render {

template <class CharT, class Separator, class Handler>
void split(std::basic_string_view<CharT> string, Separator separator,
    Handler handler) {
    size_t start = 0ull;
    size_t end;

    while ((end = string.find(separator, start)) !=
           std::basic_string_view<CharT>::npos) {
        if (start != end) {
            handler(string.data() + start, end - start);
        }
        if constexpr (std::is_same<CharT, Separator>()) {
            start = end + 1;
        } else {
            start = end + separator.length();
        }
    }
    handler(string.data() + start, string.length() - start);
}

std::vector<std::wstring> simpleN(std::wstring_view data) {
    std::vector<std::wstring> result;
    split(data, L'\n', [&result](const wchar_t* begin, size_t count) {
        result.emplace_back(begin, count);
    });
    return result;
}

}  // namespace render

namespace local {

namespace fs = std::filesystem;

std::vector<std::wstring> load(
    const std::wstring& lyricsPath, const std::wstring& song) {
    auto path = fs::path(lyricsPath) / (song + L".txt");

    if (auto input = std::ifstream(path.string())) {
        std::string content((std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
        return render::simpleN(utf8::convert(content));
    }
    return {};
}

void save(const std::wstring& lyricsPath, const std::wstring& song,
    const std::vector<std::wstring>& data) {
    auto path = fs::path(lyricsPath) / (song + L".txt");
    if (auto output = std::ofstream(path.string())) {
        for (const auto& line : data) {
            output << utf8::convert(line) << std::endl;
        }
    }
}

}  // namespace local

Lyrics::Lyrics(Sender<Msg> progressSender, const std::string& path) :
    sender_(std::move(progressSender)), path_(utf8::convert(path)) {
}

const std::vector<std::wstring>& Lyrics::text() const noexcept {
    return text_;
}

void Lyrics::setSong(const std::wstring& title) noexcept {
    loaded_ = false;
    title_ = title;
    if (active_) {
        loadLyrics();
    }
}

void Lyrics::activate(bool act) noexcept {
    active_ = act;
    if (act) {
        loadLyrics();
    }
}

void Lyrics::loadLyrics() {
    auto error = [this](const wchar_t* err) {
        text_ = std::vector(1, std::wstring(err));
    };
#ifdef ENABLE_GLYR
    std::unique_lock lock(mutex_);
#endif
    if (loaded_) {
        return;
    }

    if (title_.empty()) {
        error(L"Missing track info");
        return;
    }

    text_ = local::load(path_, title_);
    if (!text_.empty()) {
        loaded_ = true;
        return;
    }
#ifdef ENABLE_GLYR
    static glyr::Fetcher fetcher;
    error(L"Loading...");
    lock.unlock();
    std::thread([this]() {
        std::vector<std::string> components;
        render::split(std::string_view(utf8::convert(title_)),
            std::string(" - "), [&components](const auto* begin, size_t count) {
                components.emplace_back(begin, count);
            });

        auto lyrics =
            fetcher.fetchLyrics(components[0].c_str(), components[1].c_str());
        std::lock_guard lock(mutex_);
        if (lyrics) {
            text_ = render::simpleN(utf8::convert(lyrics.value()));
            local::save(path_, title_, text_);
            loaded_ = true;
        } else {
            text_ = std::vector(1, std::wstring(L"No lyrics found"));
        }
        sender_.send(input::Key::Resize);
    }).detach();
#else
    error(L"No lyrics found");
#endif
}

const wchar_t* Lyrics::title() const noexcept {
    if (!loaded_) {
        return L"Lyrics";
    }
    return title_.c_str();
}
