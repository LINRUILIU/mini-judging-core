#include "mini_judge/config.hpp"
#include "mini_judge/engine.hpp"
#include "mini_judge/reporter.hpp"

#include <Windows.h>
#include <shellapi.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

void PrintUsage() {
    std::cout << "Usage: mini_judge --config <path> [--jobs N] [--report-dir <dir>] [--quiet-progress]" << std::endl;
}

int ParseJobs(const std::wstring& raw) {
    if (raw.empty()) {
        throw std::runtime_error("--jobs requires an integer from 1 to 256");
    }
    int jobs = 0;
    for (wchar_t ch : raw) {
        if (ch < L'0' || ch > L'9') {
            throw std::runtime_error("--jobs requires an integer from 1 to 256");
        }
        jobs = jobs * 10 + static_cast<int>(ch - L'0');
        if (jobs > 256) {
            throw std::runtime_error("--jobs requires an integer from 1 to 256");
        }
    }
    if (jobs < 1) {
        throw std::runtime_error("--jobs requires an integer from 1 to 256");
    }
    return jobs;
}

}  // namespace

int main(int argc, char* argv[]) {
    // The narrow Windows argv uses the active code page and loses Unicode paths.
    // Parse the native command line so filesystem::path receives UTF-16 intact.
    (void)argc;
    (void)argv;
    try {
        int wide_argc = 0;
        std::unique_ptr<wchar_t*, decltype(&LocalFree)> wide_argv(
            CommandLineToArgvW(GetCommandLineW(), &wide_argc), &LocalFree);
        if (!wide_argv) {
            throw std::runtime_error("Failed to parse Windows command line");
        }

        std::filesystem::path config_path;
        std::filesystem::path report_dir_override;
        int jobs = 1;
        bool verbose_progress = true;
        bool have_config = false;
        bool have_jobs = false;
        bool have_report_dir = false;
        bool have_quiet_progress = false;

        for (int i = 1; i < wide_argc; ++i) {
            const std::wstring arg = wide_argv.get()[i];
            if (arg == L"--config" && i + 1 < wide_argc && !have_config) {
                config_path = std::filesystem::path(wide_argv.get()[++i]);
                have_config = true;
            } else if (arg == L"--jobs" && i + 1 < wide_argc && !have_jobs) {
                jobs = ParseJobs(wide_argv.get()[++i]);
                have_jobs = true;
            } else if (arg == L"--report-dir" && i + 1 < wide_argc && !have_report_dir) {
                report_dir_override = std::filesystem::path(wide_argv.get()[++i]);
                have_report_dir = true;
            } else if (arg == L"--quiet-progress" && !have_quiet_progress) {
                verbose_progress = false;
                have_quiet_progress = true;
            } else {
                PrintUsage();
                return 1;
            }
        }

        if (config_path.empty() || (have_report_dir && report_dir_override.empty())) {
            PrintUsage();
            return 1;
        }

        auto config = mini_judge::LoadConfig(config_path);
        if (!report_dir_override.empty()) {
            config.report_dir = std::filesystem::absolute(report_dir_override);
        }
        std::filesystem::create_directories(config.report_dir);
        if (!std::filesystem::is_directory(config.report_dir)) {
            throw std::runtime_error("Report path must be a directory: " + config.report_dir.u8string());
        }

        const auto report = mini_judge::RunJudge(config, jobs, verbose_progress);
        mini_judge::WriteJsonReport(report, config.report_dir / "report.json");
        mini_judge::WriteCsvSummary(report, config.report_dir / "summary.csv");
        mini_judge::WriteHtmlReport(report, config.report_dir / "report.html");

        std::cout << "Reports written to " << config.report_dir.u8string() << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "mini_judge failed: " << ex.what() << std::endl;
        return 1;
    }
}
