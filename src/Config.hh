#pragma once

#include <string>
#include <unordered_set>

#include "Options.hh"

struct Config {
    std::wstring home;
    std::wstring themePath;
    std::wstring keymapPath;
    std::string socketPath;
    std::unordered_set<std::wstring> whiteList;
    Options options;

    Config();
    Config(const Config&) = delete;
    Config(Config&&) = delete;
    Config& operator=(const Config&) = delete;
    Config& operator=(Config&&) = delete;
    ~Config();
};
