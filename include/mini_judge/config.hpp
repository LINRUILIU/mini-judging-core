#pragma once

#include "mini_judge/types.hpp"

#include <string>

namespace mini_judge {

ProblemConfig LoadConfig(const std::filesystem::path& config_path);
CompareMode ParseCompareMode(const std::string& raw);

}  // namespace mini_judge
