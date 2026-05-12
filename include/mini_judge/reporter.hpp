#pragma once

#include "mini_judge/types.hpp"

#include <filesystem>

namespace mini_judge {

void WriteJsonReport(const BatchReport& report, const std::filesystem::path& path);
void WriteCsvSummary(const BatchReport& report, const std::filesystem::path& path);
void WriteHtmlReport(const BatchReport& report, const std::filesystem::path& path);

}  // namespace mini_judge
