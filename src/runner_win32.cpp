#if !defined(_WIN32_WINNT) || _WIN32_WINNT < 0x0601
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "mini_judge/runner.hpp"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace mini_judge {
namespace {

using Clock = std::chrono::steady_clock;

class TimeExpired : public std::runtime_error {
public:
    TimeExpired() : std::runtime_error("Time limit exceeded") {}
};

class ScopedHandle {
public:
    ScopedHandle() = default;
    explicit ScopedHandle(HANDLE handle) : handle_(handle) {}
    ~ScopedHandle() { Reset(); }
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    HANDLE Get() const { return handle_; }
    HANDLE* Put() {
        Reset();
        return &handle_;
    }
    void Reset(HANDLE next = nullptr) {
        if (handle_ != nullptr && handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
        }
        handle_ = next;
    }

private:
    HANDLE handle_ = nullptr;
};

class ScopedThreadErrorMode {
public:
    ScopedThreadErrorMode() {
        const DWORD quiet = SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                            SEM_NOOPENFILEERRORBOX;
        if (!SetThreadErrorMode(GetThreadErrorMode() | quiet, &previous_)) {
            throw std::runtime_error("Setting thread error mode failed (Win32 error " +
                                     std::to_string(GetLastError()) + ")");
        }
    }
    ~ScopedThreadErrorMode() { SetThreadErrorMode(previous_, nullptr); }
    ScopedThreadErrorMode(const ScopedThreadErrorMode&) = delete;
    ScopedThreadErrorMode& operator=(const ScopedThreadErrorMode&) = delete;

private:
    DWORD previous_ = 0;
};

std::string Win32Error(const char* action, DWORD code) {
    return std::string(action) + " (Win32 error " + std::to_string(code) + ")";
}

int ElapsedMs(Clock::time_point start) {
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count();
    return static_cast<int>(std::min<decltype(ms)>(ms, (std::numeric_limits<int>::max)()));
}

void CheckDeadline(Clock::time_point deadline) {
    if (Clock::now() >= deadline) {
        throw TimeExpired();
    }
}

std::filesystem::path TempBase() {
    const DWORD required = GetTempPathW(0, nullptr);
    if (required == 0) {
        throw std::runtime_error(Win32Error("GetTempPathW failed", GetLastError()));
    }
    std::vector<wchar_t> buffer(static_cast<std::size_t>(required) + 1);
    const DWORD written = GetTempPathW(static_cast<DWORD>(buffer.size()), buffer.data());
    if (written == 0 || written >= buffer.size()) {
        throw std::runtime_error(Win32Error("GetTempPathW failed", GetLastError()));
    }
    auto base = std::filesystem::path(std::wstring(buffer.data(), written)).lexically_normal();
    if (base.filename().empty()) {
        base = base.parent_path();
    }
    return base;
}

class RunDirectory {
public:
    RunDirectory() : base_(TempBase()) {
        static std::atomic<unsigned long long> next_id{0};
        for (int attempt = 0; attempt < 32; ++attempt) {
            const std::wstring name = L"mini_judge_" + std::to_wstring(GetCurrentProcessId()) +
                L"_" + std::to_wstring(GetTickCount64()) + L"_" + std::to_wstring(++next_id);
            root_ = base_ / name;
            if (CreateDirectoryW(root_.c_str(), nullptr)) {
                work_ = root_ / L"work";
                if (!CreateDirectoryW(work_.c_str(), nullptr)) {
                    const DWORD error = GetLastError();
                    RemoveDirectoryW(root_.c_str());
                    root_.clear();
                    throw std::runtime_error(Win32Error("Creating working directory failed", error));
                }
                return;
            }
            const DWORD error = GetLastError();
            if (error != ERROR_ALREADY_EXISTS) {
                throw std::runtime_error(Win32Error("Creating run directory failed", error));
            }
        }
        throw std::runtime_error("Could not allocate a unique run directory");
    }

    ~RunDirectory() {
        (void)Cleanup();
    }

    const std::filesystem::path& Work() const { return work_; }
    std::filesystem::path InputFile() const { return root_ / L"stdin.bin"; }

    std::error_code Cleanup() {
        if (root_.empty()) {
            return {};
        }
        // A recursive delete is only allowed for the unique directory we created under TempBase.
        if (root_.parent_path().lexically_normal() != base_ ||
            root_.filename().wstring().find(L"mini_judge_") != 0) {
            return std::make_error_code(std::errc::invalid_argument);
        }
        std::error_code error;
        std::filesystem::remove_all(root_, error);
        if (!error) {
            root_.clear();
        }
        return error;
    }

private:
    std::filesystem::path base_;
    std::filesystem::path root_;
    std::filesystem::path work_;
};

class AttributeList {
public:
    explicit AttributeList(const HANDLE (&handles)[3]) {
        SIZE_T bytes = 0;
        InitializeProcThreadAttributeList(nullptr, 1, 0, &bytes);
        if (bytes == 0) {
            throw std::runtime_error(Win32Error("Sizing process attributes failed", GetLastError()));
        }
        storage_.resize((bytes + sizeof(std::uintptr_t) - 1) / sizeof(std::uintptr_t));
        list_ = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage_.data());
        if (!InitializeProcThreadAttributeList(list_, 1, 0, &bytes)) {
            list_ = nullptr;
            throw std::runtime_error(Win32Error("Initializing process attributes failed", GetLastError()));
        }
        if (!UpdateProcThreadAttribute(list_, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                       const_cast<HANDLE*>(handles), sizeof(handles), nullptr, nullptr)) {
            const DWORD error = GetLastError();
            DeleteProcThreadAttributeList(list_);
            list_ = nullptr;
            throw std::runtime_error(Win32Error("Restricting inherited handles failed", error));
        }
    }

    ~AttributeList() {
        if (list_ != nullptr) {
            DeleteProcThreadAttributeList(list_);
        }
    }
    AttributeList(const AttributeList&) = delete;
    AttributeList& operator=(const AttributeList&) = delete;
    LPPROC_THREAD_ATTRIBUTE_LIST Get() const { return list_; }

private:
    std::vector<std::uintptr_t> storage_;
    LPPROC_THREAD_ATTRIBUTE_LIST list_ = nullptr;
};

void MakePipe(ScopedHandle& read_end, ScopedHandle& write_end,
              SECURITY_ATTRIBUTES& inheritable) {
    if (!CreatePipe(read_end.Put(), write_end.Put(), &inheritable, 0)) {
        throw std::runtime_error(Win32Error("Creating output pipe failed", GetLastError()));
    }
    if (!SetHandleInformation(read_end.Get(), HANDLE_FLAG_INHERIT, 0)) {
        throw std::runtime_error(Win32Error("Making output reader private failed", GetLastError()));
    }
}

void StageInput(const std::filesystem::path& path, const std::string& input,
                Clock::time_point deadline) {
    CheckDeadline(deadline);
    ScopedHandle file(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                  FILE_ATTRIBUTE_TEMPORARY, nullptr));
    if (file.Get() == INVALID_HANDLE_VALUE) {
        throw std::runtime_error(Win32Error("Creating stdin file failed", GetLastError()));
    }
    std::size_t offset = 0;
    while (offset < input.size()) {
        CheckDeadline(deadline);
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(input.size() - offset, 65536));
        DWORD written = 0;
        if (!WriteFile(file.Get(), input.data() + offset, chunk, &written, nullptr)) {
            throw std::runtime_error(Win32Error("Writing stdin file failed", GetLastError()));
        }
        if (written == 0) {
            throw std::runtime_error("Writing stdin file made no progress");
        }
        offset += written;
    }
    CheckDeadline(deadline);
}

