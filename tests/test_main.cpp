#include "mini_judge/comparator.hpp"
#include "mini_judge/config.hpp"
#include "mini_judge/scheduler.hpp"
#include "test_support.hpp"

#include <atomic>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace mini_judge;

// Do not use the C assert macro here: Release builds define NDEBUG.
void Check(bool condition, const char* expression, int line) {
    if (!condition) {
        throw std::runtime_error(std::string("test_main.cpp:") + std::to_string(line) +
                                 ": CHECK failed: " + expression);
    }
}
#define CHECK(expression) Check(static_cast<bool>(expression), #expression, __LINE__)

namespace {

void WriteFile(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << text;
}

void TestComparator() {
    {
        const auto result = CompareText(CompareMode::Exact, "abc\n", "abc\n");
        CHECK(result.matched);
    }
    {
        const auto result = CompareText(CompareMode::TrimRight, "abc   \n", "abc\n");
        CHECK(result.matched);
    }
    {
        const auto result = CompareText(CompareMode::IgnoreTrailingBlankLines, "abc\n\n", "abc\n");
        CHECK(result.matched);
    }
    {
        const auto result = CompareText(CompareMode::TrimRight, "a\nb\n", "a\nc\n");
        CHECK(!result.matched);
        CHECK(result.first_diff_line == 2);
    }
    CHECK(!CompareText(CompareMode::TrimRight, "<missing>\n", "").matched);
    CHECK(!CompareText(CompareMode::IgnoreTrailingBlankLines, "", "<missing>\n").matched);
}

void TestScheduler() {
    TaskScheduler scheduler(2);
    std::atomic<int> value{0};
    for (int i = 0; i < 10; ++i) {
        scheduler.Submit([&value]() { ++value; });
    }
    scheduler.Wait();
    CHECK(value.load() == 10);
}

void TestConfigValidation() {
    TestTempDir temp("basic_config");
    const auto& root = temp.path;
    std::filesystem::create_directories(root / "programs");
    std::filesystem::create_directories(root / "tests");
    WriteFile(root / "programs" / "1001.exe", "");
    WriteFile(root / "tests" / "01.in", "1 2\n");
    WriteFile(root / "tests" / "01.out", "3\n");
    WriteFile(root / "config.json",
              "{\n"
              "  \"problem_id\": \"demo\",\n"
              "  \"time_limit_ms\": 1000,\n"
              "  \"max_output_bytes\": 65536,\n"
              "  \"compare_mode\": \"trim-right\",\n"
              "  \"paths\": {\n"
              "    \"program_dir\": \"programs\",\n"
              "    \"test_dir\": \"tests\",\n"
              "    \"report_dir\": \"out\"\n"
              "  },\n"
              "  \"programs\": [{\"id\": \"1001\", \"exe\": \"1001.exe\"}],\n"
              "  \"tests\": [{\"id\": \"01\", \"input\": \"01.in\", \"answer\": \"01.out\", \"score\": 100}]\n"
              "}\n");

    const auto config = LoadConfig(root / "config.json");
    CHECK(config.programs.size() == 1);
    CHECK(config.tests.size() == 1);
    CHECK(config.compare_mode == CompareMode::TrimRight);
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--verify-check-failure") {
            CHECK(false);
        }
        TestComparator();
        TestScheduler();
        TestConfigValidation();
        std::cout << "mini_judge_tests passed" << std::endl;
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << std::endl;
        return 1;
    }
}
