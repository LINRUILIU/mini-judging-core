#include "mini_judge/runner.hpp"

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

namespace mini_judge {
namespace {

class ScopedHandle {
public:
    ScopedHandle() = default;
    explicit ScopedHandle(HANDLE handle) : handle_(handle) {}
    ~ScopedHandle() { Reset(); }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    ScopedHandle(ScopedHandle&& other) noexcept : handle_(other.handle_) { other.handle_ = nullptr; }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            Reset();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    HANDLE Get() const { return handle_; }
    HANDLE* Put() {
        Reset();
        return &handle_;
    }
    HANDLE Release() {
        HANDLE tmp = handle_;
        handle_ = nullptr;
        return tmp;
    }
    void Reset(HANDLE value = nullptr) {
        if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
        handle_ = value;
    }

private:
    HANDLE handle_ = nullptr;
};

std::string ReadRemainingPipe(HANDLE handle) {
    std::string output;
    char buffer[4096];
    DWORD read_bytes = 0;
    while (ReadFile(handle, buffer, sizeof(buffer), &read_bytes, nullptr) && read_bytes > 0) {
        output.append(buffer, buffer + read_bytes);
    }
    return output;
}

}  // namespace

ExecResult ProcessRunner::Run(const std::filesystem::path& exe_path,
                              const std::string& stdin_text,
                              int time_limit_ms,
                              std::uint64_t max_output_bytes) const {
    ExecResult result;

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    ScopedHandle stdout_read;
    ScopedHandle stdout_write;
    ScopedHandle stdin_read;
    ScopedHandle stdin_write;

    if (!CreatePipe(stdout_read.Put(), stdout_write.Put(), &sa, 0) ||
        !SetHandleInformation(stdout_read.Get(), HANDLE_FLAG_INHERIT, 0) ||
        !CreatePipe(stdin_read.Put(), stdin_write.Put(), &sa, 0) ||
        !SetHandleInformation(stdin_write.Get(), HANDLE_FLAG_INHERIT, 0)) {
        result.status = CaseStatus::SE;
        result.message = "Failed to create process pipes";
        return result;
    }

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = stdout_write.Get();
    startup.hStdError = stdout_write.Get();
    startup.hStdInput = stdin_read.Get();

    PROCESS_INFORMATION process{};
    std::wstring command = L"\"" + exe_path.wstring() + L"\"";
    std::vector<wchar_t> command_buffer(command.begin(), command.end());
    command_buffer.push_back(L'\0');
    const std::wstring working_dir = exe_path.parent_path().wstring();

    const auto start = std::chrono::steady_clock::now();

    if (!CreateProcessW(nullptr,
                        command_buffer.data(),
                        nullptr,
                        nullptr,
                        TRUE,
                        CREATE_NO_WINDOW,
                        nullptr,
                        working_dir.c_str(),
                        &startup,
                        &process)) {
        result.status = CaseStatus::SE;
        result.message = "Failed to launch process";
        return result;
    }

    ScopedHandle process_handle(process.hProcess);
    ScopedHandle thread_handle(process.hThread);
    stdout_write.Reset();
    stdin_read.Reset();

    DWORD written = 0;
    if (!stdin_text.empty()) {
        WriteFile(stdin_write.Get(), stdin_text.data(), static_cast<DWORD>(stdin_text.size()), &written, nullptr);
    }
    stdin_write.Reset();

    std::string output;
    char buffer[4096];
    bool output_limited = false;
    bool timed_out = false;

    while (true) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        if (elapsed.count() > time_limit_ms) {
            timed_out = true;
            TerminateProcess(process_handle.Get(), 124);
            WaitForSingleObject(process_handle.Get(), 1000);
            break;
        }

        DWORD available = 0;
        if (!PeekNamedPipe(stdout_read.Get(), nullptr, 0, nullptr, &available, nullptr)) {
            break;
        }

        if (available > 0) {
            DWORD read_bytes = 0;
            if (!ReadFile(stdout_read.Get(), buffer, sizeof(buffer), &read_bytes, nullptr)) {
                break;
            }
            if (read_bytes == 0) {
                break;
            }
            const auto remaining = max_output_bytes > output.size() ? max_output_bytes - output.size() : 0;
            const auto to_copy = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, read_bytes));
            output.append(buffer, buffer + to_copy);
            if (output.size() >= max_output_bytes) {
                output_limited = true;
                TerminateProcess(process_handle.Get(), 125);
                WaitForSingleObject(process_handle.Get(), 1000);
                break;
            }
        } else {
            DWORD wait = WaitForSingleObject(process_handle.Get(), 20);
            if (wait == WAIT_OBJECT_0) {
                break;
            }
        }
    }

    output += ReadRemainingPipe(stdout_read.Get());
    result.stdout_text = output;
    result.stdout_size = output.size();

    const auto end = std::chrono::steady_clock::now();
    result.time_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());

    DWORD exit_code = 0;
    if (GetExitCodeProcess(process_handle.Get(), &exit_code)) {
        result.exit_code = static_cast<int>(exit_code);
    }

    if (timed_out) {
        result.status = CaseStatus::TLE;
        result.message = "Time limit exceeded";
    } else if (output_limited) {
        result.status = CaseStatus::OLE;
        result.message = "Output limit exceeded";
    } else if (result.exit_code != 0) {
        result.status = CaseStatus::RE;
        result.message = "Process exited with non-zero code";
    } else {
        result.status = CaseStatus::AC;
        result.message = "Execution succeeded";
    }

    return result;
}

}  // namespace mini_judge
