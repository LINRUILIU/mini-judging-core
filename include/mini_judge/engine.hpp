#pragma once

#include "mini_judge/types.hpp"

namespace mini_judge {

BatchReport RunJudge(const ProblemConfig& config, int jobs, bool verbose_progress = true);

}  // namespace mini_judge