enum class PipeState { Open, Closed };

// Only one reader owns each pipe. Read no more than PeekNamedPipe says is available.
bool DrainAvailable(HANDLE pipe, PipeState& state, std::string& captured,
                    std::uint64_t& total_captured, std::uint64_t limit,
                    bool& read_anything) {
    if (state == PipeState::Closed) {
        return false;
    }
    DWORD available = 0;
    if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr)) {
        const DWORD error = GetLastError();
        if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED) {
            state = PipeState::Closed;
            return false;
        }
        throw std::runtime_error(Win32Error("Peeking process output failed", error));
    }
    if (available == 0) {
        return false;
    }
    char buffer[4096];
    const DWORD requested = std::min<DWORD>(available, sizeof(buffer));
    DWORD received = 0;
    if (!ReadFile(pipe, buffer, requested, &received, nullptr)) {
        const DWORD error = GetLastError();
        if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED) {
            state = PipeState::Closed;
            return false;
        }
        throw std::runtime_error(Win32Error("Reading process output failed", error));
    }
    if (received == 0) {
        state = PipeState::Closed;
        return false;
    }
    read_anything = true;
    const std::uint64_t remaining = limit - total_captured;
    const auto kept = static_cast<std::size_t>(std::min<std::uint64_t>(remaining, received));
    captured.append(buffer, kept);
    total_captured += kept;
    return received > remaining;
}

