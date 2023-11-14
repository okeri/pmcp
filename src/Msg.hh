#pragma once

#include <variant>
#include "Action.hh"
#include "input.hh"

#ifdef ENABLE_SPECTRALIZER
#include <vector>
using Msg = std::variant<input::Key, unsigned, Action, std::vector<float>>;
#else
using Msg = std::variant<input::Key, unsigned, Action>;
#endif
