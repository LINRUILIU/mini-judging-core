#pragma once

#include "mini_judge/types.hpp"

#include <filesystem>
#include <string>

namespace mini_judge {

class ProcessRunner {
public:
    ExecResult Run(const std::filesystem::path& exe_path,
                   const std::string& stdin_text,
                   int time_limit_ms,
                   std::uint64_t max_output_bytes) const;
};

}  // namespace mini_judge
