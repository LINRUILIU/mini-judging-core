#include "mini_judge/runner.hpp"
#include "test_support.hpp"

#include <Windows.h>
#include <shellapi.h>

#include <chrono>
#include <filesystem>
#include <future>
#include <iostream>
#include <string>
#include <vector>

using namespace mini_judge;
using namespace std::chrono_literals;

namespace {

std::filesystem::path Fixture(const std::filesystem::path& dir, const std::string& name) {
    const auto path = dir / ("fixture_" + name + ".exe");
    TEST_CHECK(std::filesystem::is_regular_file(path));
    return path;
}

ExecResult Run(const std::filesystem::path& path, const std::string& input,
               int time_limit_ms, std::uint64_t output_limit, int max_wall_ms = 2500) {
    const auto start = std::chrono::steady_clock::now();
    const auto result = ProcessRunner().Run(path, input, time_limit_ms, output_limit);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    TEST_CHECK(elapsed < max_wall_ms);
    return result;
}

void TestOutputBoundaries(const std::filesystem::path& dir) {
    const auto below = Run(Fixture(dir, "out63"), "", 1000, 64);
    TEST_CHECK(below.status == CaseStatus::AC);
    TEST_CHECK(below.stdout_size == 63);
    TEST_CHECK(below.stdout_text == std::string(63, 'X'));
    const auto exact = Run(Fixture(dir, "out64"), "", 1000, 64);
    TEST_CHECK(exact.status == CaseStatus::AC);
    TEST_CHECK(exact.stdout_size == 64);
    TEST_CHECK(exact.stdout_text == std::string(64, 'X'));
    const auto above = Run(Fixture(dir, "out65"), "", 1000, 64);
    TEST_CHECK(above.status == CaseStatus::OLE);
    TEST_CHECK(above.stdout_text.size() + above.stderr_text.size() <= 64);

    const auto stderr_only = Run(Fixture(dir, "stderr_only"), "", 1000, 64);
    TEST_CHECK(stderr_only.status == CaseStatus::AC);
    TEST_CHECK(stderr_only.stdout_text == "ok");
    TEST_CHECK(stderr_only.stderr_text == "err");
    TEST_CHECK(stderr_only.stdout_size == 2);
    TEST_CHECK(Run(Fixture(dir, "stderr_only"), "", 1000, 5).status == CaseStatus::AC);
    TEST_CHECK(Run(Fixture(dir, "stderr_only"), "", 1000, 4).status == CaseStatus::OLE);
    const auto stderr_flood = Run(Fixture(dir, "stderr_flood"), "", 1000, 64);
    TEST_CHECK(stderr_flood.status == CaseStatus::OLE);
    TEST_CHECK(stderr_flood.stdout_text.size() + stderr_flood.stderr_text.size() <= 64);
    const auto stdout_flood = Run(Fixture(dir, "stdout_flood"), "", 1000, 64);
    TEST_CHECK(stdout_flood.status == CaseStatus::OLE);
    TEST_CHECK(stdout_flood.stdout_text.size() + stdout_flood.stderr_text.size() <= 64);
    TEST_CHECK(Run(Fixture(dir, "ac"), "", 1000, 0).status == CaseStatus::SE);
}

void TestInputAndLifetime(const std::filesystem::path& dir) {
    const std::string large_input(2 * 1024 * 1024, 'I');
    const auto unread = Run(Fixture(dir, "unread_sleep"), large_input, 150, 64);
    TEST_CHECK(unread.status == CaseStatus::TLE);
    const auto early = Run(Fixture(dir, "early_exit"), large_input, 1000, 64);
    TEST_CHECK(early.status == CaseStatus::AC);
    const auto output_first = Run(Fixture(dir, "output_then_read"),
                                  std::string(256 * 1024, 'I'), 2000, 100 * 1024);
    TEST_CHECK(output_first.status == CaseStatus::AC);
    TEST_CHECK(output_first.stdout_text == std::string(64 * 1024, 'P') + "ready|262144");
    const auto timeout = Run(Fixture(dir, "sleep"), "", 100, 64);
    TEST_CHECK(timeout.status == CaseStatus::TLE);
    const auto failure = Run(Fixture(dir, "re"), "", 1000, 64);
    TEST_CHECK(failure.status == CaseStatus::RE);
    TEST_CHECK(failure.exit_code == 7);

    TestTempDir temp("runner_child");
    const auto marker = temp.path / "spawn_marker.txt";
    const auto parent = Run(Fixture(dir, "spawn_parent"), marker.u8string(), 1000, 64);
    TEST_CHECK(parent.status == CaseStatus::AC);
    TEST_CHECK(std::filesystem::is_regular_file(marker));
    const auto pid = std::stoul(TestReadFile(marker));
    HANDLE child = OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid));
    if (child != nullptr) {
        const DWORD wait = WaitForSingleObject(child, 800);
        CloseHandle(child);
        TEST_CHECK(wait == WAIT_OBJECT_0);
    }
}

void TestPathsAndConcurrency(const std::filesystem::path& dir) {
    TestTempDir temp("runner_unicode");
    const auto malformed_path = temp.path / "malformed.exe";
    TestWriteFile(malformed_path, "not a Windows executable");
    const auto malformed = Run(malformed_path, "", 1000, 64);
    TEST_CHECK(malformed.status == CaseStatus::SE);
    TEST_CHECK(malformed.message.find("Inspecting executable type failed") != std::string::npos ||
               malformed.message.find("Only 32-bit and 64-bit") != std::string::npos);

    const DWORD error_mode_before = GetThreadErrorMode();
    const auto valid_launch = Run(Fixture(dir, "ac"), "", 1000, 64);
    TEST_CHECK(valid_launch.status == CaseStatus::AC);
    TEST_CHECK(GetThreadErrorMode() == error_mode_before);

    const auto target_dir = temp.path / L"含 空格";
    std::filesystem::create_directory(target_dir);
    const auto copied = target_dir / L"echo 程序.exe";
    std::filesystem::copy_file(Fixture(dir, "echo"), copied);
    const auto unicode = Run(copied, "path works", 1000, 64);
    TEST_CHECK(unicode.status == CaseStatus::AC);
    TEST_CHECK(unicode.stdout_text == "path works");

    std::vector<std::future<ExecResult>> tasks;
    for (int i = 0; i < 8; ++i) {
        tasks.push_back(std::async(std::launch::async, [&, i]() {
            return Run(Fixture(dir, "echo"), "token-" + std::to_string(i), 1000, 64);
        }));
    }
    for (int i = 0; i < 8; ++i) {
        const auto result = tasks[i].get();
        TEST_CHECK(result.status == CaseStatus::AC);
        TEST_CHECK(result.stdout_text == "token-" + std::to_string(i));
    }

    const auto missing = Run(temp.path / "missing.exe", "", 1000, 64);
    TEST_CHECK(missing.status == CaseStatus::SE);
}

} // namespace

int main() {
    try {
        int argc = 0;
        LPWSTR* wide_argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        TEST_CHECK(wide_argv != nullptr);
        TEST_CHECK(argc == 2);
        const std::filesystem::path dir(wide_argv[1]);
        LocalFree(wide_argv);
        TestOutputBoundaries(dir);
        TestInputAndLifetime(dir);
        TestPathsAndConcurrency(dir);
        std::cout << "mini_judge_runner_tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
