#include <cstdlib>
#include <stdexcept>
#include <filesystem>

#include "utf8.hh"
#include "Config.hh"
#include "Toml.hh"

namespace fs = std::filesystem;

std::wstring defaultHome() {
    const auto* home = getenv("HOME");  // NOLINT:concurrency-mt-unsafe
    if (home == nullptr) {
        throw std::runtime_error("cannot detect home dir");
    }
    return utf8::convert(home);
}

std::wstring configPath() {
    const auto* configHome =
        getenv("XDG_CONFIG_HOME");  // NOLINT:concurrency-mt-unsafe
    if (configHome != nullptr) {
        return (fs::path(utf8::convert(configHome)) / "pmcp").wstring();
    } else {
        return (fs::path(defaultHome()) / ".config" / "pmcp").wstring();
    }
}

Config::Config() {
    auto confPath = fs::path(configPath());
    auto path = (confPath / "config.toml").wstring();
    if (fs::exists(path)) {
        auto root = Toml(path);
        home = root.get<std::wstring>("music_dir").value_or(defaultHome());
        if (auto tilda = home.find('~'); tilda != std::string::npos) {
            home.replace(tilda, 1, defaultHome());
        }

        auto transformFullPath = [&confPath](const fs::path& p) {
            auto full = confPath / p;
            if (fs::exists(full)) {
                return full.wstring();
            }
            return p.wstring();
        };
        themePath = transformFullPath(
            root.get<std::wstring>("theme").value_or(L"default_theme.toml"));
        keymapPath = transformFullPath(
            root.get<std::wstring>("keymap").value_or(L"keymap.toml"));

        root.enumArray("allow_extensions",
            [this](const std::wstring& value) { whiteList.insert(value); });
    }
    auto optsPath = (confPath / "options.toml").wstring();
    if (fs::exists(optsPath)) {
        auto root = Toml(optsPath);
        auto setBoolMaybe = [&root](bool& value, const std::string& key) {
            auto v = root.get<bool>(key);
            if (v) {
                value = *v;
            }
        };
        setBoolMaybe(options.readTags, "read_tags");
        setBoolMaybe(options.showProgress, "show_progress");
        setBoolMaybe(options.spectralizer, "visualization");
        setBoolMaybe(options.shuffle, "shuffle");
        setBoolMaybe(options.repeat, "repeat");
        setBoolMaybe(options.next, "next");
    }
}

Config::~Config() {
    auto optsPath = (fs::path(configPath()) / "options.toml").wstring();
    Toml toml;
    toml.push("read_tags", options.readTags);
    toml.push("show_progress", options.showProgress);
    toml.push("visualization", options.spectralizer);
    toml.push("shuffle", options.shuffle);
    toml.push("repeat", options.repeat);
    toml.push("next", options.next);
    toml.save(optsPath);
}
