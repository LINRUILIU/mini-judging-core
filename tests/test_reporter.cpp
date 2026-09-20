#include "mini_judge/reporter.hpp"
#include "test_support.hpp"

#include <iostream>
#include <string>

using namespace mini_judge;

int main() {
    try {
        TestTempDir temp("reporter");
        BatchReport report;
        report.problem_id = "quote\" slash\\ newline\n tab\t control";
        report.problem_id.push_back(static_cast<char>(1));
        report.generated_at = "2026-09-20 12:00:00";
        report.total_programs = 1;
        report.total_cases = 1;
        ProgramReport program;
        program.program_id = "id,\"line\nnext";
        CaseResult testcase;
        testcase.test_id = "01";
        testcase.status = CaseStatus::WA;
        testcase.message = "bad\nline";
        testcase.message.push_back(static_cast<char>(2));
        program.cases.push_back(testcase);
        report.programs.push_back(program);

        const auto json_path = temp.path / "report.json";
        const auto csv_path = temp.path / "summary.csv";
        const auto html_path = temp.path / "report.html";
        WriteJsonReport(report, json_path);
        WriteCsvSummary(report, csv_path);
        WriteHtmlReport(report, html_path);
        const auto json = TestReadFile(json_path);
        const auto csv = TestReadFile(csv_path);
        const auto html = TestReadFile(html_path);
        TEST_CHECK(json.find("quote\\\"") != std::string::npos);
        TEST_CHECK(json.find("\\u0001") != std::string::npos);
        TEST_CHECK(json.find("\\u0002") != std::string::npos);
        TEST_CHECK(json.find(static_cast<char>(1)) == std::string::npos);
        TEST_CHECK(json.find(static_cast<char>(2)) == std::string::npos);
        TEST_CHECK(csv.find("\"id,\"\"line\nnext\"") != std::string::npos);
        TEST_CHECK(html.find("quote&quot;") != std::string::npos);

        TestExpectThrows([&]() { WriteJsonReport(report, temp.path); });
        TestExpectThrows([&]() { WriteCsvSummary(report, temp.path); });
        TestExpectThrows([&]() { WriteHtmlReport(report, temp.path); });
        std::cout << "mini_judge_reporter_tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
