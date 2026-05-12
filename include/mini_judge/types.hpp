#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace mini_judge {

enum class CompareMode {
    Exact,
    TrimRight,
    IgnoreTrailingBlankLines,
};

enum class CaseStatus {
    AC,
    WA,
    TLE,
    RE,
    OLE,
    SE,
};

struct ProgramEntry {
    std::string id;
    std::string exe;
    std::filesystem::path exe_path;
};

struct TestCase {
    std::string id;
    std::string input;
    std::string answer;
    int score = 0;
    std::filesystem::path input_path;
    std::filesystem::path answer_path;
};

struct ProblemConfig {
    std::string problem_id;
    int time_limit_ms = 1000;
    std::uint64_t max_output_bytes = 65536;
    CompareMode compare_mode = CompareMode::TrimRight;
    std::filesystem::path config_path;
    std::filesystem::path program_dir;
    std::filesystem::path test_dir;
    std::filesystem::path answer_dir;
    std::filesystem::path report_dir;
    std::vector<ProgramEntry> programs;
    std::vector<TestCase> tests;
};

struct ExecResult {
    CaseStatus status = CaseStatus::SE;
    int exit_code = -1;
    int time_ms = 0;
    std::uint64_t stdout_size = 0;
    std::string stdout_text;
    std::string stderr_text;
    std::string message;
};

struct CompareResult {
    bool matched = false;
    int first_diff_line = -1;
    std::string message;
};

struct CaseResult {
    std::string program_id;
    std::string test_id;
    CaseStatus status = CaseStatus::SE;
    int score = 0;
    int max_score = 0;
    int time_ms = 0;
    int exit_code = -1;
    std::uint64_t stdout_size = 0;
    std::string message;
    CompareResult compare;
};

struct ProgramReport {
    std::string program_id;
    int total_score = 0;
    int max_score = 0;
    int passed_count = 0;
    int failed_count = 0;
    int ac_count = 0;
    int wa_count = 0;
    int tle_count = 0;
    int re_count = 0;
    int ole_count = 0;
    int se_count = 0;
    std::vector<CaseResult> cases;
};

struct BatchReport {
    std::string problem_id;
    std::string generated_at;
    int jobs = 1;
    int total_programs = 0;
    int total_cases = 0;
    std::vector<ProgramReport> programs;
};

std::string ToString(CompareMode mode);
std::string ToString(CaseStatus status);

}  // namespace mini_judge
