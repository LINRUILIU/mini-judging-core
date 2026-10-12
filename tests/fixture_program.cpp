#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#ifndef FIXTURE_MODE
#error FIXTURE_MODE must be defined by CMake
#endif

namespace {

void WriteRepeated(std::ostream& out, std::size_t bytes, char value) {
    const std::string block(4096, value);
    while (bytes > 0) {
        const auto count = bytes < block.size() ? bytes : block.size();
        out.write(block.data(), static_cast<std::streamsize>(count));
        out.flush();
        bytes -= count;
    }
}

int SpawnChild() {
    wchar_t module[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, module, MAX_PATH) == 0) {
        return 30;
    }
    auto child = std::filesystem::path(module).parent_path() / L"fixture_spawn_child.exe";
    const std::string marker_bytes(std::istreambuf_iterator<char>(std::cin), {});
    const std::wstring marker = std::filesystem::u8path(marker_bytes).wstring();
    if (!SetEnvironmentVariableW(L"MINI_JUDGE_CHILD_MARKER", marker.c_str())) {
        return 31;
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION process{};
    std::wstring command = L"\"" + child.wstring() + L"\"";
    if (!CreateProcessW(child.c_str(), command.data(), nullptr, nullptr, TRUE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
        return 32;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(1500)) {
        if (std::filesystem::exists(marker)) {
            return 0;
        }
        Sleep(10);
    }
    return 33;
}

int MarkAndSleep() {
    wchar_t marker[32768]{};
    const DWORD copied = GetEnvironmentVariableW(L"MINI_JUDGE_CHILD_MARKER", marker, 32768);
    if (copied == 0 || copied >= 32768) {
        return 34;
    }
    const auto final_path = std::filesystem::path(marker);
    const auto staging_path = final_path.wstring() + L".tmp";
    std::ofstream out(std::filesystem::path(staging_path), std::ios::binary);
    if (!out) {
        return 35;
    }
    out << GetCurrentProcessId();
    out.close();
    std::filesystem::rename(staging_path, final_path);
    Sleep(2500); // Last-resort self-termination if the runner fails to clean the Job.
    return 0;
}

} // namespace

int main() {
#if FIXTURE_MODE == 0
    std::cout << "ok";
#elif FIXTURE_MODE == 1
    std::cout << "wrong";
#elif FIXTURE_MODE == 2
    Sleep(2500);
#elif FIXTURE_MODE == 3
    return 7;
#elif FIXTURE_MODE == 4
    WriteRepeated(std::cout, 63, 'X');
#elif FIXTURE_MODE == 5
    WriteRepeated(std::cout, 64, 'X');
#elif FIXTURE_MODE == 6
    WriteRepeated(std::cout, 65, 'X');
#elif FIXTURE_MODE == 7
    std::cerr << "err";
    std::cout << "ok";
#elif FIXTURE_MODE == 8
    WriteRepeated(std::cerr, 1024 * 1024, 'E');
#elif FIXTURE_MODE == 9
    Sleep(2500);
#elif FIXTURE_MODE == 10
    WriteRepeated(std::cout, 64 * 1024, 'P');
    std::cout << "ready|" << std::flush;
    std::size_t total = 0;
    char buffer[4096];
    while (std::cin.read(buffer, sizeof(buffer)) || std::cin.gcount() > 0) {
        total += static_cast<std::size_t>(std::cin.gcount());
    }
    std::cout << total;
#elif FIXTURE_MODE == 11
    return 0;
#elif FIXTURE_MODE == 12
    return SpawnChild();
#elif FIXTURE_MODE == 13
    return MarkAndSleep();
#elif FIXTURE_MODE == 14
    std::cout << std::cin.rdbuf();
#elif FIXTURE_MODE == 15
    WriteRepeated(std::cout, 1024 * 1024, 'O');
#else
#error Unsupported fixture mode
#endif
    return 0;
}
