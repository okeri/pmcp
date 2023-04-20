#pragma once

#include <variant>

#include "input.hh"
using Msg = std::variant<input::Key, unsigned>;