void TerminateTree(HANDLE job) {
    if (!TerminateJobObject(job, 125)) {
        throw std::runtime_error(Win32Error("Terminating process tree failed", GetLastError()));
    }
}

void WaitForTreeExit(HANDLE job, Clock::time_point cleanup_deadline) {
    while (true) {
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting{};
        if (!QueryInformationJobObject(job, JobObjectBasicAccountingInformation,
                                       &accounting, sizeof(accounting), nullptr)) {
            throw std::runtime_error(Win32Error("Querying process tree failed", GetLastError()));
        }
        if (accounting.ActiveProcesses == 0) {
            return;
        }
        if (Clock::now() >= cleanup_deadline) {
            throw std::runtime_error("Process tree cleanup exceeded one second");
        }
        Sleep(5);
    }
}

ExecResult RunImpl(const std::filesystem::path& exe_path, const std::string& stdin_text,
                   std::uint64_t max_output_bytes, RunDirectory& directory,
                   Clock::time_point start, Clock::time_point deadline) {
    ExecResult result;
    const auto absolute_exe = std::filesystem::absolute(exe_path);
    DWORD binary_type = 0;
    if (!GetBinaryTypeW(absolute_exe.c_str(), &binary_type)) {
        throw std::runtime_error(Win32Error("Inspecting executable type failed", GetLastError()));
    }
    if (binary_type != SCS_32BIT_BINARY && binary_type != SCS_64BIT_BINARY) {
        throw std::runtime_error("Only 32-bit and 64-bit Windows executables are supported");
    }
    StageInput(directory.InputFile(), stdin_text, deadline);

    SECURITY_ATTRIBUTES inheritable{};
    inheritable.nLength = sizeof(inheritable);
    inheritable.bInheritHandle = TRUE;
    ScopedHandle stdin_file(CreateFileW(directory.InputFile().c_str(), GENERIC_READ,
                                        FILE_SHARE_READ, &inheritable, OPEN_EXISTING,
                                        FILE_ATTRIBUTE_NORMAL, nullptr));
    if (stdin_file.Get() == INVALID_HANDLE_VALUE) {
        throw std::runtime_error(Win32Error("Opening staged stdin failed", GetLastError()));
    }
    ScopedHandle stdout_read, stdout_write, stderr_read, stderr_write;
    MakePipe(stdout_read, stdout_write, inheritable);
    MakePipe(stderr_read, stderr_write, inheritable);

    ScopedHandle job(CreateJobObjectW(nullptr, nullptr));
    if (job.Get() == nullptr) {
        throw std::runtime_error(Win32Error("Creating process job failed", GetLastError()));
    }
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION job_limits{};
    job_limits.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;
    if (!SetInformationJobObject(job.Get(), JobObjectExtendedLimitInformation,
                                 &job_limits, sizeof(job_limits))) {
        throw std::runtime_error(Win32Error("Configuring process job failed", GetLastError()));
    }

    const HANDLE inherited_handles[3] = {stdin_file.Get(), stdout_write.Get(), stderr_write.Get()};
    AttributeList attributes(inherited_handles);
    STARTUPINFOEXW startup{};
    startup.StartupInfo.cb = sizeof(startup);
    startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    startup.StartupInfo.hStdInput = stdin_file.Get();
    startup.StartupInfo.hStdOutput = stdout_write.Get();
    startup.StartupInfo.hStdError = stderr_write.Get();
    startup.lpAttributeList = attributes.Get();

    std::wstring command = L"\"" + absolute_exe.wstring() + L"\"";
    std::vector<wchar_t> command_buffer(command.begin(), command.end());
    command_buffer.push_back(L'\0');
    PROCESS_INFORMATION process_info{};
    CheckDeadline(deadline);
    BOOL launched = FALSE;
    DWORD launch_error = 0;
    {
        // CreateProcess may itself show system error UI for a bad image. Use a thread
        // setting so concurrent judge workers do not race on the process error mode.
        ScopedThreadErrorMode quiet_errors;
        launched = CreateProcessW(absolute_exe.c_str(), command_buffer.data(),
                                  nullptr, nullptr, TRUE,
                                  CREATE_SUSPENDED | CREATE_NO_WINDOW |
                                      EXTENDED_STARTUPINFO_PRESENT,
                                  nullptr, directory.Work().c_str(),
                                  &startup.StartupInfo, &process_info);
        if (!launched) {
            launch_error = GetLastError();
        }
    }
    if (!launched) {
        throw std::runtime_error(Win32Error("Launching process failed", launch_error));
    }
    ScopedHandle process(process_info.hProcess);
    ScopedHandle thread(process_info.hThread);
    if (!AssignProcessToJobObject(job.Get(), process.Get())) {
        const DWORD error = GetLastError();
        if (!TerminateProcess(process.Get(), 126)) {
            throw std::runtime_error(Win32Error("Assigning process to job failed and termination failed", GetLastError()));
        }
        const DWORD wait = WaitForSingleObject(process.Get(), 1000);
        if (wait != WAIT_OBJECT_0) {
            throw std::runtime_error(wait == WAIT_FAILED
                ? Win32Error("Waiting after failed job assignment failed", GetLastError())
                : "Waiting after failed job assignment exceeded one second");
        }
        throw std::runtime_error(Win32Error("Assigning process to job failed", error));
    }
    if (ResumeThread(thread.Get()) == static_cast<DWORD>(-1)) {
        const DWORD error = GetLastError();
        TerminateTree(job.Get());
        WaitForTreeExit(job.Get(), Clock::now() + std::chrono::seconds(1));
        throw std::runtime_error(Win32Error("Resuming process failed", error));
    }
    thread.Reset();
    stdin_file.Reset();
    stdout_write.Reset();
    stderr_write.Reset();

    PipeState stdout_state = PipeState::Open;
    PipeState stderr_state = PipeState::Open;
    std::uint64_t captured_total = 0;
    bool main_exited = false;
    bool tree_terminated = false;
    bool output_exceeded = false;
    bool timed_out = false;
    try {
        while (true) {
            if (Clock::now() >= deadline) {
                timed_out = true;
                break;
            }
            bool read_anything = false;
            if (DrainAvailable(stdout_read.Get(), stdout_state, result.stdout_text,
                               captured_total, max_output_bytes, read_anything)) {
                output_exceeded = true;
                break;
            }
            if (DrainAvailable(stderr_read.Get(), stderr_state, result.stderr_text,
                               captured_total, max_output_bytes, read_anything)) {
                output_exceeded = true;
                break;
            }

            if (!main_exited) {
                const DWORD wait = WaitForSingleObject(process.Get(), 0);
                if (wait == WAIT_FAILED) {
                    throw std::runtime_error(Win32Error("Waiting for process failed", GetLastError()));
                }
                if (wait == WAIT_OBJECT_0) {
                    main_exited = true;
                    DWORD exit_code = 0;
                    if (!GetExitCodeProcess(process.Get(), &exit_code)) {
                        throw std::runtime_error(Win32Error("Querying exit code failed", GetLastError()));
                    }
                    result.exit_code = static_cast<int>(exit_code);
                    TerminateTree(job.Get()); // Also stops descendants retaining output handles.
                    tree_terminated = true;
                }
            }
            if (main_exited && stdout_state == PipeState::Closed &&
                stderr_state == PipeState::Closed) {
                break;
            }
            if (!read_anything) {
                Sleep(2);
            }
        }
    } catch (...) {
        const std::exception_ptr original = std::current_exception();
        try {
            if (!tree_terminated) {
                TerminateTree(job.Get());
            }
            WaitForTreeExit(job.Get(), Clock::now() + std::chrono::seconds(1));
        } catch (const std::exception& cleanup_error) {
            std::string original_message = "unknown execution error";
            try {
                std::rethrow_exception(original);
            } catch (const std::exception& execution_error) {
                original_message = execution_error.what();
            } catch (...) {
            }
            throw std::runtime_error(original_message +
                "; process cleanup failed: " + cleanup_error.what());
        }
        std::rethrow_exception(original);
    }
    if (!tree_terminated) {
        TerminateTree(job.Get());
    }
    WaitForTreeExit(job.Get(), Clock::now() + std::chrono::seconds(1));
    if (!main_exited) {
        const DWORD wait = WaitForSingleObject(process.Get(), 1000);
        if (wait != WAIT_OBJECT_0) {
            throw std::runtime_error(wait == WAIT_FAILED
                ? Win32Error("Waiting for terminated process failed", GetLastError())
                : "Waiting for terminated process exceeded one second");
        }
        DWORD exit_code = 0;
        if (!GetExitCodeProcess(process.Get(), &exit_code)) {
            throw std::runtime_error(Win32Error("Querying terminated process failed", GetLastError()));
        }
        result.exit_code = static_cast<int>(exit_code);
    }
    result.stdout_size = result.stdout_text.size();
    result.time_ms = ElapsedMs(start);
    if (timed_out) {
        result.status = CaseStatus::TLE;
        result.message = "Time limit exceeded";
    } else if (output_exceeded) {
        result.status = CaseStatus::OLE;
        result.message = "Combined stdout/stderr limit exceeded";
    } else if (result.exit_code != 0) {
        result.status = CaseStatus::RE;
        result.message = "Process exited with non-zero code";
    } else {
        result.status = CaseStatus::AC;
        result.message = "Execution succeeded";
    }
    return result;
}

}  // namespace

