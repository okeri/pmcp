#pragma once

#include <string>
#include <unordered_set>

#include "Options.hh"

struct Config {
    std::string home;
    std::string themePath;
    std::string keymapPath;
    std::string playlistPath;
    std::string socketPath;
    std::unordered_set<std::string> whiteList;
    Options options;

    Config();
    Config(const Config&) = delete;
    Config(Config&&) = delete;
    Config& operator=(const Config&) = delete;
    Config& operator=(Config&&) = delete;
    ~Config();
};
