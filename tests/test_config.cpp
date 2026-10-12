#include "mini_judge/config.hpp"
#include "test_support.hpp"

#include <filesystem>
#include <iostream>
#include <string>

using namespace mini_judge;

namespace {

std::string BaseConfig() {
    return R"json({
  "problem_id": "demo",
  "time_limit_ms": 1000,
  "max_output_bytes": 65536,
  "compare_mode": "trim-right",
  "paths": {"program_dir": "programs", "test_dir": "tests", "report_dir": "out"},
  "programs": [{"id": "1001", "exe": "1001.exe"}],
  "tests": [{"id": "01", "input": "01.in", "answer": "01.out", "score": 100}]
})json";
}

void TestConfig() {
    TestTempDir temp("config");
    TestWriteFile(temp.path / "programs" / "1001.exe", "stub");
    TestWriteFile(temp.path / "programs" / "1002.exe", "stub");
    TestWriteFile(temp.path / "tests" / "01.in", "1 2\n");
    TestWriteFile(temp.path / "tests" / "01.out", "3\n");
    TestWriteFile(temp.path / "tests" / "02.in", "2 3\n");
    TestWriteFile(temp.path / "tests" / "02.out", "5\n");
    std::filesystem::create_directories(temp.path / "programs" / "exe-directory.exe");
    std::filesystem::create_directories(temp.path / "tests" / "input-directory.in");
    std::filesystem::create_directories(temp.path / "tests" / "answer-directory.out");
    TestWriteFile(temp.path / "answer-file", "not a directory");

    const auto config_path = temp.path / "config.json";
    const auto load = [&](const std::string& text) {
        TestWriteFile(config_path, text);
        return LoadConfig(config_path);
    };
    const auto expect_invalid = [&](const std::string& text) {
        TestExpectThrows([&]() { (void)load(text); });
    };

    const auto valid = load(BaseConfig());
    TEST_CHECK(valid.programs.size() == 1);
    TEST_CHECK(valid.tests.size() == 1);
    TEST_CHECK(valid.compare_mode == CompareMode::TrimRight);
    TEST_CHECK(valid.answer_dir == valid.test_dir);
    TEST_CHECK(!std::filesystem::exists(valid.report_dir));

    // Loading resolves output paths without creating or validating a destination
    // that the CLI may replace with --report-dir.
    const auto blocked_report_path = temp.path / "report-file";
    TestWriteFile(blocked_report_path, "keep this file");
    const auto blocked_report = load(TestReplaceOnce(BaseConfig(), "\"report_dir\": \"out\"",
                                                     "\"report_dir\": \"report-file\""));
    TEST_CHECK(blocked_report.report_dir == blocked_report_path);
    TEST_CHECK(TestReadFile(blocked_report_path) == "keep this file");

    for (const auto& value : {"0", "-1", "1.5", "1e2", "3600001", "2147483648"}) {
        expect_invalid(TestReplaceOnce(BaseConfig(), "\"time_limit_ms\": 1000",
                                       std::string("\"time_limit_ms\": ") + value));
    }
    for (const auto& value : {"0", "-1", "1.5", "1e2", "67108865", "18446744073709551616"}) {
        expect_invalid(TestReplaceOnce(BaseConfig(), "\"max_output_bytes\": 65536",
                                       std::string("\"max_output_bytes\": ") + value));
    }
    for (const auto& value : {"-1", "1.5", "2147483648"}) {
        expect_invalid(TestReplaceOnce(BaseConfig(), "\"score\": 100", std::string("\"score\": ") + value));
    }
    expect_invalid(TestReplaceOnce(BaseConfig(), "\"id\": \"1001\"", "\"id\": \"\""));
    expect_invalid(TestReplaceOnce(BaseConfig(), "\"id\": \"01\"", "\"id\": \"\""));
    expect_invalid(TestReplaceOnce(BaseConfig(),
                                   "\"programs\": [{\"id\": \"1001\", \"exe\": \"1001.exe\"}]",
                                   "\"programs\": [{\"id\": \"1001\", \"exe\": \"1001.exe\"},"
                                   "{\"id\": \"1001\", \"exe\": \"1002.exe\"}]"));
    expect_invalid(TestReplaceOnce(BaseConfig(),
                                   "\"tests\": [{\"id\": \"01\", \"input\": \"01.in\", \"answer\": \"01.out\", \"score\": 100}]",
                                   "\"tests\": [{\"id\": \"01\", \"input\": \"01.in\", \"answer\": \"01.out\", \"score\": 100},"
                                   "{\"id\": \"01\", \"input\": \"02.in\", \"answer\": \"02.out\", \"score\": 100}]"));
    expect_invalid(TestReplaceOnce(BaseConfig(),
                                   "\"tests\": [{\"id\": \"01\", \"input\": \"01.in\", \"answer\": \"01.out\", \"score\": 100}]",
                                   "\"tests\": [{\"id\": \"01\", \"input\": \"01.in\", \"answer\": \"01.out\", \"score\": 2147483647},"
                                   "{\"id\": \"02\", \"input\": \"02.in\", \"answer\": \"02.out\", \"score\": 1}]"));

    expect_invalid(TestReplaceOnce(BaseConfig(), "\"report_dir\": \"out\"",
                                   "\"answer_dir\": 123, \"report_dir\": \"out\""));
    expect_invalid(TestReplaceOnce(BaseConfig(), "\"report_dir\": \"out\"",
                                   "\"answer_dir\": \"answer-file\", \"report_dir\": \"out\""));
    expect_invalid(TestReplaceOnce(BaseConfig(), "\"exe\": \"1001.exe\"",
                                   "\"exe\": \"exe-directory.exe\""));
    expect_invalid(TestReplaceOnce(BaseConfig(), "\"input\": \"01.in\"",
                                   "\"input\": \"input-directory.in\""));
    expect_invalid(TestReplaceOnce(BaseConfig(), "\"answer\": \"01.out\"",
                                   "\"answer\": \"answer-directory.out\""));

    expect_invalid(TestReplaceOnce(BaseConfig(), "\"problem_id\": \"demo\"",
                                   "\"problem_id\": \"demo\", \"problem_id\": \"other\""));
    expect_invalid(TestReplaceOnce(BaseConfig(), "\"problem_id\": \"demo\"",
                                   "\"problem_id\": \"\\uD800\""));
    expect_invalid(TestReplaceOnce(BaseConfig(), "\"problem_id\": \"demo\"",
                                   "\"problem_id\": \"\\uDE00\""));
    expect_invalid(TestReplaceOnce(BaseConfig(), "\"problem_id\": \"demo\"",
                                   "\"problem_id\": \"\\uZZZZ\""));
    expect_invalid(TestReplaceOnce(BaseConfig(), "\"exe\": \"1001.exe\"",
                                   "\"exe\": \"bad\\u0000path.exe\""));
    const auto invalid_utf8 = TestReplaceOnce(BaseConfig(), "\"problem_id\": \"demo\"",
                                              std::string("\"problem_id\": \"de") +
                                              static_cast<char>(0xFF) + "mo\"");
    expect_invalid(invalid_utf8);

    const auto unicode_bmp = load(TestReplaceOnce(BaseConfig(), "\"problem_id\": \"demo\"",
                                                  "\"problem_id\": \"\\u4E2D\""));
    TEST_CHECK(unicode_bmp.problem_id == "\xE4\xB8\xAD");
    const auto unicode_pair = load(TestReplaceOnce(BaseConfig(), "\"problem_id\": \"demo\"",
                                                   "\"problem_id\": \"\\uD83D\\uDE00\""));
    TEST_CHECK(unicode_pair.problem_id == "\xF0\x9F\x98\x80");

    const std::string too_deep = TestReplaceOnce(BaseConfig(), "\"problem_id\": \"demo\"",
                                                  "\"problem_id\": \"demo\", \"extra\": " +
                                                  std::string(65, '[') + "0" + std::string(65, ']'));
    expect_invalid(too_deep);
    expect_invalid(BaseConfig() + std::string(17 * 1024 * 1024, ' '));

    // A valid maximum is accepted; validation is about range, not arbitrary small limits.
    auto upper_bound = TestReplaceOnce(BaseConfig(), "\"time_limit_ms\": 1000",
                                       "\"time_limit_ms\": 3600000");
    upper_bound = TestReplaceOnce(upper_bound, "\"max_output_bytes\": 65536",
                                  "\"max_output_bytes\": 67108864");
    TEST_CHECK(load(upper_bound).time_limit_ms == 3600000);
}

} // namespace

int main() {
    try {
        TestConfig();
        std::cout << "mini_judge_config_tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
