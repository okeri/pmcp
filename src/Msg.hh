#pragma once

#include <variant>
#include "Action.hh"
#include "input.hh"

struct SetVolume {
    double value;
};

#ifdef ENABLE_SPECTRALIZER
#include <vector>
using Msg =
    std::variant<input::Key, unsigned, Action, SetVolume, std::vector<float>>;
#else
using Msg = std::variant<input::Key, unsigned, Action, SetVolume>;
#endif
