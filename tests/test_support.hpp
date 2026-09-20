#pragma once

#include <Windows.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <stdexcept>
#include <string>

inline void TestCheck(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        throw std::runtime_error(std::string(file) + ":" + std::to_string(line) +
                                 ": CHECK failed: " + expression);
    }
}

#define TEST_CHECK(expression) TestCheck(static_cast<bool>(expression), #expression, __FILE__, __LINE__)

template <typename Function>
void TestExpectThrows(Function&& function) {
    bool threw = false;
    try {
        function();
    } catch (const std::exception&) {
        threw = true;
    }
    TEST_CHECK(threw);
}

class TestTempDir {
public:
    explicit TestTempDir(const std::string& label) {
        static std::atomic<unsigned long> serial{0};
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int attempt = 0; attempt < 100; ++attempt) {
            path = std::filesystem::temp_directory_path() /
                   ("mini_judge_" + label + "_" + std::to_string(GetCurrentProcessId()) + "_" +
                    std::to_string(stamp) + "_" + std::to_string(++serial));
            if (std::filesystem::create_directory(path)) {
                created_ = true;
                return;
            }
        }
        throw std::runtime_error("Could not create unique test directory");
    }

    ~TestTempDir() {
        if (created_) {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    }

    TestTempDir(const TestTempDir&) = delete;
    TestTempDir& operator=(const TestTempDir&) = delete;

    std::filesystem::path path;

private:
    bool created_ = false;
};

inline void TestWriteFile(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    TEST_CHECK(out.is_open());
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    TEST_CHECK(out.good());
}

inline std::string TestReadFile(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    TEST_CHECK(in.is_open());
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

inline std::string TestReplaceOnce(std::string text, const std::string& from, const std::string& to) {
    const auto position = text.find(from);
    TEST_CHECK(position != std::string::npos);
    text.replace(position, from.size(), to);
    return text;
}
