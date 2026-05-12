#pragma once

#include "mini_judge/types.hpp"

#include <string>

namespace mini_judge {

CompareResult CompareText(CompareMode mode, const std::string& expected, const std::string& actual);

}  // namespace mini_judge