ExecResult ProcessRunner::Run(const std::filesystem::path& exe_path,
                              const std::string& stdin_text,
                              int time_limit_ms,
                              std::uint64_t max_output_bytes) const {
    ExecResult result;
    if (time_limit_ms <= 0 || time_limit_ms > 3600000 ||
        max_output_bytes == 0 || max_output_bytes > 64ULL * 1024 * 1024) {
        result.status = CaseStatus::SE;
        result.message = "Limits must be within 1..3600000 ms and 1..67108864 bytes";
        return result;
    }
    const auto start = Clock::now();
    const auto deadline = start + std::chrono::milliseconds(time_limit_ms);
    try {
        RunDirectory directory;
        try {
            result = RunImpl(exe_path, stdin_text, max_output_bytes, directory, start, deadline);
        } catch (const TimeExpired& ex) {
            result.status = CaseStatus::TLE;
            result.message = ex.what();
        } catch (const std::exception& ex) {
            result.status = CaseStatus::SE;
            result.message = ex.what();
        }
        if (const auto error = directory.Cleanup()) {
            result.status = CaseStatus::SE;
            result.message += "; cleaning run directory failed: " + error.message();
        }
    } catch (const std::exception& ex) {
        result.status = CaseStatus::SE;
        result.message = ex.what();
    }
    result.time_ms = ElapsedMs(start);
    result.stdout_size = result.stdout_text.size();
    return result;
}

}  // namespace mini_judge
