#include <cstdlib>
#include <stdexcept>
#include <filesystem>

#include "Config.hh"
#include "Toml.hh"

namespace fs = std::filesystem;

std::string defaultHome() {
    const auto* home = getenv("HOME");  // NOLINT:concurrency-mt-unsafe
    if (home == nullptr) {
        throw std::runtime_error("cannot detect home dir");
    }
    return home;
}

std::string configPath() noexcept {
    const auto* configHome =
        getenv("XDG_CONFIG_HOME");  // NOLINT:concurrency-mt-unsafe
    if (configHome != nullptr) {
        return (fs::path(configHome) / "pmcp").string();
    } else {
        return (fs::path(defaultHome()) / ".config" / "pmcp").string();
    }
}

std::string sockPath() {
    const auto* runtimePath =
        getenv("XDG_RUNTIME_DIR");  // NOLINT:concurrency-mt-unsafe
    if (runtimePath != nullptr) {
        return (fs::path(runtimePath) / "pmcp.sock").string();
    } else {
        return "/tmp/pmcp.sock";
    }
}

Config::Config() {
    auto confPath = fs::path(configPath());
    auto path = (confPath / "config.toml").string();
    if (fs::exists(path)) {
        auto root = Toml(path);
        home = root.get<std::string>("music_dir").value_or(defaultHome());
        if (auto tilda = home.find('~'); tilda != std::string::npos) {
            home.replace(tilda, 1, defaultHome());
        }

        auto transformFullPath = [&confPath](const fs::path& p) {
            auto full = confPath / p;
            if (fs::exists(full)) {
                return full.string();
            }
            return p.string();
        };
        themePath = transformFullPath(
            root.get<std::string>("theme").value_or("default_theme.toml"));
        keymapPath = transformFullPath(
            root.get<std::string>("keymap").value_or("keymap.toml"));

        root.enumArray("allow_extensions",
            [this](const std::string& value) { whiteList.insert(value); });
    }
    auto optsPath = (confPath / "options.toml").string();
    playlistPath = (confPath / "playlist.m3u").string();
    socketPath = sockPath();
    if (fs::exists(optsPath)) {
        auto root = Toml(optsPath);
        auto setBoolMaybe = [&root](bool& value, const std::string& key) {
            auto v = root.get<bool>(key);
            if (v) {
                value = *v;
            }
        };
        setBoolMaybe(options.showProgress, "show_progress");
        setBoolMaybe(options.spectralizer, "visualization");
        setBoolMaybe(options.shuffle, "shuffle");
        setBoolMaybe(options.repeat, "repeat");
        setBoolMaybe(options.next, "next");
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
