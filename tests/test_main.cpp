#include "mini_judge/comparator.hpp"
#include "mini_judge/config.hpp"
#include "mini_judge/scheduler.hpp"

#include <atomic>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace mini_judge;

namespace {

void WriteFile(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    out << text;
}

void TestComparator() {
    {
        const auto result = CompareText(CompareMode::Exact, "abc\n", "abc\n");
        assert(result.matched);
    }
    {
        const auto result = CompareText(CompareMode::TrimRight, "abc   \n", "abc\n");
        assert(result.matched);
    }
    {
        const auto result = CompareText(CompareMode::IgnoreTrailingBlankLines, "abc\n\n", "abc\n");
        assert(result.matched);
    }
    {
        const auto result = CompareText(CompareMode::TrimRight, "a\nb\n", "a\nc\n");
        assert(!result.matched);
        assert(result.first_diff_line == 2);
    }
}

void TestScheduler() {
    TaskScheduler scheduler(2);
    std::atomic<int> value{0};
    for (int i = 0; i < 10; ++i) {
        scheduler.Submit([&value]() { ++value; });
    }
    scheduler.Wait();
    assert(value.load() == 10);
}

void TestConfigValidation() {
    const auto root = std::filesystem::temp_directory_path() / "mini_judge_config_test";
    std::filesystem::remove_all(root);
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
    assert(config.programs.size() == 1);
    assert(config.tests.size() == 1);
    assert(config.compare_mode == CompareMode::TrimRight);
    std::filesystem::remove_all(root);
}

}  // namespace

int main() {
    TestComparator();
    TestScheduler();
    TestConfigValidation();
    std::cout << "mini_judge_tests passed" << std::endl;
    return 0;
}
