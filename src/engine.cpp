#include "mini_judge/engine.hpp"

#include "mini_judge/comparator.hpp"
#include "mini_judge/runner.hpp"
#include "mini_judge/scheduler.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <mutex>
#include <sstream>

namespace mini_judge {
namespace {

std::string ReadTextFile(const std::filesystem::path& path) {
    FILE* file = _wfopen(path.c_str(), L"rb");
    if (file == nullptr) {
        throw std::runtime_error("Failed to open text file: " + path.string());
    }
    std::string buffer;
    char chunk[4096];
    while (true) {
        const std::size_t read = fread(chunk, 1, sizeof(chunk), file);
        if (read > 0) {
            buffer.append(chunk, chunk + read);
        }
        if (read < sizeof(chunk)) {
            break;
        }
    }
    fclose(file);
    return buffer;
}

std::string TimestampNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm local_tm{};
    localtime_s(&local_tm, &tt);
    std::ostringstream out;
    out << std::put_time(&local_tm, "%Y-%m-%d %H:%M:%S");
    return out.str();
}

void UpdateCounters(ProgramReport& report, const CaseResult& case_result) {
    report.total_score += case_result.score;
    report.max_score += case_result.max_score;
    if (case_result.status == CaseStatus::AC) {
        ++report.passed_count;
        ++report.ac_count;
    } else {
        ++report.failed_count;
    }

    switch (case_result.status) {
        case CaseStatus::AC:
            break;
        case CaseStatus::WA:
            ++report.wa_count;
            break;
        case CaseStatus::TLE:
            ++report.tle_count;
            break;
        case CaseStatus::RE:
            ++report.re_count;
            break;
        case CaseStatus::OLE:
            ++report.ole_count;
            break;
        case CaseStatus::SE:
            ++report.se_count;
            break;
    }
}

}  // namespace

BatchReport RunJudge(const ProblemConfig& config, int jobs, bool verbose_progress) {
    BatchReport report;
    report.problem_id = config.problem_id;
    report.generated_at = TimestampNow();
    report.jobs = jobs > 0 ? jobs : 1;
    report.total_programs = static_cast<int>(config.programs.size());
    report.total_cases = static_cast<int>(config.programs.size() * config.tests.size());

    ProcessRunner runner;
    TaskScheduler scheduler(report.jobs);

    std::map<std::string, ProgramReport> reports_by_program;
    std::mutex reports_mutex;
    std::mutex console_mutex;
    std::atomic<int> completed{0};

    for (const auto& program : config.programs) {
        ProgramReport initial;
        initial.program_id = program.id;
        reports_by_program.emplace(program.id, std::move(initial));
    }

    for (const auto& program : config.programs) {
        for (const auto& test : config.tests) {
            const ProgramEntry* program_ptr = &program;
            const TestCase* test_ptr = &test;
            scheduler.Submit([&, program_ptr, test_ptr]() {
                const auto& program = *program_ptr;
                const auto& test = *test_ptr;
                CaseResult case_result;
                case_result.program_id = program.id;
                case_result.test_id = test.id;
                case_result.max_score = test.score;

                try {
                    const std::string stdin_text = ReadTextFile(test.input_path);
                    const std::string expected = ReadTextFile(test.answer_path);
                    const ExecResult exec = runner.Run(program.exe_path, stdin_text, config.time_limit_ms, config.max_output_bytes);

                    case_result.status = exec.status;
                    case_result.time_ms = exec.time_ms;
                    case_result.exit_code = exec.exit_code;
                    case_result.stdout_size = exec.stdout_size;
                    case_result.message = exec.message;

                    if (exec.status == CaseStatus::AC) {
                        case_result.compare = CompareText(config.compare_mode, expected, exec.stdout_text);
                        if (case_result.compare.matched) {
                            case_result.status = CaseStatus::AC;
                            case_result.score = test.score;
                            case_result.message = "Accepted";
                        } else {
                            case_result.status = CaseStatus::WA;
                            case_result.message = case_result.compare.message;
                        }
                    }
                } catch (const std::exception& ex) {
                    case_result.status = CaseStatus::SE;
                    case_result.message = ex.what();
                }

                {
                    std::lock_guard<std::mutex> lock(reports_mutex);
                    auto& program_report = reports_by_program.at(program.id);
                    UpdateCounters(program_report, case_result);
                    program_report.cases.push_back(case_result);
                }

                const int done = ++completed;
                if (verbose_progress) {
                    std::lock_guard<std::mutex> lock(console_mutex);
                    std::cout << "[" << done << "/" << report.total_cases << "] "
                              << program.id << " / " << test.id << " -> " << ToString(case_result.status) << std::endl;
                }
            });
        }
    }

    scheduler.Wait();

    for (auto& [program_id, program_report] : reports_by_program) {
        std::sort(program_report.cases.begin(), program_report.cases.end(),
                  [](const CaseResult& lhs, const CaseResult& rhs) { return lhs.test_id < rhs.test_id; });
        report.programs.push_back(std::move(program_report));
    }

    std::sort(report.programs.begin(), report.programs.end(),
              [](const ProgramReport& lhs, const ProgramReport& rhs) { return lhs.program_id < rhs.program_id; });

    int total_ac = 0;
    int total_wa = 0;
    int total_tle = 0;
    int total_re = 0;
    int total_ole = 0;
    int total_se = 0;
    for (const auto& program : report.programs) {
        total_ac += program.ac_count;
        total_wa += program.wa_count;
        total_tle += program.tle_count;
        total_re += program.re_count;
        total_ole += program.ole_count;
        total_se += program.se_count;
    }

    std::cout << "Completed " << report.total_cases << " cases across "
              << report.total_programs << " programs." << std::endl;
    std::cout << "Status summary: "
              << "AC=" << total_ac << ", "
              << "WA=" << total_wa << ", "
              << "TLE=" << total_tle << ", "
              << "RE=" << total_re << ", "
              << "OLE=" << total_ole << ", "
              << "SE=" << total_se << std::endl;
    return report;
}

}  // namespace mini_judge
