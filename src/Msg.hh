#pragma once

#include <variant>
#include "Action.hh"
#include "input.hh"

using Msg = std::variant<input::Key, unsigned, Action>;
