#pragma once

#include "mini_judge/types.hpp"

#include <string>

namespace mini_judge {

// Validates inputs and resolves paths without creating output directories.
// Callers prepare the selected report directory after applying any override.
ProblemConfig LoadConfig(const std::filesystem::path& config_path);
CompareMode ParseCompareMode(const std::string& raw);

}  // namespace mini_judge
