#include "mini_judge/config.hpp"
#include "mini_judge/engine.hpp"
#include "mini_judge/reporter.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void PrintUsage() {
    std::cout << "Usage: mini_judge --config <path> [--jobs N] [--report-dir <dir>] [--quiet-progress]" << std::endl;
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        std::filesystem::path config_path;
        std::filesystem::path report_dir_override;
        int jobs = 1;
        bool verbose_progress = true;

        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--config" && i + 1 < argc) {
                config_path = argv[++i];
            } else if (arg == "--jobs" && i + 1 < argc) {
                jobs = std::stoi(argv[++i]);
            } else if (arg == "--report-dir" && i + 1 < argc) {
                report_dir_override = argv[++i];
            } else if (arg == "--quiet-progress") {
                verbose_progress = false;
            } else {
                PrintUsage();
                return 1;
            }
        }

        if (config_path.empty()) {
            PrintUsage();
            return 1;
        }

        auto config = mini_judge::LoadConfig(config_path);
        if (!report_dir_override.empty()) {
            config.report_dir = std::filesystem::absolute(report_dir_override);
            std::filesystem::create_directories(config.report_dir);
        }

        const auto report = mini_judge::RunJudge(config, jobs, verbose_progress);
        mini_judge::WriteJsonReport(report, config.report_dir / "report.json");
        mini_judge::WriteCsvSummary(report, config.report_dir / "summary.csv");
        mini_judge::WriteHtmlReport(report, config.report_dir / "report.html");

        std::cout << "Reports written to " << config.report_dir.string() << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "mini_judge failed: " << ex.what() << std::endl;
        return 1;
    }
}
