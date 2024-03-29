#include <cstdlib>
#include <stdexcept>
#include <filesystem>

#include "Config.hh"
#include "Toml.hh"

namespace fs = std::filesystem;

namespace {

std::string defaultHome() {
    const auto* home = getenv("HOME");  // NOLINT(concurrency-mt-unsafe)
    if (home == nullptr) {
        throw std::runtime_error("cannot detect home dir");
    }
    return home;
}

std::string configPath() noexcept {
    const auto* configHome =
        getenv("XDG_CONFIG_HOME");  // NOLINT(concurrency-mt-unsafe)
    if (configHome != nullptr) {
        return (fs::path(configHome) / "pmcp").string();
    }
    return (fs::path(defaultHome()) / ".config" / "pmcp").string();
}

std::string sockPath() {
    const auto* runtimePath =
        getenv("XDG_RUNTIME_DIR");  // NOLINT(concurrency-mt-unsafe)
    if (runtimePath != nullptr) {
        return (fs::path(runtimePath) / "pmcp.sock").string();
    }
    return "/tmp/pmcp.sock";
}

}  // namespace

Config::Config() {
    auto confPath = fs::path(configPath());
    auto path = (confPath / "config.toml").string();
    lyricsPath = (confPath / "lyrics").string();

    auto tildaFixup = [](std::string& path) {
        if (auto tilda = path.find('~'); tilda != std::string::npos) {
            path.replace(tilda, 1, defaultHome());
        }
    };

    if (fs::exists(path)) {
        auto root = Toml(path);
        home = root.get<std::string>("music_dir").value_or(defaultHome());
        tildaFixup(home);

        themePath = root.get<std::string>("theme").value_or(themePath);
        keymapPath = root.get<std::string>("keymap").value_or(keymapPath);
        lyricsPath = root.get<std::string>("lyrics_dir").value_or(lyricsPath);
        lyricsProvider =
            root.get<std::string>("lyrics_provider").value_or(lyricsProvider);
        tildaFixup(lyricsPath);
        root.enumArray("allow_extensions",
            [this](const std::string& value) { whiteList.insert(value); });
    } else {
        if (!fs::exists(confPath)) {
            if (!fs::create_directory(confPath)) {
                throw std::runtime_error(
                    "cannot create path: " + confPath.string());
            }
        }
        home = defaultHome();
        tildaFixup(home);
    }

    auto searchFullPath = [&confPath, &tildaFixup](const fs::path& path) {
        auto full = confPath / path;
        if (fs::exists(full)) {
            return full.string();
        }
        auto result = path.string();
        tildaFixup(result);
        return result;
    };

    themePath = searchFullPath(themePath);
    keymapPath = searchFullPath(keymapPath);

    auto optsPath = (confPath / "options.toml").string();
    playlistPath = (confPath / "playlist.m3u").string();
    socketPath = sockPath();
    if (fs::exists(optsPath)) {
        auto root = Toml(optsPath);
        auto setBoolMaybe = [&root](bool& value, const std::string& key) {
            auto val = root.get<bool>(key);
            if (val) {
                value = *val;
            }
        };
        setBoolMaybe(options.showProgress, "show_progress");
        setBoolMaybe(options.spectralizer, "visualization");
        setBoolMaybe(options.shuffle, "shuffle");
        setBoolMaybe(options.repeat, "repeat");
        setBoolMaybe(options.next, "next");
    }
    if (!fs::exists(lyricsPath)) {
        if (!fs::create_directory(lyricsPath)) {
            throw std::runtime_error("cannot create path: " + lyricsPath);
        }
    }
}

Config::~Config() {
    auto optsPath = (fs::path(configPath()) / "options.toml").string();
    Toml toml;
    toml.push("show_progress", options.showProgress);
    toml.push("visualization", options.spectralizer);
    toml.push("shuffle", options.shuffle);
    toml.push("repeat", options.repeat);
    toml.push("next", options.next);
    toml.save(optsPath);
}
