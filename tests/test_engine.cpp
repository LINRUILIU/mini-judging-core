#include "mini_judge/engine.hpp"
#include "test_support.hpp"

#include <Windows.h>
#include <shellapi.h>

#include <iostream>
#include <limits>

using namespace mini_judge;

int main() {
    try {
        int argc = 0;
        LPWSTR* wide_argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        TEST_CHECK(wide_argv != nullptr);
        TEST_CHECK(argc == 2);
        const std::filesystem::path sample_exe(wide_argv[1]);
        LocalFree(wide_argv);
        TestTempDir temp("engine");
        TestWriteFile(temp.path / "01.in", "1 2\n");
        TestWriteFile(temp.path / "01.out", "3\n");
        ProblemConfig config;
        config.problem_id = "engine-regression";
        config.time_limit_ms = 5000;
        ProgramEntry program;
        program.id = "p";
        program.exe_path = std::filesystem::absolute(sample_exe);
        config.programs.push_back(program);
        TestCase good;
        good.id = "01";
        good.score = 7;
        good.input_path = temp.path / "01.in";
        good.answer_path = temp.path / "01.out";
        config.tests.push_back(good);
        auto missing = good;
        missing.id = "02";
        missing.input_path = temp.path / "missing.in";
        config.tests.push_back(missing);

        // A case I/O failure must not lose the other case or corrupt totals.
        const auto report = RunJudge(config, 2, false);
        TEST_CHECK(report.total_programs == 1);
        TEST_CHECK(report.total_cases == 2);
        TEST_CHECK(report.programs.size() == 1);
        const auto& result = report.programs.front();
        TEST_CHECK(result.cases.size() == 2);
        TEST_CHECK(result.cases[0].status == CaseStatus::AC);
        TEST_CHECK(result.cases[1].status == CaseStatus::SE);
        TEST_CHECK(result.cases[1].message.find("Failed to open text file") != std::string::npos);
        TEST_CHECK(result.passed_count == 1 && result.failed_count == 1);
        TEST_CHECK(result.ac_count == 1 && result.se_count == 1);
        TEST_CHECK(result.total_score == 7 && result.max_score == 14);

        TestExpectThrows([&]() { RunJudge(config, 0, false); });
        TestExpectThrows([&]() { RunJudge(config, 257, false); });
        auto invalid = config;
        invalid.programs.push_back(program);
        TestExpectThrows([&]() { RunJudge(invalid, 1, false); });
        invalid = config;
        invalid.tests.push_back(good);
        TestExpectThrows([&]() { RunJudge(invalid, 1, false); });
        invalid = config;
        invalid.tests[0].score = (std::numeric_limits<int>::max)();
        TestExpectThrows([&]() { RunJudge(invalid, 1, false); });
        invalid = config;
        invalid.tests[0].score = -1;
        TestExpectThrows([&]() { RunJudge(invalid, 1, false); });
        invalid = config;
        invalid.programs.clear();
        TestExpectThrows([&]() { RunJudge(invalid, 1, false); });
        invalid = config;
        invalid.time_limit_ms = 0;
        TestExpectThrows([&]() { RunJudge(invalid, 1, false); });
        invalid = config;
        invalid.max_output_bytes = 0;
        TestExpectThrows([&]() { RunJudge(invalid, 1, false); });
        std::cout << "mini_judge_engine_tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
